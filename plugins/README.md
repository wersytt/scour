# scour plugins

scour loads every `*.so` in this directory at startup (alphabetical order) and
calls their hooks during a scan. Drop a plugin here, run `make plugins`, and
scour picks it up automatically. Disable loading with `--no-plugins`, or point
at another directory with `-p/--plugins <dir>`.

## Writing a plugin

Include [`../plugin.h`](../plugin.h) and export a single function,
`scour_plugin_register()`, returning a pointer to a static `scour_plugin`:

```c
#include "../plugin.h"

static int result(const scour_request *req, const scour_response *res) {
    /* req->label, req->url, req->host, req->redirect, req->vhost
       res->status, res->size */
    return 1;   /* 1 = let scour print normally; 0 = suppress its output */
}

static const scour_plugin plugin = {
    .abi = SCOUR_PLUGIN_ABI,
    .name = "example",
    .version = "1.0",
    .on_result = result,      /* on_init and on_exit are optional */
};

const scour_plugin *scour_plugin_register(void) { return &plugin; }
```

### Hooks (all optional except the struct fields `abi`/`name`/`version`)

| Hook | When | Return |
|------|------|--------|
| `on_init()` | once, before scanning | `0` to enable, nonzero to disable the plugin |
| `on_result(req, res)` | once per response | nonzero = keep default output, `0` = suppress it |
| `on_exit()` | once, after scanning | — |

### Notes

- `on_result` is **serialized** across worker threads, so you can write to
  files or shared state without locking.
- It is called for **every** response, including 404s that scour would hide —
  so a logging plugin sees everything.
- `abi` must equal `SCOUR_PLUGIN_ABI`; a mismatched plugin is skipped with a
  warning. Bump happens only on incompatible API changes.

## Building

```sh
make plugins          # builds every plugins/*.c into plugins/*.so
# or, by hand:
cc -O2 -fPIC -shared -I.. -o myplugin.so myplugin.c
```

## Security

A plugin is native code running with scour's privileges. Only place `.so`
files you trust in this directory.

## Example

[`jsonlog.c`](jsonlog.c) — appends each result to `./scour.jsonl` as JSON lines
while leaving the normal output intact.
