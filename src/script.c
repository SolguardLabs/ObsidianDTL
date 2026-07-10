#define _CRT_SECURE_NO_WARNINGS
#include "obsidian.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim_left(char *text) {
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }
    return text;
}

static void trim_right(char *text) {
    size_t len = strlen(text);
    while (len > 0) {
        char ch = text[len - 1];
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
            break;
        }
        text[len - 1] = '\0';
        len--;
    }
}

static int split_tokens(char *line, char **tokens, size_t max_tokens) {
    size_t count = 0;
    char *cursor = line;
    while (*cursor != '\0') {
        cursor = trim_left(cursor);
        if (*cursor == '\0') {
            break;
        }
        if (count >= max_tokens) {
            return -1;
        }
        tokens[count++] = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != '\r' && *cursor != '\n') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }
        *cursor = '\0';
        cursor++;
    }
    return (int)count;
}

static int parse_u64(const char *text, uint64_t *out) {
    char *end = NULL;
    unsigned long long value;
    if (text == NULL || text[0] == '\0' || out == NULL) {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return 0;
    }
    *out = (uint64_t)value;
    return 1;
}

static int parse_kind(const char *text, ObsPacketKind *kind) {
    if (text == NULL || kind == NULL) {
        return 0;
    }
    if (obs_streq_ci(text, "release")) {
        *kind = OBS_PACKET_RELEASE;
        return 1;
    }
    if (obs_streq_ci(text, "withdraw")) {
        *kind = OBS_PACKET_WITHDRAW;
        return 1;
    }
    if (obs_streq_ci(text, "settle")) {
        *kind = OBS_PACKET_SETTLE;
        return 1;
    }
    return 0;
}

static int command_packet(ObsLedger *ledger, char **tokens, int count, ObsError *err) {
    ObsPacketSpec spec;
    uint64_t nonce;
    uint64_t amount;
    uint64_t max_fee;
    uint64_t ttl;
    ObsPacketKind kind;
    if (count != 11) {
        obs_error_set(err, OBS_ERR_PARSE, "PACKET requires 10 arguments");
        return 0;
    }
    if (!parse_u64(tokens[6], &nonce) ||
        !parse_u64(tokens[7], &amount) ||
        !parse_u64(tokens[8], &max_fee) ||
        !parse_u64(tokens[9], &ttl) ||
        !parse_kind(tokens[10], &kind)) {
        obs_error_set(err, OBS_ERR_PARSE, "PACKET contains invalid numeric field or kind");
        return 0;
    }
    memset(&spec, 0, sizeof(spec));
    spec.id = tokens[1];
    spec.logical_origin = tokens[2];
    spec.target_domain = tokens[3];
    spec.sender = tokens[4];
    spec.beneficiary = tokens[5];
    spec.nonce = nonce;
    spec.amount = amount;
    spec.max_fee = max_fee;
    spec.ttl = ttl;
    spec.kind = kind;
    return obs_submit_packet(ledger, &spec, err);
}

static int command_deliver(ObsLedger *ledger, char **tokens, int count, int tolerate_error, ObsError *err) {
    uint64_t delay;
    ObsError local;
    if (count != 4) {
        obs_error_set(err, OBS_ERR_PARSE, "DELIVER requires packet, route and delay");
        return 0;
    }
    if (!parse_u64(tokens[3], &delay)) {
        obs_error_set(err, OBS_ERR_PARSE, "DELIVER delay must be numeric");
        return 0;
    }
    obs_error_clear(&local);
    if (!obs_deliver_packet(ledger, tokens[1], tokens[2], delay, &local)) {
        if (tolerate_error) {
            return 1;
        }
        if (err != NULL) {
            *err = local;
        }
        return 0;
    }
    return 1;
}

static int command_route(ObsLedger *ledger, char **tokens, int count, ObsError *err) {
    uint64_t fee;
    uint64_t max_delay;
    uint64_t priority;
    uint64_t active;
    if (count != 10) {
        obs_error_set(err, OBS_ERR_PARSE, "ROUTE requires 9 arguments");
        return 0;
    }
    if (!parse_u64(tokens[6], &fee) ||
        !parse_u64(tokens[7], &max_delay) ||
        !parse_u64(tokens[8], &priority) ||
        !parse_u64(tokens[9], &active)) {
        obs_error_set(err, OBS_ERR_PARSE, "ROUTE contains invalid numeric field");
        return 0;
    }
    return obs_ledger_add_route(
        ledger,
        tokens[1],
        tokens[2],
        tokens[3],
        tokens[4],
        tokens[5],
        fee,
        max_delay,
        priority,
        active != 0,
        err);
}

static int command_quote(ObsLedger *ledger, char **tokens, int count, ObsError *err) {
    uint64_t amount;
    uint64_t max_fee;
    ObsRouteQuote quote;
    if (count != 5) {
        obs_error_set(err, OBS_ERR_PARSE, "QUOTE requires origin, target, amount and max_fee");
        return 0;
    }
    if (!parse_u64(tokens[3], &amount) || !parse_u64(tokens[4], &max_fee)) {
        obs_error_set(err, OBS_ERR_PARSE, "QUOTE contains invalid numeric field");
        return 0;
    }
    if (!obs_select_route(ledger, tokens[1], tokens[2], amount, max_fee, &quote, err)) {
        return 0;
    }
    obs_ledger_event(
        ledger,
        OBS_EVENT_ROUTE,
        "quote selected %s for %s to %s",
        quote.route_id,
        tokens[1],
        tokens[2]);
    return 1;
}

static int command_domain(ObsLedger *ledger, char **tokens, int count, ObsError *err) {
    uint64_t vault;
    if (count != 4) {
        obs_error_set(err, OBS_ERR_PARSE, "DOMAIN requires id, label and vault");
        return 0;
    }
    if (!parse_u64(tokens[3], &vault)) {
        obs_error_set(err, OBS_ERR_PARSE, "DOMAIN vault must be numeric");
        return 0;
    }
    return obs_ledger_add_domain(ledger, tokens[1], tokens[2], vault, err);
}

static int command_account(ObsLedger *ledger, char **tokens, int count, ObsError *err) {
    uint64_t balance;
    if (count != 4) {
        obs_error_set(err, OBS_ERR_PARSE, "ACCOUNT requires id, label and balance");
        return 0;
    }
    if (!parse_u64(tokens[3], &balance)) {
        obs_error_set(err, OBS_ERR_PARSE, "ACCOUNT balance must be numeric");
        return 0;
    }
    return obs_ledger_add_account(ledger, tokens[1], tokens[2], balance, err);
}

static int handle_command(ObsLedger *ledger, char **tokens, int count, ObsError *err) {
    uint64_t value;
    if (count == 0) {
        return 1;
    }
    if (obs_streq_ci(tokens[0], "EMPTY")) {
        char scenario[OBS_ID_LEN];
        obs_copy(scenario, sizeof(scenario), ledger->scenario);
        obs_ledger_init(ledger, scenario);
        return 1;
    }
    if (obs_streq_ci(tokens[0], "RESET")) {
        char scenario[OBS_ID_LEN];
        obs_copy(scenario, sizeof(scenario), ledger->scenario);
        obs_ledger_init(ledger, scenario);
        return obs_load_default_fixture(ledger, err);
    }
    if (obs_streq_ci(tokens[0], "DOMAIN")) {
        return command_domain(ledger, tokens, count, err);
    }
    if (obs_streq_ci(tokens[0], "ACCOUNT")) {
        return command_account(ledger, tokens, count, err);
    }
    if (obs_streq_ci(tokens[0], "ROUTE")) {
        return command_route(ledger, tokens, count, err);
    }
    if (obs_streq_ci(tokens[0], "QUOTE")) {
        return command_quote(ledger, tokens, count, err);
    }
    if (obs_streq_ci(tokens[0], "PACKET")) {
        return command_packet(ledger, tokens, count, err);
    }
    if (obs_streq_ci(tokens[0], "DELIVER")) {
        return command_deliver(ledger, tokens, count, 0, err);
    }
    if (obs_streq_ci(tokens[0], "TRY_DELIVER")) {
        return command_deliver(ledger, tokens, count, 1, err);
    }
    if (obs_streq_ci(tokens[0], "ADVANCE")) {
        if (count != 2 || !parse_u64(tokens[1], &value)) {
            obs_error_set(err, OBS_ERR_PARSE, "ADVANCE requires numeric delta");
            return 0;
        }
        obs_ledger_advance(ledger, value);
        return 1;
    }
    if (obs_streq_ci(tokens[0], "CLOCK")) {
        if (count != 2 || !parse_u64(tokens[1], &value)) {
            obs_error_set(err, OBS_ERR_PARSE, "CLOCK requires numeric value");
            return 0;
        }
        ledger->clock = value;
        obs_ledger_event(ledger, OBS_EVENT_BOOT, "clock set to %" PRIu64, value);
        return 1;
    }
    if (obs_streq_ci(tokens[0], "SCENARIO")) {
        if (count != 2) {
            obs_error_set(err, OBS_ERR_PARSE, "SCENARIO requires a name");
            return 0;
        }
        obs_copy(ledger->scenario, sizeof(ledger->scenario), tokens[1]);
        return 1;
    }
    obs_error_set(err, OBS_ERR_PARSE, "unknown command %s", tokens[0]);
    return 0;
}

int obs_run_script_stream(FILE *stream, const char *name, ObsLedger *ledger, ObsError *err) {
    char line[512];
    char *tokens[16];
    unsigned int line_no = 0;
    if (stream == NULL || ledger == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "script stream and ledger required");
        return 0;
    }
    obs_ledger_init(ledger, name == NULL ? "script" : name);
    if (!obs_load_default_fixture(ledger, err)) {
        return 0;
    }
    while (fgets(line, sizeof(line), stream) != NULL) {
        char *work;
        int count;
        line_no++;
        trim_right(line);
        work = trim_left(line);
        if (*work == '\0' || *work == '#') {
            continue;
        }
        count = split_tokens(work, tokens, sizeof(tokens) / sizeof(tokens[0]));
        if (count < 0) {
            obs_error_set(err, OBS_ERR_PARSE, "line %u has too many tokens", line_no);
            return 0;
        }
        if (!handle_command(ledger, tokens, count, err)) {
            char detail[OBS_ERROR_LEN];
            obs_copy(detail, sizeof(detail), err != NULL ? err->message : "script error");
            obs_error_set(err, err != NULL ? err->code : OBS_ERR_PARSE, "line %u: %s", line_no, detail);
            return 0;
        }
    }
    return 1;
}

int obs_run_script_file(const char *path, ObsLedger *ledger, ObsError *err) {
    FILE *file;
    int ok;
    if (path == NULL || path[0] == '\0') {
        obs_error_set(err, OBS_ERR_ARGUMENT, "script path required");
        return 0;
    }
    file = fopen(path, "r");
    if (file == NULL) {
        obs_error_set(err, OBS_ERR_IO, "unable to open script %s", path);
        return 0;
    }
    ok = obs_run_script_stream(file, path, ledger, err);
    fclose(file);
    return ok;
}
