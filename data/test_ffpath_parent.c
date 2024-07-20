void test_ffpath_parent()
{
	ffstr a, b, parent;

	ffstr_setz(&a, "/a/b");
	ffstr_setz(&b, "/b/c");
	x(0 == ffpath_parent(&a, &b, &parent));
	xseq(&parent, "/");

	ffstr_setz(&a, "/d/x");
	ffstr_setz(&b, "/d/x");
	x(0 == ffpath_parent(&a, &b, &parent));
	xseq(&parent, "/d/x");

	ffstr_setz(&a, "/d/x");
	ffstr_setz(&b, "/d/x2");
	x(0 == ffpath_parent(&a, &b, &parent));
	xseq(&parent, "/d");

	ffstr_setz(&a, "/d/x/1");
	ffstr_setz(&b, "/d/x/2");
	x(0 == ffpath_parent(&a, &b, &parent));
	xseq(&parent, "/d/x");

	ffstr_setz(&a, "/d/x");
	ffstr_setz(&b, "/d/x/2");
	x(0 == ffpath_parent(&a, &b, &parent));
	xseq(&parent, "/d/x");

	ffstr_setz(&a, "/d/x/2");
	ffstr_setz(&b, "/d/x");
	x(0 == ffpath_parent(&a, &b, &parent));
	xseq(&parent, "/d/x");
}
