/** util: test runner */

#include <ffsys/globals.h>
#include <ffsys/test.h>

struct ffos_test fftest;

void test_ip();

int main()
{
	test_ip();
	fflog("Test checks made: %u", fftest.checks_success);
	return 0;
}
