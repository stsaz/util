/** http1.h tester */

#include <ffsys/test.h>
#include <ffbase/vector.h>
#include "http1.h"

void test_http1_escape()
{
	char buf[100];
	ffstr s;
	s.ptr = buf;
	ffstr url = FFSTR_INITZ("/path with space  ");
	s.len = httpurl_escape(buf, sizeof(buf), url);
	xseq(&s, "/path%20with%20space%20%20");

	ffstr_setz(&url, "/path%20with%20space%20%20");
	s.len = httpurl_unescape(buf, sizeof(buf), url);
	xseq(&s, "/path with space  ");

	ffstr_setz(&url, "%2z");
	x(httpurl_unescape(buf, 2, url) < 0);
	ffstr_setz(&url, "%20%20");
	x(httpurl_unescape(buf, 1, url) < 0);
}

void test_http1_write()
{
	char buf[100];
	ffstr s;

	ffstr m = FFSTR_INITZ("GET"), p = FFSTR_INITZ("/path");
	s.ptr = buf;
	s.len = http_req_write(buf, sizeof(buf), m, p, 0);
	xseq(&s, "GET /path HTTP/1.1\r\n");

	s.len = 0;
	ffstr r = FFSTR_INITZ("OK");
	s.len += http_resp_write(buf, sizeof(buf), 123, r);
	ffstr k = FFSTR_INITZ("Key"), v = FFSTR_INITZ("Val");
	s.len += http_hdr_write(s.ptr+s.len, sizeof(buf)-s.len, k, v);
	xseq(&s, "HTTP/1.1 123 OK\r\nKey: Val\r\n");
}

void test_http1_chunked()
{
	struct httpchunked c = {};
	ffstr in = {}, out;
	static const char s[] = "2\r\nhi\r\na\r\n0123456789\n0\r\n\n";
	ffvec v = {};
	for (int i = 0;  i != FFS_LEN(s);  ) {
		int r = httpchunked_parse(&c, in, &out);
		if (r == 0) {
			ffstr_set(&in, s+i, 1);
			i++;
		} else if (r > 0) {
			ffstr_shift(&in, r);
			ffvec_addstr(&v, &out);
		} else if (r == -1) {
			x(i == FFS_LEN(s));
			break;
		} else
			x(0);
	}
	xseq((ffstr*)&v, "hi0123456789");
	ffvec_free(&v);

	ffstr_setz(&in, "x");
	x(httpchunked_parse(&c, in, &out) < -1);

	ffstr_setz(&in, "123x");
	x(httpchunked_parse(&c, in, &out) < -1);

	ffstr_setz(&in, "123\rx");
	x(httpchunked_parse(&c, in, &out) < -1);

	ffstr h, t;
	char buf[99];
	httpchunked_write(buf, 0xa, &h, &t);
	xseq(&h, "a\r\n");
	xseq(&t, "\r\n");
}


void test_httpurl_split()
{
	{
		ffstr url = FFSTR_INITZ("http://host:8080/path/my%20file?query%20string#hash");
		struct httpurl_parts p = {};
		httpurl_split(&p, url);
		xseq(&p.scheme, "http://");
		xseq(&p.host, "host");
		xseq(&p.port, ":8080");
		xseq(&p.path, "/path/my%20file");
		xseq(&p.query, "?query%20string");
		xseq(&p.hash, "#hash");
	}
	{
		ffstr url = FFSTR_INITZ("host:8080/path/my%20file?query%20string#hash");
		struct httpurl_parts p = {};
		httpurl_split(&p, url);
		xseq(&p.scheme, "");
		xseq(&p.host, "host");
		xseq(&p.port, ":8080");
		xseq(&p.path, "/path/my%20file");
		xseq(&p.query, "?query%20string");
		xseq(&p.hash, "#hash");
	}
	{
		ffstr url = FFSTR_INITZ("host/path/my%20file?query%20string#hash");
		struct httpurl_parts p = {};
		httpurl_split(&p, url);
		xseq(&p.scheme, "");
		xseq(&p.host, "host");
		xseq(&p.port, "");
		xseq(&p.path, "/path/my%20file");
		xseq(&p.query, "?query%20string");
		xseq(&p.hash, "#hash");
	}
	{
		ffstr url = FFSTR_INITZ("host/path/my%20file");
		struct httpurl_parts p = {};
		httpurl_split(&p, url);
		xseq(&p.scheme, "");
		xseq(&p.host, "host");
		xseq(&p.port, "");
		xseq(&p.path, "/path/my%20file");
		xseq(&p.query, "");
		xseq(&p.hash, "");
	}
	{
		ffstr url = FFSTR_INITZ("host:8080");
		struct httpurl_parts p = {};
		httpurl_split(&p, url);
		xseq(&p.scheme, "");
		xseq(&p.host, "host");
		xseq(&p.port, ":8080");
		xseq(&p.path, "");
		xseq(&p.query, "");
		xseq(&p.hash, "");
	}
	{
		ffstr url = FFSTR_INITZ("host");
		struct httpurl_parts p = {};
		httpurl_split(&p, url);
		xseq(&p.scheme, "");
		xseq(&p.host, "host");
		xseq(&p.port, "");
		xseq(&p.path, "");
		xseq(&p.query, "");
		xseq(&p.hash, "");
	}
	{
		ffstr url = FFSTR_INITZ("http://[::1]:8080/path/my%20file?query%20string#hash");
		struct httpurl_parts p = {};
		httpurl_split(&p, url);
		xseq(&p.scheme, "http://");
		xseq(&p.host, "[::1]");
		xseq(&p.port, ":8080");
		xseq(&p.path, "/path/my%20file");
		xseq(&p.query, "?query%20string");
		xseq(&p.hash, "#hash");
	}
	{
		ffstr url = FFSTR_INITZ("[::1]:8080/path/my%20file?query%20string#hash");
		struct httpurl_parts p = {};
		httpurl_split(&p, url);
		xseq(&p.scheme, "");
		xseq(&p.host, "[::1]");
		xseq(&p.port, ":8080");
		xseq(&p.path, "/path/my%20file");
		xseq(&p.query, "?query%20string");
		xseq(&p.hash, "#hash");
	}
	{
		ffstr url = FFSTR_INITZ("[::1]/path/my%20file?query%20string#hash");
		struct httpurl_parts p = {};
		httpurl_split(&p, url);
		xseq(&p.scheme, "");
		xseq(&p.host, "[::1]");
		xseq(&p.path, "/path/my%20file");
		xseq(&p.query, "?query%20string");
		xseq(&p.hash, "#hash");
	}
	{
		ffstr url = FFSTR_INITZ("[::1]:8080");
		struct httpurl_parts p = {};
		httpurl_split(&p, url);
		xseq(&p.scheme, "");
		xseq(&p.host, "[::1]");
		xseq(&p.port, ":8080");
		xseq(&p.path, "");
	}
	{
		ffstr url = FFSTR_INITZ("[::1]");
		struct httpurl_parts p = {};
		httpurl_split(&p, url);
		xseq(&p.scheme, "");
		xseq(&p.host, "[::1]");
		xseq(&p.path, "");
	}
	{
		ffstr url = FFSTR_INITZ(":host:8080");
		struct httpurl_parts p = {};
		httpurl_split(&p, url);
		xseq(&p.host, "");
		xseq(&p.port, ":host:8080");
	}
	{
		ffstr url = FFSTR_INITZ("[host:8080");
		struct httpurl_parts p = {};
		httpurl_split(&p, url);
		xseq(&p.host, "[host:8080");
		xseq(&p.port, "");
	}
	{
		ffstr url = FFSTR_INITZ("/path/my%20file?query%20string#hash");
		struct httpurl_parts p = {};
		httpurl_split(&p, url);
		xseq(&p.host, "");
		xseq(&p.path, "/path/my%20file");
	}
}

static ffstr longurl = FFSTR_INITZ("http://hosthosthosthosthosthosthosthosthosthost:8080/path/my%20file/path/my%20file/path/my%20file/path/my%20file/path/my%20file/path/my%20file/path/my%20file/path/my%20file?query%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20string#sharpsharpsharpsharpsharp");
void bench_ffurl_split()
{
	for (int i = 0;  i != 10*1000*1000;  i++) {
		struct httpurl_parts p = {};
		httpurl_split(&p, longurl);
		// xseq(&p.scheme, "http://");
		// xseq(&p.host, "hosthosthosthosthosthosthosthosthosthost");
		// xseq(&p.port, ":8080");
		// xseq(&p.path, "/path/my%20file/path/my%20file/path/my%20file/path/my%20file/path/my%20file/path/my%20file/path/my%20file/path/my%20file");
		// xseq(&p.query, "?query%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20stringquery%20string");
		// xseq(&p.hash, "#sharpsharpsharpsharpsharp");
	}
}

void test_http1_req()
{
	ffstr m, p, t;
	ffstr s = FFSTR_INITZ("GET /path/path/path/path HTTP/1.1\r\n");
	xieq(s.len, http_req_parse(s, &m, &p, &t));
	xseq(&m, "GET");
	xseq(&p, "/path/path/path/path");
	xseq(&t, "HTTP/1.1");

	ffstr_setz(&s, "GET  /path/path/path/path  HTTP/1.1  \r\n");
	xieq(s.len, http_req_parse(s, &m, &p, &t));
	xseq(&m, "GET");
	xseq(&p, "/path/path/path/path");
	xseq(&t, "HTTP/1.1");

	ffstr_setz(&s, "GET");
	xieq(0, http_req_parse(s, &m, &p, &t));
	ffstr_setz(&s, "GET /path/path/path/path");
	xieq(0, http_req_parse(s, &m, &p, &t));
	ffstr_setz(&s, "GET /path/path/path/path HTTP/1.1");
	xieq(0, http_req_parse(s, &m, &p, &t));
	ffstr_setz(&s, "GET /path/path/path/path HTTP/1.1\r");
	xieq(0, http_req_parse(s, &m, &p, &t));

	ffstr_setz(&s, "GET0");
	xieq(-1, http_req_parse(s, &m, &p, &t));
	ffstr_setz(&s, "GET /path/path/path/path\x01");
	xieq(-1, http_req_parse(s, &m, &p, &t));
	ffstr_setz(&s, "GET /path/path/path/path HTTP/1.1\x01");
	xieq(-1, http_req_parse(s, &m, &p, &t));
	ffstr_setz(&s, "GET /path/path/path/path HTTP:1.1");
	xieq(-1, http_req_parse(s, &m, &p, &t));
}

void test_http1_hdr()
{
	ffstr k, v;
	ffstr s = FFSTR_INITZ("Key: Value\r\n");
	xieq(s.len, http_hdr_parse(s, &k, &v));
	xseq(&k, "Key");
	xseq(&v, "Value");

	ffstr_setz(&s, "Key-Key  :  My Value  \r\n");
	xieq(s.len, http_hdr_parse(s, &k, &v));
	xseq(&k, "Key-Key");
	xseq(&v, "My Value");

	ffstr_setz(&s, "Key-Key:");
	xieq(0, http_hdr_parse(s, &k, &v));

	ffstr_setz(&s, "Key-Key: My Value ");
	xieq(0, http_hdr_parse(s, &k, &v));

	ffstr_setz(&s, "Key-Key: My Value \r");
	xieq(0, http_hdr_parse(s, &k, &v));

	ffstr_setz(&s, "-Key-Key: My Value \r\n");
	xieq(-1, http_hdr_parse(s, &k, &v));
}

static void test_range()
{
#if 0
	ffuint64 sz;

	sz = 789;
	x(123 == http_range(FFSTR("123-456"), &sz) && sz == 456 - 123 + 1);

	sz = 789;
	x(0 == http_range(FFSTR("0-0"), &sz) && sz == 1);

	sz = 789;
	x(789 - 456 == http_range(FFSTR("-456"), &sz) && sz == 456);

	sz = 789;
	x(0 == http_range(FFSTR("-900"), &sz) && sz == 789);

	sz = 789;
	x(123 == http_range(FFSTR("123-"), &sz) && sz == 789 - 123);

	sz = 789;
	x(-1 == http_range(FFSTR("123-x"), &sz) && sz == 789);
#endif
}

void test_http1()
{
	test_http1_req();
	test_http1_hdr();

	{
	ffstr t, m;
	ffuint c;
	ffstr s = FFSTR_INITZ("HTTP/1.1 404 Not Found\r\n");
	xieq(s.len, http_resp_parse(s, &t, &c, &m));
	xseq(&t, "HTTP/1.1");
	xieq(404, c);
	xseq(&m, "Not Found");
	}

	test_http1_chunked();
	test_http1_write();
	test_http1_escape();
	test_httpurl_split();
	test_range();
}
