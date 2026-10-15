#ifndef SCOUR_PLUGIN_H
#define SCOUR_PLUGIN_H

/*
 * Public plugin API for scour.
 *
 * A plugin is a shared object (.so) placed in the plugins directory. It must
 * export one symbol, scour_plugin_register(), returning a pointer to a static
 * scour_plugin describing itself and its hooks. scour loads every .so in the
 * directory at startup, in alphabetical order.
 *
 * Build a plugin with, e.g.:
 *     cc -O2 -fPIC -shared -I. -o plugins/mine.so plugins/mine.c
 * or just `make plugins` from the project root.
 *
 * SECURITY: a plugin is native code that runs with scour's privileges. Only
 * place .so files you trust in the plugins directory.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Bump this whenever the structs below change incompatibly. A plugin built
 * against a different ABI is rejected at load time. */
#define SCOUR_PLUGIN_ABI 1

/* What was requested. */
typedef struct {
	const char *label;    /* the string scour would print (path, or Host in vhost mode) */
	const char *url;      /* full URL that was requested */
	const char *host;     /* Host header value, or NULL when not in vhost mode */
	const char *redirect; /* for a 3xx, the Location target URL; NULL otherwise */
	int vhost;            /* nonzero if this was a vhost-mode request */
} scour_request;

/* What came back. */
typedef struct {
	long status;         /* HTTP status code */
	long long size;      /* bytes downloaded */
} scour_response;

typedef struct {
	int abi;                 /* must be set to SCOUR_PLUGIN_ABI */
	const char *name;        /* short plugin name */
	const char *version;     /* plugin version string */

	/* Optional. Called once before scanning starts. Return 0 to enable the
	 * plugin; return nonzero to disable it (it is unloaded, no further hooks). */
	int (*on_init)(void);

	/* Optional. Called once per response. Calls are serialized across worker
	 * threads, so a plugin may touch its own state or files without locking.
	 * Return nonzero to let scour print its normal result line; return 0 to
	 * suppress scour's default output for this result (e.g. the plugin prints
	 * or filters its own). Note: this is also called for 404s even when scour
	 * would hide them, so plugins can observe every response. */
	int (*on_result)(const scour_request *req, const scour_response *res);

	/* Optional. Called once after scanning finishes, before the .so is unloaded. */
	void (*on_exit)(void);
} scour_plugin;

/* Every plugin must export this symbol. */
const scour_plugin *scour_plugin_register(void);

#ifdef __cplusplus
}
#endif

#endif /* SCOUR_PLUGIN_H */
