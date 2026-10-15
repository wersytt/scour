#include <stdio.h>

#include "scour.h"
#include "colors.h"
#include "plugins.h"

/* color string or "" depending on k->color, so info lines honor --no-color */
static const char *C(const scour_t *k, const char *s) {
	return k->color ? s : "";
}

int main(int argc, char *argv[]) {
	if (argc == 1) {
		printf(BOLD LRED "Please provide an argument. For more info check -h" RESET "\n");
		return 1;
	}

	scour_t k = {0};

	switch (parse_args(argc, argv, &k)) {
	case PARSE_HELP:
		return 0;
	case PARSE_ERR:
		return 1;
	}

	if (load_words(&k) != 0) {
		free_args(&k);
		return 1;
	}
	if (k.word_count == 0) {
		fprintf(stderr, BOLD LRED "Wordlist is empty." RESET "\n");
		free_args(&k);
		return 1;
	}

	if (k.output) {
		k.outfp = fopen(k.output, "w");
		if (!k.outfp) {
			fprintf(stderr, BOLD LRED "Could not open output file: %s" RESET "\n", k.output);
			free_args(&k);
			return 1;
		}
	}

	int nplug = k.no_plugins ? 0 : plugins_load(k.plugins_dir);

	if (!k.quiet) {
		const char *c = C(&k, LCYAN), *r = C(&k, RESET);
		banner(k.color);
		printf("%sTarget : %s%s\n", c, r, k.url);
		printf("%sMode   : %s%s\n", c, r,
		       k.vhost ? "vhost (Host header)" : "path discovery");
		if (k.vhost && k.base_host[0])
			printf("%sDomain : %s%s\n", c, r, k.base_host);
		printf("%sMethod : %s%s\n", c, r, k.method);
		printf("%sWords  : %s%zu\n", c, r, k.word_count);
		printf("%sThreads: %s%d\n", c, r, k.threads);
		if (k.n_headers > 0)
			printf("%sHeaders: %s%d custom\n", c, r, k.n_headers);
		if (nplug > 0) {
			printf("%sPlugins: %s%d (", c, r, nplug);
			for (int i = 0; i < nplug; i++)
				printf("%s%s", i ? ", " : "", plugins_name(i));
			printf(")\n");
		}
		if (k.output)
			printf("%sOutput : %s%s\n", c, r, k.output);
		printf("%sTLS    : %s%s\n\n", c, r,
		       k.insecure ? "insecure (verification off)" : "verified");
	}

	run_scan(&k);

	plugins_unload();
	if (k.outfp)
		fclose(k.outfp);

	if (!k.quiet)
		printf("%s\nDone.%s\n", C(&k, LCYAN BOLD), C(&k, RESET));

	free_args(&k);
	return 0;
}
