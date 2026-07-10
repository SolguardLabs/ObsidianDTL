#define _CRT_SECURE_NO_WARNINGS
#include "obsidian.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void obs_error_clear(ObsError *err) {
    if (err == NULL) {
        return;
    }
    err->code = OBS_OK;
    err->message[0] = '\0';
}

void obs_error_set(ObsError *err, ObsStatus code, const char *fmt, ...) {
    va_list args;
    if (err == NULL) {
        return;
    }
    err->code = code;
    if (fmt == NULL) {
        snprintf(err->message, sizeof(err->message), "%s", obs_status_name(code));
        return;
    }
    va_start(args, fmt);
    vsnprintf(err->message, sizeof(err->message), fmt, args);
    va_end(args);
}

const char *obs_status_name(ObsStatus status) {
    switch (status) {
    case OBS_OK:
        return "ok";
    case OBS_ERR_ARGUMENT:
        return "argument";
    case OBS_ERR_NOT_FOUND:
        return "not_found";
    case OBS_ERR_LIMIT:
        return "limit";
    case OBS_ERR_POLICY:
        return "policy";
    case OBS_ERR_NONCE:
        return "nonce";
    case OBS_ERR_BALANCE:
        return "balance";
    case OBS_ERR_RECEIPT:
        return "receipt";
    case OBS_ERR_IO:
        return "io";
    case OBS_ERR_PARSE:
        return "parse";
    default:
        return "unknown";
    }
}

const char *obs_kind_name(ObsPacketKind kind) {
    switch (kind) {
    case OBS_PACKET_RELEASE:
        return "release";
    case OBS_PACKET_WITHDRAW:
        return "withdraw";
    case OBS_PACKET_SETTLE:
        return "settle";
    default:
        return "unknown";
    }
}

int obs_streq(const char *a, const char *b) {
    if (a == NULL || b == NULL) {
        return 0;
    }
    return strcmp(a, b) == 0;
}

int obs_streq_ci(const char *a, const char *b) {
    unsigned char ca;
    unsigned char cb;
    if (a == NULL || b == NULL) {
        return 0;
    }
    while (*a != '\0' && *b != '\0') {
        ca = (unsigned char)*a;
        cb = (unsigned char)*b;
        if (tolower(ca) != tolower(cb)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

void obs_copy(char *dst, size_t dst_len, const char *src) {
    if (dst == NULL || dst_len == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_len, "%s", src);
}

