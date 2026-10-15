#define _DEFAULT_SOURCE   /* scandir, alphasort */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <dlfcn.h>

#include "plugins.h"
#include "colors.h"

#define MAX_PLUGINS 64

typedef struct {
	void *handle;
	const scour_plugin *p;
} loaded_t;

static loaded_t g_plugins[MAX_PLUGINS];
static int g_count;

static int is_so(const struct dirent *e) {
	size_t len = strlen(e->d_name);
	return len > 3 && strcmp(e->d_name + len - 3, ".so") == 0;
}

int plugins_load(const char *dir) {
	struct dirent **names = NULL;
	int n = scandir(dir, &names, is_so, alphasort);
	if (n < 0)
		return 0;   /* dir missing / unreadable: just no plugins */

	for (int i = 0; i < n; i++) {
		if (g_count >= MAX_PLUGINS) {
			free(names[i]);
			continue;
		}

		char path[4096];
		snprintf(path, sizeof path, "%s/%s", dir, names[i]->d_name);
		free(names[i]);

		void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
		if (!h) {
			fprintf(stderr, BOLD LYELLOW "Plugin skipped (%s)" RESET "\n", dlerror());
			continue;
		}

		/* object pointer -> function pointer; fine under -Wall -Wextra */
		const scour_plugin *(*reg)(void) =
			(const scour_plugin *(*)(void))dlsym(h, "scour_plugin_register");
		if (!reg) {
			fprintf(stderr, BOLD LYELLOW
			        "Plugin skipped (%s: no scour_plugin_register)" RESET "\n", path);
			dlclose(h);
			continue;
		}

		const scour_plugin *p = reg();
		if (!p || p->abi != SCOUR_PLUGIN_ABI) {
			fprintf(stderr, BOLD LYELLOW
			        "Plugin skipped (%s: ABI mismatch, want %d)" RESET "\n",
			        path, SCOUR_PLUGIN_ABI);
			dlclose(h);
			continue;
		}

		if (p->on_init && p->on_init() != 0) {
			dlclose(h);   /* plugin opted out */
			continue;
		}

		g_plugins[g_count].handle = h;
		g_plugins[g_count].p = p;
		g_count++;
	}

	free(names);
	return g_count;
}

int plugins_count(void) {
	return g_count;
}

const char *plugins_name(int i) {
	if (i < 0 || i >= g_count)
		return NULL;
	return g_plugins[i].p->name;
}

int plugins_on_result(const scour_request *req, const scour_response *res) {
	int suppress = 0;
	for (int i = 0; i < g_count; i++) {
		if (g_plugins[i].p->on_result &&
		    g_plugins[i].p->on_result(req, res) == 0)
			suppress = 1;
	}
	return suppress;
}

void plugins_unload(void) {
	for (int i = 0; i < g_count; i++) {
		if (g_plugins[i].p->on_exit)
			g_plugins[i].p->on_exit();
		dlclose(g_plugins[i].handle);
	}
	g_count = 0;
}
