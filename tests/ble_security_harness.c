/* Exercise production pairing callbacks against deterministic GPIO/BT fakes. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>

struct bt_conn { int unused; };
struct bt_conn_info { uint8_t id; };
static bool bonded_peer = true;
struct bt_conn_pairing_feat { int unused; };
struct gpio_dt_spec { int unused; };
typedef int bt_security_t;
enum bt_security_err { BT_SECURITY_ERR_SUCCESS, BT_SECURITY_ERR_PAIR_NOT_ALLOWED };
struct bt_conn_auth_cb {
    enum bt_security_err (*pairing_accept)(struct bt_conn *, const struct bt_conn_pairing_feat *const);
};
struct bt_conn_auth_info_cb {
    void (*pairing_complete)(struct bt_conn *, bool);
    void (*pairing_failed)(struct bt_conn *, enum bt_security_err);
};
struct bt_conn_cb {
    void (*security_changed)(struct bt_conn *, bt_security_t, enum bt_security_err);
};
#define BT_SECURITY_L2 2
#define BT_HCI_ERR_AUTH_FAIL 5
/* Both configurations are compiled; the default mirrors Kconfig's gate=y. */
#ifndef CONFIG_LINKR_BLE_BRIDGE_PAIRING_GPIO_AUTH
#define CONFIG_LINKR_BLE_BRIDGE_PAIRING_GPIO_AUTH 1
#endif
/* The panel's on-screen PAIR window is compiled both ways as well; the
 * default mirrors Kconfig (n). */
#ifndef CONFIG_LINKR_BLE_PAIRING_AUTH_TOUCH
#define CONFIG_LINKR_BLE_PAIRING_AUTH_TOUCH 0
#endif
#define IS_ENABLED(option) (option)
#define GPIO_INPUT 1
#define GPIO_DT_SPEC_GET(...) { 0 }
#define BT_CONN_CB_DEFINE(name) struct bt_conn_cb name
#define ARG_UNUSED(x) (void)(x)
#define LOG_MODULE_REGISTER(...)
static void quiet_log(const char *fmt, ...) { (void)fmt; }
#define LOG_WRN(...) quiet_log(__VA_ARGS__)
#define LOG_INF(...) quiet_log(__VA_ARGS__)
#define LOG_ERR(...) quiet_log(__VA_ARGS__)
static int gpio_value, gpio_reads, configured_error, request_error, disconnects;
static int security_requested;
static bool gpio_ready = true;
static const struct bt_conn_auth_cb *registered_auth;
static bool gpio_is_ready_dt(const struct gpio_dt_spec *p) { (void)p; return gpio_ready; }
static int gpio_pin_configure_dt(const struct gpio_dt_spec *p, int mode)
{ (void)p; assert(mode == GPIO_INPUT); return configured_error; }
static int gpio_pin_get_dt(const struct gpio_dt_spec *p)
{ (void)p; gpio_reads++; return gpio_value; }
static int bt_conn_auth_cb_register(const struct bt_conn_auth_cb *cb)
{ registered_auth = cb; return 0; }
static int bt_conn_auth_info_cb_register(struct bt_conn_auth_info_cb *cb)
{ assert(cb->pairing_complete && cb->pairing_failed); return 0; }
static int bt_conn_disconnect(struct bt_conn *conn, int reason)
{ (void)conn; assert(reason == BT_HCI_ERR_AUTH_FAIL); disconnects++; return 0; }
static int bt_conn_set_security(struct bt_conn *conn, int security)
{ (void)conn; security_requested = security; return request_error; }
static int bt_conn_get_info(struct bt_conn *conn, struct bt_conn_info *info)
{ (void)conn; info->id = 1; return 0; }
static const void *bt_conn_get_dst(struct bt_conn *conn) { return conn; }
static bool bt_le_bond_exists(uint8_t id, const void *dst)
{ assert(id == 1 && dst); return bonded_peer; }

/* Zephyr's atomic_t, and the accessors the pairing window uses. atomic_set()
 * returns the previous value, and the window logic reads it back through
 * atomic_get(), so the fake keeps both contracts. */
typedef long atomic_t;
typedef long atomic_val_t;
static atomic_val_t atomic_get(const atomic_t *target) { return *target; }
static atomic_val_t atomic_set(atomic_t *target, atomic_val_t value)
{ atomic_val_t old = *target; *target = value; return old; }

/* Controllable millisecond clock: the window is pure deadline arithmetic
 * against k_uptime_get_32(), rollover included, so the scenarios move it. */
static uint32_t uptime_ms;
static uint32_t k_uptime_get_32(void) { return uptime_ms; }

/* linkr_ble_pairing_authorize() calls linkr_ble_pairing_cancel() before the
 * latter is defined, and normally gets the prototype from
 * src/ble_security.h, which cannot be included here because it pulls in
 * <zephyr/bluetooth/conn.h>. Declare the one entry point needed that early. */
void linkr_ble_pairing_cancel(void);

#include "security_production.inc"

int main(int argc, char **argv)
{
    struct bt_conn conn = { 0 };
    struct bt_conn_pairing_feat feat = { 0 };
    assert(argc == 2);
    assert(linkr_ble_security_init() == 0);
    if (!strcmp(argv[1], "gate")) {
        gpio_value = 0; /* GPIO1 high: inactive logical value. */
        assert(registered_auth->pairing_accept(&conn, &feat) == BT_SECURITY_ERR_PAIR_NOT_ALLOWED);
        gpio_value = -EIO;
        assert(registered_auth->pairing_accept(&conn, &feat) == BT_SECURITY_ERR_PAIR_NOT_ALLOWED);
        gpio_value = 1; /* GPIO1 low: active logical value. */
        assert(registered_auth->pairing_accept(&conn, &feat) == BT_SECURITY_ERR_SUCCESS);
        pairing_complete(&conn, true);
        gpio_value = 0;
        /* Successful bonding cannot grant blanket permission to replace keys. */
        assert(registered_auth->pairing_accept(&conn, &feat) == BT_SECURITY_ERR_PAIR_NOT_ALLOWED);
        gpio_value = 1;
        assert(registered_auth->pairing_accept(&conn, &feat) == BT_SECURITY_ERR_SUCCESS);
        assert(disconnects == 0);
    } else if (!strcmp(argv[1], "open_pairing")) {
        assert(!IS_ENABLED(CONFIG_LINKR_BLE_BRIDGE_PAIRING_GPIO_AUTH));
        gpio_ready = false;
        configured_error = -EIO;
        assert(linkr_ble_security_init() == 0);
        for (int value = -1; value <= 1; value++) {
            gpio_value = value;
            assert(registered_auth->pairing_accept(&conn, &feat) == BT_SECURITY_ERR_SUCCESS);
        }
        assert(gpio_reads == 0);
        pairing_complete(&conn, true);
        assert(disconnects == 0);
    } else if (!strcmp(argv[1], "reconnect")) {
        gpio_value = 0;
        linkr_ble_security_connected(&conn);
        assert(security_requested == BT_SECURITY_L2); /* No FORCE_PAIR. */
        assert(gpio_reads == 0);
        security_changed(&conn, BT_SECURITY_L2, BT_SECURITY_ERR_SUCCESS);
        assert(disconnects == 0);
        request_error = -EIO;
        linkr_ble_security_connected(&conn);
        assert(disconnects == 1);
        bonded_peer = false;
        security_requested = 0;
        linkr_ble_security_connected(&conn);
        assert(security_requested == 0 && gpio_reads == 0);
    } else if (!strcmp(argv[1], "fail_closed")) {
        security_changed(&conn, 1, BT_SECURITY_ERR_SUCCESS);
        security_changed(&conn, 2, BT_SECURITY_ERR_PAIR_NOT_ALLOWED);
        pairing_complete(&conn, false);
        pairing_failed(&conn, BT_SECURITY_ERR_PAIR_NOT_ALLOWED);
        assert(disconnects == 4);
        gpio_ready = false;
        assert(linkr_ble_security_init() == (IS_ENABLED(CONFIG_LINKR_BLE_BRIDGE_PAIRING_GPIO_AUTH) ? -ENODEV : 0));
        gpio_ready = true;
        configured_error = -EIO;
        assert(linkr_ble_security_init() == (IS_ENABLED(CONFIG_LINKR_BLE_BRIDGE_PAIRING_GPIO_AUTH) ? -EIO : 0));
    } else if (!strcmp(argv[1], "touch_window")) {
        /* CONFIG_LINKR_BLE_PAIRING_AUTH_TOUCH=1 with the GPIO gate still on:
         * the two sources are OR-ed, so a tap alone must authorize. */
        assert(IS_ENABLED(CONFIG_LINKR_BLE_PAIRING_AUTH_TOUCH));
        gpio_value = 0; /* GPIO inactive: the tap is the only proof. */
        assert(linkr_ble_pairing_remaining_ms() == -1);
        assert(registered_auth->pairing_accept(&conn, &feat) == BT_SECURITY_ERR_PAIR_NOT_ALLOWED);

        assert(linkr_ble_pairing_authorize(60000) == 0);
        assert(linkr_ble_pairing_remaining_ms() == 60000);
        assert(registered_auth->pairing_accept(&conn, &feat) == BT_SECURITY_ERR_SUCCESS);

        /* Single use: the window closes as soon as it has done its job. */
        pairing_complete(&conn, true);
        assert(linkr_ble_pairing_remaining_ms() == -1);
        assert(registered_auth->pairing_accept(&conn, &feat) == BT_SECURITY_ERR_PAIR_NOT_ALLOWED);
        assert(disconnects == 0);

        /* The operator can close it early, and 0 ms means "cancel". */
        assert(linkr_ble_pairing_authorize(1000) == 0);
        linkr_ble_pairing_cancel();
        assert(linkr_ble_pairing_remaining_ms() == -1);
        assert(linkr_ble_pairing_authorize(1000) == 0);
        assert(linkr_ble_pairing_authorize(0) == 0);
        assert(linkr_ble_pairing_remaining_ms() == -1);

        /* 300 s is the longest window the panel may ask for. */
        assert(linkr_ble_pairing_authorize(300000) == 0);
        assert(linkr_ble_pairing_remaining_ms() == 300000);
        assert(linkr_ble_pairing_authorize(300001) == -EINVAL);

        /* Expiry closes the window instead of leaving it stale. */
        assert(linkr_ble_pairing_authorize(500) == 0);
        uptime_ms += 499;
        assert(linkr_ble_pairing_remaining_ms() == 1);
        assert(registered_auth->pairing_accept(&conn, &feat) == BT_SECURITY_ERR_SUCCESS);
        uptime_ms += 2;
        assert(linkr_ble_pairing_remaining_ms() == -1);
        assert(registered_auth->pairing_accept(&conn, &feat) == BT_SECURITY_ERR_PAIR_NOT_ALLOWED);

        /* A deadline that computes to 0 encodes "closed", so it is nudged. */
        uptime_ms = 0xFFFFFFFEu;
        assert(linkr_ble_pairing_authorize(2) == 0);
        assert(linkr_ble_pairing_remaining_ms() == 1);

        /* The 32-bit counter wraps after ~49 days; the signed difference in
         * pairing_window_open() keeps the window correct across the rollover. */
        uptime_ms = 0xFFFFFF00u;
        assert(linkr_ble_pairing_authorize(1000) == 0);
        uptime_ms = 0x10u; /* 0x110 ms of wall time later, wrapped */
        assert(linkr_ble_pairing_remaining_ms() == 1000 - 0x110);
        assert(registered_auth->pairing_accept(&conn, &feat) == BT_SECURITY_ERR_SUCCESS);
    } else if (!strcmp(argv[1], "touch_only")) {
        /* The panel build: GPIO authorization is off, so the tap is the only
         * gate and the GPIO is never even read. */
        assert(IS_ENABLED(CONFIG_LINKR_BLE_PAIRING_AUTH_TOUCH));
        assert(!IS_ENABLED(CONFIG_LINKR_BLE_BRIDGE_PAIRING_GPIO_AUTH));
        gpio_ready = false;
        configured_error = -EIO;
        assert(linkr_ble_security_init() == 0);
        assert(registered_auth->pairing_accept(&conn, &feat) == BT_SECURITY_ERR_PAIR_NOT_ALLOWED);
        assert(linkr_ble_pairing_authorize(60000) == 0);
        assert(registered_auth->pairing_accept(&conn, &feat) == BT_SECURITY_ERR_SUCCESS);
        /* An unbonded host waits for the tap: no security request is made. */
        bonded_peer = false;
        linkr_ble_security_connected(&conn);
        assert(security_requested == 0);
        assert(gpio_reads == 0);
    } else if (!strcmp(argv[1], "touch_absent")) {
        /* Compiled without the window: the panel API reports "unsupported"
         * rather than silently authorizing or refusing. */
        assert(!IS_ENABLED(CONFIG_LINKR_BLE_PAIRING_AUTH_TOUCH));
        assert(linkr_ble_pairing_authorize(60000) == -ENOTSUP);
        assert(linkr_ble_pairing_remaining_ms() == -1);
        linkr_ble_pairing_cancel();
        assert(linkr_ble_pairing_remaining_ms() == -1);
    } else return 1;
    return 0;
}
