#define _CRT_SECURE_NO_WARNINGS
#include "obsidian.h"

#include <stdio.h>
#include <string.h>

static ObsPacketSpec make_packet(const char *id, uint64_t nonce, ObsAmount amount, ObsAmount max_fee, uint64_t ttl) {
    ObsPacketSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.id = id;
    spec.logical_origin = "edge";
    spec.target_domain = "core";
    spec.sender = "alice";
    spec.beneficiary = "merchant";
    spec.nonce = nonce;
    spec.amount = amount;
    spec.max_fee = max_fee;
    spec.ttl = ttl;
    spec.kind = OBS_PACKET_RELEASE;
    return spec;
}

int obs_load_default_fixture(ObsLedger *ledger, ObsError *err) {
    if (ledger == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "ledger required");
        return 0;
    }
    if (!obs_ledger_add_domain(ledger, "edge", "Edge Retail Domain", 7500000000ULL, err) ||
        !obs_ledger_add_domain(ledger, "core", "Core Treasury Domain", 9000000000ULL, err) ||
        !obs_ledger_add_domain(ledger, "treasury", "Treasury Settlement Domain", 4000000000ULL, err)) {
        return 0;
    }
    if (!obs_ledger_add_account(ledger, "alice", "Origin Operator", 750000000ULL, err) ||
        !obs_ledger_add_account(ledger, "merchant", "Merchant Beneficiary", 100000000ULL, err) ||
        !obs_ledger_add_account(ledger, "relay-edge", "Primary Relay", 0ULL, err) ||
        !obs_ledger_add_account(ledger, "relay-core", "Fallback Relay", 0ULL, err) ||
        !obs_ledger_add_account(ledger, "ops", "Operations Desk", 250000000ULL, err) ||
        !obs_ledger_add_account(ledger, "auditor", "Read Only Auditor", 0ULL, err)) {
        return 0;
    }
    if (!obs_ledger_add_route(ledger, "edge-direct", "edge", "edge", "core", "relay-edge", 20000ULL, 20ULL, 10ULL, 1, err) ||
        !obs_ledger_add_route(ledger, "edge-fallback", "edge", "core", "core", "relay-core", 25000ULL, 40ULL, 20ULL, 1, err) ||
        !obs_ledger_add_route(ledger, "treasury-direct", "treasury", "treasury", "core", "relay-core", 30000ULL, 20ULL, 5ULL, 1, err)) {
        return 0;
    }
    obs_ledger_event(ledger, OBS_EVENT_BOOT, "default fixture loaded");
    return 1;
}

static int run_snapshot(ObsLedger *ledger, ObsError *err) {
    (void)err;
    obs_ledger_event(ledger, OBS_EVENT_BOOT, "snapshot generated");
    return 1;
}

static int run_direct(ObsLedger *ledger, ObsError *err) {
    ObsPacketSpec spec = make_packet("pkt-direct-001", 11ULL, 1000000000ULL, 50000ULL, 30ULL);
    if (!obs_submit_packet(ledger, &spec, err)) {
        return 0;
    }
    return obs_deliver_packet(ledger, "pkt-direct-001", "edge-direct", 2ULL, err);
}

static int run_expiry(ObsLedger *ledger, ObsError *err) {
    ObsPacketSpec spec = make_packet("pkt-expiry-001", 12ULL, 900000000ULL, 50000ULL, 2ULL);
    ObsError ignored;
    if (!obs_submit_packet(ledger, &spec, err)) {
        return 0;
    }
    obs_ledger_advance(ledger, 3ULL);
    obs_error_clear(&ignored);
    (void)obs_deliver_packet(ledger, "pkt-expiry-001", "edge-direct", 1ULL, &ignored);
    return 1;
}

static int run_duplicate(ObsLedger *ledger, ObsError *err) {
    ObsPacketSpec spec = make_packet("pkt-dup-001", 13ULL, 750000000ULL, 50000ULL, 30ULL);
    ObsError ignored;
    if (!obs_submit_packet(ledger, &spec, err)) {
        return 0;
    }
    if (!obs_deliver_packet(ledger, "pkt-dup-001", "edge-direct", 1ULL, err)) {
        return 0;
    }
    obs_error_clear(&ignored);
    (void)obs_deliver_packet(ledger, "pkt-dup-001", "edge-direct", 1ULL, &ignored);
    return 1;
}

static int run_late(ObsLedger *ledger, ObsError *err) {
    ObsPacketSpec spec = make_packet("pkt-late-001", 14ULL, 625000000ULL, 50000ULL, 8ULL);
    if (!obs_submit_packet(ledger, &spec, err)) {
        return 0;
    }
    return obs_deliver_packet(ledger, "pkt-late-001", "edge-direct", 25ULL, err);
}

static int run_fallback(ObsLedger *ledger, ObsError *err) {
    ObsPacketSpec spec = make_packet("pkt-fallback-001", 15ULL, 1500000000ULL, 60000ULL, 30ULL);
    if (!obs_submit_packet(ledger, &spec, err)) {
        return 0;
    }
    return obs_deliver_packet(ledger, "pkt-fallback-001", "edge-fallback", 4ULL, err);
}

static int run_routing(ObsLedger *ledger, ObsError *err) {
    ObsPacketSpec first = make_packet("pkt-routing-001", 16ULL, 250000000ULL, 50000ULL, 30ULL);
    ObsPacketSpec second = make_packet("pkt-routing-002", 17ULL, 300000000ULL, 22000ULL, 30ULL);
    ObsRouteQuote selected;
    if (!obs_submit_packet(ledger, &first, err)) {
        return 0;
    }
    if (!obs_select_route(ledger, "edge", "core", first.amount, first.max_fee, &selected, err)) {
        return 0;
    }
    if (!obs_deliver_packet(ledger, first.id, selected.route_id, 1ULL, err)) {
        return 0;
    }
    if (!obs_submit_packet(ledger, &second, err)) {
        return 0;
    }
    if (!obs_select_route(ledger, "edge", "core", second.amount, second.max_fee, &selected, err)) {
        return 0;
    }
    return obs_deliver_packet(ledger, second.id, selected.route_id, 2ULL, err);
}

int obs_run_scenario(const char *name, ObsLedger *ledger, ObsError *err) {
    const char *scenario = name == NULL || name[0] == '\0' ? "direct" : name;
    obs_ledger_init(ledger, scenario);
    if (!obs_load_default_fixture(ledger, err)) {
        return 0;
    }
    if (obs_streq_ci(scenario, "snapshot")) {
        return run_snapshot(ledger, err);
    }
    if (obs_streq_ci(scenario, "direct")) {
        return run_direct(ledger, err);
    }
    if (obs_streq_ci(scenario, "expiry")) {
        return run_expiry(ledger, err);
    }
    if (obs_streq_ci(scenario, "duplicate")) {
        return run_duplicate(ledger, err);
    }
    if (obs_streq_ci(scenario, "late")) {
        return run_late(ledger, err);
    }
    if (obs_streq_ci(scenario, "fallback")) {
        return run_fallback(ledger, err);
    }
    if (obs_streq_ci(scenario, "routing")) {
        return run_routing(ledger, err);
    }
    obs_error_set(err, OBS_ERR_ARGUMENT, "unknown scenario %s", scenario);
    return 0;
}
