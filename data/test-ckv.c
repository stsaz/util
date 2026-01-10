/** ckv.h tester
2026, Simon Zolin */

#include <ffsys/error.h>
#include "ckv.h"
#include <ffbase/../test/test.h>
#include <ffsys/globals.h>

/* Scenarios:
Cache+Replace:
CR1. k=v -> {k=v}
CR2a. k=v -> [...] {k=v}
	k=v -> {k=v}
CR2b. k=v -> [...] {k=v}
	{}
	k=v -> [...]
CR3. k=v -> {} [k=v]

Replace:
R1. k=v -> [... k=v ...]
R2. k=val -> [... k=v ...]
	a. [... 0 ...] // size (len)0 (key)\0
	b. k=val -> [...]

Cache:
C1. k=v -> {}
C2. k=v -> {k2=v}
	k2=v -> [...]
	k=v -> {}
*/

void test_ckv()
{
	ffstr k, v;
	uint i;
	struct ckv c = {}, c2 = {};

	x(sizeof(c) == CKV_STRUCT_SIZE);

	// Set/find/list

	x(CKV_E_OK == ckv_set(&c, FFSTR_Z("k"), FFSTR_Z("v"), 0));
	x(CKV_E_OK == ckv_set(&c, FFSTR_Z("k"), FFSTR_Z("vdup"), 0)); // add duplicate
	x(CKV_E_OK == ckv_set(&c, FFSTR_Z("k2"), FFSTR_Z("v2"), 0));
	x(c.n == 3);

	x(CKV_E_OK == ckv_find(&c, FFSTR_Z("k"), &v, 0));
	xseq(&v, "v");

	x(CKV_E_OK == ckv_find(&c, FFSTR_Z("k2"), &v, 0));
	xseq(&v, "v2");

	x(CKV_E_NOTEXIST == ckv_find(&c, FFSTR_Z("k3"), &v, 0));

	i = 0;
	x(CKV_E_OK == ckv_list(&c, &i, &k, &v, 0));
	xseq(&k, "k");
	xseq(&v, "v");

	x(CKV_E_OK == ckv_list(&c, &i, &k, &v, 0));
	xseq(&k, "k");
	xseq(&v, "vdup");

	x(CKV_E_OK == ckv_list(&c, &i, &k, &v, 0));
	xseq(&k, "k2");
	xseq(&v, "v2");

	x(CKV_E_DONE == ckv_list(&c, &i, &k, &v, 0));

	// List Unique

	i = 0;
	x(CKV_E_OK == ckv_list(&c, &i, &k, &v, CKV_F_UNIQUE));
	xseq(&k, "k");
	xseq(&v, "v");

	x(CKV_E_OK == ckv_list(&c, &i, &k, &v, CKV_F_UNIQUE));
	xseq(&k, "k2");
	xseq(&v, "v2");

	x(CKV_E_DONE == ckv_list(&c, &i, &k, &v, CKV_F_UNIQUE));

	// Copy

	ckv_copy(&c2, &c, 0);

	i = 0;
	x(CKV_E_OK == ckv_list(&c2, &i, &k, &v, 0));
	xseq(&k, "k");
	xseq(&v, "v");

	x(CKV_E_OK == ckv_list(&c2, &i, &k, &v, 0));
	xseq(&k, "k");
	xseq(&v, "vdup");

	x(CKV_E_OK == ckv_list(&c2, &i, &k, &v, 0));
	xseq(&k, "k2");
	xseq(&v, "v2");

	x(CKV_E_DONE == ckv_list(&c2, &i, &k, &v, 0));

	ckv_destroy(&c2);

	// Replace

	x(CKV_E_OK_REPLACED == ckv_set(&c, FFSTR_Z("k2"), FFSTR_Z("v"), CKV_F_REPLACE)); // replace inline
	x(CKV_E_OK == ckv_find(&c, FFSTR_Z("k2"), &v, 0));
	xseq(&v, "v");
	x(CKV_E_OK_REPLACED == ckv_set(&c, FFSTR_Z("k2"), FFSTR_Z("v222"), CKV_F_REPLACE));
	x(CKV_E_OK == ckv_set(&c, FFSTR_Z("k3"), FFSTR_Z("v3"), CKV_F_REPLACE)); // new
	x(CKV_E_OK == ckv_find(&c, FFSTR_Z("k2"), &v, 0));
	xseq(&v, "v222");
	x(CKV_E_OK == ckv_find(&c, FFSTR_Z("k"), &v, 0));
	xseq(&v, "v");
	x(CKV_E_OK == ckv_find(&c, FFSTR_Z("k3"), &v, 0));
	xseq(&v, "v3");

	// Unique

	x(CKV_E_OK == ckv_set(&c, FFSTR_Z("k4"), FFSTR_Z("v4"), CKV_F_UNIQUE));
	x(CKV_E_EXISTS == ckv_set(&c, FFSTR_Z("k4"), FFSTR_Z("v444"), CKV_F_UNIQUE));
	x(CKV_E_OK == ckv_find(&c, FFSTR_Z("k4"), &v, 0));
	xseq(&v, "v4");

	// Cache

	x(CKV_E_OK_CACHED == ckv_set(&c, FFSTR_Z("k5"), FFSTR_Z("v5"), CKV_F_CACHE));
	x(CKV_E_OK_CACHED == ckv_find(&c, FFSTR_Z("k5"), &v, 0));
	xseq(&v, "v5");
	x(CKV_E_OK_CACHED == ckv_set(&c, FFSTR_Z("k5"), FFSTR_Z("v555"), CKV_F_CACHE | CKV_F_REPLACE)); // replace cached
	x(CKV_E_OK_CACHED == ckv_find(&c, FFSTR_Z("k5"), &v, 0));
	xseq(&v, "v555");
	x(CKV_E_OK_CACHED == ckv_set(&c, FFSTR_Z("k6"), FFSTR_Z("v6"), CKV_F_CACHE)); // push out k5
	x(CKV_E_OK_CACHED == ckv_find(&c, FFSTR_Z("k6"), &v, 0));
	xseq(&v, "v6");
	x(CKV_E_OK == ckv_find(&c, FFSTR_Z("k5"), &v, 0));
	xseq(&v, "v555");

	x(CKV_E_OK_REPLACED == ckv_set(&c, FFSTR_Z("k6"), FFSTR_Z("123456789012345678901234567890123456789012345678901"), CKV_F_CACHE | CKV_F_REPLACE)); // too large for cache
	x(CKV_E_OK == ckv_find(&c, FFSTR_Z("k6"), &v, 0));
	xseq(&v, "123456789012345678901234567890123456789012345678901");

	x(CKV_E_OK_CACHED == ckv_set(&c, FFSTR_Z("k6"), FFSTR_Z("v6"), CKV_F_CACHE | CKV_F_REPLACE)); // replace bufferred
	x(CKV_E_OK_CACHED == ckv_find(&c, FFSTR_Z("k6"), &v, 0));
	xseq(&v, "v6");

	ckv_destroy(&c);

	// Error

	v.len = 0xffffffff;
	x(CKV_E_LIMIT == ckv_set(&c, FFSTR_Z("k"), v, 0));
}

int main()
{
	test_ckv();
	xlog("DONE");
	return 0;
}
