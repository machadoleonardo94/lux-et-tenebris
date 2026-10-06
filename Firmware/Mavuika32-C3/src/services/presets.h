#if !defined(SERVICE_PRESETS)
#define SERVICE_PRESETS

#include "shared/dependencies.h"

//? ---------------------- PRESET SERVICE ----------------------
//? Five slots of "known good" device configuration, stored in NVS beside the
//? live configuration and swapped from the device page. A slot holds the whole
//? /api/config surface - mode, palette, brightness, strip length and every
//? tunable - so applying one is exactly "put the device back the way it was".
//?
//? Why a packed struct and not JSON on the device:
//?   * the firmware has no JSON parser and does not want one. ArduinoJson is
//?     10-20 KB of flash against an app partition that is 97% full, and the
//?     page already configures the device with form fields (/api/config), so
//?     JSON never has to travel in that direction at all;
//?   * a slot is 31 numbers. As a struct that is sizeof(struct_preset) bytes,
//?     as text it is three to five times that, and NVS charges a 32-byte entry
//?     header for every chunk.
//? JSON is the import/export FILE format, and it is produced and consumed by the
//? browser: the device only ever emits the field names it already knows, and
//? import re-posts them through the existing /api/config path.
//?
//? This header is included AFTER services/webpage.h: it uses web_server, the
//? JSON helper and webpage_config_save() from there, and webpage.h in turn
//? forward-declares the three handlers below for its route table.

#define PRESET_SLOTS 5
//? Includes the terminating NUL. Names come from a browser prompt, so this is
//? also the widest label the page will ever be handed back.
#define PRESET_NAME_LEN 16

//! Bump this whenever struct_preset changes shape. Both the stored blob length
//! and this byte are checked on every read, and a slot that fails either check
//! reads as EMPTY rather than as garbage: a struct that gained a field, or a
//! toolchain that laid the old one out differently, would otherwise hand the
//! render maths whatever bytes happened to sit at those offsets.
#define PRESET_FORMAT_VERSION 1

//* ---------------------- STORED FORM ----------------------

//? Field order inside this struct is irrelevant - the table below addresses
//? every member by offset, so members can be added or moved freely as long as
//? PRESET_FORMAT_VERSION is bumped.
typedef struct presetSlot
{
    uint8_t version;            // PRESET_FORMAT_VERSION as written
    uint8_t mode;               // id from the mode registry (led_scripts.h)
    uint8_t brightness;         // led_strip.brightness
    char name[PRESET_NAME_LEN]; // user label, NUL terminated
    uint16_t active_leds;       // strip_settings.active_leds
    struct_colorRGB palette;
    struct_whipSettings whip;
    struct_rainbowSettings rainbow;
    struct_flameSettings flame;
    struct_stepSettings step;
} struct_preset;

//* ---------------------- FIELD TABLE ----------------------
//? One row per /api/config argument, in the order the page and the export file
//? see them. The names are the POST argument names verbatim, so the JSON this
//? service emits can be posted straight back to /api/config with no translation
//? table anywhere - which is what makes import cheap on both sides.
//!
//! If a field is added to /api/config, add it here and bump
//! PRESET_FORMAT_VERSION. A preset that does not carry a setting will leave
//! that setting at whatever the device currently has, which is survivable, but
//! a preset that carries the WRONG one is not.

enum
{
    PRESET_U8,
    PRESET_U16,
    PRESET_F32
};

typedef struct presetField
{
    const char *name;
    uint8_t type;
    uint16_t offset;
} struct_presetField;

#define PRESET_FLAT(member) ((uint16_t)offsetof(struct_preset, member))
#define PRESET_NEST(member, type, field) \
    ((uint16_t)(offsetof(struct_preset, member) + offsetof(type, field)))

const struct_presetField PRESET_FIELDS[] = {
    {"mode", PRESET_U8, PRESET_FLAT(mode)},
    {"red", PRESET_U8, PRESET_NEST(palette, struct_colorRGB, red)},
    {"green", PRESET_U8, PRESET_NEST(palette, struct_colorRGB, green)},
    {"blue", PRESET_U8, PRESET_NEST(palette, struct_colorRGB, blue)},
    {"brightness", PRESET_U8, PRESET_FLAT(brightness)},
    {"whip_speed", PRESET_F32, PRESET_NEST(whip, struct_whipSettings, speed)},
    {"whip_length", PRESET_U8, PRESET_NEST(whip, struct_whipSettings, length)},
    {"whip_period", PRESET_U16, PRESET_NEST(whip, struct_whipSettings, period)},
    {"whip_peak", PRESET_U8, PRESET_NEST(whip, struct_whipSettings, peak)},
    {"whip_offset", PRESET_U8, PRESET_NEST(whip, struct_whipSettings, offset)},
    {"rainbow_speed", PRESET_F32, PRESET_NEST(rainbow, struct_rainbowSettings, speed)},
    {"rainbow_min_ms", PRESET_U16, PRESET_NEST(rainbow, struct_rainbowSettings, min_ms)},
    {"rainbow_max_ms", PRESET_U16, PRESET_NEST(rainbow, struct_rainbowSettings, max_ms)},
    {"leds", PRESET_U16, PRESET_FLAT(active_leds)},
    {"flame_decay", PRESET_F32, PRESET_NEST(flame, struct_flameSettings, decay)},
    {"flame_spatial", PRESET_F32, PRESET_NEST(flame, struct_flameSettings, spatial)},
    {"flame_power_slope", PRESET_F32, PRESET_NEST(flame, struct_flameSettings, power_slope)},
    {"flame_power_max", PRESET_U8, PRESET_NEST(flame, struct_flameSettings, power_max)},
    {"flame_power_idle", PRESET_U8, PRESET_NEST(flame, struct_flameSettings, power_idle)},
    {"flame_red_gain", PRESET_U8, PRESET_NEST(flame, struct_flameSettings, red_gain)},
    {"flame_green_gain", PRESET_U8, PRESET_NEST(flame, struct_flameSettings, green_gain)},
    {"flame_blue_gain", PRESET_U8, PRESET_NEST(flame, struct_flameSettings, blue_gain)},
    {"step_baseline_tau_ms", PRESET_U16, PRESET_NEST(step, struct_stepSettings, baseline_tau_ms)},
    {"step_threshold", PRESET_F32, PRESET_NEST(step, struct_stepSettings, threshold)},
    {"step_release", PRESET_F32, PRESET_NEST(step, struct_stepSettings, release)},
    {"step_jerk_min", PRESET_F32, PRESET_NEST(step, struct_stepSettings, jerk_min)},
    {"step_min_gap_ms", PRESET_U16, PRESET_NEST(step, struct_stepSettings, min_gap_ms)},
    {"step_impact_min", PRESET_F32, PRESET_NEST(step, struct_stepSettings, impact_min)},
    {"step_impact_max", PRESET_F32, PRESET_NEST(step, struct_stepSettings, impact_max)},
    {"step_magnitude_min", PRESET_U8, PRESET_NEST(step, struct_stepSettings, magnitude_min)},
    {"step_magnitude_max", PRESET_U8, PRESET_NEST(step, struct_stepSettings, magnitude_max)},
};

const uint8_t PRESET_FIELD_COUNT = sizeof(PRESET_FIELDS) / sizeof(PRESET_FIELDS[0]);

//* ---------------------- STORAGE ----------------------

//? The NVS key for a slot. Two characters: short because NVS keys are limited
//? to 15, and stored in the same "device" namespace as the live configuration
//? so a factory erase takes presets with it.
const char *preset_key(uint8_t slot)
{
    static char key[3];
    key[0] = 'p';
    key[1] = (char)('0' + slot);
    key[2] = '\0';
    return key;
}

//* Read one slot. False means "nothing usable here": absent, a blob of another
//* length, or one written by a different PRESET_FORMAT_VERSION.
bool preset_read(uint8_t slot, struct_preset &out)
{
    if (slot >= PRESET_SLOTS)
        return false;

    //! Read-only handle. Listing the slots must not take a write lock on NVS,
    //! and a read on a namespace that does not exist yet is simply a miss.
    if (!preferences.begin(WEBPAGE_NVS_NAMESPACE, true))
        return false;

    size_t stored = preferences.getBytes(preset_key(slot), &out, sizeof(out));
    preferences.end();

    //? Preferences::getBytes() copies nothing and returns 0 when the stored
    //? blob is LONGER than the buffer, so a preset written by a future firmware
    //! with more fields lands on the version check rather than overrunning it.
    return stored == sizeof(out) && out.version == PRESET_FORMAT_VERSION;
}

//* Copy the live configuration into a slot.
bool preset_write(uint8_t slot, const char *name)
{
    if (slot >= PRESET_SLOTS)
        return false;

    //? Zero-initialised so the struct's padding bytes hold a constant value:
    //? they are part of the blob, and a CRC-free format should not carry stack
    //? noise into flash.
    struct_preset p = {};
    p.version = PRESET_FORMAT_VERSION;
    p.mode = (uint8_t)mode;
    p.brightness = led_strip.brightness;
    p.active_leds = strip_settings.active_leds;
    p.palette = palette;
    p.whip = whip;
    p.rainbow = rainbow;
    p.flame = flame;
    p.step = step;

    //* Name resolution, in order: what the caller asked for, then the name the
    //* slot already had (so re-saving over a named slot keeps its label), and
    //* only then a generic one.
    if (name != nullptr && name[0] != '\0')
    {
        strncpy(p.name, name, PRESET_NAME_LEN - 1);
    }
    else
    {
        struct_preset old;
        if (preset_read(slot, old))
            strncpy(p.name, old.name, PRESET_NAME_LEN - 1);
    }
    if (p.name[0] == '\0')
        snprintf(p.name, PRESET_NAME_LEN, "Preset %u", (unsigned)(slot + 1));

    preferences.begin(WEBPAGE_NVS_NAMESPACE, false);
    size_t written = preferences.putBytes(preset_key(slot), &p, sizeof(p));
    preferences.end();

    Serial.printf("[WEB] Preset %u saved as \"%s\" (%u bytes)\n",
                  (unsigned)slot, p.name, (unsigned)written);
    return written == sizeof(p);
}

bool preset_clear(uint8_t slot)
{
    if (slot >= PRESET_SLOTS)
        return false;

    preferences.begin(WEBPAGE_NVS_NAMESPACE, false);
    preferences.remove(preset_key(slot));
    preferences.end();

    Serial.printf("[WEB] Preset %u cleared\n", (unsigned)slot);
    return true;
}

//* Push a stored slot back onto the live configuration.
bool preset_apply(uint8_t slot)
{
    struct_preset p;
    if (!preset_read(slot, p))
        return false;

    //! The mode has to be checked against the registry, exactly as
    //! webpage_config_load() does for the boot state: a slot saved on a build
    //! that had a mode this one does not would otherwise set `mode` to an id
    //! nothing renders.
    const struct_ledMode *entry = find_led_mode(p.mode);
    if (entry == nullptr)
    {
        Serial.printf("[WEB] Preset %u names unknown mode %u - not applied\n",
                      (unsigned)slot, (unsigned)p.mode);
        return false;
    }

    mode = entry->id;
    led_strip.brightness = p.brightness;
    strip_settings.active_leds = p.active_leds;
    palette = p.palette;
    whip = p.whip;
    rainbow = p.rainbow;
    flame = p.flame;
    step = p.step;

    //! Same order and same reasoning as webpage_config_load(): strip_sanitize()
    //! owns the strip length and pulls every length measured in LEDs down with
    //! it, and step_sanitize() has to have run before anything reads the
    //! magnitude scale. A preset is a full configuration and is trusted exactly
    //! as far as a hand-written request is - not at all.
    strip_sanitize();
    step_sanitize();
    whip_sanitize();
    flame_sanitize();
    rainbow_sanitize();

    //? Persist it as the boot configuration too: the page's contract everywhere
    //? else is that what you just set is what comes back after a reset, and a
    //? preset that evaporated on reboot would be worse than no preset at all.
    webpage_config_save();

    Serial.printf("[WEB] Preset %u \"%s\" applied: mode=%u rgb=%u,%u,%u bright=%u\n",
                  (unsigned)slot, p.name, (unsigned)mode, palette.red, palette.green,
                  palette.blue, led_strip.brightness);
    return true;
}

//* Append one slot's values as a /api/config-shaped JSON object.
void preset_json_config(String &json, const struct_preset &p)
{
    const uint8_t *base = (const uint8_t *)&p;
    json += '{';
    for (uint8_t i = 0; i < PRESET_FIELD_COUNT; i++)
    {
        const struct_presetField &f = PRESET_FIELDS[i];
        if (i > 0)
            json += ',';
        json += '"';
        json += f.name;
        json += F("\":");
        if (f.type == PRESET_U8)
            json += String(*(const uint8_t *)(base + f.offset));
        else if (f.type == PRESET_U16)
            json += String(*(const uint16_t *)(base + f.offset));
        else
            json += String(*(const float *)(base + f.offset), 3);
    }
    json += '}';
}

//* ---------------------- HANDLERS ----------------------

//* GET /api/presets - what the page needs to draw the slot bar: which slots
//* hold something and what they are called. Values are fetched per slot, so a
//* page load does not pay for four presets it will not show.
void webpage_handle_presets()
{
    String json;
    json.reserve(320);
    json += '[';
    for (uint8_t i = 0; i < PRESET_SLOTS; i++)
    {
        struct_preset p;
        bool used = preset_read(i, p);
        if (i > 0)
            json += ',';
        json += F("{\"slot\":");
        json += String(i);
        json += F(",\"used\":");
        json += used ? "true" : "false";
        json += F(",\"name\":\"");
        if (used)
            json += webpage_json_escape(p.name);
        json += F("\"}");
    }
    json += F("]\n");

    web_server.send(200, "application/json", json);
}

//* GET /api/preset?slot=N - one slot, values included: the export payload.
void webpage_handle_preset()
{
    int slot = web_server.hasArg("slot") ? web_server.arg("slot").toInt() : -1;
    struct_preset p;

    if (slot < 0 || slot >= PRESET_SLOTS || !preset_read((uint8_t)slot, p))
    {
        web_server.send(404, "application/json", "{\"ok\":false,\"error\":\"empty slot\"}\n");
        return;
    }

    String json;
    json.reserve(700);
    json += F("{\"slot\":");
    json += String(slot);
    json += F(",\"name\":\"");
    json += webpage_json_escape(p.name);
    json += F("\",\"format\":");
    json += String(PRESET_FORMAT_VERSION);
    json += F(",\"fields\":");
    json += String(PRESET_FIELD_COUNT);
    json += F(",\"config\":");
    preset_json_config(json, p);
    json += F("}\n");

    web_server.send(200, "application/json", json);
}

//* POST /api/preset - slot=N plus action=save|apply|clear.
//? `save` takes the LIVE configuration, never values from the request: import
//? works by posting the wanted configuration to /api/config first and then
//? saving the slot, which keeps exactly one code path that can write a
//? configuration and one that can validate it.
void webpage_handle_set_preset()
{
    int slot = web_server.hasArg("slot") ? web_server.arg("slot").toInt() : -1;
    if (slot < 0 || slot >= PRESET_SLOTS)
    {
        web_server.send(400, "application/json", "{\"ok\":false,\"error\":\"slot out of range\"}\n");
        return;
    }

    String action = web_server.arg("action");

    if (action == "save")
    {
        String name = web_server.arg("name");
        if (!preset_write((uint8_t)slot, name.c_str()))
        {
            web_server.send(500, "application/json", "{\"ok\":false,\"error\":\"write failed\"}\n");
            return;
        }
        webpage_handle_presets(); // Reply with the resulting slot list
        return;
    }

    if (action == "apply")
    {
        if (!preset_apply((uint8_t)slot))
        {
            web_server.send(404, "application/json", "{\"ok\":false,\"error\":\"nothing to apply\"}\n");
            return;
        }
        webpage_handle_state(); // Reply with the resulting state
        return;
    }

    if (action == "clear")
    {
        preset_clear((uint8_t)slot);
        webpage_handle_presets();
        return;
    }

    web_server.send(400, "application/json", "{\"ok\":false,\"error\":\"unknown action\"}\n");
}

#endif // SERVICE_PRESETS
