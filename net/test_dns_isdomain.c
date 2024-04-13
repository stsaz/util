
void test_dns_isdomain()
{
	xieq(1, ffdns_isdomain(FFSTR("cc1")));
	xieq(1, ffdns_isdomain(FFSTR("com")));
	xieq(2, ffdns_isdomain(FFSTR("a.c")));
	xieq(2, ffdns_isdomain(FFSTR("1.cc")));
	xieq(2, ffdns_isdomain(FFSTR("a.c-c")));
	xieq(2, ffdns_isdomain(FFSTR("a.1cc")));
	xieq(2, ffdns_isdomain(FFSTR("a.cc1")));
	xieq(3, ffdns_isdomain(FFSTR("1.2.cc")));
	xieq(3, ffdns_isdomain(FFSTR("a.b.cc")));
	xieq(3, ffdns_isdomain(FFSTR("abc.abc.abc")));
	xieq(3, ffdns_isdomain(FFSTR("a-bc.ab--c.abc")));
	xieq(2, ffdns_isdomain(FFSTR("abc.xn--p1ai")));
	xieq(2, ffdns_isdomain(FFSTR("xn--p1ai.xn--p1ai")));
	xieq(2, ffdns_isdomain(FFSTR("xn--asd.xn--p1ai")));
	xieq(2, ffdns_isdomain(FFSTR("123456789012345678901234567890123456789012345678901234567890123.cc")));

	xieq(-1, ffdns_isdomain(FFSTR("#cc")));
	xieq(-1, ffdns_isdomain(FFSTR("a.cc#")));
	xieq(-1, ffdns_isdomain(FFSTR("abc.xn--")));
	xieq(-1, ffdns_isdomain(FFSTR("abc.xn--asd")));

	xieq(-1, ffdns_isdomain(FFSTR(".a.cc")));
	xieq(-1, ffdns_isdomain(FFSTR("a.cc.")));

	xieq(-1, ffdns_isdomain(FFSTR("-a.cc")));
	xieq(-1, ffdns_isdomain(FFSTR("a-.cc")));

	xieq(-1, ffdns_isdomain(FFSTR("1234567890123456789012345678901234567890123456789012345678901234.cc")));
}
