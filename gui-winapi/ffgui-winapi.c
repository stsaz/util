/**
Copyright (c) 2014 Simon Zolin
*/

#define COBJMACROS
#include "winapi.h"
#include <FFOS/file.h>
#include <FFOS/process.h>
#include <FFOS/dir.h>
#include <FFOS/thread.h>


static uint _curthd_id; //ID of the thread running GUI message loop
int _ffui_dpi;
RECT _ffui_screen_area;

enum {
	_FFUI_WNDSTYLE = 1
};
static uint _ffui_flags;

static HWND create(enum FFUI_UID uid, const ffsyschar *text, HWND parent, const ffui_pos *r, uint style, uint exstyle, void *param);
static int ctl_create(ffui_ctl *c, enum FFUI_UID uid, HWND parent);
static void getpos_noscale(HWND h, ffui_pos *r);
static int setpos_noscale(void *ctl, int x, int y, int cx, int cy, int flags);
static HWND base_parent(HWND h);
static void _ffui_ctl_subclass(ffui_label *c);
static LRESULT __stdcall _ffui_ctl_proc(HWND h, uint msg, WPARAM w, LPARAM l);

static void wnd_onaction(ffui_wnd *wnd, int id);
static LRESULT __stdcall wnd_proc(HWND h, uint msg, WPARAM w, LPARAM l);

static int process_accels(MSG *m);

#define _ffui_log(...)


struct ctlinfo {
	const char *stype;
	const ffsyschar *sid;
	uint style;
	uint exstyle;
};

static const struct ctlinfo ctls[] = {
	{ "",	L"", 0, 0 },
	{ "window",	L"FF_WNDCLASS", WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0 },
	{ "label",	L"STATIC", SS_NOTIFY, 0 },
	{ "image",	L"STATIC", SS_ICON | SS_NOTIFY, 0 },
	{ "editbox",	L"EDIT", ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_NOHIDESEL/* | WS_TABSTOP*/
		, WS_EX_CLIENTEDGE },
	{ "text",	L"EDIT", ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_NOHIDESEL | WS_HSCROLL | WS_VSCROLL
		, WS_EX_CLIENTEDGE },
	{ "combobox",	L"COMBOBOX", CBS_DROPDOWN | CBS_AUTOHSCROLL, WS_EX_CLIENTEDGE },
	{ "button",	L"BUTTON", 0, 0 },
	{ "checkbox",	L"BUTTON", BS_AUTOCHECKBOX, 0 },
	{ "radiobutton",	L"BUTTON", BS_AUTORADIOBUTTON, 0 },

	{ "trackbar",	L"msctls_trackbar32", 0, 0 },
	{ "progressbar",	L"msctls_progress32", 0, 0 },
	{ "status_bar",	L"msctls_statusbar32", SBARS_SIZEGRIP, 0 },

	{ "tab",	L"SysTabControl32", TCS_FOCUSNEVER, 0 },
	{ "listview",	L"SysListView32", WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS
		| LVS_AUTOARRANGE | LVS_SHAREIMAGELISTS, 0 },
	{ "treeview",	L"SysTreeView32", WS_BORDER | TVS_SHOWSELALWAYS | TVS_INFOTIP, 0 },
};


int ffui_init(void)
{
	_curthd_id = ffthread_curid();
	HDC hdc = GetDC(NULL);
	if (hdc != NULL) {
		_ffui_dpi = GetDeviceCaps(hdc, LOGPIXELSX);
		ReleaseDC(NULL, hdc);
	}

	CoInitializeEx(NULL, 0);
	ffui_screenarea(&_ffui_screen_area);
	return 0;
}

void ffui_uninit(void)
{
	if (_ffui_flags & _FFUI_WNDSTYLE)
		UnregisterClassW(ctls[FFUI_UID_WINDOW].sid, GetModuleHandleW(NULL));
	_ffui_dpi = 0;
}


static const byte _ffui_ikeys[] = {
	VK_OEM_7,
	VK_OEM_2,
	VK_OEM_4,
	VK_OEM_5,
	VK_OEM_6,
	VK_OEM_3,
	VK_BACK,
	VK_PAUSE,
	VK_DELETE,
	VK_DOWN,
	VK_END,
	VK_RETURN,
	VK_ESCAPE,
	VK_F1,
	VK_F10,
	VK_F11,
	VK_F12,
	VK_F2,
	VK_F3,
	VK_F4,
	VK_F5,
	VK_F6,
	VK_F7,
	VK_F8,
	VK_F9,
	VK_HOME,
	VK_INSERT,
	VK_LEFT,
	VK_RIGHT,
	VK_SPACE,
	VK_TAB,
	VK_UP,
};

static const char *const _ffui_keystr[] = {
	"'",
	"/",
	"[",
	"\\",
	"]",
	"`",
	"backspace",
	"break",
	"delete",
	"down",
	"end",
	"enter",
	"escape",
	"f1",
	"f10",
	"f11",
	"f12",
	"f2",
	"f3",
	"f4",
	"f5",
	"f6",
	"f7",
	"f8",
	"f9",
	"home",
	"insert",
	"left",
	"right",
	"space",
	"tab",
	"up",
};

ffui_hotkey ffui_hotkey_parse(const char *s, size_t len)
{
	int r = 0, f;
	ffstr v, d = FFSTR_INITN(s, len);
	enum {
		fctrl = FCONTROL << 16,
		fshift = FSHIFT << 16,
		falt = FALT << 16,
	};

	if (d.len == 0)
		goto fail;

	while (d.len != 0) {
		ffstr_splitby(&d, '+', &v, &d);

		if (ffstr_ieqcz(&v, "ctrl"))
			f = fctrl;
		else if (ffstr_ieqcz(&v, "alt"))
			f = falt;
		else if (ffstr_ieqcz(&v, "shift"))
			f = fshift;
		else {
			if (d.len != 0)
				goto fail; //the 2nd key is an error
			break;
		}

		if (r & f)
			goto fail;
		r |= f;
	}
	int ch = v.ptr[0], lch = v.ptr[0] | 0x20;
	if (v.len == 1 && ((lch >= 'a' && lch <= 'z') || (ch >= '0' && ch <= '9')))
		r |= v.ptr[0];

	else {
		ssize_t ikey = ffszarr_ifindsorted(_ffui_keystr, FF_COUNT(_ffui_keystr), v.ptr, v.len);
		if (ikey == -1)
			goto fail; //unknown key
		r |= _ffui_ikeys[ikey];
	}

	return r;

fail:
	return 0;
}

int ffui_hotkey_register(void *ctl, ffui_hotkey hk)
{
	ffui_ctl *c = ctl;
	char name[32];
	ATOM id;

	ffs_format(name, sizeof(name), "ffhk%xu%xu%Z", (uint)(hk >> 16) & 0xff, (uint)hk & 0xff);
	id = GlobalAddAtomA(name);

	uint mod = 0;
	if ((hk >> 16) & FCONTROL)
		mod |= MOD_CONTROL;
	if ((hk >> 16) & FALT)
		mod |= MOD_ALT;
	if ((hk >> 16) & FSHIFT)
		mod |= MOD_SHIFT;
	if (!RegisterHotKey(c->h, id, mod, hk & 0xff)) {
		GlobalDeleteAtom(id);
		return 0;
	}

	return id;
}

void ffui_hotkey_unreg(void *ctl, int id)
{
	ffui_ctl *c = ctl;
	UnregisterHotKey(c->h, id);
	GlobalDeleteAtom(id);
}


#define dpi_descale(x)  (((x) * 96) / _ffui_dpi)
#define dpi_scale(x)  (((x) * _ffui_dpi) / 96)


int ffui_iconlist_create(ffui_iconlist *il, uint width, uint height)
{
	if (NULL == (il->h = ImageList_Create(dpi_scale(width), dpi_scale(height), ILC_MASK | ILC_COLOR32, 1, 0)))
		return -1;
	return 0;
}


void ffui_font_set(ffui_font *fnt, const ffstr *name, int height, uint flags)
{
	if (name != NULL) {
		fnt->lf.lfCharSet = OEM_CHARSET;
		fnt->lf.lfQuality = PROOF_QUALITY;
		ffsz_utow_n(fnt->lf.lfFaceName, FF_COUNT(fnt->lf.lfFaceName), name->ptr, name->len);
	}
	if (height != 0) {
		fnt->lf.lfHeight = -(LONG)(height * _ffui_dpi / 72);
	}
	if (flags & FFUI_FONT_BOLD)
		fnt->lf.lfWeight = FW_BOLD;
	if (flags & FFUI_FONT_ITALIC)
		fnt->lf.lfItalic = 1;
	if (flags & FFUI_FONT_UNDERLINE)
		fnt->lf.lfUnderline = 1;
}


static int setpos_noscale(void *ctl, int x, int y, int cx, int cy, int flags)
{
	return !SetWindowPos(((ffui_ctl*)ctl)->h, HWND_TOP, x, y, cx, cy, SWP_NOACTIVATE | flags);
}

int ffui_setpos(void *ctl, int x, int y, int cx, int cy, int flags)
{
	return !SetWindowPos(((ffui_ctl*)ctl)->h, HWND_TOP, dpi_scale(x), dpi_scale(y)
		, dpi_scale(cx), dpi_scale(cy), SWP_NOACTIVATE | flags);
}

static void ffui_pos_fromrect(ffui_pos *pos, const RECT *rect)
{
	pos->x = rect->left;
	pos->y = rect->top;
	pos->cx = rect->right - rect->left;
	pos->cy = rect->bottom - rect->top;
}

static void getpos_noscale(HWND h, ffui_pos *r)
{
	HWND parent;
	RECT rect;
	GetWindowRect(h, &rect);
	ffui_pos_fromrect(r, &rect);

	parent = GetParent(h);
	if (parent != NULL) {
		POINT pt;
		pt.x = rect.left;
		pt.y = rect.top;
		ScreenToClient(parent, &pt);
		r->x = pt.x;
		r->y = pt.y;
	}
}

void ffui_getpos2(void *ctl, ffui_pos *r, uint flags)
{
	ffui_ctl *c = ctl;
	if (flags & FFUI_FPOS_REL)
		getpos_noscale(c->h, r);
	else {
		RECT rect;
		if (flags & FFUI_FPOS_CLIENT)
			GetClientRect(c->h, &rect);
		else
			GetWindowRect(c->h, &rect);
		ffui_pos_fromrect(r, &rect);
	}
	if (flags & FFUI_FPOS_DPISCALE) {
		ffui_dpi_descalepos(r);
	}
}

static HWND create(enum FFUI_UID uid, const ffsyschar *text, HWND parent, const ffui_pos *r, uint style, uint exstyle, void *param)
{
	HINSTANCE inst = NULL;
	if (uid == FFUI_UID_WINDOW)
		inst = GetModuleHandleW(NULL);

	return CreateWindowExW(exstyle, ctls[uid].sid, text, style
		, dpi_scale(r->x), dpi_scale(r->y), dpi_scale(r->cx), dpi_scale(r->cy)
		, parent, NULL, inst, param);
}

static int ctl_create2(ffui_ctl *c, enum FFUI_UID uid, HWND parent, uint style, uint exstyle)
{
	ffui_pos r = {};
	c->uid = uid;
	if (0 == (c->h = create(uid, L"", parent, &r
		, ctls[uid].style | WS_CHILD | style
		, ctls[uid].exstyle | exstyle
		, NULL)))
		return 1;
	ffui_setctl(c->h, c);
	return 0;
}

static int ctl_create(ffui_ctl *c, enum FFUI_UID uid, HWND parent)
{
	return ctl_create2(c, uid, parent, 0, 0);
}

int ffui_ctl_destroy(void *_c)
{
	ffui_ctl *c = _c;
	int r = 0;
	if (c->font != NULL)
		DeleteObject(c->font);
	if (c->h != NULL)
		r = !DestroyWindow(c->h);
	return r;
}

int ffui_settext(void *c, const char *text, size_t len)
{
	ffsyschar *w, ws[255];
	size_t n = FF_COUNT(ws) - 1;
	int r;
	if (NULL == (w = ffs_utow(ws, &n, text, len)))
		return -1;
	w[n] = '\0';
	r = ffui_settext_q(((ffui_ctl*)c)->h, w);
	if (w != ws)
		ffmem_free(w);
	return r;
}

int ffui_textstr(void *_c, ffstr *dst)
{
	ffui_ctl *c = _c;
	size_t len = ffui_ctl_send(c, WM_GETTEXTLENGTH, 0, 0);
	ffsyschar *w, ws[255];

	if (len < FF_COUNT(ws)) {
		w = ws;
	} else {
		if (NULL == (w = ffws_alloc(len + 1)))
			goto fail;
	}
	ffui_text_q(c->h, w, len + 1);

	dst->len = ff_wtou(NULL, 0, w, len, 0);
	if (NULL == (dst->ptr = ffmem_alloc(dst->len + 1)))
		goto fail;

	ff_wtou(dst->ptr, dst->len + 1, w, len + 1, 0);
	if (w != ws)
		ffmem_free(w);
	return (int)dst->len;

fail:
	if (w != ws)
		ffmem_free(w);
	dst->len = 0;
	return -1;
}

int ffui_ctl_setcursor(void *c, HCURSOR h)
{
	union ffui_anyctl any;
	any.ctl = c;
	if (any.ctl->uid == FFUI_UID_LABEL) {
		any.lbl->cursor = h;
		_ffui_ctl_subclass(any.lbl);
	}
	return 0;
}

void* ffui_ctl_parent(void *c)
{
	ffui_ctl *ctl = c;
	HWND h;
	if (NULL == (h = (HWND)GetWindowLongPtrW(ctl->h, GWLP_HWNDPARENT)))
		return NULL;
	return ffui_getctl(h);
}

static void _ffui_ctl_subclass(ffui_label *c)
{
	c->oldwndproc = (WNDPROC)SetWindowLongPtrW(c->h, GWLP_WNDPROC, (LONG_PTR)&_ffui_ctl_proc);
}

static int ffui_ctl_wndproc(ffui_ctl *c, size_t *code, uint msg, size_t w, size_t l)
{
	union ffui_anyctl any;
	any.ctl = c;

	switch (msg) {

	case WM_SETCURSOR: {
		HCURSOR cur = NULL;
		if (c->uid == FFUI_UID_LABEL)
			cur = any.lbl->cursor;
		if (cur != NULL) {
			// if (LOWORD(l) == HTCLIENT)
			SetCursor(cur);
			return 1;
		}
	}
	}

	if (c->uid == FFUI_UID_LABEL)
		return CallWindowProc(any.lbl->oldwndproc, c->h, msg, w, l);
	return 0;
}

static LRESULT __stdcall _ffui_ctl_proc(HWND h, uint msg, WPARAM w, LPARAM l)
{
	ffui_ctl *c = ffui_getctl(h);
	return ffui_ctl_wndproc((void*)c, NULL, msg, w, l);
}


const char* ffui_fdrop_next(ffui_fdrop *df)
{
	uint nbuf;
	wchar_t *w, ws[255];

	nbuf = DragQueryFileW(df->hdrop, df->idx, NULL, 0);
	if (nbuf == 0)
		return NULL;
	nbuf++;

	if (nbuf < FF_COUNT(ws))
		w = ws;
	else if (NULL == (w = ffws_alloc(nbuf)))
		return NULL;

	DragQueryFileW(df->hdrop, df->idx++, w, nbuf);

	ffmem_free(df->fn);
	df->fn = ffsz_alloc_wtou(w);
	if (w != ws)
		ffmem_free(w);
	return df->fn;
}


int ffui_menu_settext(ffui_menuitem *mi, const char *s, size_t len)
{
	ffsyschar *w;
	size_t n;
	if (NULL == (w = ffs_utow(NULL, &n, s, len)))
		return -1;
	w[n] = '\0';
	ffui_menu_settext_q(mi, w);
	// ffmem_free(w)
	return 0;
}

void ffui_menu_sethotkey(ffui_menuitem *mi, const char *s, size_t len)
{
	ffsyschar *w = mi->dwTypeData;
	size_t textlen = ffq_len(w);
	size_t cap = textlen + FFS_LEN("\t") + len + 1;
	if (mi->dwTypeData == NULL)
		return;

	if (NULL == (w = ffmem_realloc(w, cap * sizeof(ffsyschar))))
		return;
	mi->dwTypeData = w;
	w += textlen;
	*w++ = '\t';
	ffsz_utow_n(w, cap, s, len);
}


void ffui_paned_create(ffui_paned *pn, ffui_wnd *parent)
{
	ffui_paned *p;

	if (parent->paned_first == NULL) {
		parent->paned_first = pn;
		return;
	}

	for (p = parent->paned_first;  p->next != NULL;  p = p->next) {
	}
	p->next = pn;
}


int ffui_stbar_create(ffui_stbar *c, ffui_wnd *parent)
{
	if (0 != ctl_create((void*)c, FFUI_UID_STATUSBAR, parent->h))
		return 1;
	parent->stbar = c;
	return 0;
}

void ffui_stbar_settext(ffui_stbar *sb, int idx, const char *text, size_t len)
{
	ffsyschar *w, ws[255];
	size_t n = FF_COUNT(ws) - 1;
	if (NULL == (w = ffs_utow(ws, &n, text, len)))
		return;
	w[n] = '\0';
	ffui_stbar_settext_q(sb->h, idx, w);
	if (w != ws)
		ffmem_free(w);
}


int ffui_lbl_create(ffui_label *c, ffui_wnd *parent)
{
	if (0 != ctl_create((void*)c, FFUI_UID_LABEL, parent->h))
		return 1;

	if (parent->font != NULL)
		ffui_ctl_send(c, WM_SETFONT, parent->font, 0);

	return 0;
}


int ffui_img_create(ffui_img *im, ffui_wnd *parent)
{
	if (0 != ctl_create((void*)im, FFUI_UID_IMAGE, parent->h))
		return 1;

	return 0;
}


int ffui_edit_create(ffui_ctl *c, ffui_wnd *parent)
{
	if (0 != ctl_create(c, FFUI_UID_EDITBOX, parent->h))
		return 1;

	if (parent->font != NULL)
		ffui_ctl_send(c, WM_SETFONT, parent->font, 0);

	return 0;
}

int ffui_text_create(ffui_ctl *c, ffui_wnd *parent)
{
	if (0 != ctl_create(c, FFUI_UID_TEXT, parent->h))
		return 1;

	if (parent->font != NULL)
		ffui_ctl_send(c, WM_SETFONT, parent->font, 0);

	return 0;
}

void ffui_edit_addtext_q(HWND h, const wchar_t *text)
{
	size_t len = ffui_send(h, WM_GETTEXTLENGTH, 0, 0);
	ffui_send(h, EM_SETSEL, len, -1);
	ffui_send(h, EM_REPLACESEL, 0 /*can undo*/, text);
}

int ffui_edit_addtext(ffui_edit *c, const char *text, size_t len)
{
	ffsyschar *w, ws[255];
	size_t n = FF_COUNT(ws) - 1;
	if (NULL == (w = ffs_utow(ws, &n, text, len)))
		return -1;
	w[n] = '\0';
	ffui_edit_addtext_q(c->h, w);
	if (w != ws)
		ffmem_free(w);
	return 0;
}


int ffui_combx_create(ffui_ctl *c, ffui_wnd *parent)
{
	if (0 != ctl_create(c, FFUI_UID_COMBOBOX, parent->h))
		return 1;

	if (parent->font != NULL)
		ffui_ctl_send(c, WM_SETFONT, parent->font, 0);

	return 0;
}

void ffui_combx_ins(ffui_combx *c, int idx, const char *txt, size_t len)
{
	ffsyschar *w, ws[255];
	size_t n = FF_COUNT(ws) - 1;
	if (NULL == (w = ffs_utow(ws, &n, txt, len)))
		return;
	w[n] = '\0';
	ffui_combx_ins_q(c, idx, w);
	if (w != ws)
		ffmem_free(w);
}

int ffui_combx_textstr(ffui_ctl *c, uint idx, ffstr *dst)
{
	size_t len = ffui_ctl_send(c, CB_GETLBTEXTLEN, idx, 0);
	ffsyschar *w, ws[255];

	if (len < FF_COUNT(ws)) {
		w = ws;
	} else {
		if (NULL == (w = ffws_alloc(len + 1)))
			goto fail;
	}
	ffui_combx_text_q(c, idx, w);

	dst->len = ff_wtou(NULL, 0, w, len, 0);
	if (NULL == (dst->ptr = ffmem_alloc(dst->len + 1)))
		goto fail;

	ff_wtou(dst->ptr, dst->len + 1, w, len + 1, 0);
	if (w != ws)
		ffmem_free(w);
	return (int)dst->len;

fail:
	if (w != ws)
		ffmem_free(w);
	dst->len = 0;
	return -1;
}


int ffui_btn_create(ffui_ctl *c, ffui_wnd *parent)
{
	if (0 != ctl_create(c, FFUI_UID_BUTTON, parent->h))
		return 1;

	if (parent->font != NULL)
		ffui_ctl_send(c, WM_SETFONT, parent->font, 0);

	return 0;
}


int ffui_chbox_create(ffui_ctl *c, ffui_wnd *parent)
{
	if (0 != ctl_create(c, FFUI_UID_CHECKBOX, parent->h))
		return 1;

	if (parent->font != NULL)
		ffui_ctl_send(c, WM_SETFONT, parent->font, 0);

	return 0;
}


int ffui_radio_create(ffui_ctl *c, ffui_wnd *parent)
{
	if (0 != ctl_create(c, FFUI_UID_RADIO, parent->h))
		return 1;

	if (parent->font != NULL)
		ffui_ctl_send(c, WM_SETFONT, parent->font, 0);

	return 0;
}


int ffui_trk_create(ffui_trkbar *t, ffui_wnd *parent)
{
	if (0 != ctl_create((ffui_ctl*)t, FFUI_UID_TRACKBAR, parent->h))
		return 1;
	return 0;
}

void ffui_trk_move(ffui_trkbar *t, uint cmd)
{
	uint pgsize = ffui_ctl_send(t, TBM_GETPAGESIZE, 0, 0);
	uint pos = ffui_trk_val(t);
	switch (cmd) {
	case FFUI_TRK_PGUP:
		pos += pgsize;
		break;
	case FFUI_TRK_PGDN:
		pos -= pgsize;
		break;
	}
	ffui_trk_set(t, pos);
}


int ffui_pgs_create(ffui_ctl *c, ffui_wnd *parent)
{
	if (0 != ctl_create(c, FFUI_UID_PROGRESSBAR, parent->h))
		return 1;
	return 0;
}


int ffui_tab_create(ffui_tab *t, ffui_wnd *parent)
{
	if (0 != ctl_create((ffui_ctl*)t, FFUI_UID_TAB, parent->h))
		return 1;
	if (parent->font != NULL)
		ffui_ctl_send((ffui_ctl*)t, WM_SETFONT, parent->font, 0);
	return 0;
}

void ffui_tab_settext(ffui_tabitem *it, const char *txt, size_t len)
{
	size_t n = FF_COUNT(it->wtext) - 1;
	if (NULL == (it->w = ffs_utow(it->wtext, &n, txt, len)))
		return;
	it->w[n] = '\0';
	ffui_tab_settext_q(it, it->w);
}


int ffui_view_create(ffui_view *c, ffui_wnd *parent)
{
	uint s = (c->dispinfo_id != 0) ? LVS_OWNERDATA : 0;
	if (0 != ctl_create2((ffui_ctl*)c, FFUI_UID_LISTVIEW, parent->h, s, 0))
		return 1;

	if (c->dispinfo_id != 0)
		ListView_SetCallbackMask(c->h, LVIS_STATEIMAGEMASK);

	int n = LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP;
	ListView_SetExtendedListViewStyleEx(c->h, n, n);
	return 0;
}

int ffui_view_itempos(ffui_view *v, uint idx, ffui_pos *pos)
{
	RECT r = {};
	r.left = LVIR_BOUNDS;
	int ret = ffui_ctl_send(v, LVM_GETITEMRECT, idx, &r);
	ffui_pos_fromrect(pos, &r);
	return ret;
}

void ffui_viewcol_settext(ffui_viewcol *vc, const char *text, size_t len)
{
	size_t n;
	if (0 == (n = ff_utow(vc->text, FF_COUNT(vc->text) - 1, text, len, 0))
		&& len != 0)
		return;
	vc->text[n] = '\0';
	ffui_viewcol_settext_q(vc, vc->text);
}

uint ffui_viewcol_width(ffui_viewcol *vc)
{
	return dpi_descale(vc->col.cx);
}

void ffui_viewcol_setwidth(ffui_viewcol *vc, uint w)
{
	vc->col.mask |= LVCF_WIDTH;
	vc->col.cx = dpi_scale(w);
}


void ffui_viewgrp_settext(ffui_viewgrp *vg, const char *text, size_t len)
{
	size_t n;
	if (0 == (n = ff_utow(vg->text, FF_COUNT(vg->text) - 1, text, len, 0))
		&& len != 0)
		return;
	vg->text[n] = '\0';
	ffui_viewgrp_settext_q(vg, vg->text);
}


void ffui_view_itemreset(ffui_viewitem *it)
{
	it->item.mask = 0;
	it->item.stateMask = 0;
	it->item.state = 0;
	if (it->w != it->wtext && it->w != NULL) {
		ffmem_free(it->w);
		it->w = NULL;
	}
}

void ffui_view_settext(ffui_viewitem *it, const char *text, size_t len)
{
	size_t n = FF_COUNT(it->wtext) - 1;
	if (NULL == (it->w = ffs_utow(it->wtext, &n, text, len)))
		return;
	it->w[n] = '\0';
	ffui_view_settext_q(it, it->w);
}

int ffui_view_ins(ffui_view *v, int pos, ffui_viewitem *it)
{
	it->item.iItem = pos;
	it->item.iSubItem = 0;
	pos = ListView_InsertItem(v->h, it);
	ffui_view_itemreset(it);
	return pos;
}

int ffui_view_set(ffui_view *v, int sub, ffui_viewitem *it)
{
	uint check_id = v->check_id, chsel_id = v->chsel_id;
	v->check_id = 0;  v->chsel_id = 0;
	int r;
	it->item.iSubItem = sub;
	r = (0 == ListView_SetItem(v->h, it));
	ffui_view_itemreset(it);
	v->check_id = check_id;
	v->chsel_id = chsel_id;
	return r;
}

int ffui_view_search(ffui_view *v, size_t by)
{
	ffui_viewitem it = {};
	uint i;
	ffui_view_setparam(&it, 0);
	for (i = 0;  ;  i++) {
		ffui_view_setindex(&it, i);
		if (0 != ffui_view_get(v, 0, &it))
			break;
		if (by == (size_t)ffui_view_param(&it))
			return i;
	}
	return -1;
}

int ffui_view_sel_invert(ffui_view *v)
{
	uint i, cnt = 0;
	ffui_viewitem it = {};

	for (i = 0;  ;  i++) {
		ffui_view_setindex(&it, i);
		ffui_view_select(&it, 0);
		if (0 != ffui_view_get(v, 0, &it))
			break;

		if (ffui_view_selected(&it)) {
			ffui_view_select(&it, 0);
		} else {
			ffui_view_select(&it, 1);
			cnt++;
		}

		ffui_view_set(v, 0, &it);
	}

	return cnt;
}

int ffui_view_hittest2(ffui_view *v, const ffui_point *pt, int *subitem, uint *flags)
{
	LVHITTESTINFO ht;
	ffui_point cpt = *pt;

	ffui_screen2client(v, &cpt);

	ht.pt.x = cpt.x;
	ht.pt.y = cpt.y;
	ht.flags = *flags;
	ht.iItem = -1;
	ht.iSubItem = -1;
#if FF_WIN >= 0x0600
	ht.iGroup = -1;
#endif
	if (0 > (int)ffui_ctl_send(v, LVM_SUBITEMHITTEST, -1, &ht))
		return -1;
	if (!(ht.flags & *flags))
		return -1;
	if (subitem != NULL)
		*subitem = ht.iSubItem;
	*flags = ht.flags;
	return ht.iItem;
}

HWND ffui_view_edit(ffui_view *v, uint i, uint sub)
{
	ffui_viewitem it;

	ffui_view_iteminit(&it);
	ffui_view_setindex(&it, i);
	ffui_view_gettext(&it);
	ffui_view_get(v, sub, &it);

	ffui_edit e;
	e.h = _ffui_view_edit(v, i);
	ffui_settext_q(e.h, ffui_view_textq(&it));
	ffui_view_itemreset(&it);
	ffui_edit_selall(&e);
	return e.h;
}

void ffui_view_edit_set(ffui_view *v, uint i, uint sub)
{
	ffui_viewitem it = {};
	ffui_view_setindex(&it, i);
	ffui_view_settextz(&it, v->text);
	ffui_view_set(v, sub, &it);
	ffui_view_itemreset(&it);
}

int ffui_view_edit_hittest(ffui_view *v, uint sub)
{
	int i, isub;
	ffui_point pt;
	ffui_cur_pos(&pt);
	if (-1 == (i = ffui_view_hittest(v, &pt, &isub))
		|| (uint)isub != sub)
		return -1;
	ffui_view_edit(v, i, sub);
	return i;
}

/*
Getting large text: call ListView_GetItem() with a larger buffer until the whole data has been received. */
int ffui_view_get(ffui_view *v, int sub, ffui_viewitem *it)
{
	size_t cap = 4096;
	it->item.iSubItem = sub;

	while (ListView_GetItem(v->h, &it->item)) {

		if (!(it->item.mask & LVIF_TEXT)
			|| ffq_len(it->item.pszText) + 1 != (size_t)it->item.cchTextMax)
			return 0;

		if (NULL == (it->w = ffmem_realloc(it->w, cap * sizeof(ffsyschar))))
			return -1;
		it->item.pszText = it->w;
		it->item.cchTextMax = cap;
		cap *= 2;
	}

	return -1;
}


int ffui_tree_create(ffui_ctl *c, void *parent)
{
	if (0 != ctl_create(c, FFUI_UID_TREEVIEW, ((ffui_ctl*)parent)->h))
		return 1;

#if FF_WIN >= 0x0600
	int n = TVS_EX_DOUBLEBUFFER;
	TreeView_SetExtendedStyle(c->h, n, n);
#endif

	return 0;
}

void ffui_tree_reset(ffui_tvitem *it)
{
	it->ti.mask = 0;
	if (it->w != it->wtext && it->w != NULL) {
		ffmem_free(it->w);
		it->w = NULL;
	}
}

void ffui_tree_settext(ffui_tvitem *it, const char *text, size_t len)
{
	size_t n = FF_COUNT(it->wtext) - 1;
	if (NULL == (it->w = ffs_utow(it->wtext, &n, text, len)))
		return;
	it->w[n] = '\0';
	ffui_tree_settext_q(it, it->w);
}

void* ffui_tree_ins(ffui_view *v, void *parent, void *after, ffui_tvitem *it)
{
	TVINSERTSTRUCTW ins = { 0 };
	ins.hParent = (HTREEITEM)parent;
	ins.hInsertAfter = (HTREEITEM)after;
	ins.item = it->ti;
	void *r = (void*)ffui_send(v->h, TVM_INSERTITEM, 0, &ins);
	ffui_tree_reset(it);
	return r;
}

void ffui_tree_get(ffui_view *v, void *pitem, ffui_tvitem *it)
{
	it->ti.hItem = pitem;
	ffui_send(v->h, TVM_GETITEM, 0, &it->ti);
}

char* ffui_tree_text(ffui_view *t, void *item)
{
	ffsyschar buf[255];
	TVITEMW it = {};
	it.mask = TVIF_TEXT;
	it.pszText = buf;
	it.cchTextMax = FF_COUNT(buf);
	it.hItem = (HTREEITEM)item;
	if (ffui_ctl_send(t, TVM_GETITEM, 0, &it))
		return ffsz_alloc_wtou(buf);
	return NULL;
}


uint ffui_dpi()
{
	return _ffui_dpi;
}

void ffui_dpi_set(uint dpi)
{
	_ffui_dpi = dpi;
}

int ffui_dpi_scale(int x)
{
	return dpi_scale(x);
}
int ffui_dpi_descale(int x)
{
	return dpi_descale(x);
}

void ffui_dpi_scalepos(ffui_pos *r)
{
	r->x = dpi_scale(r->x);
	r->y = dpi_scale(r->y);
	r->cx = dpi_scale(r->cx);
	r->cy = dpi_scale(r->cy);
}

void ffui_dpi_descalepos(ffui_pos *r)
{
	r->x = dpi_descale(r->x);
	r->y = dpi_descale(r->y);
	r->cx = dpi_descale(r->cx);
	r->cy = dpi_descale(r->cy);
}

void ffui_pos_limit(ffui_pos *r, const ffui_pos *screen)
{
	if (r->x < 0)
		r->x = 0;
	if (r->y < 0)
		r->y = 0;

	uint cxmax = screen->cx - screen->x;
	if (r->x + (uint)r->cx > cxmax) {
		r->cx = ffmin(r->cx, cxmax);
		r->x = cxmax - r->cx;
	}

	uint cymax = screen->cy - screen->y;
	if (r->y + (uint)r->cy > cymax) {
		r->cy = ffmin(r->cy, cymax);
		r->y = cymax - r->cy;
	}
}


int ffui_icon_load_q(ffui_icon *ico, const ffsyschar *filename, uint index, uint flags)
{
	HICON *big = NULL, *small = NULL;
	if (flags & FFUI_ICON_SMALL)
		small = &ico->h;
	else
		big = &ico->h;
	return !ExtractIconExW(filename, index, big, small, 1);
}

int ffui_icon_load(ffui_icon *ico, const char *filename, uint index, uint flags)
{
	ffsyschar *w, ws[255];
	size_t n = FF_COUNT(ws) - 1;
	int r;
	if (NULL == (w = ffs_utow(ws, &n, filename, ffsz_len(filename))))
		return -1;
	w[n] = '\0';
	r = ffui_icon_load_q(ico, w, index, flags);
	if (w != ws)
		ffmem_free(w);
	return r;
}

int ffui_icon_loadimg_q(ffui_icon *ico, const ffsyschar *filename, uint cx, uint cy, uint flags)
{
	if (flags & FFUI_ICON_DPISCALE) {
		cx = dpi_scale(cx);
		cy = dpi_scale(cy);
	}
	ico->h = LoadImageW(NULL, filename, IMAGE_ICON, cx, cy, LR_LOADFROMFILE);
	return (ico->h == NULL);
}

int ffui_icon_loadimg(ffui_icon *ico, const char *filename, uint cx, uint cy, uint flags)
{
	ffsyschar *w, ws[255];
	size_t n = FF_COUNT(ws) - 1;
	int r;
	if (NULL == (w = ffs_utow(ws, &n, filename, ffsz_len(filename))))
		return -1;
	w[n] = '\0';
	r = ffui_icon_loadimg_q(ico, w, cx, cy, flags);
	if (w != ws)
		ffmem_free(w);
	return r;
}

int ffui_icon_loadstd(ffui_icon *ico, uint tag)
{
	const ffsyschar *fn;
	uint type = tag & ~0xffff, n = (tag & 0xffff);

	switch (type) {
	case _FFUI_ICON_STD:
		ico->h = LoadImageW(NULL, MAKEINTRESOURCEW(n), IMAGE_ICON, 0, 0, LR_SHARED);
		return (ico->h == NULL);

	case _FFUI_ICON_IMAGERES:
		fn = L"imageres.dll"; break;

	default:
		return -1;
	}
	return ffui_icon_load_q(ico, fn, n, 0);
}

int ffui_icon_loadres(ffui_icon *ico, const ffsyschar *name, uint cx, uint cy)
{
	uint f = (cx == 0 && cy == 0) ? LR_DEFAULTSIZE : 0;
	ico->h = LoadImageW(GetModuleHandleW(NULL), name, IMAGE_ICON, dpi_scale(cx), dpi_scale(cy), f);
	return (ico->h == NULL);
}


void ffui_dlg_destroy(ffui_dialog *d)
{
	ffmem_free(d->names);
	ffmem_free(d->name);
	ffmem_free((void*)d->of.lpstrTitle);
	ffmem_free((void*)d->of.lpstrFilter);
}

void ffui_dlg_title(ffui_dialog *d, const char *title, size_t len)
{
	size_t n;
	ffmem_free((void*)d->of.lpstrTitle);
	if (NULL == (d->of.lpstrTitle = ffs_utow(NULL, &n, title, len)))
		return;
	ffsyschar *w = (void*)d->of.lpstrTitle;
	w[n] = '\0';
}

/* multisel: "dir \0 name1 \0 name2 \0 \0"
   singlesel: "name \0" */
char* ffui_dlg_open(ffui_dialog *d, ffui_wnd *parent)
{
	ffsyschar *w;
	size_t cap = ((d->of.Flags & OFN_ALLOWMULTISELECT) ? 64 * 1024 : 4096);
	if (NULL == (w = ffws_alloc(cap)))
		return NULL;
	w[0] = '\0';

	d->of.hwndOwner = parent->h;
	d->of.lpstrFile = w;
	d->of.nMaxFile = cap;
	if (!GetOpenFileNameW(&d->of)) {
		ffmem_free(w);
		return NULL;
	}

	ffmem_free(d->names);
	d->names = d->pname = w;

	if ((d->of.Flags & OFN_ALLOWMULTISELECT) && w[d->of.nFileOffset - 1] == '\0') {
		d->pname += d->of.nFileOffset; //skip directory

	} else {
		d->pname += ffq_len(w); //for ffui_dlg_nextname() to return NULL
		ffmem_free(d->name);
		if (NULL == (d->name = ffsz_alloc_wtou(w)))
			return NULL;
		return d->name;
	}

	return ffui_dlg_nextname(d);
}

char* ffui_dlg_save(ffui_dialog *d, ffui_wnd *parent, const char *fn, size_t fnlen)
{
	ffsyschar ws[4096];
	size_t n = 0;

	if (fn != NULL)
		n = ff_utow(ws, FF_COUNT(ws), fn, fnlen, 0);
	ws[n] = '\0';

	d->of.hwndOwner = parent->h;
	d->of.lpstrFile = ws;
	d->of.nMaxFile = FF_COUNT(ws);
	if (!GetSaveFileNameW(&d->of))
		return NULL;

	ffmem_free(d->name);
	if (NULL == (d->name = ffsz_alloc_wtou(ws)))
		return NULL;
	return d->name;
}

char* ffui_dlg_nextname(ffui_dialog *d)
{
	size_t cap, namelen = ffq_len(d->pname);

	ffmem_free(d->name); //free the previous name

	if (namelen == 0) {
		ffmem_free(d->names);
		d->names = NULL;
		d->name = NULL;
		return NULL;
	}

	cap = d->of.nFileOffset + ff_wtou(NULL, 0, d->pname, namelen, 0) + 1;
	if (NULL == (d->name = ffmem_alloc(cap)))
		return NULL;

	ffs_format(d->name, cap, "%q\\%*q", d->names, namelen + 1, d->pname);
	d->pname += namelen + 1;
	return d->name;
}


int ffui_msgdlg_show(const char *title, const char *text, size_t len, uint flags)
{
	int r = -1;
	ffsyschar *w = NULL, *wtit = NULL;
	size_t n;

	if (NULL == (w = ffs_utow(NULL, &n, text, len)))
		goto done;
	w[n] = '\0';

	if (NULL == (wtit = ffs_utow(NULL, NULL, title, -1)))
		goto done;

	r = MessageBoxW(NULL, w, wtit, flags);

done:
	ffmem_free(wtit);
	ffmem_free(w);
	return r;
}


static uint tray_id;

void ffui_tray_create(ffui_trayicon *t, ffui_wnd *wnd)
{
	t->nid.cbSize = sizeof(t->nid);
	t->nid.hWnd = wnd->h;
	t->nid.uID = tray_id++;
	t->nid.uFlags = NIF_MESSAGE;
	t->nid.uCallbackMessage = FFUI_WM_USER_TRAY;
	wnd->trayicon = t;
}

int ffui_tray_show(ffui_trayicon *t, uint show)
{
	uint action = show ? NIM_ADD : NIM_DELETE;
	if (show && t->visible)
		action = NIM_MODIFY;
	if (!Shell_NotifyIconW(action, &t->nid))
		return -1;
	t->visible = show;
	return 0;
}


void paned_resize(ffui_paned *pn, ffui_wnd *wnd)
{
	RECT r;
	ffui_pos cr[FF_COUNT(pn->items)];
	uint i, n, x = 0, y = 0, cx, cy, f;

	GetClientRect(wnd->h, &r);

	if (wnd->stbar != NULL) {
		getpos_noscale(wnd->stbar->h, &cr[0]);
		r.bottom -= cr[0].cy;
	}

	for (i = 0;  i != FF_COUNT(pn->items);  i++) {
		if (pn->items[i].it == NULL)
			break;
		getpos_noscale(pn->items[i].it->h, &cr[i]);
	}
	n = i;

	for (i = 0;  i != n;  i++) {
		f = SWP_NOMOVE;

		if (pn->items[i].cx)
			cx = r.right - cr[i].x;
		else
			cx = cr[i].cx;

		if (pn->items[i].x) {
			x = r.right;
			for (uint k = i;  k != n;  k++) {
				x -= cr[k].cx;
			}
			y = cr[i].y;
			f = 0;
		}

		if (pn->items[i].cy)
			cy = r.bottom - cr[i].y;
		else
			cy = cr[i].cy;

		if (i == 0 && pn->items[0].cx && n >= 2) {
			cx = r.right -	cr[0].x;
			for (uint k = 1;  k != n;  k++) {
				cx -= cr[k].cx;
			}
		}

		setpos_noscale(pn->items[i].it, x, y, cx, cy, f);
	}
}


int ffui_wnd_initstyle(void)
{
	WNDCLASSEXW cls = {};
	cls.cbSize = sizeof(cls);
	cls.hInstance = GetModuleHandleW(NULL);
	cls.lpfnWndProc = &wnd_proc;
	cls.lpszClassName = ctls[FFUI_UID_WINDOW].sid;
	cls.hIcon = LoadIcon(cls.hInstance, 0);
	cls.hIconSm = cls.hIcon;
	cls.hCursor = LoadCursor(NULL, IDC_ARROW);
	cls.hbrBackground = (HBRUSH)COLOR_WINDOW;
	if (RegisterClassExW(&cls)) {
		_ffui_flags |= _FFUI_WNDSTYLE;
		return 0;
	}
	return -1;
}

int ffui_wnd_create(ffui_wnd *w)
{
	ffui_pos r = { 0, 0, CW_USEDEFAULT, CW_USEDEFAULT };
	w->uid = FFUI_UID_WINDOW;
	if (w->on_action == NULL)
		w->on_action = &wnd_onaction;
	return 0 == create(FFUI_UID_WINDOW, L"", NULL, &r
		, ctls[FFUI_UID_WINDOW].style | WS_OVERLAPPEDWINDOW
		, ctls[FFUI_UID_WINDOW].exstyle | 0
		, w);
}

int ffui_wnd_destroy(ffui_wnd *w)
{
	if (w->trayicon != NULL)
		ffui_tray_show(w->trayicon, 0);

	if (w->bgcolor != NULL)
		DeleteObject(w->bgcolor);

	ffui_wnd_ghotkey_unreg(w);

	if (w->acceltbl != NULL)
		DestroyAcceleratorTable(w->acceltbl);

	if (w->ttip != NULL)
		DestroyWindow(w->ttip);

	return ffui_ctl_destroy(w);
}

uint ffui_wnd_placement(ffui_wnd *w, ffui_pos *pos)
{
	WINDOWPLACEMENT pl = {};
	pl.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(w->h, &pl);
	ffui_pos_fromrect(pos, &pl.rcNormalPosition);
	if (pl.showCmd == SW_SHOWNORMAL && !IsWindowVisible(w->h))
		pl.showCmd = SW_HIDE;
	return pl.showCmd;
}

void ffui_wnd_setplacement(ffui_wnd *w, uint showcmd, const ffui_pos *pos)
{
	WINDOWPLACEMENT pl = {};
	pl.length = sizeof(WINDOWPLACEMENT);
	pl.showCmd = showcmd;
	ffui_pos_torect(pos, &pl.rcNormalPosition);
	SetWindowPlacement(w->h, &pl);
}

void ffui_wnd_setpopup(ffui_wnd *w)
{
	LONG st = GetWindowLongW(w->h, GWL_STYLE) & ~WS_OVERLAPPEDWINDOW;
	SetWindowLongW(w->h, GWL_STYLE, st | WS_POPUPWINDOW | WS_CAPTION | WS_THICKFRAME);
	w->popup = 1;
}

void ffui_wnd_opacity(ffui_wnd *w, uint percent)
{
	LONG_PTR L = GetWindowLongPtrW(w->h, GWL_EXSTYLE);

	if (percent >= 100) {
		SetWindowLongPtrW(w->h, GWL_EXSTYLE, L & ~WS_EX_LAYERED);
		return;
	}

	if (!(L & WS_EX_LAYERED))
		SetWindowLongPtrW(w->h, GWL_EXSTYLE, L | WS_EX_LAYERED);

	SetLayeredWindowAttributes(w->h, 0, 255 * percent / 100, LWA_ALPHA);
}

int ffui_wnd_tooltip(ffui_wnd *w, ffui_ctl *ctl, const char *text, size_t len)
{
	TTTOOLINFOW ti = {};
	ffsyschar *pw, ws[255];
	size_t n = FF_COUNT(ws) - 1;

	if (w->ttip == NULL
		&& NULL == (w->ttip = CreateWindowExW(0, TOOLTIPS_CLASSW, NULL
			, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP
			, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT
			, NULL, NULL, NULL, NULL)))
		return -1;

	if (NULL == (pw = ffs_utow(ws, &n, text, len)))
		return -1;
	pw[n] = '\0';

	ti.cbSize = sizeof(ti);
	ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
	ti.hwnd = ctl->h;
	ti.uId = (UINT_PTR)ctl->h;
	ti.lpszText = pw;
	ffui_send(w->ttip, TTM_ADDTOOL, 0, &ti);

	if (pw != ws)
		ffmem_free(pw);
	return 0;
}

int ffui_wnd_hotkeys(ffui_wnd *w, const struct ffui_wnd_hotkey *hotkeys, size_t n)
{
	ACCEL *accels;
	int r = -1;

	if (w->acceltbl == NULL) {
		DestroyAcceleratorTable(w->acceltbl);
		w->acceltbl = NULL;
	}

	if (n > (uint)-1
		|| NULL == (accels = ffmem_alloc(n * sizeof(ACCEL))))
		return -1;

	for (size_t i = 0;  i != n;  i++) {
		ACCEL *a = &accels[i];
		a->fVirt = (byte)(hotkeys[i].hk >> 16) | FVIRTKEY;
		a->key = (byte)hotkeys[i].hk;
		a->cmd = hotkeys[i].cmd;
	}

	if (NULL == (w->acceltbl = CreateAcceleratorTable(accels, n)))
		r = -1;
	r = 0;

	ffmem_free(accels);
	return r;
}

struct wnd_ghotkey {
	uint id;
	uint cmd;
};

static int wnd_ghotkey_add(ffui_wnd *w, uint hkid, uint cmd)
{
	struct wnd_ghotkey *hk;
	if (NULL == (hk = ffvec_pushT(&w->ghotkeys, struct wnd_ghotkey)))
		return -1;
	hk->id = hkid;
	hk->cmd = cmd;
	return 0;
}

int ffui_wnd_ghotkey_reg(ffui_wnd *w, uint hk, uint cmd)
{
	uint hkid;
	if (0 == (hkid = ffui_hotkey_register(w, hk)))
		return -1;
	return wnd_ghotkey_add(w, hkid, cmd);
}

void ffui_wnd_ghotkey_unreg(ffui_wnd *w)
{
	struct wnd_ghotkey *hk;
	FFSLICE_WALK(&w->ghotkeys, hk) {
		ffui_hotkey_unreg(w, hk->id);
	}
	ffvec_free(&w->ghotkeys);
}

void ffui_wnd_ghotkey_call(ffui_wnd *w, uint hkid)
{
	struct wnd_ghotkey *hk;
	FFSLICE_WALK(&w->ghotkeys, hk) {
		if (hk->id == hkid) {
			w->on_action(w, hk->cmd);
			break;
		}
	}
}

static void wnd_onaction(ffui_wnd *wnd, int id)
{
	(void)wnd;
	(void)id;
}

/* handle the creation of non-modal window. */
static LRESULT __stdcall wnd_proc(HWND h, uint msg, WPARAM w, LPARAM l)
{
	ffui_wnd *wnd = (void*)ffui_getctl(h);

	if (wnd != NULL) {
		size_t code = 0;
		if (0 != ffui_wndproc(wnd, &code, h, msg, w, l))
			return code;

	} else if (msg == WM_CREATE) {
		const CREATESTRUCT *cs = (void*)l;
		wnd = (void*)cs->lpCreateParams;
		wnd->h = h;
		ffui_setctl(h, wnd);

		if (wnd->on_create != NULL)
			wnd->on_create(wnd);
	}

	return DefWindowProc(h, msg, w, l);
}


/** Get handle of the window. */
static HWND base_parent(HWND h)
{
	HWND parent;
	ffui_wnd *w;

	while ((NULL == (w = ffui_getctl(h)) || w->uid != FFUI_UID_WINDOW)
		&& NULL != (parent = GetParent(h))) {
		h = parent;
	}
	return h;
}

static int process_accels(MSG *m)
{
	HWND h;
	ffui_wnd *w;

	if (m->hwnd == NULL)
		return 0;

	h = base_parent(m->hwnd);
	if (NULL != (w = ffui_getctl(h))) {
		if (w->acceltbl != NULL
			&& 0 != TranslateAccelerator(h, w->acceltbl, m))
			return 1;
	}
	return 0;
}

#define FFUI_THDMSG  (WM_APP + 1)

int ffui_runonce(void)
{
	MSG m;

	// 2nd arg =NULL instructs to receive window-messages AND thread-messages
	if (!GetMessage(&m, NULL, 0, 0))
		return 1;

	if (m.hwnd == NULL && m.message == FFUI_THDMSG) {
		ffui_handler func = (void*)m.wParam;
		void *udata = (void*)m.lParam;
		func(udata);
		return 0;
	}

	if (0 != process_accels(&m))
		return 0;

	TranslateMessage(&m);
	DispatchMessage(&m);
	return 0;
}

void ffui_thd_post(ffui_handler func, void *udata)
{
	PostThreadMessage(_curthd_id, FFUI_THDMSG, (WPARAM)func, (LPARAM)udata);
}


#define dpi_scale(x)  (((x) * _ffui_dpi) / 96)

static void print(const char *cmd, HWND h, size_t w, size_t l) {
	_ffui_log("%s:\th: %8xL,  w: %8xL,  l: %8xL\n"
		, cmd, (void*)h, (size_t)w, (size_t)l);
}


static void tray_nfy(ffui_wnd *wnd, ffui_trayicon *t, size_t l)
{
	switch (l) {
	case WM_LBUTTONUP:
		if (t->lclick_id != 0)
			wnd->on_action(wnd, t->lclick_id);
		break;

	case WM_RBUTTONUP:
		if (t->pmenu != NULL) {
			ffui_point pt;
			ffui_wnd_setfront(wnd);
			ffui_cur_pos(&pt);
			ffui_menu_show(t->pmenu, pt.x, pt.y, wnd->h);
		}
		break;

	case NIN_BALLOONUSERCLICK:
		if (t->balloon_click_id != 0)
			wnd->on_action(wnd, t->balloon_click_id);
		break;
	}
}

static void wnd_bordstick(uint stick, WINDOWPOS *ws)
{
	RECT r = _ffui_screen_area;
	if (stick >= (uint)ffint_abs(r.left - ws->x))
		ws->x = r.left;
	else if (stick >= (uint)ffint_abs(r.right - (ws->x + ws->cx)))
		ws->x = r.right - ws->cx;

	if (stick >= (uint)ffint_abs(r.top - ws->y))
		ws->y = r.top;
	else if (stick >= (uint)ffint_abs(r.bottom - (ws->y + ws->cy)))
		ws->y = r.bottom - ws->cy;
}

static void wnd_cmd(ffui_wnd *wnd, uint w, HWND h)
{
	uint id = 0, msg = HIWORD(w);
	union ffui_anyctl ctl;

	if (NULL == (ctl.ctl = ffui_getctl(h)))
		return;

	switch ((int)ctl.ctl->uid) {

	case FFUI_UID_LABEL:
		switch (msg) {
		case STN_CLICKED:
			id = ctl.lbl->click_id;
			break;
		case STN_DBLCLK:
			id = ctl.lbl->click2_id;
			break;
		}
		break;

	case FFUI_UID_BUTTON:
		switch (msg) {
		case BN_CLICKED:
			id = ctl.btn->action_id;
			break;
		}
		break;

	case FFUI_UID_CHECKBOX:
		switch (msg) {
		case BN_CLICKED:
			id = ctl.cb->action_id;
			break;
		}
		break;

	case FFUI_UID_EDITBOX:
	case FFUI_UID_TEXT:
		switch (msg) {
		case EN_CHANGE:
			id = ctl.edit->change_id;
			break;

		case EN_SETFOCUS:
			id = ctl.edit->focus_id;
			break;
		}
		break;

	case FFUI_UID_COMBOBOX:
		switch (msg) {
		case CBN_SELCHANGE:
			id = ctl.combx->change_id;
			break;

		case CBN_DROPDOWN:
			id = ctl.combx->popup_id;
			break;

		case CBN_CLOSEUP:
			id = ctl.combx->closeup_id;
			break;

		case CBN_EDITCHANGE:
			id = ctl.combx->edit_change_id;
			break;

		case CBN_EDITUPDATE:
			id = ctl.combx->edit_update_id;
			break;
		}
		break;
	}

	if (id != 0)
		wnd->on_action(wnd, id);
}

static int wnd_nfy(ffui_wnd *wnd, NMHDR *n, size_t *code)
{
	uint id = 0;
	union ffui_anyctl ctl;

	_ffui_log("WM_NOTIFY:\th: %8xL,  code: %8xL"
		, (void*)n->hwndFrom, (size_t)n->code);

	if (NULL == (ctl.ctl = ffui_getctl(n->hwndFrom)))
		return 0;

	switch (n->code) {
	case LVN_ITEMACTIVATE:
		id = ctl.view->dblclick_id;
		break;

	case LVN_ITEMCHANGED: {
		NM_LISTVIEW *it = (NM_LISTVIEW*)n;
		_ffui_log("LVN_ITEMCHANGED: item:%u.%u, state:%xu->%xu"
			, it->iItem, it->iSubItem, it->uOldState, it->uNewState);
		ctl.view->idx = it->iItem;
		if (0x3000 == ((it->uOldState & LVIS_STATEIMAGEMASK) ^ (it->uNewState & LVIS_STATEIMAGEMASK)))
			id = ctl.view->check_id;
		else if ((it->uOldState & LVIS_SELECTED) != (it->uNewState & LVIS_SELECTED))
			id = ctl.view->chsel_id;
		break;
	}

	case LVN_COLUMNCLICK:
		id = ctl.view->colclick_id;
		ctl.view->col = ((NM_LISTVIEW*)n)->iSubItem;
		break;

	case LVN_ENDLABELEDIT:
		{
		const LVITEMW *it = &((NMLVDISPINFOW*)n)->item;
		_ffui_log("LVN_ENDLABELEDIT: item:%u, text:%q"
			, it->iItem, (it->pszText == NULL) ? L"" : it->pszText);
		if (ctl.view->edit_id != 0) {
			ctl.view->text = (it->pszText != NULL) ? ffsz_alloc_wtou(it->pszText) : ffsz_dup("");
			wnd->on_action(wnd, ctl.view->edit_id);
			ffmem_free(ctl.view->text);  ctl.view->text = NULL;
		}
		}
		break;

	case LVN_GETDISPINFO:
		if (ctl.ctl->uid == FFUI_UID_LISTVIEW
			&& ctl.view->dispinfo_id != 0) {
			NMLVDISPINFOW *di = (NMLVDISPINFOW*)n;
			_ffui_log("LVN_GETDISPINFO: mask:%xu  item:%L, subitem:%L"
				, (int)di->item.mask, (size_t)di->item.iItem, (size_t)di->item.iSubItem);
			ctl.view->dispinfo_item = &di->item;
			if ((di->item.mask & LVIF_TEXT) && di->item.cchTextMax != 0)
				di->item.pszText[0] = '\0';
			wnd->on_action(wnd, ctl.view->dispinfo_id);
			*code = 1;
			return 1;
		}
		break;

	case LVN_KEYDOWN:
		if (ctl.ctl->uid == FFUI_UID_LISTVIEW) {
			if (ctl.view->dispinfo_id != 0 && ctl.view->check_id != 0) {
				NMLVKEYDOWN *kd = (void*)n;
				_ffui_log("LVN_KEYDOWN: vkey:%xu  flags:%xu"
					, (int)kd->wVKey, (int)kd->flags);
				if (kd->wVKey == VK_SPACE) {
					ctl.view->idx = ffui_view_focused(ctl.view);
					id = ctl.view->check_id;
				}
			}
		}
		break;

	case NM_CLICK:
		if (ctl.ctl->uid == FFUI_UID_LISTVIEW) {
			id = ctl.view->lclick_id;

			if (ctl.view->dispinfo_id != 0 && ctl.view->check_id != 0) {
				ffui_point pt;
				ffui_cur_pos(&pt);
				uint f = LVHT_ONITEMSTATEICON;
				int i = ffui_view_hittest2(ctl.view, &pt, NULL, &f);
				_ffui_log("NM_CLICK: i:%xu  f:%xu"
					, i, f);
				if (i != -1) {
					ctl.view->idx = i;
					id = ctl.view->check_id;
				}
			}
		}
		break;

	case NM_RCLICK:
		if (ctl.ctl->uid == FFUI_UID_LISTVIEW && ctl.view->pmenu != NULL) {
			ffui_point pt;
			ffui_wnd_setfront(wnd);
			ffui_cur_pos(&pt);
			ffui_menu_show(ctl.view->pmenu, pt.x, pt.y, wnd->h);
		}
		break;

	case TVN_SELCHANGED:
		_ffui_log("TVN_SELCHANGED", 0);
		id = ctl.view->chsel_id;
		break;


	case TCN_SELCHANGING:
		_ffui_log("TCN_SELCHANGING", 0);
		if (ctl.tab->changing_sel_id != 0) {
			wnd->on_action(wnd, ctl.tab->changing_sel_id);
			*code = ctl.tab->changing_sel_keep;
			ctl.tab->changing_sel_keep = 0;
			return 1;
		}
		break;

	case TCN_SELCHANGE:
		id = ctl.tab->chsel_id;
		break;
	}

	if (id != 0)
		wnd->on_action(wnd, id);
	return 0;
}

static void wnd_scroll(ffui_wnd *wnd, uint w, HWND h)
{
	union ffui_anyctl ctl;

	if (NULL == (ctl.ctl = ffui_getctl(h)))
		return;

	switch (LOWORD(w)) {
	case SB_THUMBTRACK:
	case SB_LINELEFT:
	case SB_LINERIGHT:
	case SB_PAGELEFT:
	case SB_PAGERIGHT:
		if (!ctl.trkbar->thumbtrk)
			ctl.trkbar->thumbtrk = 1;
		//fallthrough
	case SB_THUMBPOSITION: //note: SB_ENDSCROLL isn't sent
		if (ctl.trkbar->scrolling_id != 0)
			wnd->on_action(wnd, ctl.trkbar->scrolling_id);
		break;

	case SB_ENDSCROLL:
		if (ctl.trkbar->thumbtrk)
			ctl.trkbar->thumbtrk = 0;
		if (ctl.trkbar->scroll_id != 0) {
			wnd->on_action(wnd, ctl.trkbar->scroll_id);
		}
		break;
	}
}

/*
exit
  via Close button: WM_SYSCOMMAND(SC_CLOSE) -> WM_CLOSE -> WM_DESTROY
  when user signs out: WM_QUERYENDSESSION -> WM_CLOSE
*/

int ffui_wndproc(ffui_wnd *wnd, size_t *code, HWND h, uint msg, size_t w, size_t l)
{
	switch (msg) {

	case WM_COMMAND:
		print("WM_COMMAND", h, w, l);

		if (l == 0) { //menu
			/* HIWORD(w): 0 - msg sent by menu. 1 - msg sent by hot key */
			wnd->on_action(wnd, LOWORD(w));
			break;
		}

		wnd_cmd(wnd, (int)w, (HWND)l);
		break;

	case WM_NOTIFY:
		*code = 0;
		if (0 != wnd_nfy(wnd, (NMHDR*)l, code))
			return *code;
		if (*code != 0)
			return 1;
		break;

	case WM_HSCROLL:
	case WM_VSCROLL:
		print("WM_HSCROLL", h, w, l);
		wnd_scroll(wnd, (int)w, (HWND)l);
		break;

	case WM_SYSCOMMAND:
		print("WM_SYSCOMMAND", h, w, l);

		switch (w & 0xfff0) {
		case SC_CLOSE:
			wnd->on_action(wnd, wnd->onclose_id);

			if (wnd->hide_on_close) {
				ffui_show(wnd, 0);
				return 1;
			}
			return wnd->manual_close;

		case SC_MINIMIZE:
			if (wnd->onminimize_id != 0) {
				wnd->on_action(wnd, wnd->onminimize_id);
				return 1;
			}
			break;

		case SC_MAXIMIZE:
			wnd->on_action(wnd, wnd->onmaximize_id);
			break;
		}
		break;

	case WM_HOTKEY:
		print("WM_HOTKEY", h, w, l);
		ffui_wnd_ghotkey_call(wnd, w);
		break;

	case WM_ACTIVATE:
		print("WM_ACTIVATE", h, w, l);
		switch (w) {
		case WA_INACTIVE:
			wnd->focused = GetFocus();
			break;

		case WA_ACTIVE:
		case WA_CLICKACTIVE:
			if (wnd->onactivate_id != 0)
				wnd->on_action(wnd, wnd->onactivate_id);
			if (wnd->focused != NULL) {
				SetFocus(wnd->focused);
				wnd->focused = NULL;
				*code = 0;
				return 1;
			}
			break;
		}
		break;

	case WM_KEYDOWN:
		print("WM_KEYDOWN", h, w, l);
		break;

	case WM_KEYUP:
		print("WM_KEYUP", h, w, l);
		break;

	case WM_SIZE:
		// print("WM_SIZE", h, w, l);
		if (l == 0)
			break; //window has been minimized

		ffui_paned *p;
		for (p = wnd->paned_first;  p != NULL;  p = p->next) {
			paned_resize(p, wnd);
		}

		if (wnd->stbar != NULL)
			ffui_send(wnd->stbar->h, WM_SIZE, 0, 0);
		break;

	case WM_WINDOWPOSCHANGING:
		// print("WM_WINDOWPOSCHANGING", h, w, l);
		if (wnd->bordstick != 0) {
			wnd_bordstick(dpi_scale(wnd->bordstick), (WINDOWPOS *)l);
			return 0;
		}
		break;

	case WM_PAINT:
		if (wnd->on_paint != NULL)
			wnd->on_paint(wnd);
		break;

	case WM_ERASEBKGND:
		if (wnd->bgcolor != NULL) {
			HDC hdc = (HDC)w;
			RECT rect;
			GetClientRect(h, &rect);
			FillRect(hdc, &rect, wnd->bgcolor);
			*code = 1;
			return 1;
		}
		break;

	case WM_CTLCOLORBTN:
	case WM_CTLCOLORLISTBOX:
	case WM_CTLCOLORSCROLLBAR:
	case WM_CTLCOLORSTATIC: {
		union ffui_anyctl c;
		c.ctl = ffui_getctl((HWND)l);
		HDC hdc = (HDC)w;
		HBRUSH br = wnd->bgcolor;
		ffbool result = 0;
		if (c.ctl != NULL && c.ctl->uid == FFUI_UID_LABEL && c.lbl->color != 0) {
			SetTextColor(hdc, c.lbl->color);
			result = 1;
		}

		if (result || br != NULL) {
			SetBkMode(hdc, TRANSPARENT);
			if (br == NULL)
				br = GetSysColorBrush(COLOR_BTNFACE);
			*code = (size_t)br;
			return 1;
		}
		break;
	}

	case WM_DROPFILES:
		print("WM_DROPFILES", h, w, l);
		if (wnd->on_dropfiles != NULL) {
			ffui_fdrop d;
			d.hdrop = (void*)w;
			d.idx = 0;
			d.fn = NULL;
			wnd->on_dropfiles(wnd, &d);
			ffmem_free(d.fn);
		}
		DragFinish((void*)w);
		break;

	case FFUI_WM_USER_TRAY:
		print("FFUI_WM_USER_TRAY", h, w, l);

		if (wnd->trayicon != NULL && w == wnd->trayicon->nid.uID)
			tray_nfy(wnd, wnd->trayicon, l);
		break;

	case WM_CLOSE:
		print("WM_CLOSE", h, w, l);
		break;

	case WM_QUERYENDSESSION:
		print("WM_QUERYENDSESSION", h, w, l);
		wnd->on_action(wnd, wnd->onclose_id);
		break;

	case WM_DESTROY: {
		print("WM_DESTROY", h, w, l);
		int top = wnd->top;
		if (wnd->on_destroy != NULL)
			wnd->on_destroy(wnd);

		if (top)
			ffui_quitloop();
		break;
	}
	}

	return 0;
}
