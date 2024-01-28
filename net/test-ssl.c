/** ssl.h tester */

#include <net/ssl.h>
#include <FFOS/time.h>
#include <FFOS/file.h>
#include <FFOS/test.h>
#include <ffbase/vector.h>

struct ffos_test fftest;

void test_ssl_w(const char *fn)
{
	// create key
	ffssl_key *key;
	ffssl_cert *cert;
	x(!ffssl_key_create(&key, 2048, FFSSL_PKEY_RSA));

	// create cert
	struct ffssl_cert_newinfo ci = {};
	ffstr_setz(&ci.subject, "/O=Org/CN=Name");
	ci.serial = 0x12345678;

	fftime t;
	fftime_now(&t);
	ci.from_time = t.sec;
	ci.until_time = t.sec + 365 * 24*60*60;
	ci.pkey = key;
	x(!ffssl_cert_create(&cert, &ci));

	// get info from cert
	struct ffssl_cert_info info;
	ffssl_cert_info(cert, &info);
	x(ffstr_eqz(&ci.subject, info.subject));
	x(ffstr_eqz(&ci.subject, info.issuer));
	x(info.valid_from == (ffuint64)ci.from_time);
	x(info.valid_until == (ffuint64)ci.until_time);

	// create context with the generated certificate
	ffssl_ctx *ctx;
	x(!ffssl_ctx_create(&ctx));
	struct ffssl_ctx_conf conf = {};
	conf.cert = cert;
	conf.pkey = key;
	x(!ffssl_ctx_conf(ctx, &conf));
	ffssl_ctx_free(ctx);

	// write cert+key to file
	ffstr data = {};
	x(!ffssl_cert_print(cert, &data));
	fffile_writewhole(fn, data.ptr, data.len, 0);
	ffstr_free(&data);

	x(!ffssl_key_print(key, &data));
	fffile_writewhole(fn, data.ptr, data.len, FFFILE_APPEND);
	ffstr_free(&data);

	ffssl_key_free(key);
	ffssl_cert_free(cert);
}

void test_ssl_r(const char *fn)
{
	ffssl_key *key;
	ffssl_cert *cert;

	// read from file
	ffvec buf = {};
	fffile_readwhole(fn, &buf, 1*1024*1024);
	cert = ffssl_cert_read(*(ffstr*)&buf, 0);
	x(!!cert);
	key = ffssl_key_read(*(ffstr*)&buf, 0);
	x(!!key);

	// create context with certificate from file
	ffssl_ctx *ctx;
	x(!ffssl_ctx_create(&ctx));
	struct ffssl_ctx_conf conf = {};
	conf.cert = cert;
	conf.pkey = key;
	x(!ffssl_ctx_conf(ctx, &conf));
	ffssl_ctx_free(ctx);

	ffssl_key_free(key);
	ffssl_cert_free(cert);
	ffvec_free(&buf);
}

int main()
{
	ffssl_init();
	test_ssl_w("cert.pem");
	test_ssl_r("cert.pem");
	ffssl_uninit();
	return 0;
}
