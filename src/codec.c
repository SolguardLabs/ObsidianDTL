#define _CRT_SECURE_NO_WARNINGS
#include "obsidian.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

uint64_t obs_hash_seed(void) {
    return UINT64_C(1469598103934665603);
}

uint64_t obs_hash_bytes(uint64_t seed, const void *data, size_t len) {
    const unsigned char *ptr = (const unsigned char *)data;
    size_t i;
    uint64_t h = seed;
    for (i = 0; i < len; i++) {
        h ^= (uint64_t)ptr[i];
        h *= UINT64_C(1099511628211);
    }
    return h;
}

uint64_t obs_hash_text(uint64_t seed, const char *text) {
    if (text == NULL) {
        return obs_hash_bytes(seed, "", 1);
    }
    return obs_hash_bytes(seed, text, strlen(text) + 1);
}

uint64_t obs_hash_u64(uint64_t seed, uint64_t value) {
    unsigned char bytes[8];
    int i;
    for (i = 0; i < 8; i++) {
        bytes[i] = (unsigned char)((value >> (i * 8)) & 0xffu);
    }
    return obs_hash_bytes(seed, bytes, sizeof(bytes));
}

uint64_t obs_packet_digest(const ObsPacket *packet) {
    uint64_t h;
    if (packet == NULL) {
        return 0;
    }
    h = obs_hash_seed();
    h = obs_hash_text(h, "obsidian.packet.v1");
    h = obs_hash_text(h, packet->id);
    h = obs_hash_text(h, packet->logical_origin);
    h = obs_hash_text(h, packet->target_domain);
    h = obs_hash_text(h, packet->sender);
    h = obs_hash_text(h, packet->beneficiary);
    h = obs_hash_u64(h, packet->nonce);
    h = obs_hash_u64(h, packet->amount);
    h = obs_hash_u64(h, packet->max_fee);
    h = obs_hash_u64(h, packet->created_at);
    h = obs_hash_u64(h, packet->expires_at);
    h = obs_hash_u64(h, (uint64_t)packet->kind);
    return h;
}

uint64_t obs_receipt_digest(const ObsReceipt *receipt) {
    uint64_t h;
    if (receipt == NULL) {
        return 0;
    }
    h = obs_hash_seed();
    h = obs_hash_text(h, "obsidian.receipt.v1");
    h = obs_hash_text(h, receipt->id);
    h = obs_hash_text(h, receipt->packet_id);
    h = obs_hash_text(h, receipt->route_id);
    h = obs_hash_text(h, receipt->logical_origin);
    h = obs_hash_text(h, receipt->transport_origin);
    h = obs_hash_text(h, receipt->target_domain);
    h = obs_hash_text(h, receipt->relayer_account);
    h = obs_hash_u64(h, receipt->amount);
    h = obs_hash_u64(h, receipt->fee);
    h = obs_hash_u64(h, receipt->packet_nonce);
    h = obs_hash_u64(h, receipt->packet_digest);
    h = obs_hash_u64(h, receipt->confirmed_at);
    return h;
}

void obs_hex64(uint64_t value, char out[19]) {
    if (out == NULL) {
        return;
    }
    snprintf(out, 19, "0x%016" PRIx64, value);
}

