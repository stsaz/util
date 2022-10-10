/**
Copyright (c) 2019 Simon Zolin
*/

#include <FF/net/dns-client.h>
#include <FF/net/dns.h>
#include <FF/array.h>
#include <FF/net/proto.h>
#include <FFOS/random.h>
#include <FFOS/timer.h>
#include <FFOS/test.h>


static ffdnsclient *ctx;
static fftimer timer;
static uint gflags;
static int oncomplete(ffdnsclient *r, ffdnscl_res *res, const ffstr *name, uint refcount, uint ttl);
static void onresolve(void *udata, const ffdnscl_result *res);
static void dnslog(uint level, const char *fmt, ...);
static void dnstimer(fftimerqueue_node *tmr, uint value_ms);
static fftime dnstime(void);

void test_dns_client(void)
{
	FFTEST_FUNC;
	fffd kq = FF_BADFD;

	fftime now;
	fftime_now(&now);
	ffrnd_seed(now.sec);

	kq = ffkqu_create();
	timer = fftimer_create(0);
	fftimer_start(timer, kq, (void*)-1, 2000);

	ffdnscl_conf conf = {};
	conf.kq = kq;
	conf.oncomplete = &oncomplete;
	conf.log = &dnslog;
	conf.time = &dnstime;
	conf.timer = &dnstimer;

	conf.max_tries = 3;
	conf.retry_timeout = 500;
	conf.buf_size = 4096;
	conf.enable_ipv6 = 1;
	conf.edns = 1;
	conf.debug_log = 1;

	ctx = ffdnscl_new(&conf);
	ffstr s;
	ffstr_setz(&s, "8.8.8.8");
	x(0 == ffdnscl_serv_add(ctx, &s));

	ffstr_setz(&s, "google.com");
	x(0 == ffdnscl_resolve(ctx, s, &onresolve, (void*)1, 0));

	ffstr_setz(&s, "apple.com");
	x(0 == ffdnscl_resolve(ctx, s, &onresolve, (void*)2, 0));
	x(0 == ffdnscl_resolve(ctx, s, &onresolve, (void*)2, FFDNSCL_CANCEL));

	ffkqu_time tm;
	ffkqu_settm(&tm, 1000);
	for (;;) {
		if (gflags & 2)
			break;
		ffkqu_entry ev;
		int n = ffkqu_wait(kq, &ev, 1, &tm);
		if (n == 1) {
			if (ffkq_event_data(&ev) == (void*)-1) {
				gflags |= 2;
				break;
			}
			ffkev_call(&ev);
		}
	}

	x(gflags & 1);

	ffkqu_close(kq);
	ffdnscl_free(ctx);
	fftimer_close(timer, kq);
}

static void onresolve(void *udata, const ffdnscl_result *res)
{
	if (udata == (void*)1) {
		xieq(FFDNS_NOERROR, res->status);
		gflags |= 1;
		x(res->ip.len != 0);

		ffarr data = {};
		const ffip6 *ip;
		FFARR_WALKT(&res->ip, ip, ffip6) {
			char buf[FFIP6_STRLEN];
			size_t n = ffip46_tostr(ip, buf, sizeof(buf));
			ffstr_catfmt(&data, "%*s  ", n, buf);
		}
		fffile_write(ffstdout, data.ptr, data.len);
		fffile_write(ffstdout, "\r\n", 2);

	} else {
		x(0);
	}
}

static int oncomplete(ffdnsclient *r, ffdnscl_res *res, const ffstr *name, uint refcount, uint ttl)
{
	return 0;
}

static void dnslog(uint level, const char *fmt, ...)
{
	ffarr a = {};
	va_list args;
	va_start(args, fmt);
	ffstr_catfmtv(&a, fmt, args);
	va_end(args);
	fffile_write(ffstdout, a.ptr, a.len);
	fffile_write(ffstdout, "\r\n", 2);
}

static void dnstimer(fftimerqueue_node *tmr, uint value_ms)
{
}

static fftime dnstime(void)
{
	fftime t;
	fftime_now(&t);
	return t;
}
