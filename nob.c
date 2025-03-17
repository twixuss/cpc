#define NOB_IMPLEMENTATION
#include "common.h"

int main(int argc, char **argv) {
	NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "nob.h", "common.h");

	char *program_name = shift(argv, argc);
	bool run_result = false;
	if (argc) {
		char *subcommand = shift(argv, argc);
		if (strcmp(subcommand, "run") == 0) {
			run_result = true;
		}
	}

	Cmd cmd = {0};

	if (!compile_c(&cmd, "cpc.c")) return 1;

	if (!compile_c(&cmd, "cmb.c")) return 1;

	if (run_result) {
		cmd_append(&cmd, "./cpc");
		while (argc) {
			char *arg = shift(argv, argc);
			cmd_append(&cmd, arg);
		}
		if (!cmd_run_sync_and_reset(&cmd)) return 1;
	}
	return 0;
}
