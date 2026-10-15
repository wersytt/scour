#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "scour.h"
#include "colors.h"

void banner(int color) {
	const char *on = color ? LCYAN BOLD : "";
	const char *off = color ? RESET : "";
	printf("%s", on);
	printf("\n");
	printf("   ______________  __  _______\n");
	printf("  / ___/ ___/ __ \\/ / / / ___/\n");
	printf(" (__  ) /__/ /_/ / /_/ / /    \n");
	printf("/____/\\___/\\____/\\__,_/_/     \n");
	printf("\n");
	printf("%s", off);
}

void usage(const char *prog) {
	banner(isatty(fileno(stdout)));
	printf(LCYAN BOLD "Help for Scour v" SCOUR_VERSION RESET "\n");
	printf(LCYAN "Usage: %s -u <url> -w <wordlist> [options]" RESET "\n\n", prog);
	printf(BOLD "Target" RESET "\n");
	printf("  -u, --url <url>         target URL (use FUZZ as a placeholder, or the word is appended)\n");
	printf("  -w, --wordlist <file>   path to the wordlist to use\n");
	printf("  -x, --extensions <e>    comma-separated extensions to try, e.g. php,txt,html\n");
	printf("  -V, --vhost             virtual-host scan: keep the URL fixed and fuzz the Host header\n");
	printf("  -d, --domain <d>        base domain for bare vhost words (overrides the host from -u)\n");
	printf(BOLD "Request" RESET "\n");
	printf("  -X, --method <m>        HTTP method (default GET)\n");
	printf("  -H, --header <h>        add a request header (repeatable), e.g. -H \"X-Api-Key: abc\"\n");
	printf("  -A, --user-agent <ua>   set the User-Agent (default scour/" SCOUR_VERSION ")\n");
	printf("  -b, --cookie <c>        send a Cookie header, e.g. -b \"session=abc\"\n");
	printf("  -r, --follow-redirects  follow 3xx redirects\n");
	printf("  -k, --insecure          allow insecure TLS (self-signed / mismatched certs)\n");
	printf("      --proxy <url>       route requests through a proxy, e.g. http://127.0.0.1:8080\n");
	printf("  -t, --threads <n>       number of threads (default %d, max %d)\n", DEFAULT_THREADS, MAX_THREADS);
	printf("      --timeout <s>       per-request timeout in seconds (default %d)\n", DEFAULT_TIMEOUT);
	printf("      --delay <ms>        pause this many ms after each request (per thread)\n");
	printf(BOLD "Filter / output" RESET "\n");
	printf("  -mc, --match-codes <c>  only show these status codes (comma list, or \"all\")\n");
	printf("  -fc, --filter-codes <c> hide these status codes (comma list)\n");
	printf("  -ms, --match-size <n>   only show these response sizes (comma list)\n");
	printf("  -fs, --filter-size <n>  hide these response sizes (comma list)\n");
	printf("  -a, --all               show all responses, including 404\n");
	printf("  -o, --output <file>     also write results (uncolored) to this file\n");
	printf("  -q, --quiet             suppress the banner and summary (results only)\n");
	printf("      --no-color          never emit ANSI color\n");
	printf(BOLD "Plugins / misc" RESET "\n");
	printf("  -p, --plugins <dir>     directory to load .so plugins from (default \"plugins\")\n");
	printf("      --no-plugins        do not load any plugins\n");
	printf("      --version           print version and exit\n");
	printf("  -h, --help              show this help\n");
}

/* matches argv[i] against a short or long option name */
#define OPT(s, l) (strcmp(a, (s)) == 0 || strcmp(a, (l)) == 0)

/* append a heap copy of s to a growable string array; returns 0 on success */
static int add_str(char ***arr, int *n, const char *s) {
	char **tmp = realloc(*arr, (size_t)(*n + 1) * sizeof **arr);
	if (!tmp)
		return -1;
	*arr = tmp;
	(*arr)[*n] = strdup(s);
	if (!(*arr)[*n])
		return -1;
	(*n)++;
	return 0;
}

/* add one extension, tolerating a leading dot; silently caps at MAX_EXTS */
static int add_ext(scour_t *k, const char *s) {
	if (*s == '.')
		s++;
	if (*s == '\0' || k->n_exts >= MAX_EXTS)
		return 0;
	return add_str(&k->exts, &k->n_exts, s);
}

static int add_int(int **arr, int *n, int v) {
	int *tmp = realloc(*arr, (size_t)(*n + 1) * sizeof **arr);
	if (!tmp)
		return -1;
	*arr = tmp;
	(*arr)[(*n)++] = v;
	return 0;
}

static int add_ll(long long **arr, int *n, long long v) {
	long long *tmp = realloc(*arr, (size_t)(*n + 1) * sizeof **arr);
	if (!tmp)
		return -1;
	*arr = tmp;
	(*arr)[(*n)++] = v;
	return 0;
}

/* parse a comma-separated list of status codes into arr; "all" sets *all */
static int parse_codes(const char *csv, int **arr, int *n, int *all) {
	char *copy = strdup(csv);
	if (!copy)
		return -1;
	for (char *t = strtok(copy, ","); t; t = strtok(NULL, ",")) {
		if (all && strcmp(t, "all") == 0)
			*all = 1;
		else
			add_int(arr, n, atoi(t));
	}
	free(copy);
	return 0;
}

/* parse a comma-separated list of sizes into arr */
static int parse_sizes(const char *csv, long long **arr, int *n) {
	char *copy = strdup(csv);
	if (!copy)
		return -1;
	for (char *t = strtok(copy, ","); t; t = strtok(NULL, ","))
		add_ll(arr, n, atoll(t));
	free(copy);
	return 0;
}

int parse_args(int argc, char *argv[], scour_t *k) {
	k->threads = DEFAULT_THREADS;
	k->timeout = DEFAULT_TIMEOUT;
	k->plugins_dir = "plugins";
	k->method = "GET";

	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (OPT("-h", "--help")) {
			usage(argv[0]);
			return PARSE_HELP;
		} else if (strcmp(a, "--version") == 0) {
			printf("scour %s\n", SCOUR_VERSION);
			return PARSE_HELP;
		} else if (OPT("-u", "--url") && i + 1 < argc) {
			k->url = argv[++i];
		} else if (OPT("-w", "--wordlist") && i + 1 < argc) {
			k->wordlist = argv[++i];
		} else if (OPT("-t", "--threads") && i + 1 < argc) {
			k->threads = atoi(argv[++i]);
		} else if (OPT("-x", "--extensions") && i + 1 < argc) {
			char *copy = strdup(argv[++i]);
			if (!copy) {
				fprintf(stderr, "out of memory\n");
				return PARSE_ERR;
			}
			for (char *t = strtok(copy, ","); t; t = strtok(NULL, ","))
				add_ext(k, t);
			free(copy);
		} else if (OPT("-H", "--header") && i + 1 < argc) {
			const char *h = argv[++i];
			if (!strchr(h, ':'))
				fprintf(stderr, BOLD LYELLOW
				        "Note: header \"%s\" has no ':'; expected \"Name: value\"." RESET "\n", h);
			if (add_str(&k->headers, &k->n_headers, h) != 0) {
				fprintf(stderr, "out of memory\n");
				return PARSE_ERR;
			}
		} else if (OPT("-d", "--domain") && i + 1 < argc) {
			k->domain = argv[++i];
		} else if (OPT("-V", "--vhost")) {
			k->vhost = 1;
		} else if (OPT("-k", "--insecure")) {
			k->insecure = 1;
		} else if (OPT("-a", "--all")) {
			k->show_all = 1;
		} else if (OPT("-X", "--method") && i + 1 < argc) {
			k->method = argv[++i];
		} else if (OPT("-A", "--user-agent") && i + 1 < argc) {
			k->user_agent = argv[++i];
		} else if (OPT("-b", "--cookie") && i + 1 < argc) {
			k->cookie = argv[++i];
		} else if (OPT("-r", "--follow-redirects")) {
			k->follow = 1;
		} else if (strcmp(a, "--proxy") == 0 && i + 1 < argc) {
			k->proxy = argv[++i];
		} else if (strcmp(a, "--timeout") == 0 && i + 1 < argc) {
			k->timeout = atoi(argv[++i]);
		} else if (strcmp(a, "--delay") == 0 && i + 1 < argc) {
			k->delay_ms = atol(argv[++i]);
		} else if (OPT("-mc", "--match-codes") && i + 1 < argc) {
			parse_codes(argv[++i], &k->mc, &k->n_mc, &k->mc_all);
		} else if (OPT("-fc", "--filter-codes") && i + 1 < argc) {
			parse_codes(argv[++i], &k->fc, &k->n_fc, NULL);
		} else if (OPT("-ms", "--match-size") && i + 1 < argc) {
			parse_sizes(argv[++i], &k->ms, &k->n_ms);
		} else if (OPT("-fs", "--filter-size") && i + 1 < argc) {
			parse_sizes(argv[++i], &k->fs, &k->n_fs);
		} else if (OPT("-o", "--output") && i + 1 < argc) {
			k->output = argv[++i];
		} else if (OPT("-q", "--quiet")) {
			k->quiet = 1;
		} else if (strcmp(a, "--no-color") == 0) {
			k->no_color = 1;
		} else if (OPT("-p", "--plugins") && i + 1 < argc) {
			k->plugins_dir = argv[++i];
		} else if (strcmp(a, "--no-plugins") == 0) {
			k->no_plugins = 1;
		} else {
			fprintf(stderr, BOLD LRED "Unknown or incomplete argument: %s" RESET "\n", a);
			return PARSE_ERR;
		}
	}

	if (!k->url || !k->wordlist) {
		fprintf(stderr, BOLD LRED
		        "Both -u <url> and -w <wordlist> are required. See -h." RESET "\n");
		return PARSE_ERR;
	}

	if (k->threads < 1)
		k->threads = 1;
	if (k->threads > MAX_THREADS)
		k->threads = MAX_THREADS;
	if (k->timeout < 1)
		k->timeout = DEFAULT_TIMEOUT;
	if (k->delay_ms < 0)
		k->delay_ms = 0;

	/* extensions make no sense when fuzzing hostnames */
	if (k->n_exts > 0 && k->vhost) {
		fprintf(stderr, BOLD LYELLOW "Note: -x is ignored in vhost mode." RESET "\n");
		for (int i = 0; i < k->n_exts; i++)
			free(k->exts[i]);
		free(k->exts);
		k->exts = NULL;
		k->n_exts = 0;
	}
	if (k->domain && !k->vhost)
		fprintf(stderr, BOLD LYELLOW "Note: -d only applies in vhost mode." RESET "\n");

	/* color: honor --no-color, otherwise only when stdout is a terminal */
	k->color = !k->no_color && isatty(fileno(stdout));

	/* derived values */
	k->fuzz_in_url = strstr(k->url, "FUZZ") != NULL;
	if (k->vhost) {
		if (k->domain) {
			const char *d = *k->domain == '.' ? k->domain + 1 : k->domain;
			snprintf(k->base_host, sizeof k->base_host, "%s", d);
		} else {
			url_host(k->base_host, sizeof k->base_host, k->url);
		}
	}

	return PARSE_OK;
}

int want_result(const scour_t *k, long code, long long size) {
	/* size filters */
	for (int i = 0; i < k->n_fs; i++)
		if (size == k->fs[i])
			return 0;
	if (k->n_ms > 0) {
		int ok = 0;
		for (int i = 0; i < k->n_ms; i++)
			if (size == k->ms[i])
				ok = 1;
		if (!ok)
			return 0;
	}

	/* status-code filters */
	if (k->mc_all)
		return 1;
	if (k->n_mc > 0) {
		for (int i = 0; i < k->n_mc; i++)
			if (code == k->mc[i])
				return 1;
		return 0;
	}
	if (k->n_fc > 0) {
		for (int i = 0; i < k->n_fc; i++)
			if (code == k->fc[i])
				return 0;
		return 1;
	}

	/* default: hide 404 unless -a */
	if (code == 404 && !k->show_all)
		return 0;
	return 1;
}

void free_args(scour_t *k) {
	for (int i = 0; i < k->n_exts; i++)
		free(k->exts[i]);
	free(k->exts);
	for (int i = 0; i < k->n_headers; i++)
		free(k->headers[i]);
	free(k->headers);
	free(k->mc);
	free(k->fc);
	free(k->ms);
	free(k->fs);
	for (size_t i = 0; i < k->word_count; i++)
		free(k->words[i]);
	free(k->words);
}
