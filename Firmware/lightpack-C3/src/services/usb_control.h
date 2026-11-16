#if !defined(SERVICE_USB_CONTROL)
#define SERVICE_USB_CONTROL

#include "shared/dependencies.h"
#include <mbedtls/base64.h>
#include <esp_ota_ops.h>

//? ---------------------- USB CONTROL SERVICE ----------------------
//? A line protocol on the C3's USB Serial/JTAG, which is the same CDC port the
//? console uses (ARDUINO_USB_CDC_ON_BOOT=1 with ARDUINO_USB_MODE=1 makes
//? `Serial` HWCDC, not a UART). It exists because the ceramic antenna cannot be
//? relied on for configuration: this path configures the lamp with the radio
//? switched off, and never needs a network at all.
//?
//? WHY A PREFIX AND NOT A SECOND PORT
//?   The console is not quiet - boot report, WiFi events, [MODE], [ADC] - so the
//?   protocol does not assume it owns the stream. Every protocol line starts
//?   with '@' and ends with '\n'; nothing else in the firmware starts a line
//?   with '@'. A host ignores the rest and shows it as a device log. Each line
//?   is written by ONE Serial.write(), and HWCDC::write() holds its tx mutex for
//?   the whole call, so another task's print can land before or after a frame
//?   but never inside it.
//?
//? WHY THE CONFIG WRITER IS SHARED WITH THE WEB PAGE
//?   webpage_config_apply_arg() is the /api/config handler's own clamp body,
//?   called once per field. A SET therefore enforces exactly the limits the
//?   browser does, and a new limit cannot be added to one transport and
//?   forgotten on the other. SET is TRANSIENT: it changes RAM and the strip but
//?   writes no flash. COMMIT is the only USB path that persists. That mirrors
//?   the page's one-write-per-slider-release without making a drag pay for NVS.
//!
//! Include order: dependencies.h puts this AFTER services/presets.h, because it
//? uses the preset field table, the preset store and webpage_state_json().
//! main.cpp must size the CDC buffers BEFORE Serial.begin(); see usb_control_begin().

#define USB_CONTROL_PROTO_VERSION 1
#define USB_CONTROL_MAX_LINE 4096
#define USB_CONTROL_MAX_FIELDS 32
#define USB_CONTROL_STREAM_MAX_HZ 50
#define USB_CONTROL_RX_BYTES_PER_LOOP 4096
#define USB_CONTROL_LINES_PER_LOOP 8
#define USB_UPDATE_CHUNK 2048

//* ---------------------- STATE ----------------------

char usb_rx_line[USB_CONTROL_MAX_LINE + 1];
uint16_t usb_rx_len = 0;
bool usb_rx_overflow = false;

uint8_t usb_stream_hz = 0;
uint32_t usb_tick_period_ms = 0;
uint32_t usb_next_tick_ms = 0;

uint32_t usb_loop_count = 0;
uint32_t usb_loop_rate_last = 0;
uint32_t usb_loop_rate_ms = 0;
//? usb_loop_rate itself lives in shared/variables.h so webpage.h can export it.
uint32_t usb_rail_mv = 0;
uint32_t usb_slow_ms = 0;

uint8_t usb_last_mode = 0;
uint32_t usb_last_impact_count = 0;
uint32_t usb_last_button_events[BUTTON_INPUT_COUNT] = {0};
bool usb_was_connected = false;

//* USB firmware update. The image goes into the inactive OTA slot through
//* Update.h, exactly like the web upload, so NVS and the running image are
//* never touched until Update.end() marks the new slot bootable.
bool usb_update_active = false;
size_t usb_update_total = 0;
size_t usb_update_received = 0;
uint32_t usb_update_reboot_at = 0;
uint8_t usb_update_buf[USB_UPDATE_CHUNK];

//* ---------------------- OUTPUT HELPERS ----------------------

bool usb_connected()
{
    return (bool)Serial;
}

void usb_write_raw(const char *data, size_t length)
{
    if (length == 0 || !usb_connected())
        return;
    Serial.write((const uint8_t *)data, length);
}

//? One write per line. Do not split a frame across calls: the CDC tx mutex is
//! what makes a frame atomic against the console, and it is held per call.
void usb_write_line(const String &line)
{
    usb_write_raw(line.c_str(), line.length());
}

void usb_json_begin(String &json, const char *type, int seq)
{
    //! The '@' is the whole reason a frame can be told apart from the console
    //! text on the same port. It goes on in one write, with the JSON, so a log
    //? line cannot land between the prefix and the object.
    json += '@';
    json += F("{\"t\":\"");
    json += type;
    json += F("\",\"v\":");
    json += String(USB_CONTROL_PROTO_VERSION);
    if (seq >= 0)
    {
        json += F(",\"seq\":");
        json += String(seq);
    }
}

void usb_send_error(const char *command, int seq, const char *message)
{
    String json;
    json.reserve(220);
    usb_json_begin(json, "ack", seq);
    json += F(",\"ok\":false,\"cmd\":\"");
    json += webpage_json_escape(command);
    json += F("\",\"err\":\"");
    json += webpage_json_escape(message);
    json += F("\"}\n");
    usb_write_line(json);
}

void usb_begin_ack(String &json, const char *command, int seq)
{
    usb_json_begin(json, "ack", seq);
    json += F(",\"ok\":true,\"cmd\":\"");
    json += webpage_json_escape(command);
    json += F("\",\"dirty\":");
    json += config_dirty ? F("true") : F("false");
}

void usb_send_ack(const char *command, int seq)
{
    String json;
    json.reserve(180);
    usb_begin_ack(json, command, seq);
    json += F("}\n");
    usb_write_line(json);
}

//* ---------------------- SMALL PARSERS ----------------------

bool usb_ieq(const char *a, const char *b)
{
    if (a == nullptr || b == nullptr)
        return false;
    while (*a != '\0' && *b != '\0')
    {
        char ca = *a++;
        char cb = *b++;
        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb - 'A' + 'a');
        if (ca != cb)
            return false;
    }
    return *a == '\0' && *b == '\0';
}

//? Accepts the numeric form every /api/config value uses: an optional sign,
//? digits, an optional fraction and an optional exponent. Rejecting "nan",
//? "inf" and trailing junk here is what stops a typo from quietly becoming 0
//* through String::toFloat().
bool usb_is_number(const char *value)
{
    if (value == nullptr)
        return false;

    const char *p = value;
    if (*p == '+' || *p == '-')
        p++;

    bool digits = false;
    bool dot = false;
    bool exponent = false;

    for (; *p != '\0'; p++)
    {
        if (*p >= '0' && *p <= '9')
        {
            digits = true;
        }
        else if (*p == '.' && !dot && !exponent)
        {
            dot = true;
        }
        else if ((*p == 'e' || *p == 'E') && digits && !exponent)
        {
            exponent = true;
            digits = false;
            if (p[1] == '+' || p[1] == '-')
                p++;
        }
        else
        {
            return false;
        }
    }

    return digits;
}

//? `seq` is a host-side request id, echoed in the ack. It is only accepted on
//? SET, where a drag can leave more than one request in flight; the preset and
//? state commands are user-paced and one at a time, so they do not need it.
int usb_find_seq(const char *args)
{
    if (args == nullptr)
        return -1;

    const char *p = args;
    while ((p = strstr(p, "seq=")) != nullptr)
    {
        if (p == args || p[-1] == ' ' || p[-1] == '&')
            return atoi(p + 4);
        p += 4;
    }
    return -1;
}

bool usb_field_known(const char *key)
{
    //* The power budget is a /api/config field but deliberately not a preset
    //? field (device property), so it is not in PRESET_FIELDS and needs its own
    //* check here.
    if (strcmp(key, "budget") == 0)
        return true;

    for (uint8_t i = 0; i < PRESET_FIELD_COUNT; i++)
        if (strcmp(PRESET_FIELDS[i].name, key) == 0)
            return true;
    return false;
}

//* Set by usb_handle_set() when the budget moved, so the ACK can report it even
//? though it is not one of the preset fields the diff walks.
bool usb_budget_changed = false;

//* ---------------------- CONFIG DIFF ----------------------
//? A SET answers with the fields that actually moved, not with the whole state:
//? the state is ~3 KB and building it opens NVS for the saved SSID, which is
//? far too much to do for every slider tick. The snapshot is the same packed
//? preset struct the preset store uses, so there is one field table to walk.

bool usb_field_differs(const struct_preset &before, const struct_preset &after,
                       const struct_presetField &field)
{
    const uint8_t *a = (const uint8_t *)&before + field.offset;
    const uint8_t *b = (const uint8_t *)&after + field.offset;

    if (field.type == PRESET_U8)
        return *(const uint8_t *)a != *(const uint8_t *)b;
    if (field.type == PRESET_U16)
        return *(const uint16_t *)a != *(const uint16_t *)b;
    return *(const float *)a != *(const float *)b;
}

void usb_append_field_value(String &json, const struct_preset &preset,
                            const struct_presetField &field)
{
    const uint8_t *base = (const uint8_t *)&preset + field.offset;

    if (field.type == PRESET_U8)
        json += String(*(const uint8_t *)base);
    else if (field.type == PRESET_U16)
        json += String(*(const uint16_t *)base);
    else
        json += String(*(const float *)base, 3);
}

void usb_send_set_ack(int seq, uint8_t count, const struct_preset &before,
                      const struct_preset &after)
{
    String json;
    json.reserve(420);
    usb_json_begin(json, "ack", seq);
    json += F(",\"ok\":true,\"cmd\":\"set\",\"n\":");
    json += String(count);
    json += F(",\"dirty\":");
    json += config_dirty ? F("true") : F("false");
    json += F(",\"changed\":{");

    bool first = true;
    for (uint8_t i = 0; i < PRESET_FIELD_COUNT; i++)
    {
        const struct_presetField &field = PRESET_FIELDS[i];
        if (!usb_field_differs(before, after, field))
            continue;

        if (!first)
            json += ',';
        first = false;
        json += '"';
        json += field.name;
        json += F("\":");
        usb_append_field_value(json, after, field);
    }

    if (usb_budget_changed)
    {
        if (!first)
            json += ',';
        first = false;
        json += F("\"budget\":");
        json += String(power_budget_ma);
    }

    json += F("}}\n");
    usb_write_line(json);
}

//* ---------------------- COMMANDS ----------------------

void usb_send_pong(int seq)
{
    String json;
    json.reserve(320);
    usb_json_begin(json, "pong", seq);
    json += F(",\"proto\":");
    json += String(USB_CONTROL_PROTO_VERSION);
    json += F(",\"fw\":\"");
    json += LIGHTPACK_FW_VERSION;
    json += F("\",\"build\":\"");
    json += __DATE__ " " __TIME__;
    json += F("\",\"chip\":\"");
    json += ESP.getChipModel();
    json += F("\",\"cpu_mhz\":");
    json += String(ESP.getCpuFreqMHz());
    json += F(",\"mac\":\"");
    json += WiFi.macAddress();
    json += F("\",\"uptime_ms\":");
    json += String(millis());
    json += F("}\n");
    usb_write_line(json);
}

void usb_send_state(int seq)
{
    String json;
    json.reserve(3200);
    usb_json_begin(json, "state", seq);
    json += F(",\"state\":");
    webpage_state_json(json);
    json += F("}\n");
    usb_write_line(json);
}

void usb_send_modes(int seq)
{
    String json;
    json.reserve(420);
    usb_json_begin(json, "modes", seq);
    json += F(",\"modes\":[");

    bool first = true;
    for (uint8_t i = 0; i < led_mode_count(); i++)
    {
        const struct_ledMode *entry = led_mode_at(i);
        if (entry == nullptr)
            continue;

        if (!first)
            json += ',';
        first = false;
        json += F("{\"id\":");
        json += String(entry->id);
        json += F(",\"key\":\"");
        json += webpage_json_escape(entry->key);
        json += F("\",\"label\":\"");
        json += webpage_json_escape(entry->label);
        json += F("\"}");
    }

    json += F("]}\n");
    usb_write_line(json);
}

void usb_send_presets(int seq)
{
    String slots;
    preset_slots_json(slots);

    String json;
    json.reserve(slots.length() + 64);
    usb_json_begin(json, "presets", seq);
    json += F(",\"slots\":");
    json += slots;
    json += F("}\n");
    usb_write_line(json);
}

bool usb_send_preset(int seq, int slot)
{
    String body;
    if (!preset_slot_json(body, slot))
        return false;

    String json;
    json.reserve(body.length() + 64);
    usb_json_begin(json, "preset", seq);
    json += F(",\"preset\":");
    json += body;
    json += F("}\n");
    usb_write_line(json);
    return true;
}

void usb_handle_set(char *args)
{
    const int seq = usb_find_seq(args);
    const char *keys[USB_CONTROL_MAX_FIELDS];
    const char *values[USB_CONTROL_MAX_FIELDS];
    uint8_t count = 0;

    //? First pass: validate every pair before touching a single setting. A
    //! batch that names one bad field must apply none of them, or a host that
    //* mistyped one key would leave the device half-configured and not know it.
    char *pair = args;
    while (pair != nullptr && *pair != '\0')
    {
        char *amp = strchr(pair, '&');
        if (amp != nullptr)
            *amp = '\0';

        char *eq = strchr(pair, '=');
        if (eq == nullptr)
        {
            usb_send_error("set", seq, "expected key=value");
            return;
        }
        *eq = '\0';

        const char *key = pair;
        const char *value = eq + 1;

        if (usb_ieq(key, "seq"))
        {
            // Already parsed; not a configuration field.
        }
        else if (!usb_field_known(key))
        {
            char message[96];
            snprintf(message, sizeof(message), "unknown field '%s'", key);
            usb_send_error("set", seq, message);
            return;
        }
        else if (!usb_is_number(value))
        {
            char message[96];
            snprintf(message, sizeof(message), "value for '%s' is not a number", key);
            usb_send_error("set", seq, message);
            return;
        }
        else if (usb_ieq(key, "mode") && find_led_mode((uint8_t)atoi(value)) == nullptr)
        {
            usb_send_error("set", seq, "unknown mode id");
            return;
        }
        else if (count >= USB_CONTROL_MAX_FIELDS)
        {
            usb_send_error("set", seq, "too many fields");
            return;
        }
        else
        {
            keys[count] = key;
            values[count] = value;
            count++;
        }

        pair = (amp != nullptr) ? amp + 1 : nullptr;
    }

    if (count == 0)
    {
        usb_send_error("set", seq, "no fields");
        return;
    }

    struct_preset before = {};
    struct_preset after = {};
    preset_capture(before);
    const uint16_t budget_before = power_budget_ma;

    for (uint8_t i = 0; i < count; i++)
        webpage_config_apply_arg(keys[i], String(values[i]));

    //! Same order and same reasoning as the web handler: the strip length is
    //! sanitised first because every LED-measured tunable is bounded by it, and
    //* the detector's scale has to be legal before the flame values are read.
    strip_sanitize();
    step_sanitize();
    whip_sanitize();
    flame_sanitize();
    rainbow_sanitize();
    fade_sanitize();
    preset_capture(after);
    usb_budget_changed = (budget_before != power_budget_ma);

    bool changed = false;
    for (uint8_t i = 0; i < PRESET_FIELD_COUNT; i++)
    {
        if (usb_field_differs(before, after, PRESET_FIELDS[i]))
        {
            changed = true;
            break;
        }
    }
    if (changed || usb_budget_changed)
        config_dirty = true;

    usb_send_set_ack(seq, count, before, after);
}

void usb_handle_get(char *args)
{
    (void)args;
    usb_send_state(-1);
}

void usb_handle_modes(char *args)
{
    (void)args;
    usb_send_modes(-1);
}

void usb_handle_stream(char *args)
{
    int hz = atoi(args);
    if (hz < 0)
        hz = 0;
    if (hz > USB_CONTROL_STREAM_MAX_HZ)
        hz = USB_CONTROL_STREAM_MAX_HZ;

    usb_stream_hz = (uint8_t)hz;
    usb_tick_period_ms = (hz > 0) ? (1000U / (uint32_t)hz) : 0;
    usb_next_tick_ms = millis();

    String json;
    json.reserve(180);
    usb_begin_ack(json, "stream", -1);
    json += F(",\"hz\":");
    json += String(usb_stream_hz);
    json += F("}\n");
    usb_write_line(json);
}

void usb_handle_impact(char *args)
{
    int magnitude = atoi(args);
    if (magnitude < 0)
        magnitude = 0;
    if (magnitude > 255)
        magnitude = 255;

    //? The detector's latch, not a second event queue: if a real impact of the
    //? same frame is stronger, it wins, exactly as it does between two modes.
    if ((uint8_t)magnitude > impact_pending)
        impact_pending = (uint8_t)magnitude;

    String json;
    json.reserve(200);
    usb_begin_ack(json, "impact", -1);
    json += F(",\"mag\":");
    json += String((unsigned)magnitude);
    json += F(",\"mode\":");
    json += String(mode);
    json += F("}\n");
    usb_write_line(json);
}

void usb_handle_stress(char *args)
{
    //* The stress test measures the strip, so the radio has to be quiet. Refusing
    //? here is the same rule the test itself enforces; this one can say why.
    if (net_state != NET_OFFLINE)
    {
        usb_send_error("stress", -1, "switch the radio off before a stress test");
        return;
    }

    char wants[16];
    uint8_t length = 0;
    const char *p = args;
    while (*p == ' ')
        p++;
    while (*p != '\0' && *p != ' ' && length < sizeof(wants) - 1)
        wants[length++] = *p++;
    wants[length] = '\0';

    //* Colour is a 3-bit mask so the console can mix channels: R=1, G=2, B=4.
    //? The old colour names stay accepted for hand-written lines.
    uint8_t color;
    const int mask_arg = atoi(wants);
    if (mask_arg >= 1 && mask_arg <= 7)
        color = (uint8_t)mask_arg;
    else if (usb_ieq(wants, "white"))
        color = 7;
    else if (usb_ieq(wants, "red"))
        color = 1;
    else if (usb_ieq(wants, "green"))
        color = 2;
    else if (usb_ieq(wants, "blue"))
        color = 4;
    else
    {
        usb_send_error("stress", -1, "expected a mask 1..7 (R=1 G=2 B=4), or white/red/green/blue");
        return;
    }

    //* Optional test-only ceiling and step: `@STRESS white 4000 500`.
    //? Absent values keep the normal 2 A / 500 ms defaults. Above 2 A is
    //* lab-only; the test is the exception, never the sustained budget.
    uint16_t ceiling_ma = (uint16_t)STRESS_CEILING_DEFAULT_MA;
    uint16_t step_ms = (uint16_t)STRESS_STEP_FAST_MS;

    while (*p == ' ')
        p++;
    if (*p != '\0')
    {
        ceiling_ma = (uint16_t)atoi(p);
        while (*p != '\0' && *p != ' ')
            p++;
    }
    while (*p == ' ')
        p++;
    if (*p != '\0')
        step_ms = (uint16_t)atoi(p);

    //* Optional lab-only rail floor: 0 disables the 3.0 V collapse guard so the
    //? test can find the absolute limit. Absent means the normal 3000 mV.
    while (*p != '\0' && *p != ' ')
        p++;
    while (*p == ' ')
        p++;
    uint16_t rail_floor_mv = (uint16_t)STRESS_RAIL_FLOOR_DEFAULT_MV;
    if (*p != '\0')
        rail_floor_mv = (uint16_t)atoi(p);
    if (rail_floor_mv > STRESS_RAIL_FLOOR_MAX_MV)
        rail_floor_mv = (uint16_t)STRESS_RAIL_FLOOR_MAX_MV;

    if (ceiling_ma < STRESS_CEILING_MIN_MA)
        ceiling_ma = (uint16_t)STRESS_CEILING_MIN_MA;
    if (ceiling_ma > STRESS_CEILING_MAX_MA)
        ceiling_ma = (uint16_t)STRESS_CEILING_MAX_MA;
    if (step_ms != STRESS_STEP_FAST_MS && step_ms != STRESS_STEP_PRECISE_MS)
        step_ms = (uint16_t)STRESS_STEP_FAST_MS;

    if (!stress_test_request(color, ceiling_ma, step_ms, rail_floor_mv))
    {
        usb_send_error("stress", -1, "a stress test is already pending or running");
        return;
    }

    String json;
    json.reserve(300);
    usb_begin_ack(json, "stress", -1);
    json += F(",\"color\":\"");
    json += wants;
    json += F("\",\"mask\":");
    json += String((unsigned)color);
    json += F(",\"ceiling_ma\":");
    json += String((unsigned)ceiling_ma);
    json += F(",\"step_ms\":");
    json += String((unsigned)step_ms);
    json += F(",\"rail_floor_mv\":");
    json += String((unsigned)rail_floor_mv);
    json += F(",\"started\":true}\n");
    usb_write_line(json);
}

void usb_handle_sense(char *args)
{
    if (net_state != NET_OFFLINE)
    {
        usb_send_error("sense", -1, "switch the radio off before a sense run");
        return;
    }

    const int count = atoi(args);
    if (count < SENSE_COUNT_MIN || count > SENSE_COUNT_MAX)
    {
        usb_send_error("sense", -1, "count must be 1..100 LEDs");
        return;
    }

    const char *p = args;
    while (*p != '\0' && *p != ' ')
        p++;
    while (*p == ' ')
        p++;

    uint16_t burst_ms = (uint16_t)SENSE_BURST_FAST_MS;
    if (*p != '\0')
        burst_ms = (uint16_t)atoi(p);
    if (burst_ms != SENSE_BURST_FAST_MS && burst_ms != SENSE_BURST_PRECISE_MS)
        burst_ms = (uint16_t)SENSE_BURST_FAST_MS;

    if (!sense_test_request((uint8_t)count, burst_ms))
    {
        usb_send_error("sense", -1, "a sense run is already pending or running");
        return;
    }

    String json;
    json.reserve(200);
    usb_begin_ack(json, "sense", -1);
    json += F(",\"count\":");
    json += String((unsigned)count);
    json += F(",\"burst_ms\":");
    json += String((unsigned)burst_ms);
    json += F(",\"started\":true}\n");
    usb_write_line(json);
}

void usb_handle_safe(char *args)
{
    if (!usb_ieq(args, "off"))
    {
        usb_send_error("safe", -1, "expected OFF");
        return;
    }

    safe_mode_clear();

    String json;
    json.reserve(160);
    usb_begin_ack(json, "safe", -1);
    json += F(",\"safe_boot\":false}\n");
    usb_write_line(json);
}

void usb_handle_radio(char *args)
{
    if (usb_portal_window)
    {
        usb_send_error("radio", -1, "the config hotspot is open - close it before switching the radio");
        return;
    }

    char wants[16];
    uint8_t length = 0;
    const char *p = args;
    while (*p == ' ')
        p++;
    while (*p != '\0' && *p != ' ' && length < sizeof(wants) - 1)
        wants[length++] = *p++;
    wants[length] = '\0';

    const bool is_offline = (net_state == NET_OFFLINE);
    bool want_offline;

    if (usb_ieq(wants, "on"))
        want_offline = false;
    else if (usb_ieq(wants, "off"))
        want_offline = true;
    else if (usb_ieq(wants, "toggle"))
        want_offline = !is_offline;
    else
    {
        usb_send_error("radio", -1, "expected on, off or toggle");
        return;
    }

    //? Only flip when the request disagrees with the live state. Calling the
    //! toggle unconditionally would invert a radio that is already where the
    //* caller wants it, which is the classic way a UI checkbox desynchronises.
    if (want_offline != is_offline)
        wifi_toggle_offline();

    String json;
    json.reserve(240);
    usb_begin_ack(json, "radio", -1);
    json += F(",\"offline\":");
    json += (net_state == NET_OFFLINE) ? F("true") : F("false");
    json += F(",\"net\":\"");
    json += wifi_state_name();
    json += F("\",\"rssi\":");
    json += String(WiFi.RSSI());
    json += F("}\n");
    usb_write_line(json);
}

void usb_handle_preset(char *args)
{
    char *p = args;
    while (*p == ' ')
        p++;

    char *rest = strchr(p, ' ');
    if (rest != nullptr)
    {
        *rest = '\0';
        rest++;
        while (*rest == ' ')
            rest++;
    }
    else
    {
        rest = p + strlen(p);
    }

    if (usb_ieq(p, "LIST"))
    {
        usb_send_presets(-1);
        return;
    }

    if (usb_ieq(p, "GET") || usb_ieq(p, "APPLY") || usb_ieq(p, "CLEAR") || usb_ieq(p, "SAVE"))
    {
        const int slot = atoi(rest);
        if (slot < 0 || slot >= PRESET_SLOTS)
        {
            usb_send_error("preset", -1, "slot out of range");
            return;
        }

        if (usb_ieq(p, "GET"))
        {
            if (!usb_send_preset(-1, slot))
                usb_send_error("preset", -1, "nothing stored in that slot");
            return;
        }

        if (usb_ieq(p, "CLEAR"))
        {
            preset_clear((uint8_t)slot);
            usb_send_presets(-1);
            return;
        }

        if (usb_ieq(p, "APPLY"))
        {
            if (!preset_apply((uint8_t)slot))
            {
                const bool incompatible = preset_slot_state((uint8_t)slot) == PRESET_INCOMPATIBLE;
                usb_send_error("preset", -1,
                               incompatible
                                   ? "slot holds a preset from another firmware version"
                                   : "nothing to apply");
                return;
            }
            //? preset_apply() saves the live configuration, so RAM and NVS agree.
            config_dirty = false;
            usb_send_state(-1);
            return;
        }

        // SAVE: the name is the remainder of the line, after "name=".
        const char *name = strstr(rest, "name=");
        if (name != nullptr)
            name += 5;
        if (!preset_write((uint8_t)slot, name))
        {
            usb_send_error("preset", -1, "write failed");
            return;
        }
        usb_send_presets(-1);
        return;
    }

    usb_send_error("preset", -1, "expected LIST, GET, SAVE, APPLY or CLEAR");
}

void usb_handle_reboot()
{
    usb_send_ack("reboot", -1);
    //? Let the ack leave the CDC ring before the reset tears the port down.
    //! HWCDC::flush() waits for the host to drain it, bounded by the write
    //* timeout set in main.cpp, so an absent host cannot hang the reboot.
    Serial.flush();
    delay(100);
    ESP.restart();
}

//* ---------------------- USB FIRMWARE UPDATE ----------------------
//? The image is written to the inactive OTA slot, exactly like the web page's
//? upload. The host sends BEGIN <size>, then DATA <offset> <base64> chunks and
//? END; each DATA is acknowledged so the host can keep a window of one and
//? update its progress from the board's own received count.
//!
//? Offset, not sequence: a retried chunk whose ack was lost is recognized as
//? already received and acknowledged without being written twice.

void usb_send_update_state(const char *state)
{
    String json;
    json.reserve(180);
    usb_begin_ack(json, "update", -1);
    json += F(",\"state\":\"");
    json += state;
    json += F("\",\"received\":");
    json += String((unsigned long)usb_update_received);
    json += F(",\"total\":");
    json += String((unsigned long)usb_update_total);
    json += F("}\n");
    usb_write_line(json);
}

void usb_handle_update(char *args)
{
    if (args == nullptr || *args == '\0')
    {
        usb_send_error("update", -1, "expected BEGIN, DATA, END or ABORT");
        return;
    }

    char *rest = strchr(args, ' ');
    if (rest != nullptr)
    {
        *rest = '\0';
        rest++;
        while (*rest == ' ')
            rest++;
    }
    else
    {
        rest = args + strlen(args);
    }

    if (usb_ieq(args, "BEGIN"))
    {
        if (usb_update_active || Update.isRunning())
        {
            usb_send_error("update", -1, "an update is already running");
            return;
        }

        char *end = nullptr;
        const unsigned long size = strtoul(rest, &end, 10);
        if (size == 0 || end == rest || *end != '\0')
        {
            usb_send_error("update", -1, "expected BEGIN <image bytes>");
            Serial.println("[UPDATE] bad BEGIN size");
            ota_fail_status();
            return;
        }

        usb_update_total = (size_t)size;
        usb_update_received = 0;

        //! A CLI upload writes app0 but does not necessarily update otadata.
        //! In that state esp_ota_get_next_update_partition() can return the
        //! RUNNING slot, and Update.write would overwrite the code executing
        //* this function. Make the running slot the boot slot first, then
        //* re-query so the inactive one is chosen.
        const esp_partition_t *running = esp_ota_get_running_partition();
        const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
        if (target == running)
        {
            if (esp_ota_set_boot_partition(running) != ESP_OK)
            {
                usb_send_error("update", -1, "could not select the inactive OTA slot");
                Serial.println("[UPDATE] could not set the running slot as boot");
                ota_fail_status();
                return;
            }
            target = esp_ota_get_next_update_partition(nullptr);
        }
        if (target == nullptr || target == running)
        {
            usb_send_error("update", -1, "no inactive OTA slot");
            Serial.println("[UPDATE] no inactive OTA slot");
            ota_fail_status();
            return;
        }
        if (usb_update_total > target->size)
        {
            usb_send_error("update", -1, "image does not fit the OTA slot");
            Serial.printf("[UPDATE] image %u > slot %u bytes\n",
                          (unsigned)usb_update_total, (unsigned)target->size);
            ota_fail_status();
            return;
        }

        if (!Update.begin(usb_update_total, U_FLASH))
        {
            usb_send_error("update", -1, Update.errorString());
            Serial.printf("[UPDATE] begin failed: %s\n", Update.errorString());
            ota_fail_status();
            return;
        }

        //! Flash writes draw current. With 250 LEDs lit the rail sits near
        //! 4.2 V and the C3 can brown out; blank the strip before the first
        //* sector erase and keep every mode from repainting it until END.
        strip_update_hold = true;
        strip.setBrightness(0);
        strip.clear();
        strip.show();
        //* The OTA heartbeat in update_onboard_LED starts from this timestamp.
        ota_last_chunk_ms = millis();

        usb_update_active = true;
        usb_update_reboot_at = 0;
        Serial.printf("[UPDATE] begin %u bytes -> %s 0x%08X slot %u KB, heap %u\n",
                      (unsigned)usb_update_total, target->label,
                      (unsigned)target->address, (unsigned)(target->size / 1024),
                      (unsigned)ESP.getFreeHeap());
        usb_send_update_state("ready");
        return;
    }

    if (usb_ieq(args, "DATA"))
    {
        if (!usb_update_active)
        {
            usb_send_error("update", -1, "no update in progress");
            return;
        }

        char *b64 = strchr(rest, ' ');
        if (b64 == nullptr)
        {
            usb_send_error("update", -1, "expected DATA <offset> <base64>");
            return;
        }
        *b64 = '\0';
        b64++;
        while (*b64 == ' ')
            b64++;

        char *end = nullptr;
        const unsigned long offset = strtoul(rest, &end, 10);
        if (end == rest || *end != '\0')
        {
            usb_send_error("update", -1, "bad chunk offset");
            return;
        }

        size_t decoded = 0;
        const int rc = mbedtls_base64_decode(usb_update_buf, sizeof(usb_update_buf),
                                             &decoded, (const unsigned char *)b64,
                                             strlen(b64));
        if (rc != 0 || decoded == 0)
        {
            usb_send_error("update", -1, "bad base64 chunk");
            return;
        }

        // A lost ack makes the host resend the same offset. The bytes already
        // landed; acknowledge them again instead of writing a second copy.
        if (offset < usb_update_received)
        {
            usb_send_update_state("data");
            return;
        }
        if (offset > usb_update_received)
        {
            usb_send_error("update", -1, "chunk gap");
            return;
        }
        if (offset + decoded > usb_update_total)
        {
            usb_send_error("update", -1, "chunk exceeds image size");
            return;
        }

        const size_t written = Update.write(usb_update_buf, decoded);
        if (written != decoded)
        {
            const char *message = Update.errorString();
            Update.abort();
            usb_update_active = false;
            Serial.printf("[UPDATE] write failed at %u: %s\n",
                          (unsigned)offset, message);
            usb_send_error("update", -1, message);
            return;
        }

        usb_update_received += decoded;
        ota_last_chunk_ms = millis();
        if (offset == 0 || (usb_update_received / USB_UPDATE_CHUNK) % 16 == 0)
            Serial.printf("[UPDATE] data %u/%u\n",
                          (unsigned)usb_update_received, (unsigned)usb_update_total);
        usb_send_update_state("data");
        return;
    }

    if (usb_ieq(args, "END"))
    {
        if (!usb_update_active)
        {
            usb_send_error("update", -1, "no update in progress");
            return;
        }
        if (usb_update_received != usb_update_total)
        {
            Serial.printf("[UPDATE] end incomplete %u/%u\n",
                          (unsigned)usb_update_received, (unsigned)usb_update_total);
            usb_send_error("update", -1, "image incomplete");
            return;
        }
        if (!Update.end(true))
        {
            const char *message = Update.errorString();
            Update.abort();
            usb_update_active = false;
            Serial.printf("[UPDATE] end failed: %s\n", message);
            usb_send_error("update", -1, message);
            return;
        }

        usb_update_active = false;
        usb_update_reboot_at = millis() + 700;
        Serial.printf("[UPDATE] end ok %u bytes, restarting\n",
                      (unsigned)usb_update_received);
        usb_send_update_state("complete");
        return;
    }

    if (usb_ieq(args, "ABORT"))
    {
        if (usb_update_active)
            Update.abort();
        usb_update_active = false;
        usb_update_reboot_at = 0;
        strip_update_hold = false;
        strip_brightness_dirty = true;
        Serial.printf("[UPDATE] abort at %u/%u\n",
                      (unsigned)usb_update_received, (unsigned)usb_update_total);
        //? Answer before the blink so the console is not held for 900 ms.
        usb_send_update_state("aborted");
        ota_fail_status();
        return;
    }

    usb_send_error("update", -1, "expected BEGIN, DATA, END or ABORT");
}

void usb_handle_line(char *line)
{
    if (*line == '\0')
    {
        usb_send_error(nullptr, -1, "empty command");
        return;
    }

    char *args = strchr(line, ' ');
    if (args != nullptr)
    {
        *args = '\0';
        args++;
        while (*args == ' ')
            args++;
    }
    else
    {
        args = line + strlen(line);
    }

    if (usb_ieq(line, "PING"))
    {
        usb_send_pong(-1);
        return;
    }
    if (usb_ieq(line, "GET"))
    {
        usb_handle_get(args);
        return;
    }
    if (usb_ieq(line, "MODES"))
    {
        usb_handle_modes(args);
        return;
    }
    if (usb_ieq(line, "SET"))
    {
        usb_handle_set(args);
        return;
    }
    if (usb_ieq(line, "STREAM"))
    {
        usb_handle_stream(args);
        return;
    }
    if (usb_ieq(line, "PRESET"))
    {
        usb_handle_preset(args);
        return;
    }
    if (usb_ieq(line, "IMPACT"))
    {
        usb_handle_impact(args);
        return;
    }
    if (usb_ieq(line, "RADIO"))
    {
        usb_handle_radio(args);
        return;
    }
    if (usb_ieq(line, "STRESS"))
    {
        usb_handle_stress(args);
        return;
    }
    if (usb_ieq(line, "SENSE"))
    {
        usb_handle_sense(args);
        return;
    }
    if (usb_ieq(line, "SAFE"))
    {
        usb_handle_safe(args);
        return;
    }
    if (usb_ieq(line, "COMMIT"))
    {
        webpage_config_save();
        usb_send_ack("commit", -1);
        return;
    }
    if (usb_ieq(line, "RELOAD"))
    {
        webpage_config_load();
        usb_send_ack("reload", -1);
        usb_send_state(-1);
        return;
    }
    if (usb_ieq(line, "REBOOT"))
    {
        usb_handle_reboot();
        return;
    }
    if (usb_ieq(line, "UPDATE"))
    {
        usb_handle_update(args);
        return;
    }

    usb_send_error(line, -1, "unknown command");
}

//* ---------------------- INPUT ----------------------

//? Byte-at-a-time because the line buffer, the overflow flag and the command
//? are one state machine; a chunk parser would only move the same logic around.
//? Returns true when a line completed, so usb_control_loop() can bound how many
//* commands one pass of loop() executes.
bool usb_feed_byte(uint8_t byte)
{
    if (byte == '\r')
        return false;

    if (byte == '\n')
    {
        if (usb_rx_overflow)
        {
            usb_rx_overflow = false;
            usb_rx_len = 0;
            usb_send_error(nullptr, -1, "line too long");
            return true;
        }

        if (usb_rx_len == 0)
            return true;

        usb_rx_line[usb_rx_len] = '\0';
        usb_rx_len = 0;

        //? Console echo, a stray terminal line or noise: not ours, not an error.
        if (usb_rx_line[0] == '@')
            usb_handle_line(usb_rx_line + 1);
        return true;
    }

    if (usb_rx_len < USB_CONTROL_MAX_LINE)
        usb_rx_line[usb_rx_len++] = (char)byte;
    else
        usb_rx_overflow = true;

    return false;
}

//* ---------------------- UNSOLICITED FRAMES ----------------------

void usb_update_slow()
{
    const uint32_t now = millis();
    if (now - usb_slow_ms < 1000)
        return;
    usb_slow_ms = now;

    usb_loop_rate = (uint16_t)(usb_loop_count - usb_loop_rate_last);
    usb_loop_rate_last = usb_loop_count;
    usb_rail_mv = supply_rail_mv();
}

uint8_t usb_button_mask();

void usb_emit_tick()
{
    if (usb_stream_hz == 0)
        return;

    const uint32_t now = millis();
    if ((int32_t)(now - usb_next_tick_ms) < 0)
        return;

    usb_next_tick_ms += usb_tick_period_ms;
    if ((int32_t)(usb_next_tick_ms - now) <= 0)
        usb_next_tick_ms = now + usb_tick_period_ms;

    const uint32_t impact_age = (impact_count > 0) ? (now - impact_last_ms) : 0;

    char frame[300];
    snprintf(frame, sizeof(frame),
             "@{\"t\":\"tick\",\"v\":%u,\"ms\":%lu,\"mode\":%d,"
             "\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,"
             "\"gx\":%.2f,\"gy\":%.2f,\"gz\":%.2f,"
             "\"imp\":%u,\"ic\":%lu,\"ia\":%lu,"
             "\"lp\":%u,\"rail\":%lu,\"heap\":%lu,\"net\":%u,"
             "\"dirty\":%s,\"portal\":%s,\"btn\":%u}\n",
             (unsigned)USB_CONTROL_PROTO_VERSION,
             (unsigned long)now,
             mode,
             a.acceleration.x, a.acceleration.y, a.acceleration.z,
             g.gyro.x, g.gyro.y, g.gyro.z,
             (unsigned)impact_last_magnitude,
             (unsigned long)impact_count,
             (unsigned long)impact_age,
             (unsigned)usb_loop_rate,
             (unsigned long)usb_rail_mv,
             (unsigned long)ESP.getFreeHeap(),
             (unsigned)net_state,
             config_dirty ? "true" : "false",
             usb_portal_window ? "true" : "false",
             (unsigned)usb_button_mask());

    usb_write_raw(frame, strlen(frame));
}

uint8_t usb_button_mask()
{
    uint8_t mask = 0;
    for (uint8_t i = 0; i < BUTTON_INPUT_COUNT; i++)
        if (button_inputs[i] != nullptr && button_is_down(*button_inputs[i]))
            mask |= (uint8_t)(1U << i);
    return mask;
}

void usb_emit_events()
{
    if ((uint8_t)mode != usb_last_mode)
    {
        usb_last_mode = (uint8_t)mode;
        const struct_ledMode *entry = find_led_mode((uint8_t)mode);

        String json;
        json.reserve(220);
        usb_json_begin(json, "evt", -1);
        json += F(",\"k\":\"mode\",\"id\":");
        json += String(mode);
        json += F(",\"key\":\"");
        json += webpage_json_escape(entry ? entry->key : "unknown");
        json += F("\",\"label\":\"");
        json += webpage_json_escape(entry ? entry->label : "Unknown");
        json += F("\"}\n");
        usb_write_line(json);
    }

    if (impact_count != usb_last_impact_count)
    {
        usb_last_impact_count = impact_count;

        String json;
        json.reserve(180);
        usb_json_begin(json, "evt", -1);
        json += F(",\"k\":\"impact\",\"mag\":");
        json += String((unsigned)impact_last_magnitude);
        json += F(",\"count\":");
        json += String((unsigned long)impact_count);
        json += F(",\"ms\":");
        json += String((unsigned long)impact_last_ms);
        json += F("}\n");
        usb_write_line(json);
    }

    for (uint8_t i = 0; i < BUTTON_INPUT_COUNT; i++)
    {
        const struct_buttonInput *button = button_inputs[i];
        if (button == nullptr || button->event_count == usb_last_button_events[i])
            continue;

        usb_last_button_events[i] = button->event_count;

        String json;
        json.reserve(200);
        usb_json_begin(json, "evt", -1);
        json += F(",\"k\":\"button\",\"b\":\"");
        json += webpage_json_escape(button->key);
        json += F("\",\"e\":\"");
        json += button_event_name(*button);
        json += F("\",\"n\":");
        json += String((unsigned long)button->event_count);
        json += F("}\n");
        usb_write_line(json);
    }
}

//* ---------------------- LIFECYCLE ----------------------

void usb_control_begin()
{
    usb_last_mode = (uint8_t)mode;
    usb_last_impact_count = impact_count;
    for (uint8_t i = 0; i < BUTTON_INPUT_COUNT; i++)
        usb_last_button_events[i] = button_inputs[i] ? button_inputs[i]->event_count : 0;

    usb_loop_rate_last = usb_loop_count;
    usb_loop_rate_ms = millis();
    usb_slow_ms = millis();

    Serial.printf("[USB] Control link ready: proto %u, fw %s\n",
                  USB_CONTROL_PROTO_VERSION, LIGHTPACK_FW_VERSION);
}

void usb_control_loop()
{
    usb_loop_count++;

    //* Bound both the bytes and the commands per pass. A host that pipelines a
    //? hundred SETs must not be able to hold the strip off for a whole frame:
    //* the rest of the input stays in the CDC queue and is drained next pass.
    uint16_t budget = USB_CONTROL_RX_BYTES_PER_LOOP;
    uint8_t lines = 0;
    while (budget-- > 0 && lines < USB_CONTROL_LINES_PER_LOOP && Serial.available() > 0)
    {
        const int byte = Serial.read();
        if (byte < 0)
            break;
        if (usb_feed_byte((uint8_t)byte))
            lines++;
    }

    const bool connected = usb_connected();
    if (usb_was_connected && !connected)
    {
        //? A reset or an unplug ends the session mid-line. Drop the partial
        //* line and stop the stream; the host re-handshakes when it comes back.
        //! Do NOT abort an in-flight Update here: writing flash can make the
        //! USB CDC drop for a moment, and the host resumes the same byte
        //* offset when it comes back. BEGIN rejects a second update, so a
        //* genuinely abandoned image is cleared by ABORT or a reboot.
        usb_rx_len = 0;
        usb_rx_overflow = false;
        usb_stream_hz = 0;
    }
    usb_was_connected = connected;

    if (usb_update_reboot_at != 0 &&
        (int32_t)(millis() - usb_update_reboot_at) >= 0)
    {
        usb_update_reboot_at = 0;
        Serial.flush();
        delay(100);
        ESP.restart();
    }

    if (!connected)
        return;

    //* One unsolicited state frame when a stress test finishes on USB. The test
    //? blocks the control loop for its whole run, so a host that stayed attached
    //? would otherwise not see the result until it asked again.
    if (stress_report_pending)
    {
        stress_report_pending = false;
        usb_send_state(-1);
    }
    if (sense_report_pending)
    {
        sense_report_pending = false;
        usb_send_state(-1);
    }

    usb_update_slow();
    usb_emit_events();
    usb_emit_tick();
}

#endif // SERVICE_USB_CONTROL
