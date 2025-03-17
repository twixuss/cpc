#ifndef CPC_COMMON_H_
#define CPC_COMMON_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NOB_STRIP_PREFIX
#include "nob.h"

#define brk __debugbreak()

char const *sb_end(String_Builder sb) {
	return sb.items + sb.count;
}

char const *sv_end(String_View string) {
	return string.data + string.count;
}

String_View sv_from_pointers(char const *begin, char const *end) {
	return sv_from_parts(begin, end - begin);
}

// Update beginning of str, keeping end the same.
String_View sv_set_begin(String_View str, char const *new_begin) {
	return sv_from_parts(new_begin, sv_end(str) - new_begin);
}

// Returns the first occurrence of `what` in `where`.
// If not found, returns empty sv pointing to end of `what`
String_View sv_find(String_View where, String_View what) {
	if (where.count >= what.count) {
		for (size_t i = 0; i < where.count - what.count + 1; ++i) {
			for (size_t j = 0; j < what.count; ++j) {
				if (where.data[i + j] != what.data[j]) {
					goto continue_outer;
				}
			}
			return sv_from_parts(where.data + i, what.count);
		continue_outer:;
		}
	}
	return sv_from_parts(sv_end(where), 0);
}

// Returns the first occurrence of `what` in `where`.
// If not found, returns empty sv pointing to beginning of `what`
String_View sv_find_last(String_View where, String_View what) {
	if (where.count >= what.count) {
		for (size_t i = where.count - what.count; i != (size_t)-1; --i) {
			for (size_t j = 0; j < what.count; ++j) {
				if (where.data[i + j] != what.data[j]) {
					goto continue_outer;
				}
			}
			return sv_from_parts(where.data + i, what.count);
		continue_outer:;
		}
	}
	return sv_from_parts(where.data, 0);
}

char *with_extension(char const *path, char const *ext) {
	String_View last_dot = sv_find_last(sv_from_cstr(path), sv_from_cstr("."));
	String_View last_slash = sv_find_last(sv_from_cstr(path), sv_from_cstr("/"));
	if (!last_slash.count)
		last_slash = sv_find_last(sv_from_cstr(path), sv_from_cstr("\\"));

	if (last_dot.count) {
		if (!last_slash.count || last_dot.data > last_slash.data) {
			return temp_sprintf(SV_Fmt".%s", SV_Arg(sv_from_pointers(path, last_dot.data)), ext);
		}
	}
	return temp_strdup(path);
}


bool force_recompile = true;
bool compile_c(Cmd *cmd, char const *c_source) {
	char *output_path = with_extension(c_source, "exe");

	if (!force_recompile && !needs_rebuild1(output_path, c_source)) {
		return true;
	}

	cmd_append(cmd, "cl", "/nologo", "/W4", "/D_CRT_SECURE_NO_WARNINGS", "/D_CRT_NONSTDC_NO_WARNINGS", "/Zi", c_source, "/link", temp_sprintf("/out:%s", output_path));
	//cmd_append(cmd, "gcc", "-Wall", "-Wextra", "-ggdb", c_source, "-o", with_extension(c_source, "exe"));

	if (!cmd_run_sync_and_reset(cmd))
		return false;
	return true;
}

#endif // CPC_COMMON_H_