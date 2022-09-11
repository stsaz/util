
#include <FFOS/error.h>
#include "util/ltconf.h"
#include <FFOS/test.h>
#include <FFOS/ffos-extern.h>

void test_ltconf()
{
	struct ltconf c = {};
	ffstr in, out;

	ffstr_setz(&in, "k");
	xieq(LTCONF_CHUNK, ltconf_read(&c, &in, &out));
	xseq(&out, "k");
	x(!(c.flags & LTCONF_FKEY));
	xieq(LTCONF_MORE, ltconf_read(&c, &in, &out));
	ffmem_zero_obj(&c);

	ffstr_setz(&in,
"  key  val1  \"val w space\" \"val w\\\" quote\" \"\\\\val w\\x01 binary\" \"val\n\
w\n\
newline\" val4 \"\"  # comment \n\
  key1  val1\n");

	xieq(LTCONF_KEY, ltconf_read(&c, &in, &out));
	xseq(&out, "key");
	xieq(0, c.line);

	xieq(LTCONF_VAL, ltconf_read(&c, &in, &out));
	xseq(&out, "val1");

	xieq(LTCONF_VAL_NEXT, ltconf_read(&c, &in, &out));
	xseq(&out, "val w space");
	x(!!(c.flags & LTCONF_FKEY));
	x(!!(c.flags & LTCONF_FQUOTED));

	xieq(LTCONF_CHUNK, ltconf_read(&c, &in, &out));
	xseq(&out, "val w");
	xieq(LTCONF_CHUNK, ltconf_read(&c, &in, &out));
	xseq(&out, "\"");
	xieq(LTCONF_VAL_NEXT, ltconf_read(&c, &in, &out));
	xseq(&out, " quote");

	xieq(LTCONF_CHUNK, ltconf_read(&c, &in, &out));
	xseq(&out, "\\");
	xieq(LTCONF_CHUNK, ltconf_read(&c, &in, &out));
	xseq(&out, "val w");
	xieq(LTCONF_CHUNK, ltconf_read(&c, &in, &out));
	xseq(&out, "\x01");
	xieq(LTCONF_VAL_NEXT, ltconf_read(&c, &in, &out));
	xseq(&out, " binary");

	xieq(LTCONF_VAL_NEXT, ltconf_read(&c, &in, &out));
	xseq(&out, "val\nw\nnewline");
	xieq(2, c.line);

	xieq(LTCONF_VAL_NEXT, ltconf_read(&c, &in, &out));
	xseq(&out, "val4");
	x(!(c.flags & LTCONF_FQUOTED));

	xieq(LTCONF_VAL_NEXT, ltconf_read(&c, &in, &out));
	xseq(&out, "");

	xieq(LTCONF_KEY, ltconf_read(&c, &in, &out));
	xseq(&out, "key1");
	xieq(3, c.line);

	xieq(LTCONF_VAL, ltconf_read(&c, &in, &out));
	xseq(&out, "val1");

	xieq(LTCONF_MORE, ltconf_read(&c, &in, &out));

	ffstr_setz(&in, "\"key w space\" val\n");
	xieq(LTCONF_KEY, ltconf_read(&c, &in, &out));
	xseq(&out, "key w space");
	x(in.ptr[-1] == '"');
	xieq(LTCONF_VAL, ltconf_read(&c, &in, &out));
	xseq(&out, "val");

	ffmem_zero_obj(&c);
	ffstr_setz(&in, "ke\"y "); // require whitespace before quoted text
	xieq(LTCONF_KEY, ltconf_read(&c, &in, &out));
	xieq(LTCONF_ERROR, ltconf_read(&c, &in, &out));
	xieq(FFS_LEN("ke"), c.line_off);

	ffmem_zero_obj(&c);
	ffstr_setz(&in, "\"key\"1 "); // require whitespace after quoted text
	xieq(LTCONF_KEY, ltconf_read(&c, &in, &out));
	xseq(&out, "key");
	xieq(LTCONF_ERROR, ltconf_read(&c, &in, &out));
	xieq(FFS_LEN("\"key\""), c.line_off);

	ffmem_zero_obj(&c);
	ffstr_setz(&in, "k\x01""ey"); // non-printable character
	xieq(LTCONF_KEY, ltconf_read(&c, &in, &out));
	xseq(&out, "k");
	xieq(LTCONF_ERROR, ltconf_read(&c, &in, &out));
	xieq(FFS_LEN("k"), c.line_off);

	ffmem_zero_obj(&c);
	ffstr_setz(&in, "\"k\x01""ey\""); // non-printable character within quotes
	xieq(LTCONF_ERROR, ltconf_read(&c, &in, &out));
	xieq(FFS_LEN("\"k"), c.line_off);
}

int main()
{
	test_ltconf();
	return 0;
}
