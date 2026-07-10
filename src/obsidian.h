#ifndef OBSIDIAN_DTL_H
#define OBSIDIAN_DTL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define OBS_MAX_DOMAINS 12
#define OBS_MAX_ACCOUNTS 24
#define OBS_MAX_ROUTES 24
#define OBS_MAX_PACKETS 96
#define OBS_MAX_RECEIPTS 128
#define OBS_MAX_EVENTS 256
#define OBS_NONCE_SLOTS 256
#define OBS_ID_LEN 32
#define OBS_LABEL_LEN 64
#define OBS_EVENT_LEN 192
#define OBS_ERROR_LEN 192

typedef uint64_t ObsAmount;

typedef enum ObsStatus {
    OBS_OK = 0,
    OBS_ERR_ARGUMENT = 1,
    OBS_ERR_NOT_FOUND = 2,
    OBS_ERR_LIMIT = 3,
    OBS_ERR_POLICY = 4,
    OBS_ERR_NONCE = 5,
    OBS_ERR_BALANCE = 6,
    OBS_ERR_RECEIPT = 7,
    OBS_ERR_IO = 8,
    OBS_ERR_PARSE = 9
} ObsStatus;

typedef enum ObsPacketKind {
    OBS_PACKET_RELEASE = 1,
    OBS_PACKET_WITHDRAW = 2,
    OBS_PACKET_SETTLE = 3
} ObsPacketKind;

typedef enum ObsEventKind {
    OBS_EVENT_BOOT = 1,
    OBS_EVENT_ROUTE = 2,
    OBS_EVENT_PACKET = 3,
    OBS_EVENT_RECEIPT = 4,
    OBS_EVENT_REJECT = 5,
    OBS_EVENT_BALANCE = 6
} ObsEventKind;

typedef struct ObsError {
    ObsStatus code;
    char message[OBS_ERROR_LEN];
} ObsError;

typedef struct ObsNonceWindow {
    char owner[OBS_ID_LEN];
    uint64_t floor;
    uint64_t span;
    uint64_t accepted;
    uint64_t rejected;
    uint64_t values[OBS_NONCE_SLOTS];
    unsigned char used[OBS_NONCE_SLOTS];
} ObsNonceWindow;

typedef struct ObsDomain {
    char id[OBS_ID_LEN];
    char label[OBS_LABEL_LEN];
    ObsAmount vault;
    ObsAmount pending_outbound;
    ObsAmount delivery_fees;
    uint64_t delivered_packets;
    uint64_t rejected_packets;
    ObsNonceWindow nonce_window;
} ObsDomain;

typedef struct ObsAccount {
    char id[OBS_ID_LEN];
    char label[OBS_LABEL_LEN];
    ObsAmount balance;
    uint64_t credits;
    uint64_t debits;
} ObsAccount;

typedef struct ObsRoute {
    char id[OBS_ID_LEN];
    char logical_origin[OBS_ID_LEN];
    char transport_origin[OBS_ID_LEN];
    char target_domain[OBS_ID_LEN];
    char relayer_account[OBS_ID_LEN];
    ObsAmount relay_fee;
    uint64_t max_delay;
    uint64_t priority;
    int active;
    uint64_t deliveries;
    uint64_t failures;
} ObsRoute;

typedef struct ObsPacket {
    char id[OBS_ID_LEN];
    char logical_origin[OBS_ID_LEN];
    char target_domain[OBS_ID_LEN];
    char sender[OBS_ID_LEN];
    char beneficiary[OBS_ID_LEN];
    uint64_t nonce;
    ObsAmount amount;
    ObsAmount max_fee;
    uint64_t created_at;
    uint64_t expires_at;
    ObsPacketKind kind;
    uint64_t digest;
    int submitted;
    int settled;
    int rejected;
    char rejection[OBS_ERROR_LEN];
} ObsPacket;

typedef struct ObsReceipt {
    char id[OBS_ID_LEN];
    char packet_id[OBS_ID_LEN];
    char route_id[OBS_ID_LEN];
    char logical_origin[OBS_ID_LEN];
    char transport_origin[OBS_ID_LEN];
    char target_domain[OBS_ID_LEN];
    char relayer_account[OBS_ID_LEN];
    ObsAmount amount;
    ObsAmount fee;
    uint64_t packet_nonce;
    uint64_t packet_digest;
    uint64_t confirmed_at;
    uint64_t receipt_digest;
    int late;
    int accepted;
} ObsReceipt;

typedef struct ObsEvent {
    ObsEventKind kind;
    uint64_t at;
    char text[OBS_EVENT_LEN];
} ObsEvent;

typedef struct ObsLedger {
    ObsDomain domains[OBS_MAX_DOMAINS];
    size_t domain_count;
    ObsAccount accounts[OBS_MAX_ACCOUNTS];
    size_t account_count;
    ObsRoute routes[OBS_MAX_ROUTES];
    size_t route_count;
    ObsPacket packets[OBS_MAX_PACKETS];
    size_t packet_count;
    ObsReceipt receipts[OBS_MAX_RECEIPTS];
    size_t receipt_count;
    ObsEvent events[OBS_MAX_EVENTS];
    size_t event_count;
    uint64_t clock;
    uint64_t packets_submitted;
    uint64_t packets_settled;
    uint64_t packets_rejected;
    uint64_t receipts_late;
    uint64_t receipts_rejected;
    ObsAmount initial_supply;
    char scenario[OBS_ID_LEN];
} ObsLedger;

typedef struct ObsPacketSpec {
    const char *id;
    const char *logical_origin;
    const char *target_domain;
    const char *sender;
    const char *beneficiary;
    uint64_t nonce;
    ObsAmount amount;
    ObsAmount max_fee;
    uint64_t ttl;
    ObsPacketKind kind;
} ObsPacketSpec;

typedef struct ObsRouteQuote {
    char route_id[OBS_ID_LEN];
    char logical_origin[OBS_ID_LEN];
    char transport_origin[OBS_ID_LEN];
    char target_domain[OBS_ID_LEN];
    ObsAmount amount;
    ObsAmount fee;
    ObsAmount net_amount;
    ObsAmount available_liquidity;
    uint64_t max_delay;
    uint64_t priority;
    int accepted;
    char reason[OBS_ERROR_LEN];
} ObsRouteQuote;

typedef struct ObsHealthReport {
    size_t active_routes;
    size_t inactive_routes;
    size_t routes_with_failures;
    size_t domains_low_liquidity;
    ObsAmount total_vault;
    ObsAmount total_available;
    ObsAmount total_pending;
    uint64_t route_digest;
} ObsHealthReport;

void obs_error_clear(ObsError *err);
void obs_error_set(ObsError *err, ObsStatus code, const char *fmt, ...);
const char *obs_status_name(ObsStatus status);
const char *obs_kind_name(ObsPacketKind kind);
int obs_streq(const char *a, const char *b);
int obs_streq_ci(const char *a, const char *b);
void obs_copy(char *dst, size_t dst_len, const char *src);

int obs_amount_add(ObsAmount a, ObsAmount b, ObsAmount *out, ObsError *err);
int obs_amount_sub(ObsAmount a, ObsAmount b, ObsAmount *out, ObsError *err);
int obs_amount_charge(ObsAmount gross, ObsAmount fee, ObsAmount *net, ObsError *err);

void obs_nonce_init(ObsNonceWindow *window, const char *owner, uint64_t floor, uint64_t span);
int obs_nonce_check(const ObsNonceWindow *window, uint64_t nonce, ObsError *err);
int obs_nonce_mark(ObsNonceWindow *window, uint64_t nonce, ObsError *err);
size_t obs_nonce_used_count(const ObsNonceWindow *window);
uint64_t obs_nonce_digest(const ObsNonceWindow *window);

void obs_ledger_init(ObsLedger *ledger, const char *scenario);
int obs_ledger_add_domain(ObsLedger *ledger, const char *id, const char *label, ObsAmount vault, ObsError *err);
int obs_ledger_add_account(ObsLedger *ledger, const char *id, const char *label, ObsAmount balance, ObsError *err);
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
    ObsError *err);
ObsDomain *obs_ledger_domain(ObsLedger *ledger, const char *id);
const ObsDomain *obs_ledger_domain_const(const ObsLedger *ledger, const char *id);
ObsAccount *obs_ledger_account(ObsLedger *ledger, const char *id);
const ObsAccount *obs_ledger_account_const(const ObsLedger *ledger, const char *id);
ObsRoute *obs_ledger_route(ObsLedger *ledger, const char *id);
const ObsRoute *obs_ledger_route_const(const ObsLedger *ledger, const char *id);
ObsPacket *obs_ledger_packet(ObsLedger *ledger, const char *id);
const ObsPacket *obs_ledger_packet_const(const ObsLedger *ledger, const char *id);
void obs_ledger_event(ObsLedger *ledger, ObsEventKind kind, const char *fmt, ...);
void obs_ledger_advance(ObsLedger *ledger, uint64_t delta);
ObsAmount obs_ledger_total_supply(const ObsLedger *ledger);
int obs_ledger_conservation_ok(const ObsLedger *ledger);
uint64_t obs_ledger_digest(const ObsLedger *ledger);
ObsAmount obs_domain_available(const ObsDomain *domain);
int obs_domain_debit(ObsDomain *domain, ObsAmount amount, ObsError *err);
int obs_account_credit(ObsAccount *account, ObsAmount amount, ObsError *err);
int obs_account_debit(ObsAccount *account, ObsAmount amount, ObsError *err);
int obs_allocate_packet(ObsLedger *ledger, ObsPacket **packet, ObsError *err);
int obs_allocate_receipt(ObsLedger *ledger, ObsReceipt **receipt, ObsError *err);
uint64_t obs_route_book_digest(const ObsLedger *ledger);
int obs_quote_route(
    const ObsLedger *ledger,
    const char *route_id,
    ObsAmount amount,
    ObsAmount max_fee,
    ObsRouteQuote *quote,
    ObsError *err);
int obs_select_route(
    const ObsLedger *ledger,
    const char *logical_origin,
    const char *target_domain,
    ObsAmount amount,
    ObsAmount max_fee,
    ObsRouteQuote *quote,
    ObsError *err);
int obs_health_report(const ObsLedger *ledger, ObsHealthReport *report, ObsError *err);

uint64_t obs_hash_seed(void);
uint64_t obs_hash_bytes(uint64_t seed, const void *data, size_t len);
uint64_t obs_hash_text(uint64_t seed, const char *text);
uint64_t obs_hash_u64(uint64_t seed, uint64_t value);
uint64_t obs_packet_digest(const ObsPacket *packet);
uint64_t obs_receipt_digest(const ObsReceipt *receipt);
void obs_hex64(uint64_t value, char out[19]);

int obs_submit_packet(ObsLedger *ledger, const ObsPacketSpec *spec, ObsError *err);
int obs_deliver_packet(ObsLedger *ledger, const char *packet_id, const char *route_id, uint64_t confirmation_delay, ObsError *err);
int obs_verify_receipt(const ObsLedger *ledger, const ObsPacket *packet, const ObsRoute *route, const ObsReceipt *receipt, ObsError *err);

int obs_load_default_fixture(ObsLedger *ledger, ObsError *err);
int obs_run_scenario(const char *name, ObsLedger *ledger, ObsError *err);
int obs_run_script_file(const char *path, ObsLedger *ledger, ObsError *err);
int obs_run_script_stream(FILE *stream, const char *name, ObsLedger *ledger, ObsError *err);

void obs_print_report(const ObsLedger *ledger, FILE *out);

#endif
