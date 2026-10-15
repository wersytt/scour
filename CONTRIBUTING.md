# Contributing to Scour

Thanks for your interest in improving Scour! Contributions of all sizes are
welcome — bug fixes, new features, plugins, and documentation.

## Getting started

```sh
git clone <your-fork-url>
cd scour
make            # builds ./scour and the example plugins
```

Requirements: a C compiler and the `libcurl` development headers (see the
README for package names).

## Building and checking your change

- `make` must succeed with no new warnings. The project builds with
  `-Wall -Wextra`; please keep it warning-clean.
- Test against a local throwaway server (e.g. `python3 -m http.server`) rather
  than someone else's site.

## Code style

- C, indented with tabs, following the style of the surrounding code.
- Keep each file focused on its responsibility (see the "Project layout" table
  in the README). Prefer small, composable functions.
- Options live on the `scour_t` context struct in `scour.h`; thread them
  through rather than adding globals.

## Writing a plugin

Plugins live in `plugins/` and are loaded at runtime. See
[`plugins/README.md`](plugins/README.md) for the API and a worked example.
Plugins are a great first contribution because they don't touch the core.

## Pull requests

1. Create a branch for your change.
2. Keep the PR focused; describe what it does and how you tested it.
3. Make sure `make` is clean and the binary runs.

## Scope and responsible use

Scour is a security-testing tool meant for systems you own or are authorized to
assess. Please don't contribute features whose primary purpose is abuse (e.g.
denial-of-service, mass untargeted scanning, or evasion for malicious intent).

## License

By contributing, you agree that your contributions are licensed under the
project's [MIT License](LICENSE).
