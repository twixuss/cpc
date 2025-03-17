#define NOB_IMPLEMENTATION
#include "common.h"

int get_line(String_View source, char const *c) {
	int lines = 0;
	while (c >= source.data) lines += *c-- == '\n';
	return lines + 1;
}
int get_column(String_View source, char const *c) {
	char const *original = c;
	while (c >= source.data && *c != '\n') --c;
	return (int)(original - c + 1);
}

#define Loc_Fmt "%s:%d:%d"
#define Loc_Arg(path, source, cursor) path, get_line(source, cursor), get_column(source, cursor)

bool mkdir_silent(const char *path)
{
#ifdef _WIN32
    int result = mkdir(path);
#else
    int result = mkdir(path, 0755);
#endif
    if (result < 0) {
        if (errno == EEXIST) {
            return true;
        }
        nob_log(NOB_ERROR, "could not create directory `%s`: %s", path, strerror(errno));
        return false;
    }

    nob_log(NOB_INFO, "created directory `%s`", path);
    return true;
}



bool is_file(char const *path) {
	FILE *file = fopen(path, "rb");
	if (file) {
		fclose(file);
		return true;
	}
	return false;
}

bool delete_directory(char const *dir, bool delete_contents);
bool delete_directory_contents(char const *dir) {
	File_Paths items = {0};
	if (!read_entire_dir(dir, &items)) {
		da_free(items);
		return false;
	}
	
	for (size_t i = 0; i < items.count; ++i) {
		char const *item = items.items[i];
		if (strcmp(item, ".") == 0 || strcmp(item, "..") == 0)
			continue;

		char const *path = temp_sprintf("%s/%s", dir, item);
		if (is_file(path)) {
			if (!delete_file(path))
				return false;
		} else {
			if (!delete_directory(path, true))
				return false;
		}
	}

	da_free(items);
	return true;
}
bool delete_directory(char const *dir, bool delete_contents) {
	if (delete_contents) {
		if (!delete_directory_contents(dir)) {
			return false;
		}
	}

	if (rmdir(dir) < 0)
		return false;

	return true;
}

char *meta_compiler;
String_View meta_token;
String_View share_token;
Cmd cmd;

typedef struct Context {
	char const *source_path;
	String_View source;
	
	// Not actually a command, just an array of strings.
	Cmd meta_args;

	bool is_meta;
	int meta_index;
	String_Builder builder;

	String_Builder result_builder;
} Context;

#define Loc_Arg_context(cursor) Loc_Arg(context->source_path, context->source, cursor)

bool context_update(Context *context, String_View code) {
	if (context->is_meta) {
		String_Builder wrap = {0};
		String_View wrap_prefix = sv_from_cstr("WRAP=");
		char *wrap_path = "./wraps/default.wrap";
		if (sv_starts_with(code, wrap_prefix)) {
			String_View found_new_line = sv_find(code, sv_from_cstr("\n"));
			if (!found_new_line.count) {
				fprintf(stderr, Loc_Fmt": ERROR: Could not parse parameter. \\n not found.\n", Loc_Arg_context(code.data));
				return false;
			}
			String_View wrap_path_sv = sv_from_pointers(code.data + wrap_prefix.count, found_new_line.data);
			wrap_path = temp_sprintf(SV_Fmt, SV_Arg(wrap_path_sv));

			code = sv_set_begin(code, sv_end(found_new_line));
		}
		if (!read_entire_file(wrap_path, &wrap)) {
			fprintf(stderr, Loc_Fmt": ERROR: Could not read wrap file %s\n", Loc_Arg_context(code.data), wrap_path);
			return false;
		}

		String_View pre_wrap  = {0};
		String_View post_wrap = {0};

		String_View found = sv_find(sb_to_sv(wrap), meta_token);
		if (found.count) {
			pre_wrap  = sv_from_pointers(wrap.items, found.data);
			post_wrap = sv_from_pointers(sv_end(found), sb_end(wrap));
		} else {
			fprintf(stderr, Loc_Fmt": ERROR: Could not find meta token \"" SV_Fmt "\"", Loc_Arg(wrap_path, sb_to_sv(wrap), wrap.items), SV_Arg(meta_token));
			return false;
		}

		String_View shared_code = {0};
		found = sv_find_last(sb_to_sv(context->result_builder), share_token);
		if (found.count) {
			shared_code = sv_from_pointers(context->result_builder.items, found.data);

			// Remove share_token from result_builder.
			// Don't like this much, but it works, so whatever.
			memmove((char *)found.data, sv_end(found), context->result_builder.items + context->result_builder.count - sv_end(found));
			context->result_builder.count -= found.count;
		}

		sb_append_buf(&context->builder, shared_code.data, shared_code.count);
		sb_append_buf(&context->builder, pre_wrap.data, pre_wrap.count);
		sb_append_buf(&context->builder, code.data, code.count);
		sb_append_buf(&context->builder, post_wrap.data, post_wrap.count);

		mkdir_silent("./.cpc");
		{
			Log_Level old = minimal_log_level;
			minimal_log_level = NOB_WARNING;
				
			delete_directory_contents("./.cpc");
			
			minimal_log_level = old;
		}
		char *meta_source_path = temp_sprintf("./.cpc/%d.c", context->meta_index);
		char *meta_exe_path    = temp_sprintf("./.cpc/%d.exe", context->meta_index);
		char *meta_log_path    = temp_sprintf("./.cpc/%d.log", context->meta_index);
		char *meta_out_path    = temp_sprintf("./.cpc/%d.output", context->meta_index);

		write_entire_file(meta_source_path, context->builder.items, context->builder.count);
		context->builder.count = 0;

		cmd_append(&cmd, meta_compiler, meta_source_path);

		Fd meta_log_fd = fd_open_for_write(meta_log_path);
		if (meta_log_fd == INVALID_FD)
			return false;
		if (!cmd_run_sync_redirect_and_reset(&cmd, (Nob_Cmd_Redirect) { .fdout = &meta_log_fd })) {
			fprintf(stderr, Loc_Fmt": ERROR: Could not compile meta block #%d\n", Loc_Arg_context(code.data), context->meta_index);

			read_entire_file(meta_log_path, &context->builder);
			fprintf(stderr, "%.*s", (int)context->builder.count, context->builder.items);
			context->builder.count = 0;

			return false;
		}

		cmd_append(&cmd, meta_exe_path);
		da_append_many(&cmd, context->meta_args.items, context->meta_args.count);

		Fd meta_out_fd = fd_open_for_write(meta_out_path);
		if (meta_out_fd == INVALID_FD)
			return false;
		if (!cmd_run_sync_redirect_and_reset(&cmd, (Nob_Cmd_Redirect) { .fdout = &meta_out_fd })) {
			fprintf(stderr, Loc_Fmt": ERROR: Could not execute meta block #%d\n", Loc_Arg_context(code.data), context->meta_index);
			
			return false;
		}

		read_entire_file(meta_out_path, &context->builder);
		da_append_many(&context->result_builder, context->builder.items, context->builder.count);
		context->builder.count = 0;

		++context->meta_index;
	} else {
		da_append_many(&context->result_builder, code.data, code.count);
	}

	context->is_meta ^= true;

	return true;
}

void usage(char const *program_name) {
	fprintf(stderr, "Usage: %s input.cm [-o output.c] <meta-compiler> <meta-args>\n", program_name);
}

int main(int argc, char **argv) {
	Context context = {0};

	char *program_name = shift(argv, argc);
	
	if (!argc) {
		fprintf(stderr, "ERROR: Expected <input.cm> argument.\n");
		return 1;
	}
	context.source_path = shift(argv, argc);
	
	if (strcmp(context.source_path, "help") == 0 ||
		strcmp(context.source_path, "-help") == 0 ||
		strcmp(context.source_path, "--help") == 0)
	{
		usage(program_name);
		return 1;
	}

	char *output_path = 0;
	if (argc) {
		if (strcmp(argv[0], "-o") == 0) {
			shift(argv, argc);
			if (!argc) {
				fprintf(stderr, "ERROR: Expected an argument after -o\n");
				return 1;
			}
			output_path = shift(argv, argc);
		}
	}
	
	if (!argc) {
		fprintf(stderr, "ERROR: Expected <meta-compiler> argument.\n");
		return 1;
	}
	meta_compiler = shift(argv, argc);
	
	while (argc) {
		da_append(&context.meta_args, shift(argv, argc));
	}

	String_Builder sb = {0};
	if (!read_entire_file(context.source_path, &sb)) {
		return 1;
	}

	context.source = sv_from_parts(sb.items, sb.count);

	String_View remaining = context.source;

	meta_token = sv_from_cstr("$$");
	share_token = sv_from_cstr("@@");

	while (true) {
		String_View found = sv_find(remaining, meta_token);
		if (found.count) {
			if (!context_update(&context, sv_from_pointers(remaining.data, found.data)))
				return 1;
			remaining = sv_set_begin(remaining, sv_end(found));
		} else {
			break;
		}
	}
	if (remaining.count) {
		if (!context_update(&context, remaining))
			return 1;
	}

	if (!output_path) {
		output_path = with_extension(context.source_path, "c");
	}

	if (!write_entire_file(output_path, context.result_builder.items, context.result_builder.count))
		return 1;

	return 0;
}