/** util: test runner */

#include <ffsys/globals.h>
#include <ffsys/test.h>

struct ffos_test fftest;

void test_ip();
void test_http1();

int main()
{
	test_ip();
	test_http1();
	fflog("Test checks made: %u", fftest.checks_success);
	return 0;
}
