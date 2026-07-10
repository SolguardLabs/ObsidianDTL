#define _CRT_SECURE_NO_WARNINGS
#include "obsidian.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static void quote_reset(ObsRouteQuote *quote) {
    if (quote != NULL) {
        memset(quote, 0, sizeof(*quote));
    }
}

static void quote_reject(ObsRouteQuote *quote, ObsStatus status, const char *reason, ObsError *err) {
    if (quote != NULL) {
        quote->accepted = 0;
        obs_copy(quote->reason, sizeof(quote->reason), reason);
    }
    obs_error_set(err, status, "%s", reason);
}

ObsAmount obs_domain_available(const ObsDomain *domain) {
    if (domain == NULL) {
        return 0;
    }
    if (domain->vault <= domain->pending_outbound) {
        return 0;
    }
    return domain->vault - domain->pending_outbound;
}

uint64_t obs_route_book_digest(const ObsLedger *ledger) {
    uint64_t h;
    size_t i;
    if (ledger == NULL) {
        return 0;
    }
    h = obs_hash_seed();
    h = obs_hash_text(h, "obsidian.routebook.v1");
    for (i = 0; i < ledger->route_count; i++) {
        const ObsRoute *route = &ledger->routes[i];
        h = obs_hash_text(h, route->id);
        h = obs_hash_text(h, route->logical_origin);
        h = obs_hash_text(h, route->transport_origin);
        h = obs_hash_text(h, route->target_domain);
        h = obs_hash_text(h, route->relayer_account);
        h = obs_hash_u64(h, route->relay_fee);
        h = obs_hash_u64(h, route->max_delay);
        h = obs_hash_u64(h, route->priority);
        h = obs_hash_u64(h, (uint64_t)route->active);
        h = obs_hash_u64(h, route->deliveries);
        h = obs_hash_u64(h, route->failures);
    }
    return h;
}

int obs_quote_route(
    const ObsLedger *ledger,
    const char *route_id,
    ObsAmount amount,
    ObsAmount max_fee,
    ObsRouteQuote *quote,
    ObsError *err) {
    const ObsRoute *route;
    const ObsDomain *logical;
    ObsAmount net;
    quote_reset(quote);
    if (ledger == NULL || route_id == NULL || route_id[0] == '\0') {
        quote_reject(quote, OBS_ERR_ARGUMENT, "quote requires route id", err);
        return 0;
    }
    route = obs_ledger_route_const(ledger, route_id);
    if (route == NULL) {
        quote_reject(quote, OBS_ERR_NOT_FOUND, "route not found for quote", err);
        return 0;
    }
    if (quote != NULL) {
        obs_copy(quote->route_id, sizeof(quote->route_id), route->id);
        obs_copy(quote->logical_origin, sizeof(quote->logical_origin), route->logical_origin);
        obs_copy(quote->transport_origin, sizeof(quote->transport_origin), route->transport_origin);
        obs_copy(quote->target_domain, sizeof(quote->target_domain), route->target_domain);
        quote->amount = amount;
        quote->fee = route->relay_fee;
        quote->max_delay = route->max_delay;
        quote->priority = route->priority;
    }
    logical = obs_ledger_domain_const(ledger, route->logical_origin);
    if (logical == NULL) {
        quote_reject(quote, OBS_ERR_NOT_FOUND, "route logical domain not registered", err);
        return 0;
    }
    if (quote != NULL) {
        quote->available_liquidity = obs_domain_available(logical);
    }
    if (!route->active) {
        quote_reject(quote, OBS_ERR_POLICY, "route inactive for quote", err);
        return 0;
    }
    if (amount == 0) {
        quote_reject(quote, OBS_ERR_ARGUMENT, "quote amount must be positive", err);
        return 0;
    }
    if (route->relay_fee > max_fee) {
        quote_reject(quote, OBS_ERR_POLICY, "quote fee exceeds fee envelope", err);
        return 0;
    }
    if (obs_domain_available(logical) < amount) {
        quote_reject(quote, OBS_ERR_BALANCE, "quote exceeds available domain liquidity", err);
        return 0;
    }
    if (!obs_amount_charge(amount, route->relay_fee, &net, err)) {
        if (quote != NULL) {
            obs_copy(quote->reason, sizeof(quote->reason), err != NULL ? err->message : "quote failed");
        }
        return 0;
    }
    if (quote != NULL) {
        quote->net_amount = net;
        quote->accepted = 1;
        obs_copy(quote->reason, sizeof(quote->reason), "accepted");
    }
    obs_error_clear(err);
    return 1;
}

static int quote_is_better(const ObsRouteQuote *candidate, const ObsRouteQuote *current) {
    if (candidate == NULL || !candidate->accepted) {
        return 0;
    }
    if (current == NULL || !current->accepted) {
        return 1;
    }
    if (candidate->priority != current->priority) {
        return candidate->priority < current->priority;
    }
    if (candidate->fee != current->fee) {
        return candidate->fee < current->fee;
    }
    if (candidate->max_delay != current->max_delay) {
        return candidate->max_delay < current->max_delay;
    }
    return strcmp(candidate->route_id, current->route_id) < 0;
}

int obs_select_route(
    const ObsLedger *ledger,
    const char *logical_origin,
    const char *target_domain,
    ObsAmount amount,
    ObsAmount max_fee,
    ObsRouteQuote *quote,
    ObsError *err) {
    size_t i;
    int found_candidate = 0;
    ObsRouteQuote best;
    ObsError local;
    quote_reset(&best);
    quote_reset(quote);
    if (ledger == NULL || logical_origin == NULL || target_domain == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "route selection requires origin and target");
        return 0;
    }
    for (i = 0; i < ledger->route_count; i++) {
        const ObsRoute *route = &ledger->routes[i];
        ObsRouteQuote candidate;
        if (!obs_streq(route->logical_origin, logical_origin) || !obs_streq(route->target_domain, target_domain)) {
            continue;
        }
        found_candidate = 1;
        obs_error_clear(&local);
        if (!obs_quote_route(ledger, route->id, amount, max_fee, &candidate, &local)) {
            continue;
        }
        if (quote_is_better(&candidate, &best)) {
            best = candidate;
        }
    }
    if (best.accepted) {
        if (quote != NULL) {
            *quote = best;
        }
        obs_error_clear(err);
        return 1;
    }
    if (!found_candidate) {
        obs_error_set(err, OBS_ERR_NOT_FOUND, "no route registered for selection");
    } else {
        obs_error_set(err, OBS_ERR_POLICY, "no route satisfies selection constraints");
    }
    return 0;
}

int obs_health_report(const ObsLedger *ledger, ObsHealthReport *report, ObsError *err) {
    size_t i;
    ObsAmount total;
    if (ledger == NULL || report == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "health report requires ledger and output");
        return 0;
    }
    memset(report, 0, sizeof(*report));
    for (i = 0; i < ledger->route_count; i++) {
        const ObsRoute *route = &ledger->routes[i];
        if (route->active) {
            report->active_routes++;
        } else {
            report->inactive_routes++;
        }
        if (route->failures > 0) {
            report->routes_with_failures++;
        }
    }
    for (i = 0; i < ledger->domain_count; i++) {
        const ObsDomain *domain = &ledger->domains[i];
        ObsAmount available = obs_domain_available(domain);
        if (!obs_amount_add(report->total_vault, domain->vault, &total, err)) {
            return 0;
        }
        report->total_vault = total;
        if (!obs_amount_add(report->total_available, available, &total, err)) {
            return 0;
        }
        report->total_available = total;
        if (!obs_amount_add(report->total_pending, domain->pending_outbound, &total, err)) {
            return 0;
        }
        report->total_pending = total;
        if (domain->vault > 0 && available * 10 < domain->vault) {
            report->domains_low_liquidity++;
        }
    }
    report->route_digest = obs_route_book_digest(ledger);
    obs_error_clear(err);
    return 1;
}

