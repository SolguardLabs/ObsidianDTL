#define _CRT_SECURE_NO_WARNINGS
#include "obsidian.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void reset_packet(ObsPacket *packet) {
    if (packet != NULL) {
        memset(packet, 0, sizeof(*packet));
    }
}

static void reset_receipt(ObsReceipt *receipt) {
    if (receipt != NULL) {
        memset(receipt, 0, sizeof(*receipt));
    }
}

void obs_ledger_init(ObsLedger *ledger, const char *scenario) {
    if (ledger == NULL) {
        return;
    }
    memset(ledger, 0, sizeof(*ledger));
    ledger->clock = 1;
    obs_copy(ledger->scenario, sizeof(ledger->scenario), scenario == NULL ? "custom" : scenario);
}

ObsDomain *obs_ledger_domain(ObsLedger *ledger, const char *id) {
    size_t i;
    if (ledger == NULL || id == NULL) {
        return NULL;
    }
    for (i = 0; i < ledger->domain_count; i++) {
        if (obs_streq(ledger->domains[i].id, id)) {
            return &ledger->domains[i];
        }
    }
    return NULL;
}

const ObsDomain *obs_ledger_domain_const(const ObsLedger *ledger, const char *id) {
    size_t i;
    if (ledger == NULL || id == NULL) {
        return NULL;
    }
    for (i = 0; i < ledger->domain_count; i++) {
        if (obs_streq(ledger->domains[i].id, id)) {
            return &ledger->domains[i];
        }
    }
    return NULL;
}

ObsAccount *obs_ledger_account(ObsLedger *ledger, const char *id) {
    size_t i;
    if (ledger == NULL || id == NULL) {
        return NULL;
    }
    for (i = 0; i < ledger->account_count; i++) {
        if (obs_streq(ledger->accounts[i].id, id)) {
            return &ledger->accounts[i];
        }
    }
    return NULL;
}

const ObsAccount *obs_ledger_account_const(const ObsLedger *ledger, const char *id) {
    size_t i;
    if (ledger == NULL || id == NULL) {
        return NULL;
    }
    for (i = 0; i < ledger->account_count; i++) {
        if (obs_streq(ledger->accounts[i].id, id)) {
            return &ledger->accounts[i];
        }
    }
    return NULL;
}

ObsRoute *obs_ledger_route(ObsLedger *ledger, const char *id) {
    size_t i;
    if (ledger == NULL || id == NULL) {
        return NULL;
    }
    for (i = 0; i < ledger->route_count; i++) {
        if (obs_streq(ledger->routes[i].id, id)) {
            return &ledger->routes[i];
        }
    }
    return NULL;
}

const ObsRoute *obs_ledger_route_const(const ObsLedger *ledger, const char *id) {
    size_t i;
    if (ledger == NULL || id == NULL) {
        return NULL;
    }
    for (i = 0; i < ledger->route_count; i++) {
        if (obs_streq(ledger->routes[i].id, id)) {
            return &ledger->routes[i];
        }
    }
    return NULL;
}

ObsPacket *obs_ledger_packet(ObsLedger *ledger, const char *id) {
    size_t i;
    if (ledger == NULL || id == NULL) {
        return NULL;
    }
    for (i = 0; i < ledger->packet_count; i++) {
        if (obs_streq(ledger->packets[i].id, id)) {
            return &ledger->packets[i];
        }
    }
    return NULL;
}

const ObsPacket *obs_ledger_packet_const(const ObsLedger *ledger, const char *id) {
    size_t i;
    if (ledger == NULL || id == NULL) {
        return NULL;
    }
    for (i = 0; i < ledger->packet_count; i++) {
        if (obs_streq(ledger->packets[i].id, id)) {
            return &ledger->packets[i];
        }
    }
    return NULL;
}

void obs_ledger_event(ObsLedger *ledger, ObsEventKind kind, const char *fmt, ...) {
    va_list args;
    ObsEvent *event;
    if (ledger == NULL || fmt == NULL || ledger->event_count >= OBS_MAX_EVENTS) {
        return;
    }
    event = &ledger->events[ledger->event_count++];
    memset(event, 0, sizeof(*event));
    event->kind = kind;
    event->at = ledger->clock;
    va_start(args, fmt);
    vsnprintf(event->text, sizeof(event->text), fmt, args);
    va_end(args);
}

void obs_ledger_advance(ObsLedger *ledger, uint64_t delta) {
    if (ledger == NULL) {
        return;
    }
    ledger->clock += delta;
    obs_ledger_event(ledger, OBS_EVENT_BOOT, "clock advanced by %" PRIu64, delta);
}

int obs_ledger_add_domain(ObsLedger *ledger, const char *id, const char *label, ObsAmount vault, ObsError *err) {
    ObsDomain *domain;
    ObsAmount total;
    if (ledger == NULL || id == NULL || id[0] == '\0') {
        obs_error_set(err, OBS_ERR_ARGUMENT, "domain id required");
        return 0;
    }
    if (obs_ledger_domain(ledger, id) != NULL) {
        obs_error_set(err, OBS_ERR_POLICY, "domain %s already registered", id);
        return 0;
    }
    if (ledger->domain_count >= OBS_MAX_DOMAINS) {
        obs_error_set(err, OBS_ERR_LIMIT, "domain capacity reached");
        return 0;
    }
    domain = &ledger->domains[ledger->domain_count++];
    memset(domain, 0, sizeof(*domain));
    obs_copy(domain->id, sizeof(domain->id), id);
    obs_copy(domain->label, sizeof(domain->label), label == NULL ? id : label);
    domain->vault = vault;
    obs_nonce_init(&domain->nonce_window, id, 1, 1024);
    if (!obs_amount_add(ledger->initial_supply, vault, &total, err)) {
        return 0;
    }
    ledger->initial_supply = total;
    obs_ledger_event(ledger, OBS_EVENT_BOOT, "domain %s funded with %" PRIu64, id, vault);
    return 1;
}

int obs_ledger_add_account(ObsLedger *ledger, const char *id, const char *label, ObsAmount balance, ObsError *err) {
    ObsAccount *account;
    ObsAmount total;
    if (ledger == NULL || id == NULL || id[0] == '\0') {
        obs_error_set(err, OBS_ERR_ARGUMENT, "account id required");
        return 0;
    }
    if (obs_ledger_account(ledger, id) != NULL) {
        obs_error_set(err, OBS_ERR_POLICY, "account %s already registered", id);
        return 0;
    }
    if (ledger->account_count >= OBS_MAX_ACCOUNTS) {
        obs_error_set(err, OBS_ERR_LIMIT, "account capacity reached");
        return 0;
    }
    account = &ledger->accounts[ledger->account_count++];
    memset(account, 0, sizeof(*account));
    obs_copy(account->id, sizeof(account->id), id);
    obs_copy(account->label, sizeof(account->label), label == NULL ? id : label);
    account->balance = balance;
    if (!obs_amount_add(ledger->initial_supply, balance, &total, err)) {
        return 0;
    }
    ledger->initial_supply = total;
    obs_ledger_event(ledger, OBS_EVENT_BOOT, "account %s opened with %" PRIu64, id, balance);
    return 1;
}

int obs_ledger_add_route(
    ObsLedger *ledger,
    const char *id,
    const char *logical_origin,
    const char *transport_origin,
    const char *target_domain,
    const char *relayer_account,
    ObsAmount relay_fee,
    uint64_t max_delay,
    uint64_t priority,
    int active,
    ObsError *err) {
    ObsRoute *route;
    if (ledger == NULL ||
        id == NULL ||
        logical_origin == NULL ||
        transport_origin == NULL ||
        target_domain == NULL ||
        relayer_account == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "route fields required");
        return 0;
    }
    if (obs_ledger_route(ledger, id) != NULL) {
        obs_error_set(err, OBS_ERR_POLICY, "route %s already registered", id);
        return 0;
    }
    if (obs_ledger_domain(ledger, logical_origin) == NULL) {
        obs_error_set(err, OBS_ERR_NOT_FOUND, "route logical domain %s not registered", logical_origin);
        return 0;
    }
    if (obs_ledger_domain(ledger, transport_origin) == NULL) {
        obs_error_set(err, OBS_ERR_NOT_FOUND, "route transport domain %s not registered", transport_origin);
        return 0;
    }
    if (obs_ledger_domain(ledger, target_domain) == NULL) {
        obs_error_set(err, OBS_ERR_NOT_FOUND, "route target domain %s not registered", target_domain);
        return 0;
    }
    if (obs_ledger_account(ledger, relayer_account) == NULL) {
        obs_error_set(err, OBS_ERR_NOT_FOUND, "relayer account %s not registered", relayer_account);
        return 0;
    }
    if (ledger->route_count >= OBS_MAX_ROUTES) {
        obs_error_set(err, OBS_ERR_LIMIT, "route capacity reached");
        return 0;
    }
    route = &ledger->routes[ledger->route_count++];
    memset(route, 0, sizeof(*route));
    obs_copy(route->id, sizeof(route->id), id);
    obs_copy(route->logical_origin, sizeof(route->logical_origin), logical_origin);
    obs_copy(route->transport_origin, sizeof(route->transport_origin), transport_origin);
    obs_copy(route->target_domain, sizeof(route->target_domain), target_domain);
    obs_copy(route->relayer_account, sizeof(route->relayer_account), relayer_account);
    route->relay_fee = relay_fee;
    route->max_delay = max_delay;
    route->priority = priority;
    route->active = active ? 1 : 0;
    obs_ledger_event(ledger, OBS_EVENT_ROUTE, "route %s %s via %s to %s", id, logical_origin, transport_origin, target_domain);
    return 1;
}

ObsAmount obs_ledger_total_supply(const ObsLedger *ledger) {
    size_t i;
    ObsAmount total = 0;
    if (ledger == NULL) {
        return 0;
    }
    for (i = 0; i < ledger->domain_count; i++) {
        total += ledger->domains[i].vault;
    }
    for (i = 0; i < ledger->account_count; i++) {
        total += ledger->accounts[i].balance;
    }
    return total;
}

int obs_ledger_conservation_ok(const ObsLedger *ledger) {
    if (ledger == NULL) {
        return 0;
    }
    return obs_ledger_total_supply(ledger) == ledger->initial_supply;
}

uint64_t obs_ledger_digest(const ObsLedger *ledger) {
    size_t i;
    uint64_t h;
    if (ledger == NULL) {
        return 0;
    }
    h = obs_hash_seed();
    h = obs_hash_text(h, ledger->scenario);
    h = obs_hash_u64(h, ledger->clock);
    h = obs_hash_u64(h, ledger->initial_supply);
    h = obs_hash_u64(h, ledger->packets_submitted);
    h = obs_hash_u64(h, ledger->packets_settled);
    h = obs_hash_u64(h, ledger->packets_rejected);
    h = obs_hash_u64(h, ledger->receipts_late);
    for (i = 0; i < ledger->domain_count; i++) {
        h = obs_hash_text(h, ledger->domains[i].id);
        h = obs_hash_u64(h, ledger->domains[i].vault);
        h = obs_hash_u64(h, ledger->domains[i].pending_outbound);
        h = obs_hash_u64(h, ledger->domains[i].delivery_fees);
        h = obs_hash_u64(h, ledger->domains[i].delivered_packets);
        h = obs_hash_u64(h, ledger->domains[i].rejected_packets);
        h = obs_hash_u64(h, obs_nonce_digest(&ledger->domains[i].nonce_window));
    }
    for (i = 0; i < ledger->account_count; i++) {
        h = obs_hash_text(h, ledger->accounts[i].id);
        h = obs_hash_u64(h, ledger->accounts[i].balance);
        h = obs_hash_u64(h, ledger->accounts[i].credits);
        h = obs_hash_u64(h, ledger->accounts[i].debits);
    }
    for (i = 0; i < ledger->route_count; i++) {
        h = obs_hash_text(h, ledger->routes[i].id);
        h = obs_hash_u64(h, ledger->routes[i].deliveries);
        h = obs_hash_u64(h, ledger->routes[i].failures);
    }
    for (i = 0; i < ledger->receipt_count; i++) {
        h = obs_hash_u64(h, ledger->receipts[i].receipt_digest);
        h = obs_hash_u64(h, (uint64_t)ledger->receipts[i].late);
        h = obs_hash_u64(h, (uint64_t)ledger->receipts[i].accepted);
    }
    return h;
}

int obs_domain_debit(ObsDomain *domain, ObsAmount amount, ObsError *err) {
    ObsAmount next;
    if (domain == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "domain debit target missing");
        return 0;
    }
    if (!obs_amount_sub(domain->vault, amount, &next, err)) {
        obs_error_set(err, OBS_ERR_BALANCE, "domain %s has insufficient vault balance", domain->id);
        return 0;
    }
    domain->vault = next;
    return 1;
}

int obs_account_credit(ObsAccount *account, ObsAmount amount, ObsError *err) {
    ObsAmount next;
    if (account == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "credit account missing");
        return 0;
    }
    if (!obs_amount_add(account->balance, amount, &next, err)) {
        return 0;
    }
    account->balance = next;
    account->credits++;
    return 1;
}

int obs_account_debit(ObsAccount *account, ObsAmount amount, ObsError *err) {
    ObsAmount next;
    if (account == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "debit account missing");
        return 0;
    }
    if (!obs_amount_sub(account->balance, amount, &next, err)) {
        obs_error_set(err, OBS_ERR_BALANCE, "account %s has insufficient balance", account->id);
        return 0;
    }
    account->balance = next;
    account->debits++;
    return 1;
}

int obs_allocate_packet(ObsLedger *ledger, ObsPacket **packet, ObsError *err) {
    if (ledger == NULL || packet == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "packet allocation target missing");
        return 0;
    }
    if (ledger->packet_count >= OBS_MAX_PACKETS) {
        obs_error_set(err, OBS_ERR_LIMIT, "packet capacity reached");
        return 0;
    }
    *packet = &ledger->packets[ledger->packet_count++];
    reset_packet(*packet);
    return 1;
}

int obs_allocate_receipt(ObsLedger *ledger, ObsReceipt **receipt, ObsError *err) {
    if (ledger == NULL || receipt == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "receipt allocation target missing");
        return 0;
    }
    if (ledger->receipt_count >= OBS_MAX_RECEIPTS) {
        obs_error_set(err, OBS_ERR_LIMIT, "receipt capacity reached");
        return 0;
    }
    *receipt = &ledger->receipts[ledger->receipt_count++];
    reset_receipt(*receipt);
    return 1;
}
