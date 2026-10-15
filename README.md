# Scour

A small, threaded web content-discovery fuzzer written in C — the same family
of tool as `gobuster`, `ffuf` and `dirb`. Point it at a target with a wordlist
and it requests each candidate, reporting the HTTP status and response size.
It also does **virtual-host discovery** and supports **plugins**.

```
   ______________  __  _______
  / ___/ ___/ __ \/ / / / ___/
 (__  ) /__/ /_/ / /_/ / /
/____/\___/\____/\__,_/_/
```

## Features

- Fast multi-threaded scanning (libcurl + pthreads)
- Path/content discovery with a `FUZZ` placeholder or trailing-word append
- Extension fuzzing (`-x php,txt,html`)
- Virtual-host scanning via the `Host` header (`-V`)
- Custom request headers (`-H`, repeatable)
- Status-code coloring and response sizes (the size column is how you spot hits)
- TLS verification by default, with an opt-out for self-signed targets (`-k`)
- A plugin system for observing/filtering/exporting results

## Requirements

- A C compiler (tested with GCC 16)
- `libcurl` development headers (tested with 8.22) — e.g. `libcurl4-openssl-dev`
  on Debian/Ubuntu, `curl` on Arch
- POSIX threads and `dlopen` (standard on Linux)

## Build

```sh
make            # builds ./scour and every plugin in plugins/
make plugins    # rebuild only the plugins
make clean      # remove binaries and object files
```

## Usage

```
scour -u <url> -w <wordlist> [options]
```

**Target**

| Short | Long | Description |
|-------|------|-------------|
| `-u` | `--url <url>` | Target URL. Use `FUZZ` as a placeholder, or the word is appended to the end. |
| `-w` | `--wordlist <file>` | Path to the wordlist. |
| `-x` | `--extensions <e>` | Comma-separated extensions to try, e.g. `php,txt,html`. |
| `-V` | `--vhost` | Virtual-host scan: keep the URL fixed and fuzz the `Host` header. |
| `-d` | `--domain <d>` | Base domain for bare vhost words (overrides the host from `-u`). |

**Request**

| Short | Long | Description |
|-------|------|-------------|
| `-X` | `--method <m>` | HTTP method (default `GET`). |
| `-H` | `--header <h>` | Add a request header (repeatable), e.g. `-H "X-Api-Key: abc"`. |
| `-A` | `--user-agent <ua>` | Set the User-Agent (default `scour/<version>`). |
| `-b` | `--cookie <c>` | Send a `Cookie` header, e.g. `-b "session=abc"`. |
| `-r` | `--follow-redirects` | Follow 3xx redirects. |
| `-k` | `--insecure` | Allow insecure TLS (self-signed / mismatched certs). |
|      | `--proxy <url>` | Route requests through a proxy, e.g. `http://127.0.0.1:8080`. |
| `-t` | `--threads <n>` | Worker threads (default 10, max 64). |
|      | `--timeout <s>` | Per-request timeout in seconds (default 10). |
|      | `--delay <ms>` | Pause this many ms after each request (per thread). |

**Filter / output**

| Short | Long | Description |
|-------|------|-------------|
| `-mc` | `--match-codes <c>` | Only show these status codes (comma list, or `all`). |
| `-fc` | `--filter-codes <c>` | Hide these status codes (comma list). |
| `-ms` | `--match-size <n>` | Only show these response sizes (comma list). |
| `-fs` | `--filter-size <n>` | Hide these response sizes (comma list). |
| `-a` | `--all` | Show all responses, including 404. |
| `-o` | `--output <file>` | Also write results (uncolored) to a file. |
| `-q` | `--quiet` | Suppress the banner and summary (results only). |
|      | `--no-color` | Never emit ANSI color (also auto-off when output is piped). |

**Plugins / misc**

| Short | Long | Description |
|-------|------|-------------|
| `-p` | `--plugins <dir>` | Directory to load `.so` plugins from (default `plugins`). |
|      | `--no-plugins` | Do not load any plugins. |
|      | `--version` | Print version and exit. |
| `-h` | `--help` | Show help. |

Filtering precedence: size filters apply first (`-fs`/`-ms`), then status-code
filters (`-mc`/`-fc`), then the default (hide 404 unless `-a`). `-mc`/`-ms` are
allow-lists; `-fc`/`-fs` are deny-lists.

### Examples

Directory/content discovery:

```sh
scour -u http://target/ -w words.txt -x php,html -t 20
```

Placeholder anywhere in the URL:

```sh
scour -u http://target/api/FUZZ/v1 -w words.txt
```

Virtual-host discovery (bare words get the domain appended → `admin.example.com`):

```sh
scour -u http://10.0.0.5/ -w subdomains.txt -V -d example.com
```

With a custom header and a self-signed cert:

```sh
scour -u https://target/ -w words.txt -H "Authorization: Bearer x" -k
```

Only report 200/301/302, save to a file, and rate-limit to be gentle:

```sh
scour -u http://target/ -w words.txt -mc 200,301,302 -o results.txt --delay 50
```

Machine-friendly output for a pipeline (no banner, no color — auto-detected):

```sh
scour -u http://target/ -w words.txt -q | tee findings.txt
```

### Reading the output

Each line is `<candidate>  [Status: <code>] [Size: <bytes>]`, colored by status
class (2xx green, 3xx cyan, 401/403 yellow, other 4xx gray, 5xx red). 404s are
hidden unless you pass `-a`. In vhost mode, real virtual hosts usually stand out
by a **different Size** from the default response.

For a redirect (3xx), the line also shows the target: `-> <location>`. Use `-r`
to follow redirects instead, in which case you see the final status.

## How it works

- The wordlist is read once and expanded with any extensions into a candidate
  list.
- A pool of worker threads pulls from a shared, mutex-guarded index, each with
  its own libcurl handle. Output is serialized so lines never interleave.
- In vhost mode the URL stays fixed and each candidate drives a per-request
  `Host` header; otherwise the candidate is appended to the URL (or substituted
  for `FUZZ`).

## Project layout

| File | Responsibility |
|------|----------------|
| `main.c` | Entry point: wire everything together. |
| `args.c` | Banner, help, argument parsing. |
| `wordlist.c` | Load the wordlist and expand extensions. |
| `http.c` | libcurl worker, URL/Host building, the thread pool. |
| `plugins.c` | Plugin loader and hook dispatch. |
| `scour.h` | The `scour_t` run-context struct and internal declarations. |
| `colors.h` | ANSI color macros. |
| `plugin.h` | **Public** plugin API (what plugin authors include). |
| `plugins.h` | Internal loader interface. |
| `plugins/` | `.so` plugins loaded at runtime + their sources. |
| `Makefile` | Build the binary and plugins. |

## Plugins

scour loads every `*.so` in the plugins directory at startup and calls their
hooks during a scan — so you can log, export, filter or reshape results without
changing the core. See [`plugins/README.md`](plugins/README.md) for the full
API, and [`plugins/jsonlog.c`](plugins/jsonlog.c) for a complete example that
writes each result to `scour.jsonl`.

Minimal plugin:

```c
#include "../plugin.h"

static int result(const scour_request *req, const scour_response *res) {
    /* inspect req->label/url/host/vhost and res->status/size */
    return 1;   /* 1 = keep scour's output; 0 = suppress it */
}

static const scour_plugin plugin = {
    .abi = SCOUR_PLUGIN_ABI,
    .name = "example",
    .version = "1.0",
    .on_result = result,
};

const scour_plugin *scour_plugin_register(void) { return &plugin; }
```

> **Security:** a plugin is native code that runs with scour's privileges. Only
> place `.so` files you trust in the plugins directory, and disable loading with
> `--no-plugins` when in doubt.

## Legal / authorized use

scour is for testing systems you own or are explicitly authorized to assess
(pentest engagements, CTFs, bug-bounty scope, your own lab). Sending traffic at
hosts without permission may be illegal. You are responsible for how you use it.

## Contributing

Contributions are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md). Plugins are
a great first contribution since they don't touch the core.

## License

Released under the [MIT License](LICENSE).
