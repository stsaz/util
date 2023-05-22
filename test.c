/** util: test runner */

#include <FFOS/ffos-extern.h>
ffuint _ffos_checks_success;
ffuint _ffos_keep_running;

void test_ip();

int main()
{
	test_ip();
	return 0;
}
