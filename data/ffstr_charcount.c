
static inline ffsize ffstr_charcount(ffstr s, int ch)
{
	ffsize r = 0;
	for (;;) {
		ffssize i = ffstr_findchar(&s, ch);
		if (i < 0)
			break;
		r++;
		ffstr_shift(&s, i+1);
	}
	return r;
}
