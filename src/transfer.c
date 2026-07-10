#define _CRT_SECURE_NO_WARNINGS
#include "obsidian.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static int copy_required(char *dst, size_t dst_len, const char *src, const char *field, ObsError *err) {
    if (src == NULL || src[0] == '\0') {
        obs_error_set(err, OBS_ERR_ARGUMENT, "%s required", field);
        return 0;
    }
    obs_copy(dst, dst_len, src);
    return 1;
}

static void reject_packet(ObsLedger *ledger, ObsPacket *packet, const ObsRoute *route, ObsError *reason) {
    ObsDomain *logical;
    if (ledger == NULL || packet == NULL || reason == NULL) {
        return;
    }
    packet->rejected = 1;
    obs_copy(packet->rejection, sizeof(packet->rejection), reason->message);
    ledger->packets_rejected++;
    logical = obs_ledger_domain(ledger, packet->logical_origin);
    if (logical != NULL) {
        logical->rejected_packets++;
    }
    if (route != NULL) {
        ObsRoute *mutable_route = obs_ledger_route(ledger, route->id);
        if (mutable_route != NULL) {
            mutable_route->failures++;
        }
    }
    obs_ledger_event(ledger, OBS_EVENT_REJECT, "packet %s rejected: %s", packet->id, reason->message);
}

static int route_matches_packet(const ObsRoute *route, const ObsPacket *packet, ObsError *err) {
    if (route == NULL || packet == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "route or packet missing");
        return 0;
    }
    if (!route->active) {
        obs_error_set(err, OBS_ERR_POLICY, "route %s is inactive", route->id);
        return 0;
    }
    if (!obs_streq(route->logical_origin, packet->logical_origin)) {
        obs_error_set(err, OBS_ERR_POLICY, "route %s does not serve logical origin %s", route->id, packet->logical_origin);
        return 0;
    }
    if (!obs_streq(route->target_domain, packet->target_domain)) {
        obs_error_set(err, OBS_ERR_POLICY, "route %s does not target %s", route->id, packet->target_domain);
        return 0;
    }
    if (route->relay_fee > packet->max_fee) {
        obs_error_set(err, OBS_ERR_POLICY, "route fee exceeds packet fee envelope");
        return 0;
    }
    return 1;
}

int obs_submit_packet(ObsLedger *ledger, const ObsPacketSpec *spec, ObsError *err) {
    ObsPacket *packet;
    if (ledger == NULL || spec == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "packet spec required");
        return 0;
    }
    if (obs_ledger_packet(ledger, spec->id) != NULL) {
        obs_error_set(err, OBS_ERR_POLICY, "packet %s already submitted", spec->id);
        return 0;
    }
    if (obs_ledger_domain(ledger, spec->logical_origin) == NULL) {
        obs_error_set(err, OBS_ERR_NOT_FOUND, "logical origin %s not registered", spec->logical_origin);
        return 0;
    }
    if (obs_ledger_domain(ledger, spec->target_domain) == NULL) {
        obs_error_set(err, OBS_ERR_NOT_FOUND, "target domain %s not registered", spec->target_domain);
        return 0;
    }
    if (obs_ledger_account(ledger, spec->sender) == NULL) {
        obs_error_set(err, OBS_ERR_NOT_FOUND, "sender account %s not registered", spec->sender);
        return 0;
    }
    if (obs_ledger_account(ledger, spec->beneficiary) == NULL) {
        obs_error_set(err, OBS_ERR_NOT_FOUND, "beneficiary account %s not registered", spec->beneficiary);
        return 0;
    }
    if (spec->amount == 0) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "packet amount must be positive");
        return 0;
    }
    if (spec->ttl == 0) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "packet ttl must be positive");
        return 0;
    }
    if (!obs_allocate_packet(ledger, &packet, err)) {
        return 0;
    }
    if (!copy_required(packet->id, sizeof(packet->id), spec->id, "packet id", err) ||
        !copy_required(packet->logical_origin, sizeof(packet->logical_origin), spec->logical_origin, "logical origin", err) ||
        !copy_required(packet->target_domain, sizeof(packet->target_domain), spec->target_domain, "target domain", err) ||
        !copy_required(packet->sender, sizeof(packet->sender), spec->sender, "sender", err) ||
        !copy_required(packet->beneficiary, sizeof(packet->beneficiary), spec->beneficiary, "beneficiary", err)) {
        return 0;
    }
    packet->nonce = spec->nonce;
    packet->amount = spec->amount;
    packet->max_fee = spec->max_fee;
    packet->created_at = ledger->clock;
    packet->expires_at = ledger->clock + spec->ttl;
    packet->kind = spec->kind;
    packet->digest = obs_packet_digest(packet);
    packet->submitted = 1;
    ledger->packets_submitted++;
    obs_ledger_event(
        ledger,
        OBS_EVENT_PACKET,
        "packet %s submitted from %s nonce %" PRIu64,
        packet->id,
        packet->logical_origin,
        packet->nonce);
    return 1;
}

int obs_verify_receipt(const ObsLedger *ledger, const ObsPacket *packet, const ObsRoute *route, const ObsReceipt *receipt, ObsError *err) {
    uint64_t expected;
    (void)ledger;
    if (packet == NULL || route == NULL || receipt == NULL) {
        obs_error_set(err, OBS_ERR_RECEIPT, "receipt context missing");
        return 0;
    }
    if (!obs_streq(receipt->packet_id, packet->id)) {
        obs_error_set(err, OBS_ERR_RECEIPT, "receipt packet mismatch");
        return 0;
    }
    if (!obs_streq(receipt->route_id, route->id)) {
        obs_error_set(err, OBS_ERR_RECEIPT, "receipt route mismatch");
        return 0;
    }
    if (!obs_streq(receipt->logical_origin, packet->logical_origin)) {
        obs_error_set(err, OBS_ERR_RECEIPT, "receipt logical origin mismatch");
        return 0;
    }
    if (!obs_streq(receipt->transport_origin, route->transport_origin)) {
        obs_error_set(err, OBS_ERR_RECEIPT, "receipt transport origin mismatch");
        return 0;
    }
    if (!obs_streq(receipt->target_domain, packet->target_domain)) {
        obs_error_set(err, OBS_ERR_RECEIPT, "receipt target mismatch");
        return 0;
    }
    if (!obs_streq(receipt->relayer_account, route->relayer_account)) {
        obs_error_set(err, OBS_ERR_RECEIPT, "receipt relayer mismatch");
        return 0;
    }
    if (receipt->packet_nonce != packet->nonce || receipt->packet_digest != packet->digest) {
        obs_error_set(err, OBS_ERR_RECEIPT, "receipt packet digest mismatch");
        return 0;
    }
    if (receipt->amount != packet->amount || receipt->fee != route->relay_fee) {
        obs_error_set(err, OBS_ERR_RECEIPT, "receipt amount mismatch");
        return 0;
    }
    expected = obs_receipt_digest(receipt);
    if (expected != receipt->receipt_digest) {
        obs_error_set(err, OBS_ERR_RECEIPT, "receipt digest mismatch");
        return 0;
    }
    return 1;
}

int obs_deliver_packet(ObsLedger *ledger, const char *packet_id, const char *route_id, uint64_t confirmation_delay, ObsError *err) {
    ObsPacket *packet;
    ObsRoute *route;
    ObsDomain *logical_domain;
    ObsDomain *transport_domain;
    ObsAccount *beneficiary;
    ObsAccount *relayer;
    ObsReceipt *receipt;
    ObsAmount net;
    ObsError local;
    uint64_t confirmed_at;
    if (ledger == NULL || packet_id == NULL || route_id == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "deliver requires packet and route");
        return 0;
    }
    obs_error_clear(&local);
    packet = obs_ledger_packet(ledger, packet_id);
    if (packet == NULL) {
        obs_error_set(err, OBS_ERR_NOT_FOUND, "packet %s not found", packet_id);
        return 0;
    }
    route = obs_ledger_route(ledger, route_id);
    if (route == NULL) {
        obs_error_set(err, OBS_ERR_NOT_FOUND, "route %s not found", route_id);
        reject_packet(ledger, packet, NULL, err);
        return 0;
    }
    if (!route_matches_packet(route, packet, &local)) {
        reject_packet(ledger, packet, route, &local);
        if (err != NULL) {
            *err = local;
        }
        return 0;
    }
    if (ledger->clock > packet->expires_at) {
        obs_error_set(&local, OBS_ERR_POLICY, "packet %s expired", packet->id);
        reject_packet(ledger, packet, route, &local);
        if (err != NULL) {
            *err = local;
        }
        return 0;
    }
    logical_domain = obs_ledger_domain(ledger, packet->logical_origin);
    transport_domain = obs_ledger_domain(ledger, route->transport_origin);
    beneficiary = obs_ledger_account(ledger, packet->beneficiary);
    relayer = obs_ledger_account(ledger, route->relayer_account);
    if (logical_domain == NULL || transport_domain == NULL || beneficiary == NULL || relayer == NULL) {
        obs_error_set(&local, OBS_ERR_NOT_FOUND, "delivery references missing ledger participant");
        reject_packet(ledger, packet, route, &local);
        if (err != NULL) {
            *err = local;
        }
        return 0;
    }
    if (!obs_nonce_check(&logical_domain->nonce_window, packet->nonce, &local)) {
        reject_packet(ledger, packet, route, &local);
        if (err != NULL) {
            *err = local;
        }
        return 0;
    }
    if (!obs_nonce_mark(&transport_domain->nonce_window, packet->nonce, &local)) {
        reject_packet(ledger, packet, route, &local);
        if (err != NULL) {
            *err = local;
        }
        return 0;
    }
    if (!obs_amount_charge(packet->amount, route->relay_fee, &net, &local)) {
        reject_packet(ledger, packet, route, &local);
        if (err != NULL) {
            *err = local;
        }
        return 0;
    }
    if (!obs_domain_debit(logical_domain, packet->amount, &local)) {
        reject_packet(ledger, packet, route, &local);
        if (err != NULL) {
            *err = local;
        }
        return 0;
    }
    if (!obs_account_credit(beneficiary, net, &local) || !obs_account_credit(relayer, route->relay_fee, &local)) {
        reject_packet(ledger, packet, route, &local);
        if (err != NULL) {
            *err = local;
        }
        return 0;
    }
    logical_domain->pending_outbound += packet->amount;
    logical_domain->delivery_fees += route->relay_fee;
    logical_domain->delivered_packets++;
    route->deliveries++;
    packet->settled = 1;
    ledger->packets_settled++;
    if (!obs_allocate_receipt(ledger, &receipt, &local)) {
        reject_packet(ledger, packet, route, &local);
        if (err != NULL) {
            *err = local;
        }
        return 0;
    }
    confirmed_at = ledger->clock + confirmation_delay;
    snprintf(receipt->id, sizeof(receipt->id), "rcpt%03u", (unsigned int)ledger->receipt_count);
    obs_copy(receipt->packet_id, sizeof(receipt->packet_id), packet->id);
    obs_copy(receipt->route_id, sizeof(receipt->route_id), route->id);
    obs_copy(receipt->logical_origin, sizeof(receipt->logical_origin), packet->logical_origin);
    obs_copy(receipt->transport_origin, sizeof(receipt->transport_origin), route->transport_origin);
    obs_copy(receipt->target_domain, sizeof(receipt->target_domain), packet->target_domain);
    obs_copy(receipt->relayer_account, sizeof(receipt->relayer_account), route->relayer_account);
    receipt->amount = packet->amount;
    receipt->fee = route->relay_fee;
    receipt->packet_nonce = packet->nonce;
    receipt->packet_digest = packet->digest;
    receipt->confirmed_at = confirmed_at;
    receipt->late = confirmed_at > packet->expires_at || confirmation_delay > route->max_delay;
    receipt->accepted = 1;
    receipt->receipt_digest = obs_receipt_digest(receipt);
    if (!obs_verify_receipt(ledger, packet, route, receipt, &local)) {
        receipt->accepted = 0;
        ledger->receipts_rejected++;
        reject_packet(ledger, packet, route, &local);
        if (err != NULL) {
            *err = local;
        }
        return 0;
    }
    if (receipt->late) {
        ledger->receipts_late++;
    }
    obs_ledger_event(ledger, OBS_EVENT_RECEIPT, "packet %s delivered by %s receipt %s", packet->id, route->id, receipt->id);
    return 1;
}
