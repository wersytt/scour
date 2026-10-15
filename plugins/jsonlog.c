/*
 * jsonlog - example scour plugin.
 *
 * Appends every response to ./scour.jsonl as one JSON object per line, and
 * leaves scour's normal colored output untouched. A small, complete example
 * of the plugin API.
 *
 * Build: `make plugins` (or cc -O2 -fPIC -shared -I.. -o jsonlog.so jsonlog.c)
 */

#include <stdio.h>
#include "../plugin.h"

static FILE *out;

/* write s as a JSON string value, escaping " and \ and control chars */
static void json_str(FILE *f, const char *s) {
	fputc('"', f);
	for (; s && *s; s++) {
		unsigned char c = (unsigned char)*s;
		if (c == '"' || c == '\\')
			fprintf(f, "\\%c", c);
		else if (c < 0x20)
			fprintf(f, "\\u%04x", c);
		else
			fputc(c, f);
	}
	fputc('"', f);
}

static int init(void) {
	out = fopen("scour.jsonl", "a");
	return out ? 0 : 1;   /* disable the plugin if the log can't be opened */
}

static int result(const scour_request *req, const scour_response *res) {
	fputs("{\"label\":", out);
	json_str(out, req->label);
	fputs(",\"url\":", out);
	json_str(out, req->url);
	if (req->host) {
		fputs(",\"host\":", out);
		json_str(out, req->host);
	}
	if (req->redirect) {
		fputs(",\"redirect\":", out);
		json_str(out, req->redirect);
	}
	fprintf(out, ",\"status\":%ld,\"size\":%lld}\n", res->status, res->size);
	fflush(out);
	return 1;   /* keep scour's default output */
}

static void finish(void) {
	if (out)
		fclose(out);
}

static const scour_plugin plugin = {
	.abi = SCOUR_PLUGIN_ABI,
	.name = "jsonlog",
	.version = "1.0",
	.on_init = init,
	.on_result = result,
	.on_exit = finish,
};

const scour_plugin *scour_plugin_register(void) {
	return &plugin;
}
