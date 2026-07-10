#include "obsidian.h"

#include <stdio.h>
#include <string.h>

static void print_usage(FILE *out) {
    fputs("ObsidianDTL local runner\n", out);
    fputs("\n", out);
    fputs("Usage:\n", out);
    fputs("  obsidian-dtl [scenario]\n", out);
    fputs("  obsidian-dtl scenario <name>\n", out);
    fputs("  obsidian-dtl script <path>\n", out);
    fputs("\n", out);
    fputs("Scenarios:\n", out);
    fputs("  snapshot   default fixture without deliveries\n", out);
    fputs("  direct     accepted delivery on primary route\n", out);
    fputs("  expiry     expired packet rejection\n", out);
    fputs("  duplicate  duplicate delivery rejection on one route\n", out);
    fputs("  late       accepted delivery with late confirmation flag\n", out);
    fputs("  fallback   accepted delivery through fallback route\n", out);
    fputs("  routing    automatic route selection with two packets\n", out);
}

int main(int argc, char **argv) {
    ObsLedger ledger;
    ObsError err;
    const char *scenario;
    obs_error_clear(&err);
    if (argc > 1 && (obs_streq(argv[1], "--help") || obs_streq(argv[1], "-h") || obs_streq(argv[1], "help"))) {
        print_usage(stdout);
        return 0;
    }
    if (argc > 1 && obs_streq(argv[1], "--version")) {
        fputs("obsidian-dtl 0.1.0\n", stdout);
        return 0;
    }
    if (argc > 1 && obs_streq(argv[1], "script")) {
        if (argc < 3) {
            fputs("script path required\n", stderr);
            return 2;
        }
        if (!obs_run_script_file(argv[2], &ledger, &err)) {
            fprintf(stderr, "script failed: %s\n", err.message);
            return (int)err.code == 0 ? 1 : (int)err.code;
        }
        obs_print_report(&ledger, stdout);
        return 0;
    }
    if (argc > 1 && obs_streq(argv[1], "scenario")) {
        if (argc < 3) {
            fputs("scenario name required\n", stderr);
            return 2;
        }
        scenario = argv[2];
    } else {
        scenario = argc > 1 ? argv[1] : "direct";
    }
    if (!obs_run_scenario(scenario, &ledger, &err)) {
        fprintf(stderr, "scenario failed: %s\n", err.message);
        return (int)err.code == 0 ? 1 : (int)err.code;
    }
    obs_print_report(&ledger, stdout);
    return 0;
}
