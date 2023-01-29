/** GUI/GTK+: listview
2019,2022 Simon Zolin
*/

#pragma once
#include "gtk.h"

struct ffui_view_disp {
	uint idx;
	uint sub;
	ffstr text;
};

struct ffui_view {
	GtkWidget *h;
	enum FFUI_UID uid;
	ffui_wnd *wnd;
	GtkTreeModel *store;
	GtkCellRenderer *rend;
	uint dblclick_id;
	uint dropfile_id;
	uint dispinfo_id;
	uint edit_id;
	ffui_menu *popup_menu;

	union {
	GtkTreePath *path; // dblclick_id
	struct { // edit_id
		ffuint idx;
		const char *new_text;
	} edited;
	ffstr drop_data;
	struct ffui_view_disp disp;
	};
};


typedef struct ffui_viewcol ffui_viewcol;
struct ffui_viewcol {
	char *text;
	uint width;
};

#define ffui_viewcol_reset(vc)  ffmem_free0((vc)->text)

static inline void ffui_viewcol_settext(ffui_viewcol *vc, const char *text, size_t len)
{
	vc->text = ffsz_dupn(text, len);
}

#define ffui_viewcol_setwidth(vc, w)  (vc)->width = (w)
#define ffui_viewcol_width(vc)  ((vc)->width)

static inline void ffui_view_inscol(ffui_view *v, int pos, ffui_viewcol *vc)
{
	_ffui_log("pos:%d", pos);
	FF_ASSERT(v->store == NULL);

	if (pos == -1)
		pos = gtk_tree_view_get_n_columns(GTK_TREE_VIEW(v->h));

	GtkTreeViewColumn *col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(col, vc->text);
	gtk_tree_view_column_set_resizable(col, 1);
	if (vc->width != 0)
		gtk_tree_view_column_set_fixed_width(col, vc->width);

	gtk_tree_view_column_pack_start(col, v->rend, 1);
	gtk_tree_view_column_add_attribute(col, v->rend, "text", pos);

	gtk_tree_view_insert_column(GTK_TREE_VIEW(v->h), col, pos);
	ffui_viewcol_reset(vc);
}

static inline void ffui_view_setcol(ffui_view *v, int pos, ffui_viewcol *vc)
{
	GtkTreeViewColumn *col = gtk_tree_view_get_column(GTK_TREE_VIEW(v->h), pos);
	if (vc->text != NULL)
		gtk_tree_view_column_set_title(col, vc->text);
	if (vc->width != 0)
		gtk_tree_view_column_set_fixed_width(col, vc->width);
}

/** Set column width */
static inline void ffui_view_setcol_width(ffui_view *v, int pos, uint width)
{
	ffui_viewcol vc = { .width = width };
	ffui_view_setcol(v, pos, &vc);
}

static inline void ffui_view_col(ffui_view *v, int pos, ffui_viewcol *vc)
{
	GtkTreeViewColumn *col = gtk_tree_view_get_column(GTK_TREE_VIEW(v->h), pos);
	vc->width = gtk_tree_view_column_get_width(col);
}

/** Get column width */
static inline uint ffui_view_col_width(ffui_view *v, int pos)
{
	ffui_viewcol vc = {};
	ffui_view_col(v, pos, &vc);
	return vc.width;
}

/** Get the number of columns. */
#define ffui_view_ncols(v) \
	gtk_tree_model_get_n_columns(GTK_TREE_MODEL((v)->store))


typedef struct ffui_viewitem {
	char *text;
	int idx;
	uint text_alloc :1;
} ffui_viewitem;

static inline void ffui_view_iteminit(ffui_viewitem *it)
{
	ffmem_tzero(it);
}

static inline void ffui_view_itemreset(ffui_viewitem *it)
{
	if (it->text_alloc) {
		ffmem_free0(it->text);
		it->text_alloc = 0;
	}
}

#define ffui_view_setindex(it, i)  (it)->idx = (i)

#define ffui_view_settextz(it, sz)  (it)->text = (char*)(sz)
static inline void ffui_view_settext(ffui_viewitem *it, const char *text, size_t len)
{
	it->text = ffsz_dupn(text, len);
	it->text_alloc = 1;
}
#define ffui_view_settextstr(it, str)  ffui_view_settext(it, (str)->ptr, (str)->len)


FF_EXTERN int ffui_view_create(ffui_view *v, ffui_wnd *parent);

enum FFUI_VIEW_STYLE {
	FFUI_VIEW_GRIDLINES = 1,
	FFUI_VIEW_MULTI_SELECT = 2,
	FFUI_VIEW_EDITABLE = 4,
};

static inline void ffui_view_style(ffui_view *v, uint flags, uint set)
{
	if (flags & FFUI_VIEW_GRIDLINES) {
		GtkTreeViewGridLines val = (set & FFUI_VIEW_GRIDLINES)
			? GTK_TREE_VIEW_GRID_LINES_BOTH
			: GTK_TREE_VIEW_GRID_LINES_NONE;
		gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(v->h), val);
	}

	if (flags & FFUI_VIEW_MULTI_SELECT) {
		GtkSelectionMode val = (set & FFUI_VIEW_MULTI_SELECT)
			? GTK_SELECTION_MULTIPLE
			: GTK_SELECTION_SINGLE;
		gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(v->h)), val);
	}

	if (flags & FFUI_VIEW_EDITABLE) {
		uint val = !!(set & FFUI_VIEW_EDITABLE);
		g_object_set(v->rend, "editable", val, NULL);
	}
}

#define ffui_view_nitems(v)  gtk_tree_model_iter_n_children((void*)(v)->store, NULL)

/**
first: first index to add or redraw
delta: 0:redraw row */
FF_EXTERN void ffui_view_setdata(ffui_view *v, uint first, int delta);

static inline void ffui_view_clear(ffui_view *v)
{
	if (v->store != NULL)
		gtk_list_store_clear(GTK_LIST_STORE(v->store));
}

#define ffui_view_selall(v)  gtk_tree_selection_select_all(gtk_tree_view_get_selection(GTK_TREE_VIEW((v)->h)))
#define ffui_view_unselall(v)  gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW((v)->h)))

typedef struct ffui_sel {
	ffsize len;
	char *ptr;
	ffsize off;
} ffui_sel;

/** Get array of selected items.
Return ffui_sel*.  Free with ffui_view_sel_free(). */
static inline ffui_sel* ffui_view_getsel(ffui_view *v)
{
	GtkTreeSelection *tvsel = gtk_tree_view_get_selection(GTK_TREE_VIEW(v->h));
	GList *rows = gtk_tree_selection_get_selected_rows(tvsel, NULL);
	uint n = gtk_tree_selection_count_selected_rows(tvsel);

	ffvec a = {};
	if (NULL == ffvec_allocT(&a, n, uint))
		return NULL;

	for (GList *l = rows;  l != NULL;  l = l->next) {
		GtkTreePath *path = (GtkTreePath*)l->data;
		int *ii = gtk_tree_path_get_indices(path);
		*ffvec_pushT(&a, uint) = ii[0];
	}
	g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free);

	ffui_sel *sel = ffmem_new(ffui_sel);
	sel->ptr = (char*)a.ptr;
	sel->len = a.len;
	sel->off = 0;
	return sel;
}

static inline void ffui_view_sel_free(ffui_sel *sel)
{
	if (sel == NULL)
		return;
	ffmem_free(sel->ptr);
	ffmem_free(sel);
}

/** Get next selected item. */
static inline int ffui_view_selnext(ffui_view *v, ffui_sel *sel)
{
	if (sel->off == sel->len)
		return -1;
	return *ffslice_itemT(sel, sel->off++, uint);
}

FF_EXTERN void ffui_view_dragdrop(ffui_view *v, uint action_id);

/** Get next drag'n'drop file name.
Return -1 if no more. */
FF_EXTERN int ffui_fdrop_next(ffvec *fn, ffstr *dropdata);


/**
Note: must be called only from wnd.on_action(). */
static inline int ffui_view_focused(ffui_view *v)
{
	FF_ASSERT(v->path != NULL);
	int *i = gtk_tree_path_get_indices(v->path);
	return i[0];
}

FF_EXTERN void ffui_view_ins(ffui_view *v, int pos, ffui_viewitem *it);

#define ffui_view_append(v, it)  ffui_view_ins(v, -1, it)

static inline void ffui_view_set(ffui_view *v, int sub, ffui_viewitem *it)
{
	_ffui_log("sub:%d", sub);
	GtkTreeIter iter;
	if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(v->store), &iter, NULL, it->idx))
		gtk_list_store_set(GTK_LIST_STORE(v->store), &iter, sub, it->text, -1);
	ffui_view_itemreset(it);
}

/** Set cell text */
static inline void ffui_view_set_i_textz(ffui_view *v, int idx, int sub, const char *sz)
{
	ffui_viewitem it = {};
	it.idx = idx;
	it.text = (char*)sz;
	ffui_view_set(v, sub, &it);
}

static inline void ffui_view_rm(ffui_view *v, ffui_viewitem *it)
{
	_ffui_log("idx:%d", it->idx);
	GtkTreeIter iter;
	if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(v->store), &iter, NULL, it->idx))
		gtk_list_store_remove(GTK_LIST_STORE(v->store), &iter);
}

/** Scroll view so that the row is visible */
static inline void ffui_view_scroll_idx(ffui_view *v, uint idx)
{
	GtkTreePath *path = gtk_tree_path_new_from_indices(idx, -1);
	gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(v->h), path, NULL, 0, 0, 0);
	gtk_tree_path_free(path);
}

static inline uint ffui_view_scroll_vert(ffui_view *v)
{
	GtkAdjustment *a = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(v->h));
	double d = gtk_adjustment_get_value(a);
	return d * 100;
}

static inline void ffui_view_scroll_setvert(ffui_view *v, uint val)
{
	GtkAdjustment *a = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(v->h));
	gtk_adjustment_set_value(a, (double)val / 100);
}

static inline void ffui_view_popupmenu(ffui_view *v, ffui_menu *m)
{
	v->popup_menu = m;
	if (m != NULL) {
		g_object_set_data(G_OBJECT(m->h), "ffdata", v->wnd);
		gtk_widget_show_all(m->h);
	}
}
