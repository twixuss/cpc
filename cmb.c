#define NOB_IMPLEMENTATION
#include "common.h"

int main(int argc, char **argv) {
	assert(argc == 2);

	Cmd cmd = {0};

	if (!compile_c(&cmd, argv[1])) return 1;
	return 0;
}
