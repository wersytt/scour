#ifndef SCOUR_H
#define SCOUR_H

#include <stddef.h>
#include <stdio.h>
#include <pthread.h>

#define SCOUR_VERSION "1.1"
#define DEFAULT_THREADS 10
#define MAX_THREADS 64
#define MAX_EXTS 32
#define DEFAULT_TIMEOUT 10

/* Full state for one run: parsed options, derived values and the shared
 * work queue the worker threads pull from. One instance lives in main(). */
typedef struct {
	/* options */
	const char *url;        /* -u: target URL (may contain FUZZ)        */
	const char *wordlist;   /* -w: path to the wordlist                 */
	const char *domain;     /* -d: base domain for bare vhost words     */
	int threads;            /* -t: worker thread count                  */
	int vhost;              /* -V: fuzz the Host header instead of path */
	int insecure;           /* -k: skip TLS verification                */
	int show_all;           /* -a: show 404s too                        */
	const char *plugins_dir;/* -p: directory to load .so plugins from   */
	int no_plugins;         /* --no-plugins: skip plugin loading        */

	/* request shaping */
	const char *method;      /* -X: HTTP method (default GET)           */
	const char *user_agent;  /* -A: User-Agent override                 */
	const char *cookie;      /* -b: Cookie header value                 */
	const char *proxy;       /* --proxy: proxy URL                      */
	int follow;              /* -r: follow redirects                    */
	int timeout;             /* --timeout: per-request seconds          */
	long delay_ms;           /* --delay: pause after each request (ms)  */

	/* output control */
	const char *output;      /* -o: write results to this file          */
	int quiet;               /* -q: suppress banner/summary             */
	int no_color;            /* --no-color: never emit ANSI             */
	int color;               /* derived: color actually used on stdout  */
	FILE *outfp;             /* runtime: open handle for -o             */

	/* -x: extensions to append to each word (without the dot) */
	char **exts;
	int n_exts;

	/* -H: extra request headers, each as a "Name: value" string */
	char **headers;
	int n_headers;

	/* response filtering */
	int *mc; int n_mc;       /* -mc: only show these status codes       */
	int mc_all;              /* -mc all: show every status code         */
	int *fc; int n_fc;       /* -fc: hide these status codes            */
	long long *ms; int n_ms; /* -ms: only show these response sizes     */
	long long *fs; int n_fs; /* -fs: hide these response sizes          */

	/* derived */
	int fuzz_in_url;         /* URL contains a FUZZ placeholder */
	char base_host[256];     /* domain appended to bare vhost words */

	/* shared runtime state */
	char **words;            /* expanded candidate list */
	size_t word_count;
	size_t next;             /* next index to hand to a worker */
	pthread_mutex_t idx_lock;
	pthread_mutex_t out_lock;
} scour_t;

/* parse_args outcomes */
enum { PARSE_OK = 0, PARSE_HELP, PARSE_ERR };

/* args.c */
void banner(int color);
void usage(const char *prog);
int parse_args(int argc, char *argv[], scour_t *k);
void free_args(scour_t *k);

/* whether a response with this status/size should be shown by default */
int want_result(const scour_t *k, long code, long long size);

/* wordlist.c */
int load_words(scour_t *k);

/* http.c */
void url_host(char *out, size_t n, const char *url);
int run_scan(scour_t *k);

#endif /* SCOUR_H */
