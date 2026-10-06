#if !defined(SERVICE_WEBPAGE)
#define SERVICE_WEBPAGE

#include "shared/dependencies.h"

#include <WebServer.h>
#include <ESPmDNS.h>

//? ---------------------- WEB CONFIGURATION SERVICE ----------------------
//? Serves the on-device configuration page and the small JSON API that backs
//? it. Two responsibilities:
//?   1. Switch the active LED script (the mode registry in led_scripts.h).
//?   2. Read/write the persistent device configuration (colour, brightness).
//?
//? Lifecycle: setup_WIFI() provisions credentials via WiFiManager, then
//? setup_webpage() binds port 80. webpage_loop() must be called from the main
//? loop - it is non-blocking and returns immediately when the service is down.
//?
//? The page is a single PROGMEM document with no external assets, so it works
//? on an isolated LAN or the provisioning AP with no internet access.
//?
//? NOTE (sketch): the API is unauthenticated, matching the rest of the
//? firmware. Anything on the same network can retune the strip. Add a shared
//| token check in the two POST handlers if the device ever leaves a trusted LAN.

#define WEBPAGE_HTTP_PORT 80
//? The .local name this page is served under. Single source of truth lives in
//? shared/variables.h, because the DHCP hostname and the OTA name must match it.
#define WEBPAGE_MDNS_HOST NETWORK_HOSTNAME
#define WEBPAGE_NVS_NAMESPACE "device"

WebServer web_server(WEBPAGE_HTTP_PORT);
bool webpage_started = false;

void setup_webpage();
void webpage_loop();

//* Preset endpoints. Their implementations live in services/presets.h, which is
//* included after this header - they need web_server and the JSON helper defined
//* above, while the route table below only needs their names.
void webpage_handle_presets();
void webpage_handle_preset();
void webpage_handle_set_preset();

//* ---------------------- PERSISTENCE ----------------------

void webpage_config_load()
{
    preferences.begin(WEBPAGE_NVS_NAMESPACE, true);

    int saved_mode = preferences.getInt("mode", mode);
    if (find_led_mode((uint8_t)saved_mode) == nullptr)
    {
        //! The stored mode is no longer in the registry (firmware downgrade or
        //! a renamed entry) - fall back to the first mode rather than boot dark.
        Serial.printf("[WEB] Stored mode %d is not in the registry, resetting\n", saved_mode);
        saved_mode = 1;
    }
    mode = saved_mode;

    //? Keys reuse the old names so an existing selection carries over.
    palette.red = preferences.getUChar("red", palette.red);
    palette.green = preferences.getUChar("green", palette.green);
    palette.blue = preferences.getUChar("blue", palette.blue);
    led_strip.brightness = preferences.getUChar("bright", led_strip.brightness);

    whip.speed = preferences.getFloat("whspd", whip.speed);
    whip.length = preferences.getUChar("whlen", whip.length);
    whip.period = preferences.getUShort("whper", whip.period);
    whip.peak = preferences.getUChar("whpk", whip.peak);
    whip.offset = preferences.getUChar("whoff", whip.offset);

    rainbow.speed = preferences.getFloat("rbwspd", rainbow.speed);
    rainbow.min_ms = preferences.getUShort("rbwmin", rainbow.min_ms);
    rainbow.max_ms = preferences.getUShort("rbwmax", rainbow.max_ms);

    strip_settings.active_leds = preferences.getUShort("leds", strip_settings.active_leds);

    flame.decay = preferences.getFloat("fldec", flame.decay);
    flame.spatial = preferences.getFloat("flspat", flame.spatial);
    flame.power_slope = preferences.getFloat("flpslope", flame.power_slope);
    flame.power_max = preferences.getUChar("flpmax", flame.power_max);
    flame.power_idle = preferences.getUChar("flpidle", flame.power_idle);
    flame.red_gain = preferences.getUChar("flrg", flame.red_gain);
    flame.green_gain = preferences.getUChar("flgg", flame.green_gain);
    flame.blue_gain = preferences.getUChar("flbg", flame.blue_gain);

    step.baseline_tau_ms = preferences.getUShort("stbal", step.baseline_tau_ms);
    step.threshold = preferences.getFloat("stthr", step.threshold);
    step.release = preferences.getFloat("strel", step.release);
    step.jerk_min = preferences.getFloat("stjerk", step.jerk_min);
    step.min_gap_ms = preferences.getUShort("stming", step.min_gap_ms);
    step.impact_min = preferences.getFloat("stimin", step.impact_min);
    step.impact_max = preferences.getFloat("stimax", step.impact_max);
    step.magnitude_min = preferences.getUChar("stmagmin", step.magnitude_min);
    step.magnitude_max = preferences.getUChar("stmagmax", step.magnitude_max);

    preferences.end();

    //! Always sanitise, even for values that never came from the page: a
    //! partially written or corrupt NVS entry must not reach the render maths.
    //! Order matters: strip_sanitize() owns the strip length and pulls every
    //! length measured in LEDs down with it, so it has to run first, and
    //! step_sanitize() has to have run before anything reads the magnitude scale.
    strip_sanitize();
    step_sanitize();
    whip_sanitize();
    flame_sanitize();
    rainbow_sanitize();

    Serial.printf("[WEB] Config loaded: mode=%d rgb=%u,%u,%u bright=%u\n",
                  mode, palette.red, palette.green, palette.blue,
                  led_strip.brightness);
    Serial.printf("[WEB] Strip: %u of %d LEDs\n", strip_leds(), NUM_LEDS);
    Serial.printf("[WEB] Impact: tau=%ums threshold=%.2fg release=%.2fg jerk=%.1fg/s gap=%ums\n",
                  step.baseline_tau_ms, step.threshold, step.release,
                  step.jerk_min, step.min_gap_ms);
    Serial.printf("[WEB] Impact: impact=%.2f..%.2fg -> magnitude=%u..%u (max %u/s)\n",
                  step.impact_min, step.impact_max, step.magnitude_min,
                  step.magnitude_max, step_cadence_max_sps());
    Serial.printf("[WEB] Whip: speed=%.2f len=%u period=%ums peak=%u offset=%u\n",
                  whip.speed, whip.length, whip.period, whip.peak, whip.offset);
    Serial.printf("[WEB] Flame: decay=%.3f spatial=%.2f\n",
                  flame.decay, flame.spatial);
    Serial.printf("[WEB] Flame: power slope=%.1f max=%u idle=%u gains=%u/%u/%u\n",
                  flame.power_slope, flame.power_max, flame.power_idle,
                  flame.red_gain, flame.green_gain, flame.blue_gain);
    Serial.printf("[WEB] Rainbow: speed=%.2f gap=%u..%ums\n",
                  rainbow.speed, rainbow.min_ms, rainbow.max_ms);
}

void webpage_config_save()
{
    //! Writes only on an explicit POST from the page, never from the loop, so
    //! NVS wear stays proportional to user actions.
    preferences.begin(WEBPAGE_NVS_NAMESPACE, false);
    preferences.putInt("mode", mode);
    preferences.putUChar("red", palette.red);
    preferences.putUChar("green", palette.green);
    preferences.putUChar("blue", palette.blue);
    preferences.putUChar("bright", led_strip.brightness);

    preferences.putFloat("whspd", whip.speed);
    preferences.putUChar("whlen", whip.length);
    preferences.putUShort("whper", whip.period);
    preferences.putUChar("whpk", whip.peak);
    preferences.putUChar("whoff", whip.offset);

    preferences.putFloat("rbwspd", rainbow.speed);
    preferences.putUShort("rbwmin", rainbow.min_ms);
    preferences.putUShort("rbwmax", rainbow.max_ms);

    preferences.putUShort("leds", strip_settings.active_leds);

    preferences.putFloat("fldec", flame.decay);
    preferences.putFloat("flspat", flame.spatial);
    preferences.putFloat("flpslope", flame.power_slope);
    preferences.putUChar("flpmax", flame.power_max);
    preferences.putUChar("flpidle", flame.power_idle);
    preferences.putUChar("flrg", flame.red_gain);
    preferences.putUChar("flgg", flame.green_gain);
    preferences.putUChar("flbg", flame.blue_gain);

    preferences.putUShort("stbal", step.baseline_tau_ms);
    preferences.putFloat("stthr", step.threshold);
    preferences.putFloat("strel", step.release);
    preferences.putFloat("stjerk", step.jerk_min);
    preferences.putUShort("stming", step.min_gap_ms);
    //? The old cadence key is dropped rather than left behind: it described a gate
    //! that never did anything (see the note in services/gyros.h), so re-saving it
    //! would advertise a setting the firmware no longer has.
    preferences.remove("stmaxg");
    preferences.putFloat("stimin", step.impact_min);
    preferences.putFloat("stimax", step.impact_max);
    preferences.putUChar("stmagmin", step.magnitude_min);
    preferences.putUChar("stmagmax", step.magnitude_max);
    preferences.end();
}

//* ---------------------- HELPERS ----------------------

String webpage_json_escape(const char *value)
{
    String out;
    if (value == nullptr)
        return out;

    for (const char *p = value; *p; p++)
    {
        if (*p == '"' || *p == '\\')
        {
            out += '\\';
            out += *p;
        }
        else if ((uint8_t)*p >= 0x20)
        {
            out += *p;
        }
    }
    return out;
}

String webpage_url()
{
    IPAddress ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP() : WiFi.softAPIP();
    return "http://" + ip.toString() + "/";
}

//* ---------------------- HANDLERS ----------------------

void webpage_handle_state()
{
    const struct_ledMode *entry = find_led_mode((uint8_t)mode);

    String json;
    //? Grown for the strip-size, step-flame and step-detection blocks.
    json.reserve(2600); // The state document outgrew the old 320-byte reserve long ago
    json += F("{\"mode\":");
    json += String(mode);
    json += F(",\"mode_key\":\"");
    json += webpage_json_escape(entry ? entry->key : "unknown");
    json += F("\",\"mode_label\":\"");
    json += webpage_json_escape(entry ? entry->label : "Unknown");
    json += F("\",\"red\":");
    json += String(palette.red);
    json += F(",\"green\":");
    json += String(palette.green);
    json += F(",\"blue\":");
    json += String(palette.blue);
    json += F(",\"brightness\":");
    json += String(led_strip.brightness);
    json += F(",\"whip_speed\":");
    json += String(whip.speed, 2);
    json += F(",\"whip_length\":");
    json += String(whip.length);
    json += F(",\"whip_period\":");
    json += String(whip.period);
    json += F(",\"whip_peak\":");
    json += String(whip.peak);
    json += F(",\"whip_offset\":");
    json += String(whip.offset);
    //* Derived from the tunables above, so the page can warn about saturation
    //* instead of the user having to work out the crossing time by hand.
    json += F(",\"whip_travel_ms\":");
    json += String(whip_travel_ms());
    json += F(",\"whip_in_flight\":");
    json += String(whip_in_flight());
    json += F(",\"whip_max_heads\":");
    json += String(WHIP_MAX_HEADS);
    json += F(",\"rainbow_speed\":");
    json += String(rainbow.speed, 2);
    json += F(",\"rainbow_min_ms\":");
    json += String(rainbow.min_ms);
    json += F(",\"rainbow_max_ms\":");
    json += String(rainbow.max_ms);
    //* Derived, so the page can explain how the speed and the wait interact
    //* instead of leaving the user to work out band widths by hand.
    json += F(",\"rainbow_travel_ms\":");
    json += String(rainbow_travel_ms());
    json += F(",\"rainbow_band_min_leds\":");
    json += String(rainbow_band_leds(rainbow.min_ms));
    json += F(",\"rainbow_band_max_leds\":");
    json += String(rainbow_band_leds(rainbow.max_ms));
    json += F(",\"rainbow_gap_floor_ms\":");
    json += String(RAINBOW_GAP_MIN_MS);
    json += F(",\"rainbow_gap_ceiling_ms\":");
    json += String(RAINBOW_GAP_MAX_MS);
    json += F(",\"rainbow_speed_min\":");
    json += String(RAINBOW_SPEED_MIN, 1);
    json += F(",\"rainbow_speed_max\":");
    json += String(RAINBOW_SPEED_MAX, 1);

    //* Strip size. `num_leds` stays the size of the NeoPixel buffer - the hard
    //* ceiling the driver owns - while `active_leds` is what the scripts light,
    //* which is what every slider measured in LEDs has to be bounded by.
    json += F(",\"num_leds\":");
    json += String(NUM_LEDS);
    json += F(",\"active_leds\":");
    json += String(strip_leds());
    json += F(",\"strip_leds_min\":");
    json += String(STRIP_LEDS_MIN);
    json += F(",\"majoras_leds\":");
    json += String(MAJORAS_LEDS);

    //* Step flame tunables.
    json += F(",\"flame_decay\":");
    json += String(flame.decay, 3);
    json += F(",\"flame_spatial\":");
    json += String(flame.spatial, 2);
    json += F(",\"flame_power_slope\":");
    json += String(flame.power_slope, 1);
    json += F(",\"flame_power_max\":");
    json += String(flame.power_max);
    json += F(",\"flame_power_idle\":");
    json += String(flame.power_idle);
    json += F(",\"flame_red_gain\":");
    json += String(flame.red_gain);
    json += F(",\"flame_green_gain\":");
    json += String(flame.green_gain);
    json += F(",\"flame_blue_gain\":");
    json += String(flame.blue_gain);
    //* Limits and the detector's scale, shipped so retuning a range or the
    //* magnitude window in the firmware cannot leave the page asking for values
    //* that would only be clamped away.
    json += F(",\"flame_decay_max\":");
    json += String(FLAME_DECAY_MAX, 3);
    json += F(",\"flame_spatial_max\":");
    json += String(FLAME_SPATIAL_MAX, 1);
    json += F(",\"flame_power_slope_max\":");
    json += String(FLAME_POWER_SLOPE_MAX, 1);
    json += F(",\"flame_gain_max\":");
    json += String(FLAME_GAIN_MAX);

    //* Impact detection tunables. The reported scale is the LIVE one, because
    //* magnitude_min/max are themselves settings: the page's flame description
    //* and the detector's output have to be talking about the same range.
    json += F(",\"step_baseline_tau_ms\":");
    json += String(step.baseline_tau_ms);
    json += F(",\"step_threshold\":");
    json += String(step.threshold, 2);
    json += F(",\"step_release\":");
    json += String(step.release, 2);
    json += F(",\"step_jerk_min\":");
    json += String(step.jerk_min, 1);
    json += F(",\"step_min_gap_ms\":");
    json += String(step.min_gap_ms);
    json += F(",\"step_impact_min\":");
    json += String(step.impact_min, 2);
    json += F(",\"step_impact_max\":");
    json += String(step.impact_max, 2);
    json += F(",\"step_magnitude_min\":");
    json += String(step.magnitude_min);
    json += F(",\"step_magnitude_max\":");
    json += String(step.magnitude_max);
    json += F(",\"step_baseline_tau_min_ms\":");
    json += String(STEP_BASELINE_TAU_MIN_MS);
    json += F(",\"step_baseline_tau_max_ms\":");
    json += String(STEP_BASELINE_TAU_MAX_MS);
    json += F(",\"step_jerk_max\":");
    json += String(STEP_JERK_MAX, 1);
    json += F(",\"step_threshold_min\":");
    json += String(STEP_THRESHOLD_MIN, 2);
    json += F(",\"step_threshold_max\":");
    json += String(STEP_THRESHOLD_MAX, 1);
    json += F(",\"step_release_min\":");
    json += String(STEP_RELEASE_MIN, 2);
    json += F(",\"step_gap_min_ms\":");
    json += String(STEP_GAP_MIN_MS);
    json += F(",\"step_gap_max_ms\":");
    json += String(STEP_GAP_MAX_MS);
    json += F(",\"step_impact_min_floor\":");
    json += String(STEP_IMPACT_MIN, 1);
    json += F(",\"step_impact_max_ceiling\":");
    json += String(STEP_IMPACT_MAX, 1);
    json += F(",\"step_cadence_max_sps\":");
    json += String(step_cadence_max_sps());
    json += F(",\"step_scale_min\":");
    json += String(step.magnitude_min);
    json += F(",\"step_scale_max\":");
    json += String(step.magnitude_max);
    //* What the current settings actually produce at the two ends of the
    //* detector's scale, so the page can describe the flash instead of making the
    //* user work out power_slope * magnitude by hand.
    json += F(",\"flame_power_soft\":");
    json += String(flame_power_for((float)step.magnitude_min));
    json += F(",\"flame_power_hard\":");
    json += String(flame_power_for((float)step.magnitude_max));

    json += F(",\"uptime_ms\":");
    json += String(millis());
    json += F(",\"gyro\":");
    json += gyro_started ? "true" : "false";
    json += F(",\"rssi\":");
    json += String(WiFi.RSSI());
    json += F(",\"ip\":\"");
    json += (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    json += F("\",\"hostname\":\"");
    json += webpage_json_escape(WiFi.getHostname());
    json += F("\"}\n");

    web_server.send(200, "application/json", json);
}

void webpage_handle_modes()
{
    String json;
    json.reserve(192);
    json += '[';
    for (uint8_t i = 0; i < led_mode_count(); i++)
    {
        const struct_ledMode *entry = led_mode_at(i);
        if (i > 0)
            json += ',';
        json += F("{\"id\":");
        json += String(entry->id);
        json += F(",\"key\":\"");
        json += webpage_json_escape(entry->key);
        json += F("\",\"label\":\"");
        json += webpage_json_escape(entry->label);
        json += F("\"}");
    }
    json += F("]\n");

    web_server.send(200, "application/json", json);
}

void webpage_handle_set_mode()
{
    const struct_ledMode *entry = nullptr;

    if (web_server.hasArg("key"))
        entry = find_led_mode_by_key(web_server.arg("key").c_str());
    else if (web_server.hasArg("mode"))
        entry = find_led_mode((uint8_t)web_server.arg("mode").toInt());

    if (entry == nullptr)
    {
        web_server.send(400, "application/json", "{\"ok\":false,\"error\":\"unknown mode\"}\n");
        return;
    }

    mode = entry->id;
    webpage_config_save();

    Serial.printf("[WEB] Mode -> %u (%s)\n", entry->id, entry->key);
    webpage_handle_state(); // Reply with the resulting state
}

void webpage_handle_set_config()
{
    bool changed = false;

    if (web_server.hasArg("red"))
    {
        palette.red = (uint8_t)constrain(web_server.arg("red").toInt(), 0, 255);
        changed = true;
    }
    if (web_server.hasArg("green"))
    {
        palette.green = (uint8_t)constrain(web_server.arg("green").toInt(), 0, 255);
        changed = true;
    }
    if (web_server.hasArg("blue"))
    {
        palette.blue = (uint8_t)constrain(web_server.arg("blue").toInt(), 0, 255);
        changed = true;
    }
    if (web_server.hasArg("brightness"))
    {
        led_strip.brightness = (uint8_t)constrain(web_server.arg("brightness").toInt(), 0, 255);
        changed = true;
    }
    if (web_server.hasArg("mode"))
    {
        const struct_ledMode *entry = find_led_mode((uint8_t)web_server.arg("mode").toInt());
        if (entry != nullptr)
        {
            mode = entry->id;
            changed = true;
        }
    }

    //* Pulse whip tunables. Ranges are applied here so a hand-crafted request
    //* cannot push a divisor to zero; whip_sanitize() is the backstop.
    if (web_server.hasArg("whip_speed"))
    {
        float speed = web_server.arg("whip_speed").toFloat();
        if (speed < WHIP_SPEED_MIN)
            speed = WHIP_SPEED_MIN;
        if (speed > WHIP_SPEED_MAX)
            speed = WHIP_SPEED_MAX;
        whip.speed = speed;
        changed = true;
    }
    if (web_server.hasArg("whip_length"))
    {
        int length = web_server.arg("whip_length").toInt();
        if (length < WHIP_LENGTH_MIN)
            length = WHIP_LENGTH_MIN;
        if (length > NUM_LEDS)
            length = NUM_LEDS;
        whip.length = (uint8_t)length;
        changed = true;
    }
    if (web_server.hasArg("whip_period"))
    {
        int period = web_server.arg("whip_period").toInt();
        if (period < WHIP_PERIOD_MIN)
            period = WHIP_PERIOD_MIN;
        if (period > WHIP_PERIOD_MAX)
            period = WHIP_PERIOD_MAX;
        whip.period = (uint16_t)period;
        changed = true;
    }
    if (web_server.hasArg("whip_peak"))
    {
        int peak = web_server.arg("whip_peak").toInt();
        if (peak < 0)
            peak = 0;
        if (peak > 255)
            peak = 255;
        whip.peak = (uint8_t)peak;
        changed = true;
    }
    if (web_server.hasArg("whip_offset"))
    {
        int offset = web_server.arg("whip_offset").toInt();
        if (offset < 0)
            offset = 0;
        if (offset > 255)
            offset = 255;
        whip.offset = (uint8_t)offset;
        changed = true;
    }

    //* Rainbow flow tunables. Same contract as the whip block above: clamp the
    //* request here, then let rainbow_sanitize() below be the backstop.
    if (web_server.hasArg("rainbow_speed"))
    {
        float speed = web_server.arg("rainbow_speed").toFloat();
        if (speed < RAINBOW_SPEED_MIN)
            speed = RAINBOW_SPEED_MIN;
        if (speed > RAINBOW_SPEED_MAX)
            speed = RAINBOW_SPEED_MAX;
        rainbow.speed = speed;
        changed = true;
    }
    if (web_server.hasArg("rainbow_min_ms"))
    {
        int min_ms = web_server.arg("rainbow_min_ms").toInt();
        if (min_ms < RAINBOW_GAP_MIN_MS)
            min_ms = RAINBOW_GAP_MIN_MS;
        if (min_ms > RAINBOW_GAP_MAX_MS)
            min_ms = RAINBOW_GAP_MAX_MS;
        rainbow.min_ms = (uint16_t)min_ms;
        changed = true;
    }
    if (web_server.hasArg("rainbow_max_ms"))
    {
        int max_ms = web_server.arg("rainbow_max_ms").toInt();
        if (max_ms < RAINBOW_GAP_MIN_MS)
            max_ms = RAINBOW_GAP_MIN_MS;
        if (max_ms > RAINBOW_GAP_MAX_MS)
            max_ms = RAINBOW_GAP_MAX_MS;
        rainbow.max_ms = (uint16_t)max_ms;
        changed = true;
    }

    //* Strip size. Bounded by the buffer the driver allocated, not by anything
    //* the request says: NUM_LEDS pixels exist and no more.
    if (web_server.hasArg("leds"))
    {
        int leds = web_server.arg("leds").toInt();
        if (leds < STRIP_LEDS_MIN)
            leds = STRIP_LEDS_MIN;
        if (leds > NUM_LEDS)
            leds = NUM_LEDS;
        strip_settings.active_leds = (uint16_t)leds;
        changed = true;
    }

    //* Step flame tunables. Same contract as the whip and rainbow blocks: clamp
    //* the request here, then let flame_sanitize() below be the backstop.
    if (web_server.hasArg("flame_decay"))
    {
        float decay = web_server.arg("flame_decay").toFloat();
        if (decay < FLAME_DECAY_MIN)
            decay = FLAME_DECAY_MIN;
        if (decay > FLAME_DECAY_MAX)
            decay = FLAME_DECAY_MAX;
        flame.decay = decay;
        changed = true;
    }
    if (web_server.hasArg("flame_spatial"))
    {
        float spatial = web_server.arg("flame_spatial").toFloat();
        if (spatial < FLAME_SPATIAL_MIN)
            spatial = FLAME_SPATIAL_MIN;
        if (spatial > FLAME_SPATIAL_MAX)
            spatial = FLAME_SPATIAL_MAX;
        flame.spatial = spatial;
        changed = true;
    }
    if (web_server.hasArg("flame_power_slope"))
    {
        float slope = web_server.arg("flame_power_slope").toFloat();
        if (slope < 0.0f)
            slope = 0.0f;
        if (slope > FLAME_POWER_SLOPE_MAX)
            slope = FLAME_POWER_SLOPE_MAX;
        flame.power_slope = slope;
        changed = true;
    }
    if (web_server.hasArg("flame_power_max"))
    {
        int max_power = web_server.arg("flame_power_max").toInt();
        if (max_power < 0)
            max_power = 0;
        if (max_power > 255)
            max_power = 255;
        flame.power_max = (uint8_t)max_power;
        changed = true;
    }
    if (web_server.hasArg("flame_power_idle"))
    {
        int idle = web_server.arg("flame_power_idle").toInt();
        if (idle < 0)
            idle = 0;
        if (idle > 255)
            idle = 255;
        flame.power_idle = (uint8_t)idle;
        changed = true;
    }
    //* The three intensity scalers share one shape, so they share one block.
    const struct
    {
        const char *arg;
        uint8_t *field;
    } flame_gains[] = {
        {"flame_red_gain", &flame.red_gain},
        {"flame_green_gain", &flame.green_gain},
        {"flame_blue_gain", &flame.blue_gain},
    };
    const uint8_t flame_gain_count = sizeof(flame_gains) / sizeof(flame_gains[0]);
    for (uint8_t i = 0; i < flame_gain_count; i++)
    {
        if (!web_server.hasArg(flame_gains[i].arg))
            continue;
        int gain = web_server.arg(flame_gains[i].arg).toInt();
        if (gain < 0)
            gain = 0;
        if (gain > FLAME_GAIN_MAX)
            gain = FLAME_GAIN_MAX;
        *flame_gains[i].field = (uint8_t)gain;
        changed = true;
    }

    //* Impact detection tunables. Only the per-field ranges are clamped here;
    //* the relations between the fields (release vs threshold, min vs max) are
    //! step_sanitize()'s job, because a single request can move one side of a
    //! pair and a slider that has hit its ceiling must not be able to invert it.
    if (web_server.hasArg("step_baseline_tau_ms"))
    {
        int tau = web_server.arg("step_baseline_tau_ms").toInt();
        if (tau < STEP_BASELINE_TAU_MIN_MS)
            tau = STEP_BASELINE_TAU_MIN_MS;
        if (tau > STEP_BASELINE_TAU_MAX_MS)
            tau = STEP_BASELINE_TAU_MAX_MS;
        step.baseline_tau_ms = (uint16_t)tau;
        changed = true;
    }
    if (web_server.hasArg("step_threshold"))
    {
        float threshold = web_server.arg("step_threshold").toFloat();
        if (threshold < STEP_THRESHOLD_MIN)
            threshold = STEP_THRESHOLD_MIN;
        if (threshold > STEP_THRESHOLD_MAX)
            threshold = STEP_THRESHOLD_MAX;
        step.threshold = threshold;
        changed = true;
    }
    if (web_server.hasArg("step_release"))
    {
        float release = web_server.arg("step_release").toFloat();
        if (release < STEP_RELEASE_MIN)
            release = STEP_RELEASE_MIN;
        //! No ceiling on this side: `threshold` is the ceiling, and it is
        //! applied as a relation by step_sanitize() below.
        step.release = release;
        changed = true;
    }
    if (web_server.hasArg("step_min_gap_ms"))
    {
        int gap = web_server.arg("step_min_gap_ms").toInt();
        if (gap < STEP_GAP_MIN_MS)
            gap = STEP_GAP_MIN_MS;
        if (gap > STEP_GAP_MAX_MS)
            gap = STEP_GAP_MAX_MS;
        step.min_gap_ms = (uint16_t)gap;
        changed = true;
    }
    if (web_server.hasArg("step_jerk_min"))
    {
        float jerk = web_server.arg("step_jerk_min").toFloat();
        if (jerk < STEP_JERK_MIN)
            jerk = STEP_JERK_MIN;
        if (jerk > STEP_JERK_MAX)
            jerk = STEP_JERK_MAX;
        step.jerk_min = jerk;
        changed = true;
    }
    if (web_server.hasArg("step_impact_min"))
    {
        float impact = web_server.arg("step_impact_min").toFloat();
        if (impact < STEP_IMPACT_MIN)
            impact = STEP_IMPACT_MIN;
        if (impact > STEP_IMPACT_MAX)
            impact = STEP_IMPACT_MAX;
        step.impact_min = impact;
        changed = true;
    }
    if (web_server.hasArg("step_impact_max"))
    {
        float impact = web_server.arg("step_impact_max").toFloat();
        if (impact < STEP_IMPACT_MIN)
            impact = STEP_IMPACT_MIN;
        if (impact > STEP_IMPACT_MAX)
            impact = STEP_IMPACT_MAX;
        step.impact_max = impact;
        changed = true;
    }
    if (web_server.hasArg("step_magnitude_min"))
    {
        int magnitude = web_server.arg("step_magnitude_min").toInt();
        if (magnitude < 0)
            magnitude = 0;
        if (magnitude > 254)
            magnitude = 254;
        step.magnitude_min = (uint8_t)magnitude;
        changed = true;
    }
    if (web_server.hasArg("step_magnitude_max"))
    {
        int magnitude = web_server.arg("step_magnitude_max").toInt();
        if (magnitude < 1)
            magnitude = 1;
        if (magnitude > 255)
            magnitude = 255;
        step.magnitude_max = (uint8_t)magnitude;
        changed = true;
    }

    if (!changed)
    {
        web_server.send(400, "application/json", "{\"ok\":false,\"error\":\"no recognised field\"}\n");
        return;
    }

    //! strip_sanitize() first: it owns `active_leds` and pulls every length
    //! measured in LEDs down with it, so the two sanitisers after it see the
    //! strip the values will actually be rendered on. step_sanitize() follows so
    //! that the flame description below is computed on a legal magnitude scale.
    strip_sanitize();
    step_sanitize();
    whip_sanitize();
    flame_sanitize();
    rainbow_sanitize();
    webpage_config_save();

    //? Proof on serial that the write actually landed, rather than having to
    //? infer it from the strip.
    Serial.printf("[WEB] Config -> mode=%d palette=%u,%u,%u bright=%u\n",
                  mode, palette.red, palette.green, palette.blue, led_strip.brightness);
    Serial.printf("[WEB] Strip -> %u of %d LEDs\n", strip_leds(), NUM_LEDS);
    Serial.printf("[WEB] Impact -> tau=%ums threshold=%.2f release=%.2f jerk=%.1f gap=%ums impact=%.2f..%.2f mag=%u..%u\n",
                  step.baseline_tau_ms, step.threshold, step.release,
                  step.jerk_min, step.min_gap_ms, step.impact_min, step.impact_max,
                  step.magnitude_min, step.magnitude_max);
    Serial.printf("[WEB] Flame -> decay=%.3f spatial=%.2f power=%.1f max=%u idle=%u gains=%u/%u/%u\n",
                  flame.decay, flame.spatial, flame.power_slope, flame.power_max,
                  flame.power_idle, flame.red_gain, flame.green_gain, flame.blue_gain);
    Serial.printf("[WEB] Rainbow -> speed=%.2f gap=%u..ums\n",
                  rainbow.speed, rainbow.min_ms, rainbow.max_ms);

    webpage_handle_state();
}

void webpage_handle_not_found()
{
    if (web_server.uri().startsWith("/api/"))
    {
        web_server.send(404, "application/json", "{\"ok\":false,\"error\":\"no such endpoint\"}\n");
        return;
    }

    //* Anything else (including /favicon.ico) lands back on the config page.
    web_server.sendHeader(F("Location"), "/", true);
    web_server.send(302, "text/plain", "");
}

//* ---------------------- EMBEDDED PAGE ----------------------
//* Single document, no external assets. The mode buttons are built from
//* /api/modes so the registry in led_scripts.h stays the only place a mode is
//* declared.

const char WEBPAGE_HTML[] PROGMEM = R"HTMLPAGE(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Mavuika &middot; Device Configuration</title>
<style>
:root{color-scheme:dark}
*{box-sizing:border-box}
body{margin:0 auto;padding:1.25rem;max-width:44rem;background:#14121a;color:#e8e4f0;
     font:16px/1.5 system-ui,-apple-system,Segoe UI,sans-serif}
h1{font-size:1.35rem;margin:0 0 .15rem}
.sub{color:#9d93b8;font-size:.85rem;margin:0 0 .5rem}
h2{font-size:.75rem;text-transform:uppercase;letter-spacing:.09em;color:#9d93b8;margin:0 0 .6rem}
section{border-top:1px solid #2b2738;padding:1.1rem 0}
.hint{color:#8a80a6;font-size:.78rem;margin:.5rem 0 0}
.modes{display:grid;grid-template-columns:repeat(auto-fit,minmax(9.5rem,1fr));gap:.5rem}
button{font:inherit;padding:.7rem .8rem;border-radius:.6rem;border:1px solid #3a3450;
       background:#1e1b29;color:#e8e4f0;cursor:pointer;transition:background .12s,border-color .12s}
button:hover{background:#272235}
button.active{background:#6d3fd4;border-color:#a184f0;font-weight:600}
button:disabled{opacity:.5;cursor:progress}
.row{display:flex;align-items:center;gap:.9rem}
.ctl{display:grid;grid-template-columns:5.5rem 1fr 3.5rem;align-items:center;gap:.6rem;margin-bottom:.4rem}
.ctl label{color:#9d93b8;font-size:.85rem}
.swatches{display:grid;grid-template-columns:repeat(auto-fit,minmax(2.6rem,1fr));gap:.45rem;margin-bottom:.6rem}
.swatches button{height:2.3rem;padding:0;border-radius:.5rem;border:1px solid #3a3450}
.swatches button.active{outline:2px solid #e8e4f0;outline-offset:2px}
input[type=color]{flex:0 0 5rem;height:2.75rem;padding:.15rem;border:1px solid #3a3450;
                  border-radius:.6rem;background:#1e1b29;cursor:pointer}
input[type=range]{flex:1;accent-color:#8a63e8}
select{font:inherit;padding:.65rem;border-radius:.6rem;border:1px solid #3a3450;
       background:#1e1b29;color:#e8e4f0}
.presets{display:grid;grid-template-columns:repeat(auto-fit,minmax(7rem,1fr));gap:.5rem;
         margin-bottom:.6rem}
.presets button{height:2.6rem;padding:0 .4rem;overflow:hidden;text-overflow:ellipsis;
                white-space:nowrap}
.filebtn{font:inherit;padding:.7rem .8rem;border-radius:.6rem;border:1px solid #3a3450;
         background:#1e1b29;cursor:pointer;transition:background .12s}
.filebtn:hover{background:#272235}
.filebtn input{display:none}
output{min-width:2.75rem;text-align:right;font-variant-numeric:tabular-nums;color:#c9c1e0}
dl{display:grid;grid-template-columns:auto 1fr;gap:.3rem .9rem;margin:0;font-size:.9rem}
dt{color:#9d93b8}
dd{margin:0;text-align:right;font-variant-numeric:tabular-nums;word-break:break-all}
.banner{background:#4a1f2b;border:1px solid #7d3348;color:#ffd9e2;padding:.6rem .8rem;
        border-radius:.6rem;font-size:.85rem;margin-top:.6rem;display:none}
</style>
</head>
<body>
<h1>Mavuika</h1>
<p class="sub">Device configuration</p>
<div id="error" class="banner"></div>

<section>
  <h2>Mode</h2>
  <div id="modes" class="modes"></div>
  <p class="hint">Active script for the LED strip. The choice is saved to flash.</p>
</section>

<section>
  <h2>Presets</h2>
  <div id="presets" class="presets"></div>
  <div class="row">
    <select id="presetSlot"></select>
    <button type="button" id="presetSave">Save current</button>
  </div>
  <div class="row" style="margin-top:.5rem">
    <button type="button" id="presetExport">Export</button>
    <label class="filebtn">Import<input type="file" id="presetImport" accept=".json,application/json"></label>
  </div>
  <p class="hint" id="presetInfo"></p>
  <p class="hint">A slot holds everything on this page: mode, colour, brightness,
     strip length, the impact detector and every tunable below. Click a slot to
     apply it, or pick one and save the current settings into it. Export writes a
     JSON file built by this page, not by the device. Import applies each preset
     in the file and then saves it into its slot, so the device is left on the
     last one the file contained.</p>
</section>

<section>
  <h2>Palette</h2>
  <div id="swatches" class="swatches"></div>
  <div class="row"><input type="color" id="color" value="#ff00ff"></div>
  <p class="hint">Drives the <em>Solid color</em> mode and tints the
     <em>Pulse whip</em> hump. The gyro-driven modes and <em>Rainbow flow</em>
     compute their own colour.</p>
</section>

<section>
  <h2>Strip</h2>
  <div class="ctl"><label for="leds">LED count</label>
    <input type="range" id="leds" min="1" max="150" step="1">
    <output id="ledsVal"></output></div>
  <p class="hint" id="ledsInfo"></p>
  <p class="hint">How many LEDs, counting from the start of the strip, the scripts
     light. The driver still owns the whole buffer, so anything above this count is
     simply left dark - useful when the strip is shorter than the firmware was built
     for, or when only part of it should be used. A slider measured in LEDs (the
     whip length) is capped by this value, and the step flame's spatial fade is
     spread across whatever count is set here.</p>
</section>

<section>
  <h2>Impact detection</h2>
  <div class="ctl"><label for="stbal">Baseline &tau;</label>
    <input type="range" id="stbal" min="100" max="20000" step="50">
    <output id="stbalVal"></output></div>
  <div class="ctl"><label for="stthr">Threshold</label>
    <input type="range" id="stthr" min="0.05" max="20" step="0.05">
    <output id="stthrVal"></output></div>
  <div class="ctl"><label for="strel">Release</label>
    <input type="range" id="strel" min="0.01" max="20" step="0.01">
    <output id="strelVal"></output></div>
  <div class="ctl"><label for="stjerk">Jerk gate</label>
    <input type="range" id="stjerk" min="0" max="500" step="1">
    <output id="stjerkVal"></output></div>
  <div class="ctl"><label for="stming">Refractory</label>
    <input type="range" id="stming" min="20" max="10000" step="10">
    <output id="stmingVal"></output></div>
  <div class="ctl"><label for="stimin">Impact min</label>
    <input type="range" id="stimin" min="0" max="50" step="0.1">
    <output id="stiminVal"></output></div>
  <div class="ctl"><label for="stimax">Impact max</label>
    <input type="range" id="stimax" min="0" max="50" step="0.1">
    <output id="stimaxVal"></output></div>
  <div class="ctl"><label for="stmagmin">Mag min</label>
    <input type="range" id="stmagmin" min="0" max="254" step="1">
    <output id="stmagminVal"></output></div>
  <div class="ctl"><label for="stmagmax">Mag max</label>
    <input type="range" id="stmagmax" min="1" max="255" step="1">
    <output id="stmagmaxVal"></output></div>
  <p class="hint" id="stepInfo"></p>
  <p class="hint">The accelerometer's impact detector, shared by every mode that
     reacts to movement. It works on the <em>magnitude</em> of the acceleration
     vector, so how the device is worn or which way up it is does not matter.
     It learns a slow baseline of the resting magnitude (<strong>Baseline &tau;</strong>
     is how long that average remembers) and calls an impact when the departure from
     it rises above <strong>Threshold</strong>, then waits for the departure to fall
     back under <strong>Release</strong> before reporting once.
     <strong>Jerk gate</strong> is the extra test that separates a knock from a
     swing: it requires the magnitude to be <em>changing</em> at least this fast
     (in g per second) to count. An impact changes it in a couple of milliseconds, a
     swing ramps over hundreds, and rotation about an offset axis can otherwise reach
     nearly a g. Set it to 0 to disable the gate. <strong>Refractory</strong> is the
     shortest gap between two accepted impacts. The reported strength is rescaled
     from <strong>Impact min</strong>..<strong>Impact max</strong> (in g) onto
     <strong>Mag min</strong>..<strong>Mag max</strong>, which is the number
     <em>Step flame</em> above acts on - raising Threshold is what stops a nudge
     lighting the strip, and widening the impact window is what makes a light tap
     look different from a hard knock.</p>
</section>

<section>
  <h2>Step flame</h2>
  <div class="ctl"><label for="fldec">Decay</label>
    <input type="range" id="fldec" min="0" max="0.995" step="0.005">
    <output id="fldecVal"></output></div>
  <div class="ctl"><label for="flspat">Spatial decay</label>
    <input type="range" id="flspat" min="0" max="10" step="0.1">
    <output id="flspatVal"></output></div>
  <div class="ctl"><label for="flpslope">Power rise</label>
    <input type="range" id="flpslope" min="0" max="25" step="0.5">
    <output id="flpslopeVal"></output></div>
  <div class="ctl"><label for="flpmax">Power max</label>
    <input type="range" id="flpmax" min="0" max="255" step="1">
    <output id="flpmaxVal"></output></div>
  <div class="ctl"><label for="flpidle">Idle power</label>
    <input type="range" id="flpidle" min="0" max="255" step="1">
    <output id="flpidleVal"></output></div>
  <div class="ctl"><label for="flrg">Red gain</label>
    <input type="range" id="flrg" min="0" max="255" step="1">
    <output id="flrgVal"></output></div>
  <div class="ctl"><label for="flgg">Green gain</label>
    <input type="range" id="flgg" min="0" max="255" step="1">
    <output id="flggVal"></output></div>
  <div class="ctl"><label for="flbg">Blue gain</label>
    <input type="range" id="flbg" min="0" max="255" step="1">
    <output id="flbgVal"></output></div>
  <p class="hint" id="flameInfo"></p>
  <p class="hint">The <em>Step flame</em> mode lights the whole strip the moment a
     step is detected, at a brightness set by how hard the impact was:
     <strong>Power rise</strong> times the reported magnitude, capped by
     <strong>Power max</strong>. The strip then fades. <strong>Decay</strong> is the
     fraction of its brightness each LED keeps per frame (~50&nbsp;ms) at the start
     of the strip, so 0.90 falls silent about 1.5&nbsp;s after a step while 0.995
     lingers for roughly half a minute. <strong>Spatial decay</strong> is how much
     faster that fade gets along the strip: the last LED keeps
     decay<sup>(1 + spatial)</sup> per frame instead of decay, so the far end goes
     dark first and the flame reads as retreating towards the start. At 0 the whole
     strip fades together. <strong>Idle power</strong> is the floor every LED
     settles at.
     The three <strong>gains</strong> are per-channel intensity scalers: 100 is
     unity, so red+blue at 100 is the magenta flame, and 0/100/0 gives a green one.</p>
</section>

<section>
  <h2>Pulse whip</h2>
  <div class="ctl"><label for="wspeed">Speed</label>
    <input type="range" id="wspeed" min="0.1" max="20" step="0.1">
    <output id="wspeedVal"></output></div>
  <div class="ctl"><label for="wlength">Length</label>
    <input type="range" id="wlength" min="1" max="125" step="1">
    <output id="wlengthVal"></output></div>
  <div class="ctl"><label for="wperiod">Period</label>
    <input type="range" id="wperiod" min="50" max="5000" step="10">
    <output id="wperiodVal"></output></div>
  <div class="ctl"><label for="wpeak">Peak</label>
    <input type="range" id="wpeak" min="0" max="255" step="1">
    <output id="wpeakVal"></output></div>
  <div class="ctl"><label for="woffset">Offset</label>
    <input type="range" id="woffset" min="0" max="255" step="1">
    <output id="woffsetVal"></output></div>
  <p class="hint" id="whipInfo"></p>
  <p class="hint"><strong>Speed</strong> is LEDs per frame (~50&nbsp;ms);
     <strong>Length</strong> is the hump width; <strong>Period</strong> is the gap
     between launches; <strong>Peak</strong> is the hump height;
     <strong>Offset</strong> is the level the strip rests at between whips.</p>
</section>

<section>
  <h2>Rainbow flow</h2>
  <div class="ctl"><label for="rbspd">Speed</label>
    <input type="range" id="rbspd" min="0.1" max="20" step="0.1">
    <output id="rbspdVal"></output></div>
  <div class="ctl"><label for="rbmin">Min wait</label>
    <input type="range" id="rbmin" min="50" max="60000" step="50">
    <output id="rbminVal"></output></div>
  <div class="ctl"><label for="rbmax">Max wait</label>
    <input type="range" id="rbmax" min="50" max="60000" step="50">
    <output id="rbmaxVal"></output></div>
  <p class="hint" id="rainbowInfo"></p>
  <p class="hint">The starting LED is given a new random colour after a wait drawn
     between <strong>Min wait</strong> and <strong>Max wait</strong>, and that colour
     then travels toward the far end at <strong>Speed</strong> LEDs per frame
     (~50&nbsp;ms). Every LED changes colour once per wait whatever the speed is;
     the speed decides how far each colour gets.</p>
</section>

<section>
  <h2>Brightness</h2>
  <div class="row">
    <input type="range" id="bright" min="0" max="255" step="1">
    <output id="brightVal">255</output>
  </div>
  <p class="hint">Applies to every mode, on the external strip and Majora's mask.</p>
</section>

<section>
  <h2>Status</h2>
  <dl id="status"></dl>
</section>

<script>
var state = null;
var $ = function (s) { return document.querySelector(s); };

function showError(msg) {
  var b = $('#error');
  b.textContent = msg;
  b.style.display = msg ? 'block' : 'none';
}

function api(path, data) {
  var opts = data
    ? { method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: new URLSearchParams(data) }
    : { method: 'GET' };
  return fetch(path, opts).then(function (res) {
    if (!res.ok) { throw new Error(path + ' -> HTTP ' + res.status); }
    return res.json();
  });
}

function hex(r, g, b) {
  return '#' + [r, g, b].map(function (v) {
    return v.toString(16).padStart(2, '0');
  }).join('');
}

function rgbFromHex(h) {
  return [1, 3, 5].map(function (i) { return parseInt(h.substr(i, 2), 16); });
}

function render() {
  if (!state) { return; }

  document.querySelectorAll('#modes button').forEach(function (b) {
    b.classList.toggle('active', Number(b.dataset.id) === state.mode);
  });

  if (document.activeElement !== $('#color')) {
    $('#color').value = hex(state.red, state.green, state.blue);
  }

  var current = state.red + ',' + state.green + ',' + state.blue;
  document.querySelectorAll('#swatches button').forEach(function (b) {
    b.classList.toggle('active', b.dataset.rgb === current);
  });
  if (document.activeElement !== $('#bright')) {
    $('#bright').value = state.brightness;
    $('#brightVal').textContent = state.brightness;
  }

  var rows = {
    'Mode': state.mode_label + ' (' + state.mode_key + ')',
    'Colour': 'rgb(' + state.red + ', ' + state.green + ', ' + state.blue + ')',
    'IP': state.ip,
    'Hostname': state.hostname,
    'Signal': state.rssi + ' dBm',
    'LEDs': state.active_leds + ' of ' + state.num_leds + ' strip / ' +
            state.majoras_leds + ' majora',
    'Gyroscope': state.gyro ? 'ready' : 'not detected',
    'Uptime': Math.floor(state.uptime_ms / 1000) + ' s'
  };
  $('#status').innerHTML = Object.keys(rows).map(function (k) {
    return '<dt>' + k + '</dt><dd>' + rows[k] + '</dd>';
  }).join('');

  //* Strip size. The buffer size is the ceiling the device reports; the live
  //* count is what every other LED-measured slider has to stay within.
  $('#leds').min = state.strip_leds_min;
  $('#leds').max = state.num_leds;

  var leds = [
    ['leds', state.active_leds, state.active_leds + ' LED']
  ];
  leds.forEach(function (l) {
    var el = document.getElementById(l[0]);
    if (document.activeElement !== el) { el.value = l[1]; }
    document.getElementById(l[0] + 'Val').textContent = l[2];
  });

  var ledInfo = 'Lighting ' + state.active_leds + ' of the ' + state.num_leds +
                ' LEDs the driver owns.';
  if (state.active_leds !== state.num_leds) {
    ledInfo += ' The remaining ' + (state.num_leds - state.active_leds) +
               ' are held dark.';
  }
  if (state.active_leds < 2) {
    ledInfo += ' With one LED the travelling modes have nowhere to move.';
  }
  $('#ledsInfo').textContent = ledInfo;

  //* Impact detection controls. Release is capped by Threshold rather than by a
  //* constant of its own: the firmware enforces release < threshold as a
  //* relation, so the page keeps the slider inside the legal band instead of
  //* letting it write a value that would only be pulled back.
  document.getElementById('stbal').min = state.step_baseline_tau_min_ms;
  document.getElementById('stbal').max = state.step_baseline_tau_max_ms;
  document.getElementById('stthr').min = state.step_threshold_min;
  document.getElementById('stthr').max = state.step_threshold_max;
  document.getElementById('strel').min = state.step_release_min;
  document.getElementById('strel').max = state.step_threshold;
  document.getElementById('stjerk').max = state.step_jerk_max;
  document.getElementById('stming').min = state.step_gap_min_ms;
  document.getElementById('stming').max = state.step_gap_max_ms;
  document.getElementById('stimin').min = state.step_impact_min_floor;
  document.getElementById('stimin').max = state.step_impact_max_ceiling;
  document.getElementById('stimax').min = state.step_impact_min_floor;
  document.getElementById('stimax').max = state.step_impact_max_ceiling;
  document.getElementById('stmagmin').max = 254;
  document.getElementById('stmagmax').max = 255;

  var steps = [
    ['stbal', state.step_baseline_tau_ms, state.step_baseline_tau_ms + ' ms'],
    ['stthr', state.step_threshold, state.step_threshold.toFixed(2) + ' g'],
    ['strel', state.step_release, state.step_release.toFixed(2) + ' g'],
    ['stjerk', state.step_jerk_min, Number(state.step_jerk_min).toFixed(0) + ' g/s'],
    ['stming', state.step_min_gap_ms, state.step_min_gap_ms + ' ms'],
    ['stimin', state.step_impact_min, state.step_impact_min.toFixed(2) + ' g'],
    ['stimax', state.step_impact_max, state.step_impact_max.toFixed(2) + ' g'],
    ['stmagmin', state.step_magnitude_min, String(state.step_magnitude_min)],
    ['stmagmax', state.step_magnitude_max, String(state.step_magnitude_max)]
  ];
  steps.forEach(function (s) {
    var el = document.getElementById(s[0]);
    if (document.activeElement !== el) { el.value = s[1]; }
    document.getElementById(s[0] + 'Val').textContent = s[2];
  });

  //* Say what the thresholds mean in the units the user is thinking in, and name
  //* the settings that will silently defeat each other.
  var stepInfo = 'Impacts are accepted up to ' + state.step_cadence_max_sps +
                 ' per second (' + state.step_min_gap_ms + ' ms apart). An impact of ' +
                 state.step_impact_min.toFixed(2) + '-' + state.step_impact_max.toFixed(2) +
                 ' g is reported as ' + state.step_magnitude_min + '-' +
                 state.step_magnitude_max + '.';
  if (state.step_release >= state.step_threshold) {
    stepInfo += ' Release has reached Threshold, so any impact releases immediately: ' +
                'one impact can be counted again as soon as the refractory expires.';
  }
  if (state.step_threshold < 0.5) {
    stepInfo += ' A threshold this low will trigger on hand movement and table knocks.';
  }
  if (state.step_jerk_min === 0) {
    stepInfo += ' The jerk gate is disabled, so a fast swing can register as an impact.';
  } else {
    stepInfo += ' The jerk gate rejects anything changing the magnitude slower than ' +
                Number(state.step_jerk_min).toFixed(0) + ' g/s.';
  }
  if (state.step_impact_max - state.step_impact_min < 1) {
    stepInfo += ' The impact window is narrower than 1 g, so every impact reads as much ' +
                'the same magnitude and the flame will not vary.';
  }
  if (state.step_cadence_max_sps < 1) {
    stepInfo += ' The refractory allows less than one impact a second.';
  }
  if (state.gyro === false) {
    stepInfo += ' No sensor was detected, so none of this is running.';
  }
  $('#stepInfo').textContent = stepInfo;

  //* Step flame controls. The numeric limits come from the device (they mirror
  //* the FLAME_* defines in led_scripts.h), so retuning a range there cannot
  //* leave the page asking for values the firmware would only clamp.
  document.getElementById('fldec').max = state.flame_decay_max;
  document.getElementById('flspat').max = state.flame_spatial_max;
  document.getElementById('flpslope').max = state.flame_power_slope_max;
  ['flrg', 'flgg', 'flbg'].forEach(function (id) {
    document.getElementById(id).max = state.flame_gain_max;
  });

  var flames = [
    ['fldec', state.flame_decay, state.flame_decay.toFixed(3)],
    ['flspat', state.flame_spatial, Number(state.flame_spatial).toFixed(1) + ' x'],
    ['flpslope', state.flame_power_slope, state.flame_power_slope.toFixed(1) + ' /mag'],
    ['flpmax', state.flame_power_max, String(state.flame_power_max)],
    ['flpidle', state.flame_power_idle, String(state.flame_power_idle)],
    ['flrg', state.flame_red_gain, state.flame_red_gain + '%'],
    ['flgg', state.flame_green_gain, state.flame_green_gain + '%'],
    ['flbg', state.flame_blue_gain, state.flame_blue_gain + '%']
  ];
  flames.forEach(function (f) {
    var el = document.getElementById(f[0]);
    if (document.activeElement !== el) { el.value = f[1]; }
    document.getElementById(f[0] + 'Val').textContent = f[2];
  });

  //* Describe what the settings actually produce at both ends of the detector's
  //* scale, and name the way the sliders can fight each other: a power cap that
  //* clips the hard steps. The far-end decay is spelled out because it is the one
  //* number a user cannot read off a slider.
  var flameInfo = 'At rest the whole strip sits at ' + state.flame_power_idle +
                  '. A soft step (' + state.step_scale_min + ') flashes ' +
                  state.flame_power_soft + '; the hardest (' + state.step_scale_max +
                  ') flashes ' + state.flame_power_hard + '.';
  var hardRaw = Math.round(state.flame_power_slope * state.step_scale_max);
  if (hardRaw > state.flame_power_max) {
    flameInfo += ' Power max caps the hardest steps, which would otherwise ask for ' +
                 hardRaw + '.';
  }
  var farKeep = Math.pow(state.flame_decay, 1 + state.flame_spatial);
  flameInfo += ' The last LED keeps ' + farKeep.toFixed(3) + ' per frame, against ' +
               Number(state.flame_decay).toFixed(3) + ' at the first.';
  if (state.flame_decay >= 1) {
    flameInfo += ' A decay of 1 or more never fades: the flame would stay lit.';
  }
  $('#flameInfo').textContent = flameInfo;

  //* Pulse whip controls. Length can never exceed the strip, which the page
  //* learns from the device rather than hardcoding.
  $('#wlength').max = state.active_leds;

  var whips = [
    ['wspeed', state.whip_speed, state.whip_speed.toFixed(1) + ' LED/f'],
    ['wlength', state.whip_length, state.whip_length + ' LED'],
    ['wperiod', state.whip_period, state.whip_period + ' ms'],
    ['wpeak', state.whip_peak, String(state.whip_peak)],
    ['woffset', state.whip_offset, String(state.whip_offset)]
  ];
  whips.forEach(function (w) {
    var el = document.getElementById(w[0]);
    if (document.activeElement !== el) { el.value = w[1]; }
    document.getElementById(w[0] + 'Val').textContent = w[2];
  });

  var info = 'One whip crosses in ' + (state.whip_travel_ms / 1000).toFixed(2) +
             ' s; about ' + state.whip_in_flight + ' on the strip at once.';
  if (state.whip_in_flight >= state.whip_max_heads) {
    info += ' At the ' + state.whip_max_heads + '-head cap: raise the period, ' +
            'launches are being dropped.';
  } else if (state.whip_in_flight > 1) {
    info += ' Heads overlap and their humps sum.';
  }
  $('#whipInfo').textContent = info;

  //* Rainbow flow controls. The limits come from the device (they mirror the
  //* RAINBOW_* defines in led_scripts.h), so retuning a range there cannot leave
  //* the page asking for values the firmware would only clamp.
  var gapFloor = state.rainbow_gap_floor_ms;
  var gapCeiling = state.rainbow_gap_ceiling_ms;
  [['rbmin', gapFloor], ['rbmax', gapFloor]].forEach(function (lim) {
    var el = document.getElementById(lim[0]);
    el.min = lim[1];
    el.max = gapCeiling;
  });
  document.getElementById('rbspd').min = state.rainbow_speed_min;
  document.getElementById('rbspd').max = state.rainbow_speed_max;

  var bows = [
    ['rbspd', state.rainbow_speed, state.rainbow_speed.toFixed(1) + ' LED/f'],
    ['rbmin', state.rainbow_min_ms, state.rainbow_min_ms + ' ms'],
    ['rbmax', state.rainbow_max_ms, state.rainbow_max_ms + ' ms']
  ];
  bows.forEach(function (b) {
    var el = document.getElementById(b[0]);
    if (document.activeElement !== el) { el.value = b[1]; }
    document.getElementById(b[0] + 'Val').textContent = b[2];
  });

  //* Both knobs meet on the strip: a colour is drawn every Min..Max wait and
  //* travels at Speed, so it covers speed * wait LEDs. Say so, and say when the
  //* two settings disagree with what an LED can show.
  var bandMin = state.rainbow_band_min_leds;
  var bandMax = state.rainbow_band_max_leds;
  var bowInfo = 'One colour crosses the strip in ' +
                (state.rainbow_travel_ms / 1000).toFixed(2) + ' s; each colour ' +
                'covers about ' + bandMin + '-' + bandMax + ' LEDs.';
  if (bandMin < 2) {
    bowInfo += ' At the current speed a wait this short covers less than an LED, ' +
               'so colours are skipped: raise the wait or the speed.';
  } else if (bandMax >= state.active_leds) {
    bowInfo += ' Wider than the strip, so a colour can fill it before the next one ' +
               'is drawn; lower the speed or the wait for more colours on the strip.';
  } else if (state.rainbow_travel_ms > state.rainbow_max_ms) {
    bowInfo += ' Several colours travel at once.';
  }
  if (state.rainbow_min_ms === state.rainbow_max_ms) {
    bowInfo += ' Min and max are equal: colours change at a fixed interval.';
  }
  $('#rainbowInfo').textContent = bowInfo;
}

function refresh() {
  return api('/api/state').then(function (s) {
    state = s;
    render();
    showError('');
  }).catch(function (e) { showError('Lost contact with the device: ' + e.message); });
}

function setMode(id) {
  document.querySelectorAll('#modes button').forEach(function (b) {
    b.disabled = true;
  });
  api('/api/mode', { mode: id }).then(function (s) {
    state = s;
    render();
    showError('');
  }).catch(function (e) {
    showError('Could not switch mode: ' + e.message);
  }).finally(function () {
    document.querySelectorAll('#modes button').forEach(function (b) {
      b.disabled = false;
    });
  });
}

function pushConfig(data) {
  return api('/api/config', data).then(function (s) {
    state = s;
    render();
    showError('');
  }).catch(function (e) { showError('Could not save: ' + e.message); });
}

$('#color').addEventListener('change', function (e) {
  var c = rgbFromHex(e.target.value);
  pushConfig({ red: c[0], green: c[1], blue: c[2] });
});

//* Palette presets. Each swatch posts the same three fields as the picker, so
//* the device has exactly one notion of "the chosen colour".
var PRESETS = [
  ['Magenta', 255, 0, 255],
  ['Red', 255, 0, 0],
  ['Amber', 255, 140, 0],
  ['Lime', 120, 255, 0],
  ['Green', 0, 255, 90],
  ['Cyan', 0, 210, 255],
  ['Blue', 40, 90, 255],
  ['White', 255, 255, 255]
];

(function buildSwatches() {
  var box = $('#swatches');
  box.innerHTML = '';
  PRESETS.forEach(function (p) {
    var b = document.createElement('button');
    b.type = 'button';
    b.title = p[0];
    b.dataset.rgb = p[1] + ',' + p[2] + ',' + p[3];
    b.style.background = 'rgb(' + p[1] + ',' + p[2] + ',' + p[3] + ')';
    b.addEventListener('click', function () {
      pushConfig({ red: p[1], green: p[2], blue: p[3] });
    });
    box.appendChild(b);
  });
})();

$('#bright').addEventListener('input', function (e) {
  $('#brightVal').textContent = e.target.value;
});

$('#bright').addEventListener('change', function (e) {
  pushConfig({ brightness: e.target.value });
});

//* Tunable sliders (Strip, Step flame, Pulse whip and Rainbow flow). Live readout
//* while dragging, a single write on release so a drag does not hammer the flash
//* with one save per pixel of travel.
//* Each entry is [element id, API field, optional readout formatter] - the flame
//* readouts echo the unit the firmware documents them in, so 0.90 does not read
//* as the bare number it is stored as.
[['leds', 'leds'],
 ['stbal', 'step_baseline_tau_ms', function (v) { return v + ' ms'; }],
 ['stthr', 'step_threshold', function (v) { return Number(v).toFixed(2) + ' g'; }],
 ['strel', 'step_release', function (v) { return Number(v).toFixed(2) + ' g'; }],
 ['stjerk', 'step_jerk_min', function (v) { return Number(v).toFixed(0) + ' g/s'; }],
 ['stming', 'step_min_gap_ms', function (v) { return v + ' ms'; }],
 ['stimin', 'step_impact_min', function (v) { return Number(v).toFixed(2) + ' g'; }],
 ['stimax', 'step_impact_max', function (v) { return Number(v).toFixed(2) + ' g'; }],
 ['stmagmin', 'step_magnitude_min'],
 ['stmagmax', 'step_magnitude_max'],
 ['fldec', 'flame_decay', function (v) { return Number(v).toFixed(3); }],
 ['flspat', 'flame_spatial', function (v) { return Number(v).toFixed(1) + ' x'; }],
 ['flpslope', 'flame_power_slope', function (v) { return Number(v).toFixed(1) + ' /mag'; }],
 ['flpmax', 'flame_power_max'],
 ['flpidle', 'flame_power_idle'],
 ['flrg', 'flame_red_gain', function (v) { return v + '%'; }],
 ['flgg', 'flame_green_gain', function (v) { return v + '%'; }],
 ['flbg', 'flame_blue_gain', function (v) { return v + '%'; }],
 ['wspeed', 'whip_speed'],
 ['wlength', 'whip_length'],
 ['wperiod', 'whip_period'],
 ['wpeak', 'whip_peak'],
 ['woffset', 'whip_offset'],
 ['rbspd', 'rainbow_speed'],
 ['rbmin', 'rainbow_min_ms'],
 ['rbmax', 'rainbow_max_ms']].forEach(function (f) {
  var el = document.getElementById(f[0]);
  var show = f[2] || function (v) { return v; };
  el.addEventListener('input', function () {
    document.getElementById(f[0] + 'Val').textContent = show(el.value);
  });
  el.addEventListener('change', function () {
    var data = {};
    data[f[1]] = el.value;
    pushConfig(data);
  });
});

//* Preset slots. The bar, the picker and the slot count all come from
//* /api/presets, so the firmware owns how many slots exist and what they are
//* called; this page never hardcodes either.
var presetSlots = [];

function presetInfo(msg) {
  $('#presetInfo').textContent = msg;
}

function presetLoad() {
  return api('/api/presets').then(function (list) {
    presetSlots = list;

    var box = $('#presets');
    box.innerHTML = '';
    list.forEach(function (p) {
      var b = document.createElement('button');
      b.type = 'button';
      b.textContent = p.used ? p.name : 'Slot ' + (p.slot + 1) + ' empty';
      b.disabled = !p.used;
      b.title = p.used ? 'Apply "' + p.name + '"' : 'Nothing saved in this slot yet';
      if (p.used) {
        b.addEventListener('click', function () { presetApply(p.slot); });
      }
      box.appendChild(b);
    });

    var sel = $('#presetSlot');
    sel.innerHTML = '';
    list.forEach(function (p) {
      var o = document.createElement('option');
      o.value = p.slot;
      o.textContent = 'Slot ' + (p.slot + 1) + (p.used ? ': ' + p.name : ' (empty)');
      sel.appendChild(o);
    });

    presetInfo(list.filter(function (p) { return p.used; }).length +
               ' of ' + list.length + ' slots used.');
  });
}

function presetApply(slot) {
  presetInfo('Applying slot ' + (slot + 1) + '...');
  api('/api/preset', { slot: slot, action: 'apply' }).then(function (s) {
    state = s;
    render();
    presetInfo('Applied slot ' + (slot + 1) + '.');
  }).catch(function (e) { presetInfo('Could not apply: ' + e.message); });
}

$('#presetSave').addEventListener('click', function () {
  var slot = Number($('#presetSlot').value);
  var saved = presetSlots[slot];
  var fallback = (saved && saved.used) ? saved.name : 'Preset ' + (slot + 1);
  var name = prompt('Name for slot ' + (slot + 1) + ':', fallback);
  if (name === null) { return; } // Cancelled - keep whatever the slot holds
  presetInfo('Saving slot ' + (slot + 1) + '...');
  api('/api/preset', { slot: slot, action: 'save', name: name }).then(presetLoad)
    .then(function () { presetInfo('Saved to slot ' + (slot + 1) + '.'); })
    .catch(function (e) { presetInfo('Could not save: ' + e.message); });
});

$('#presetExport').addEventListener('click', function () {
  var used = presetSlots.filter(function (p) { return p.used; });
  if (!used.length) {
    presetInfo('Nothing to export: no slot has been saved.');
    return;
  }
  presetInfo('Exporting ' + used.length + ' slot(s)...');
  Promise.all(used.map(function (p) { return api('/api/preset?slot=' + p.slot); }))
    .then(function (docs) {
      //* The file is assembled here, not on the device: field names, formatting
      //* and the wrapper cost the firmware nothing, and any JSON editor can
      //* read or hand-write one.
      var doc = {
        mavuika_presets: 1,
        device: state ? state.hostname : '',
        slots: docs.length,
        presets: docs
      };
      var a = document.createElement('a');
      a.href = URL.createObjectURL(new Blob([JSON.stringify(doc, null, 1)],
                                            { type: 'application/json' }));
      a.download = 'mavuika-presets.json';
      a.click();
      URL.revokeObjectURL(a.href);
      presetInfo('Exported ' + docs.length + ' preset(s).');
    }).catch(function (e) { presetInfo('Could not export: ' + e.message); });
});

$('#presetImport').addEventListener('change', function (e) {
  var file = e.target.files[0];
  if (!file) { return; }

  var reader = new FileReader();
  reader.onload = function () {
    var doc;
    try {
      doc = JSON.parse(reader.result);
    } catch (err) {
      presetInfo('That file is not JSON.');
      e.target.value = '';
      return;
    }

    var list = (doc && doc.presets) || [];
    if (!list.length) {
      presetInfo('No presets in that file.');
      e.target.value = '';
      return;
    }

    //! Sequential on purpose. The device has no endpoint that writes values
    //! into a slot directly - a slot is saved from the LIVE configuration - so
    //! importing one means posting it to /api/config and then saving. Running
    //! those in parallel would interleave the writes and could store whichever
    //! configuration happened to land last.
    var chain = Promise.resolve();
    var done = 0;
    var skipped = 0;

    list.forEach(function (p) {
      chain = chain.then(function () {
        if (typeof p.slot !== 'number' || p.slot < 0 || p.slot >= presetSlots.length || !p.config) {
          skipped++;
          return;
        }
        return api('/api/config', p.config)
          .then(function () {
            return api('/api/preset', { slot: p.slot, action: 'save', name: p.name || '' });
          })
          .then(function () { done++; });
      });
    });

    chain.then(function () {
      e.target.value = '';
      return presetLoad();
    }).then(refresh).then(function () {
      presetInfo('Imported ' + done + ' preset(s)' +
                 (skipped ? ', skipped ' + skipped + ' unrecognised' : '') + '.');
    }).catch(function (err) {
      e.target.value = '';
      presetInfo('Import stopped after ' + done + ': ' + err.message);
    });
  };

  reader.readAsText(file);
});

api('/api/modes').then(function (modes) {
  var box = $('#modes');
  box.innerHTML = '';
  modes.forEach(function (m) {
    var b = document.createElement('button');
    b.type = 'button';
    b.textContent = m.label;
    b.dataset.id = m.id;
    b.title = m.key;
    b.addEventListener('click', function () { setMode(m.id); });
    box.appendChild(b);
  });
  //? Read through refresh() first so the export file can name the device, and
  //? keep a slot-list failure separate from a mode-list failure: an empty preset
  //! bar must not be reported as "could not load modes".
  return refresh().then(presetLoad).catch(function (e) {
    presetInfo('Could not read the preset slots: ' + e.message);
  });
}).catch(function (e) { showError('Could not load modes: ' + e.message); });

setInterval(refresh, 3000);
</script>
</body>
</html>
)HTMLPAGE";

void webpage_handle_root()
{
    web_server.sendHeader(F("Cache-Control"), F("no-store"));
    web_server.send_P(200, "text/html", WEBPAGE_HTML);
}

//* ---------------------- LIFECYCLE ----------------------

void setup_webpage()
{
    webpage_config_load();

    if (WiFi.getMode() == WIFI_OFF)
    {
        Serial.println("[WEB] Radio is off - configuration page not started");
        return;
    }

    //* Friendly .local alias. Harmless when the LAN has no mDNS resolver.
    //! Do NOT treat a false return as "mDNS is unavailable". ArduinoOTA.begin()
    //! already initialised the responder before the WiFi join (see
    //! update_firmware_ota.h, setup_ESP32), and mdns_init() cannot run twice -
    //! the second call fails with ESP_ERR_INVALID_STATE and returns false even
    //! though the responder is up and answering to the same NETWORK_HOSTNAME.
    //! Bailing out on it is what used to leave the HTTP service unadvertised.
    if (WiFi.status() == WL_CONNECTED)
    {
        if (!MDNS.begin(WEBPAGE_MDNS_HOST))
            Serial.println("[WEB] mDNS responder already running - reusing its hostname");
        MDNS.addService("http", "tcp", WEBPAGE_HTTP_PORT);
    }

    //? WiFiManager's provisioning portal has already been torn down here (see
    //? wifi_settings.h), so port 80 is free for the persistent page.
    web_server.on("/", HTTP_GET, webpage_handle_root);
    web_server.on("/api/state", HTTP_GET, webpage_handle_state);
    web_server.on("/api/modes", HTTP_GET, webpage_handle_modes);
    web_server.on("/api/mode", HTTP_POST, webpage_handle_set_mode);
    web_server.on("/api/config", HTTP_POST, webpage_handle_set_config);
    //* Preset slots (services/presets.h). The slot list is what the page draws
    //* its bar from; a single slot carries its values, and is what an export
    //* reads, so the file the browser writes needs no value handling on device.
    web_server.on("/api/presets", HTTP_GET, webpage_handle_presets);
    web_server.on("/api/preset", HTTP_GET, webpage_handle_preset);
    web_server.on("/api/preset", HTTP_POST, webpage_handle_set_preset);
    web_server.onNotFound(webpage_handle_not_found);
    web_server.begin();

    webpage_started = true;

    Serial.printf("[WEB] Device configuration at %s\n", webpage_url().c_str());
    if (WiFi.status() == WL_CONNECTED)
        Serial.printf("[WEB] Also reachable at http://%s.local/\n", WEBPAGE_MDNS_HOST);
}

void webpage_loop()
{
    if (!webpage_started)
        return;
    web_server.handleClient();
}

#endif // SERVICE_WEBPAGE
