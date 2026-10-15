#ifndef SCOUR_PLUGINS_H
#define SCOUR_PLUGINS_H

#include "plugin.h"

/* Load every *.so in dir (alphabetical). Missing dir is not an error.
 * Returns the number of plugins successfully loaded. */
int plugins_load(const char *dir);

int plugins_count(void);
const char *plugins_name(int i);   /* name of loaded plugin i, or NULL */

/* Dispatch one result to every plugin's on_result. Returns nonzero if scour's
 * default output should be suppressed (any plugin asked to suppress). The
 * caller must serialize calls; scour invokes this under its output lock. */
int plugins_on_result(const scour_request *req, const scour_response *res);

/* Run each on_exit and unload all plugins. */
void plugins_unload(void);

#endif /* SCOUR_PLUGINS_H */
