#include "obsidian.h"

#include <string.h>

static size_t nonce_slot(uint64_t nonce) {
    return (size_t)(nonce % OBS_NONCE_SLOTS);
}

void obs_nonce_init(ObsNonceWindow *window, const char *owner, uint64_t floor, uint64_t span) {
    if (window == NULL) {
        return;
    }
    memset(window, 0, sizeof(*window));
    obs_copy(window->owner, sizeof(window->owner), owner);
    window->floor = floor;
    window->span = span;
}

int obs_nonce_check(const ObsNonceWindow *window, uint64_t nonce, ObsError *err) {
    size_t slot;
    if (window == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "missing nonce window");
        return 0;
    }
    if (nonce < window->floor) {
        obs_error_set(err, OBS_ERR_NONCE, "nonce below accepted floor for %s", window->owner);
        return 0;
    }
    if (window->span > 0 && nonce >= window->floor + window->span) {
        obs_error_set(err, OBS_ERR_NONCE, "nonce outside accepted window for %s", window->owner);
        return 0;
    }
    slot = nonce_slot(nonce);
    if (window->used[slot] && window->values[slot] == nonce) {
        obs_error_set(err, OBS_ERR_NONCE, "nonce already consumed for %s", window->owner);
        return 0;
    }
    return 1;
}

int obs_nonce_mark(ObsNonceWindow *window, uint64_t nonce, ObsError *err) {
    size_t slot;
    if (!obs_nonce_check(window, nonce, err)) {
        if (window != NULL) {
            window->rejected++;
        }
        return 0;
    }
    slot = nonce_slot(nonce);
    window->used[slot] = 1;
    window->values[slot] = nonce;
    window->accepted++;
    while (window->span > 0) {
        size_t floor_slot = nonce_slot(window->floor);
        if (!window->used[floor_slot] || window->values[floor_slot] != window->floor) {
            break;
        }
        window->floor++;
    }
    return 1;
}

size_t obs_nonce_used_count(const ObsNonceWindow *window) {
    size_t i;
    size_t count = 0;
    if (window == NULL) {
        return 0;
    }
    for (i = 0; i < OBS_NONCE_SLOTS; i++) {
        if (window->used[i]) {
            count++;
        }
    }
    return count;
}

uint64_t obs_nonce_digest(const ObsNonceWindow *window) {
    uint64_t digest;
    size_t i;
    if (window == NULL) {
        return 0;
    }
    digest = obs_hash_seed();
    digest = obs_hash_text(digest, window->owner);
    digest = obs_hash_u64(digest, window->floor);
    digest = obs_hash_u64(digest, window->span);
    for (i = 0; i < OBS_NONCE_SLOTS; i++) {
        if (window->used[i]) {
            digest = obs_hash_u64(digest, window->values[i]);
        }
    }
    return digest;
}

