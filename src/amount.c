#include "obsidian.h"

#include <stdint.h>

int obs_amount_add(ObsAmount a, ObsAmount b, ObsAmount *out, ObsError *err) {
    if (out == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "missing amount output");
        return 0;
    }
    if (UINT64_MAX - a < b) {
        obs_error_set(err, OBS_ERR_BALANCE, "amount overflow");
        return 0;
    }
    *out = a + b;
    return 1;
}

int obs_amount_sub(ObsAmount a, ObsAmount b, ObsAmount *out, ObsError *err) {
    if (out == NULL) {
        obs_error_set(err, OBS_ERR_ARGUMENT, "missing amount output");
        return 0;
    }
    if (a < b) {
        obs_error_set(err, OBS_ERR_BALANCE, "insufficient amount");
        return 0;
    }
    *out = a - b;
    return 1;
}

int obs_amount_charge(ObsAmount gross, ObsAmount fee, ObsAmount *net, ObsError *err) {
    if (fee > gross) {
        obs_error_set(err, OBS_ERR_BALANCE, "fee exceeds packet amount");
        return 0;
    }
    return obs_amount_sub(gross, fee, net, err);
}

