/** ff: stream.h tester
2022, Simon Zolin
*/

#include <FFOS/test.h>
#include <util/stream.h>

void test_stream()
{
	ffstream s = {};
	ffstream_realloc(&s, 8);
	int r;
	ffstr in, v;

	// empty -> some
	ffstr_setz(&in, "12");
	xieq(2, ffstream_gather(&s, in, 4, &v));
	xseq(&v, "12");

	// some -> middle
	ffstr_setz(&in, "345");
	xieq(3, ffstream_gather(&s, in, 4, &v));
	xseq(&v, "12345");
	xieq(0, ffstream_gather(&s, in, 4, &v)); // returns the same view
	xseq(&v, "12345");
	ffstream_consume(&s, 2); // "..345"

	// middle -> full
	ffstr_setz(&in, "678");
	xieq(3, ffstream_gather(&s, in, 6, &v));
	xseq(&v, "345678");
	ffstream_consume(&s, 6); // "........"

	// full -> (move) middle
	ffstr_setz(&in, "1234");
	xieq(4, ffstream_gather(&s, in, 3, &v));
	xseq(&v, "1234");
	x(v.ptr == s.ptr);
	ffstream_consume(&s, 2); // "..34"

	// middle -> (move) middle
	ffstr_setz(&in, "56789abc");
	xieq(6, ffstream_gather(&s, in, 7, &v));
	xseq(&v, "3456789a");
	x(v.ptr == s.ptr);

	x(16 == ffstream_realloc(&s, 12));
	v = ffstream_view(&s);
	xseq(&v, "3456789a");

	ffstream_free(&s);
}

void test_stream_ref()
{
	ffstream s = {};
	ffstream_realloc(&s, 8);
	int r;
	ffstr in, v;

	// empty -> middle ref
	ffstr_setz(&in, "12345");
	x(5 == ffstream_gather_ref(&s, in, 4, &v));
	xseq(&v, "12345");
	x(v.ptr == in.ptr);
	xieq(0, ffstream_gather_ref(&s, in, 4, &v)); // returns the same view
	xseq(&v, "12345");
	x(v.ptr == in.ptr);
	ffstream_consume(&s, 4); // "....5"

	// (move)
	ffstr_setz(&in, "6789a");
	x(3 == ffstream_gather_ref(&s, in, 4, &v)); // move "....5" -> "5"
	xseq(&v, "5678");
	x(v.ptr == s.ptr);
	ffstream_consume(&s, 4); // ""

	// empty -> middle ref
	ffstr_setz(&in, "90abc");
	x(5 == ffstream_gather_ref(&s, in, 4, &v));
	xseq(&v, "90abc");
	x(v.ptr == in.ptr);

	ffstream_realloc(&s, 12);
	x(s.cap == 16);
	v = ffstream_view(&s);
	xseq(&v, "90abc");
	x(v.ptr == in.ptr);

	ffstream_free(&s);
}

int main()
{
	test_stream();
	test_stream_ref();
	return 0;
}
