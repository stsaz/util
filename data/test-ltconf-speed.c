
#include <FFOS/file.h>
#include <FFOS/signal.h>
#include <FFOS/std.h>
#include <FFOS/ffos-extern.h>
#include <ffbase/atomic.h>
#include "ltconf.h"

int stop;

void onsig(struct ffsig_info*)
{
	FFINT_WRITEONCE(stop, 1);
}

void main()
{
	ffuint sigs[] = { SIGINT };
	ffsig_subscribe(onsig, sigs, 1);

	char buf[1*1024*1024];
	ffuint64 total = 0;
	struct ltconf ltc = {};
	while (!FFINT_READONCE(stop)) {
		int r = fffile_read(ffstdin, buf, sizeof(buf));
		if (r <= 0)
			break;
		total += r;

		ffstr in = FFSTR_INITN(buf, r);
		while (in.len) {
			ffstr out;
			r = ltconf_read(&ltc, &in, &out);
			if (r == LTCONF_ERROR) {
				ffstr_shift(&in, ltc.line_off);
				continue;
			}
			if (ffstr_eqz(&out, "stop"))
				break;
		}
	}

	ffstderr_fmt("total: %,U\n", total);
}
