
static inline ffstr fntree_print(fntree_block *b, uint flags)
{
	ffvec v = {};
	fntree_cursor c = {};
	fntree_entry *e;
	while ((e = fntree_cur_next(&c, b))) {
		ffstr s = fntree_name(e);
		ffvec_addfmt(&v, "%S\n", &s);
	}
	return *(ffstr*)&v;
}
