#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scour.h"
#include "colors.h"

/* Read the wordlist into k->words, expanding each entry with every extension.
 * Blank lines and lines starting with '#' are skipped. Returns 0 on success. */
int load_words(scour_t *k) {
	FILE *f = fopen(k->wordlist, "r");
	if (!f) {
		fprintf(stderr, BOLD LRED "Could not open wordlist: %s" RESET "\n", k->wordlist);
		return -1;
	}

	size_t cap = 1024;
	k->words = malloc(cap * sizeof *k->words);
	if (!k->words) {
		fclose(f);
		return -1;
	}
	k->word_count = 0;

	char line[1024];
	while (fgets(line, sizeof line, f)) {
		line[strcspn(line, "\r\n")] = '\0';
		if (line[0] == '\0' || line[0] == '#')
			continue;

		/* the bare word (e == -1) plus one variant per extension */
		for (int e = -1; e < k->n_exts; e++) {
			char entry[1200];
			if (e < 0)
				snprintf(entry, sizeof entry, "%s", line);
			else
				snprintf(entry, sizeof entry, "%s.%s", line, k->exts[e]);

			if (k->word_count == cap) {
				cap *= 2;
				char **tmp = realloc(k->words, cap * sizeof *k->words);
				if (!tmp) {
					fclose(f);
					return -1;
				}
				k->words = tmp;
			}
			k->words[k->word_count] = strdup(entry);
			if (!k->words[k->word_count]) {
				fclose(f);
				return -1;
			}
			k->word_count++;
		}
	}

	fclose(f);
	return 0;
}
