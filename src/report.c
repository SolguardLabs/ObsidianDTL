#define _CRT_SECURE_NO_WARNINGS
#include "obsidian.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static void json_string(FILE *out, const char *text) {
    const unsigned char *ptr = (const unsigned char *)(text == NULL ? "" : text);
    fputc('"', out);
    while (*ptr != '\0') {
        unsigned char ch = *ptr++;
        if (ch == '"' || ch == '\\') {
            fputc('\\', out);
            fputc((int)ch, out);
        } else if (ch == '\n') {
            fputs("\\n", out);
        } else if (ch == '\r') {
            fputs("\\r", out);
        } else if (ch == '\t') {
            fputs("\\t", out);
        } else if (ch < 32) {
            fprintf(out, "\\u%04x", ch);
        } else {
            fputc((int)ch, out);
        }
    }
    fputc('"', out);
}

static const char *last_rejection(const ObsLedger *ledger) {
    size_t i = ledger == NULL ? 0 : ledger->packet_count;
    while (i > 0) {
        i--;
        if (ledger->packets[i].rejected) {
            return ledger->packets[i].rejection;
        }
    }
    return "";
}

static void print_domain_balances(const ObsLedger *ledger, FILE *out) {
    size_t i;
    fputs("      \"domains\": {", out);
    for (i = 0; i < ledger->domain_count; i++) {
        if (i > 0) {
            fputs(", ", out);
        }
        json_string(out, ledger->domains[i].id);
        fprintf(out, ": %" PRIu64, ledger->domains[i].vault);
    }
    fputs("},\n", out);
}

static void print_account_balances(const ObsLedger *ledger, FILE *out) {
    size_t i;
    fputs("      \"accounts\": {", out);
    for (i = 0; i < ledger->account_count; i++) {
        if (i > 0) {
            fputs(", ", out);
        }
        json_string(out, ledger->accounts[i].id);
        fprintf(out, ": %" PRIu64, ledger->accounts[i].balance);
    }
    fputs("}\n", out);
}

static void print_domain_stats(const ObsLedger *ledger, FILE *out) {
    size_t i;
    fputs("  \"domain_stats\": {\n", out);
    for (i = 0; i < ledger->domain_count; i++) {
        const ObsDomain *domain = &ledger->domains[i];
        fprintf(out, "    ");
        json_string(out, domain->id);
        fputs(": {", out);
        fprintf(out, "\"pending_outbound\": %" PRIu64, domain->pending_outbound);
        fprintf(out, ", \"fees\": %" PRIu64, domain->delivery_fees);
        fprintf(out, ", \"available_liquidity\": %" PRIu64, obs_domain_available(domain));
        fprintf(out, ", \"delivered\": %" PRIu64, domain->delivered_packets);
        fprintf(out, ", \"rejected\": %" PRIu64, domain->rejected_packets);
        fprintf(out, ", \"nonce_floor\": %" PRIu64, domain->nonce_window.floor);
        fprintf(out, ", \"nonces_used\": %u", (unsigned int)obs_nonce_used_count(&domain->nonce_window));
        fputs(", \"nonce_digest\": ", out);
        {
            char hex[19];
            obs_hex64(obs_nonce_digest(&domain->nonce_window), hex);
            json_string(out, hex);
        }
        fputs("}", out);
        if (i + 1 < ledger->domain_count) {
            fputs(",", out);
        }
        fputc('\n', out);
    }
    fputs("  },\n", out);
}

static void print_route_stats(const ObsLedger *ledger, FILE *out) {
    size_t i;
    fputs("  \"routes\": {\n", out);
    for (i = 0; i < ledger->route_count; i++) {
        const ObsRoute *route = &ledger->routes[i];
        fprintf(out, "    ");
        json_string(out, route->id);
        fprintf(out,
                ": {\"logical_origin\": ");
        json_string(out, route->logical_origin);
        fprintf(out,
                ", \"transport_origin\": ");
        json_string(out, route->transport_origin);
        fprintf(out,
                ", \"target\": ");
        json_string(out, route->target_domain);
        fprintf(out,
                ", \"deliveries\": %" PRIu64 ", \"failures\": %" PRIu64 ", \"active\": %s}",
                route->deliveries,
                route->failures,
                route->active ? "true" : "false");
        if (i + 1 < ledger->route_count) {
            fputs(",", out);
        }
        fputc('\n', out);
    }
    fputs("  },\n", out);
}

static void print_quote_json(const ObsRouteQuote *quote, FILE *out) {
    fputs("{\"accepted\": ", out);
    fputs(quote->accepted ? "true" : "false", out);
    fputs(", \"logical_origin\": ", out);
    json_string(out, quote->logical_origin);
    fputs(", \"transport_origin\": ", out);
    json_string(out, quote->transport_origin);
    fputs(", \"target\": ", out);
    json_string(out, quote->target_domain);
    fprintf(out, ", \"amount\": %" PRIu64, quote->amount);
    fprintf(out, ", \"fee\": %" PRIu64, quote->fee);
    fprintf(out, ", \"net_amount\": %" PRIu64, quote->net_amount);
    fprintf(out, ", \"available_liquidity\": %" PRIu64, quote->available_liquidity);
    fprintf(out, ", \"max_delay\": %" PRIu64, quote->max_delay);
    fprintf(out, ", \"priority\": %" PRIu64, quote->priority);
    fputs(", \"reason\": ", out);
    json_string(out, quote->reason);
    fputs("}", out);
}

static void print_planner(const ObsLedger *ledger, FILE *out) {
    size_t i;
    ObsHealthReport health;
    ObsError ignored;
    char digest[19];
    obs_error_clear(&ignored);
    if (!obs_health_report(ledger, &health, &ignored)) {
        memset(&health, 0, sizeof(health));
    }
    obs_hex64(health.route_digest, digest);
    fputs("  \"planner\": {\n", out);
    fputs("    \"route_digest\": ", out);
    json_string(out, digest);
    fputs(",\n    \"health\": {", out);
    fprintf(out, "\"active_routes\": %u", (unsigned int)health.active_routes);
    fprintf(out, ", \"inactive_routes\": %u", (unsigned int)health.inactive_routes);
    fprintf(out, ", \"routes_with_failures\": %u", (unsigned int)health.routes_with_failures);
    fprintf(out, ", \"domains_low_liquidity\": %u", (unsigned int)health.domains_low_liquidity);
    fprintf(out, ", \"total_vault\": %" PRIu64, health.total_vault);
    fprintf(out, ", \"total_available\": %" PRIu64, health.total_available);
    fprintf(out, ", \"total_pending\": %" PRIu64, health.total_pending);
    fputs("},\n    \"quotes\": {\n", out);
    for (i = 0; i < ledger->route_count; i++) {
        ObsRouteQuote quote;
        obs_error_clear(&ignored);
        (void)obs_quote_route(ledger, ledger->routes[i].id, 100000000ULL, 100000ULL, &quote, &ignored);
        fputs("      ", out);
        json_string(out, ledger->routes[i].id);
        fputs(": ", out);
        print_quote_json(&quote, out);
        if (i + 1 < ledger->route_count) {
            fputs(",", out);
        }
        fputc('\n', out);
    }
    fputs("    }\n  },\n", out);
}

static void print_packets(const ObsLedger *ledger, FILE *out) {
    size_t i;
    fputs("  \"packet_log\": [\n", out);
    for (i = 0; i < ledger->packet_count; i++) {
        const ObsPacket *packet = &ledger->packets[i];
        char digest[19];
        obs_hex64(packet->digest, digest);
        fputs("    {\"id\": ", out);
        json_string(out, packet->id);
        fprintf(out, ", \"logical_origin\": ");
        json_string(out, packet->logical_origin);
        fprintf(out, ", \"nonce\": %" PRIu64 ", \"amount\": %" PRIu64 ", \"kind\": ", packet->nonce, packet->amount);
        json_string(out, obs_kind_name(packet->kind));
        fprintf(out, ", \"settled\": %s, \"rejected\": %s, \"digest\": ",
                packet->settled ? "true" : "false",
                packet->rejected ? "true" : "false");
        json_string(out, digest);
        fputs("}", out);
        if (i + 1 < ledger->packet_count) {
            fputs(",", out);
        }
        fputc('\n', out);
    }
    fputs("  ],\n", out);
}

static void print_receipts(const ObsLedger *ledger, FILE *out) {
    size_t i;
    fputs("  \"receipt_log\": [\n", out);
    for (i = 0; i < ledger->receipt_count; i++) {
        const ObsReceipt *receipt = &ledger->receipts[i];
        char digest[19];
        obs_hex64(receipt->receipt_digest, digest);
        fputs("    {\"id\": ", out);
        json_string(out, receipt->id);
        fputs(", \"packet_id\": ", out);
        json_string(out, receipt->packet_id);
        fputs(", \"route_id\": ", out);
        json_string(out, receipt->route_id);
        fputs(", \"transport_origin\": ", out);
        json_string(out, receipt->transport_origin);
        fprintf(out, ", \"confirmed_at\": %" PRIu64 ", \"late\": %s, \"accepted\": %s, \"digest\": ",
                receipt->confirmed_at,
                receipt->late ? "true" : "false",
                receipt->accepted ? "true" : "false");
        json_string(out, digest);
        fputs("}", out);
        if (i + 1 < ledger->receipt_count) {
            fputs(",", out);
        }
        fputc('\n', out);
    }
    fputs("  ],\n", out);
}

void obs_print_report(const ObsLedger *ledger, FILE *out) {
    char state_digest[19];
    if (ledger == NULL || out == NULL) {
        return;
    }
    obs_hex64(obs_ledger_digest(ledger), state_digest);
    fputs("{\n", out);
    fputs("  \"scenario\": ", out);
    json_string(out, ledger->scenario);
    fprintf(out, ",\n  \"clock\": %" PRIu64 ",\n", ledger->clock);
    fprintf(out,
            "  \"surface\": {\"domains\": %u, \"accounts\": %u, \"routes\": %u},\n",
            (unsigned int)ledger->domain_count,
            (unsigned int)ledger->account_count,
            (unsigned int)ledger->route_count);
    fprintf(out,
            "  \"packets\": {\"submitted\": %" PRIu64 ", \"settled\": %" PRIu64 ", \"rejected\": %" PRIu64 "},\n",
            ledger->packets_submitted,
            ledger->packets_settled,
            ledger->packets_rejected);
    fprintf(out,
            "  \"receipts\": {\"total\": %u, \"late\": %" PRIu64 ", \"rejected\": %" PRIu64 "},\n",
            (unsigned int)ledger->receipt_count,
            ledger->receipts_late,
            ledger->receipts_rejected);
    fputs("  \"balances\": {\n", out);
    print_domain_balances(ledger, out);
    print_account_balances(ledger, out);
    fputs("  },\n", out);
    print_domain_stats(ledger, out);
    print_route_stats(ledger, out);
    print_planner(ledger, out);
    print_packets(ledger, out);
    print_receipts(ledger, out);
    fputs("  \"last_rejection\": ", out);
    json_string(out, last_rejection(ledger));
    fprintf(out, ",\n  \"events\": %u", (unsigned int)ledger->event_count);
    fprintf(out, ",\n  \"total_supply\": %" PRIu64, obs_ledger_total_supply(ledger));
    fprintf(out, ",\n  \"initial_supply\": %" PRIu64, ledger->initial_supply);
    fprintf(out, ",\n  \"conservation_ok\": %s", obs_ledger_conservation_ok(ledger) ? "true" : "false");
    fputs(",\n  \"state_digest\": ", out);
    json_string(out, state_digest);
    fputs("\n}\n", out);
}
