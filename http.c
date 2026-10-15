#define _DEFAULT_SOURCE   /* nanosleep */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <curl/curl.h>

#include "scour.h"
#include "colors.h"
#include "plugins.h"

/* discard response bodies */
static size_t sink(void *ptr, size_t size, size_t nmemb, void *ud) {
	(void)ptr; (void)ud;
	return size * nmemb;
}

/* copy the host part of a URL (between scheme:// and the next /, : or ?) into out */
void url_host(char *out, size_t n, const char *url) {
	const char *p = strstr(url, "://");
	p = p ? p + 3 : url;
	size_t i = 0;
	while (p[i] && p[i] != '/' && p[i] != ':' && p[i] != '?' && i + 1 < n) {
		out[i] = p[i];
		i++;
	}
	out[i] = '\0';
}

/* build the request URL for a candidate into out (size n) */
static void build_url(const scour_t *k, char *out, size_t n, const char *word) {
	if (k->vhost) {
		snprintf(out, n, "%s", k->url);        /* URL fixed; the word drives Host */
	} else if (k->fuzz_in_url) {
		const char *p = strstr(k->url, "FUZZ");
		size_t pre = (size_t)(p - k->url);
		snprintf(out, n, "%.*s%s%s", (int)pre, k->url, word, p + 4);
	} else {
		size_t len = strlen(k->url);
		int slash = (len > 0 && k->url[len - 1] == '/') ? 1 : 0;
		const char *w = word;
		if (slash && w[0] == '/') w++;         /* avoid double slash */
		snprintf(out, n, "%s%s%s", k->url, slash ? "" : "/", w);
	}
}

/* build the Host header value for a vhost candidate into out (size n) */
static void build_host(const scour_t *k, char *out, size_t n, const char *word) {
	if (strchr(word, '.') || k->base_host[0] == '\0')
		snprintf(out, n, "%s", word);          /* already a full hostname */
	else
		snprintf(out, n, "%s.%s", word, k->base_host);
}

static const char *code_color(long code) {
	if (code >= 200 && code < 300) return LGREEN;
	if (code >= 300 && code < 400) return LCYAN;
	if (code == 401 || code == 403) return LYELLOW;
	if (code >= 400 && code < 500) return GRAY;
	if (code >= 500) return LRED;
	return WHITE;
}

/* print one result to stdout (colored per k->color) and, if set, to the
 * output file (always uncolored). Caller holds the output lock. */
static void emit_result(const scour_t *k, const char *label, long code,
                        long long size, const char *redirect) {
	char suffix[4200];
	suffix[0] = '\0';
	if (redirect && *redirect)
		snprintf(suffix, sizeof suffix, " -> %s", redirect);

	if (k->color) {
		const char *cc = code_color(code);
		printf("%s%s%-40s" RESET " [Status: %s%ld" RESET "] [Size: %lld]%s%s" RESET "\n",
		       cc, BOLD, label, cc, code, size, *suffix ? LMAGENTA : "", suffix);
	} else {
		printf("%-40s [Status: %ld] [Size: %lld]%s\n", label, code, size, suffix);
	}
	fflush(stdout);

	if (k->outfp) {
		fprintf(k->outfp, "%-40s [Status: %ld] [Size: %lld]%s\n", label, code, size, suffix);
		fflush(k->outfp);
	}
}

/* fresh header list for one request: the user's -H headers plus an optional
 * Host override. Built per request so it is never shared between threads. */
static struct curl_slist *build_headers(const scour_t *k, const char *host) {
	struct curl_slist *list = NULL, *tmp;
	for (int i = 0; i < k->n_headers; i++) {
		tmp = curl_slist_append(list, k->headers[i]);
		if (tmp) list = tmp;
	}
	if (host) {
		char hdr[600];
		snprintf(hdr, sizeof hdr, "Host: %s", host);
		tmp = curl_slist_append(list, hdr);
		if (tmp) list = tmp;
	}
	return list;
}

static void *worker(void *arg) {
	scour_t *k = arg;
	CURL *curl = curl_easy_init();
	if (!curl)
		return NULL;

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, sink);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, k->follow ? 1L : 0L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)k->timeout);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT,
	                 k->user_agent ? k->user_agent : "scour/" SCOUR_VERSION);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, k->insecure ? 0L : 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, k->insecure ? 0L : 2L);
	if (strcmp(k->method, "GET") != 0)
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, k->method);
	if (strcmp(k->method, "HEAD") == 0)
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
	if (k->cookie)
		curl_easy_setopt(curl, CURLOPT_COOKIE, k->cookie);
	if (k->proxy)
		curl_easy_setopt(curl, CURLOPT_PROXY, k->proxy);

	char url[4096];
	for (;;) {
		pthread_mutex_lock(&k->idx_lock);
		size_t i = k->next < k->word_count ? k->next++ : k->word_count;
		pthread_mutex_unlock(&k->idx_lock);
		if (i >= k->word_count)
			break;

		build_url(k, url, sizeof url, k->words[i]);
		curl_easy_setopt(curl, CURLOPT_URL, url);

		char host[512];
		const char *label = k->words[i];
		if (k->vhost) {
			build_host(k, host, sizeof host, k->words[i]);
			label = host;
		}
		struct curl_slist *headers = build_headers(k, k->vhost ? host : NULL);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		CURLcode res = curl_easy_perform(curl);
		if (res == CURLE_OK) {
			long code = 0;
			curl_off_t dl = 0;
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
			curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &dl);

			/* for a redirect, where would it send us? (valid until the next
			 * curl call on this handle, which is why we use it before looping) */
			char *redirect = NULL;
			if (code >= 300 && code < 400)
				curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &redirect);

			int wanted = want_result(k, code, (long long)dl);
			/* skip entirely only when there's nothing to print and no plugin to notify */
			if (code != 0 && (plugins_count() > 0 || wanted)) {
				scour_request preq = {
					.label = label, .url = url,
					.host = k->vhost ? host : NULL,
					.redirect = redirect, .vhost = k->vhost,
				};
				scour_response pres = { .status = code, .size = (long long)dl };

				pthread_mutex_lock(&k->out_lock);
				int suppress = plugins_on_result(&preq, &pres);
				if (!suppress && wanted)
					emit_result(k, label, code, (long long)dl, redirect);
				pthread_mutex_unlock(&k->out_lock);
			}
		}

		curl_slist_free_all(headers);

		if (k->delay_ms > 0) {
			struct timespec ts = {
				.tv_sec = k->delay_ms / 1000,
				.tv_nsec = (k->delay_ms % 1000) * 1000000L,
			};
			nanosleep(&ts, NULL);
		}
	}

	curl_easy_cleanup(curl);
	return NULL;
}

/* spin up the worker pool and run the scan to completion */
int run_scan(scour_t *k) {
	pthread_mutex_init(&k->idx_lock, NULL);
	pthread_mutex_init(&k->out_lock, NULL);
	k->next = 0;

	curl_global_init(CURL_GLOBAL_DEFAULT);

	int threads = k->threads;
	if ((size_t)threads > k->word_count)
		threads = (int)k->word_count;

	pthread_t tids[MAX_THREADS];
	int made = 0;
	for (int i = 0; i < threads; i++) {
		if (pthread_create(&tids[i], NULL, worker, k) == 0)
			made++;
		else
			break;
	}
	for (int i = 0; i < made; i++)
		pthread_join(tids[i], NULL);

	curl_global_cleanup();
	pthread_mutex_destroy(&k->idx_lock);
	pthread_mutex_destroy(&k->out_lock);
	return 0;
}
