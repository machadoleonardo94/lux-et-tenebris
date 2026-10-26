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
//? Access: every route is behind the optional login below, and that login is OFF
//? until a password is stored - so a board out of the box answers anyone on the
//! network exactly as it always did. Storing a password turns the check on for
//* every route at once, firmware upload included. See WEBPAGE_AUTH_NAMESPACE.

#define WEBPAGE_HTTP_PORT 80
//? The .local name this page is served under. Single source of truth lives in
//? shared/variables.h, because the DHCP hostname and the OTA name must match it.
#define WEBPAGE_MDNS_HOST NETWORK_HOSTNAME
#define WEBPAGE_NVS_NAMESPACE "device"

//* The login lives in a namespace of its OWN, never in "device". That is what
//? makes a recovery boot's wipe exact: clearing "wifi" and "auth" removes the
//! saved networks and the login, while "device" - mode, palette, every tunable
//* and the presets - is never opened at all.
#define WEBPAGE_AUTH_NAMESPACE "auth"
#define WEBPAGE_AUTH_USER_KEY "user"
#define WEBPAGE_AUTH_PASS_KEY "pass"
//? Shown by the browser in its login dialog. Not a secret, and not the hostname.
#define WEBPAGE_AUTH_REALM "Lightpack"
//? Used when a password is stored with an empty username. Every browser wants a
//* username, and refusing the password instead would silently leave the page open.
#define WEBPAGE_AUTH_DEFAULT_USER "admin"

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

//* Firmware upload endpoints. Their implementations live in
//* components/ESP32/features/update_firmware_web.h, included after this header
//* for the same reason as the preset handlers above: they need web_server, the
//* JSON helper and the route table needs only their names.
void webpage_handle_update_info();
void webpage_handle_update_upload();
void webpage_handle_update_done();
void webpage_update_poll();

//* ---------------------- LOGIN ----------------------
//? HTTP Basic, with one rule that matters: an EMPTY PASSWORD means no login at
//? all. The page then answers exactly as it did before any of this existed, which
//* is the shipped default, and storing a password is the only thing that arms it.
//! What this buys and what it does not: the credentials are base64, not encrypted,
//? so it keeps a passer-by on the LAN out and stops there. It covers this server
//! only - provisioning has its own page on the same port - and there is no
//* session, so the browser re-sends the pair with every single request.
void webpage_auth_load();
void webpage_auth_store(const char *user, const char *password);
void webpage_auth_clear();
bool webpage_auth_enabled();
bool webpage_auth_allows();
bool webpage_auth_ok();
WebServer::THandlerFunction webpage_auth_gate(WebServer::THandlerFunction handler);
void webpage_handle_auth_get();
void webpage_handle_auth_set();

//* The network, from the page: the two saved slots, and a scan the page can run
//? without blocking anything. Both implementations live in this header next to
//* the auth handlers, because both need web_server and the JSON helper.
void webpage_handle_wifi_get();
void webpage_handle_wifi_set();
void webpage_handle_wifi_scan();
void webpage_handle_wifi_scan_start();

//* The stored login, cached in RAM. Every route reads it and /api/buttons is
//? polled four times a second, so asking NVS per request would be a flash read per
//* poll to answer a question that only changes when this page writes it.
String webpage_auth_user_value;
String webpage_auth_password_value;

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
  Serial.printf("[WEB] Flame: power slope=%.1f max=%u idle=%u (drawn in the palette)\n",
                flame.power_slope, flame.power_max, flame.power_idle);
  Serial.printf("[WEB] Rainbow: speed=%.2f gap=%u..%ums\n",
                rainbow.speed, rainbow.min_ms, rainbow.max_ms);
}

void webpage_config_save()
{
  //? Called from two places: an explicit POST from the page, and led_mode_next()
  //? on the button path, so a mode chosen with the pad survives a reboot exactly
  //! like one chosen in a browser. The note that used to sit here claimed this ran
  //* "only on an explicit POST from the page, never from the loop" - which stopped
  //* being true the moment the button started saving through it.
  //?
  //? The wear that note was guarding against is not a concern here, and the reason
  //? is worth recording so it does not have to be re-derived: of the 28 set calls
  //? below, Storage::writeItem() compares each value against what is already
  //* stored and returns WITHOUT writing when they match - primitives through
  //? cmpItem(), blobs (the ten putFloat calls) through cmpMultiPageBlob(). A mode
  //? change therefore writes exactly one 32-byte entry, the `mode` key; a page
  //* holds 126 of them and flash is rated on the order of 100k erases per sector,
  //? so a page erase lands about every 126 mode changes. nvs_commit() is a no-op
  //* in this IDF version, so the per-put calls to it cost nothing either.
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

  //* Which of the three network states the board is in. Reaching this endpoint at
  //? all means NET_CONNECTED, so this is mostly for the page's own consistency -
  //* but it is also what says whether the radio is on, which the page cannot infer.
  json += F(",\"net\":\"");
  json += wifi_state_name();
  json += F("\",\"offline_mode\":");
  json += (net_state == NET_OFFLINE) ? F("true") : F("false");

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
  //* Limits and the detector's scale, shipped so retuning a range or the
  //* magnitude window in the firmware cannot leave the page asking for values
  //* that would only be clamped away.
  json += F(",\"flame_decay_max\":");
  json += String(FLAME_DECAY_MAX, 3);
  json += F(",\"flame_spatial_max\":");
  json += String(FLAME_SPATIAL_MAX, 1);
  json += F(",\"flame_power_slope_max\":");
  json += String(FLAME_POWER_SLOPE_MAX, 1);

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
  json += F("\",\"wifi_ssid\":\"");
  //? The network the board is aiming at, not necessarily the one it is on: in
  //* NET_CONNECTING those differ, and that is exactly the state worth seeing.
  json += webpage_json_escape(wifi_saved_ssid().c_str());
  json += F("\"}\n");

  web_server.send(200, "application/json", json);
}

//* Both switches, polled by the page. Deliberately separate from /api/state:
//? that document is ~2.6 KB of settings the page only needs on change, while
//? this one is polled four times a second to show a press as it happens, so it is
//? kept to the handful of numbers the input panels draw.
//!
//! This endpoint is why check_button() (services/readings.h) stops blocking. The
//! old release-wait held loop() - and therefore web_server.handleClient() - for
//! the whole press, so a request sent while a switch was down was answered only
//! after the finger came off, and no panel could ever show "held".
void webpage_handle_buttons()
{
  const uint32_t now = millis();

  String json;
  //? Two switches, so roughly twice the old single-button document.
  json.reserve(768);
  //* The mode the firmware is actually on. A 1 s press on the touch button
  //! changes it on the device, and the page's mode buttons have to notice that
  //! without waiting for its own slower /api/state poll.
  json += F("{\"mode\":");
  json += String(mode);
  //* The thresholds that are not per-switch, sent once instead of in each row.
  //? multi_gap_ms now only governs the running "recent presses" counter shown on
  //* each panel. The gesture window that actually drives behaviour is click_gap_ms:
  //? the wait after the last click before the firmware decides which gesture the
  //! user made. triple_count is gone - clicks are grouped by the caller now, so
  //* three presses are just three SHORT events (see check_boot_button()).
  json += F(",\"multi_gap_ms\":");
  json += String(BUTTON_MULTI_PRESS_GAP_MS);
  json += F(",\"click_gap_ms\":");
  json += String(BUTTON_CLICK_GAP_MS);
  json += F(",\"click_max_ms\":");
  json += String(BUTTON_CLICK_MAX_MS);
  //* The switch list comes from the firmware's own table (services/readings.h),
  //? so adding a switch there adds a panel to the page with no change here.
  json += F(",\"buttons\":[");

  for (uint8_t i = 0; i < BUTTON_INPUT_COUNT; i++)
  {
    const struct_buttonInput *b = button_inputs[i];
    if (i > 0)
      json += ',';

    json += F("{\"key\":\"");
    json += webpage_json_escape(b->key);
    json += F("\",\"label\":\"");
    json += webpage_json_escape(b->label);
    json += F("\",\"role\":\"");
    json += webpage_json_escape(b->role);
    json += F("\",\"action\":\"");
    json += webpage_json_escape(b->action);
    json += F("\",\"pin\":");
    json += String(b->pin);
    json += F(",\"pressed\":");
    json += b->pressed ? F("true") : F("false");
    //* Elapsed time of the press in flight, so the page can count up without
    //* doing clock arithmetic against a device it is not synchronised with.
    json += F(",\"held_now_ms\":");
    json += String(b->pressed ? (unsigned)(now - b->press_start_ms) : 0);
    json += F(",\"hold_ms\":");
    json += String((unsigned)b->hold_ms);
    json += F(",\"counter\":");
    json += String((unsigned)b->counter);
    json += F(",\"events\":");
    json += String((unsigned)b->event_count);
    json += F(",\"last_event\":");
    json += String((unsigned)b->last_event);
    //* Age rather than a timestamp: the two ends have no shared clock, and the
    //? page only ever asks "how long ago was that".
    json += F(",\"last_event_age_ms\":");
    //! Not a ternary over F("-1") and String: the two have no common type.
    if (b->last_event_ms == 0)
      json += F("-1");
    else
      json += String((unsigned)(now - b->last_event_ms));
    //* The thresholds in force, so a panel explains the behaviour with the
    //? same numbers check_button() applies rather than a copy of them. A zero
    //* means this switch does not act at that threshold at all.
    json += F(",\"long_press_ms\":");
    json += String(b->long_press_ms);
    json += F(",\"action_hold_ms\":");
    json += String(b->action_hold_ms);
    json += F(",\"monitor_timeout_ms\":");
    json += String(b->monitor_timeout_ms);
    json += F("}");
  }

  json += F("]}\n");

  //? Same reasoning as /api/state: this answer is only ever fresh for an
  //? instant, and a cached one would show a press that is long over.
  web_server.sendHeader(F("Cache-Control"), F("no-store"));
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
  Serial.printf("[WEB] Flame -> decay=%.3f spatial=%.2f power=%.1f max=%u idle=%u (drawn in the palette)\n",
                flame.decay, flame.spatial, flame.power_slope, flame.power_max,
                flame.power_idle);
  Serial.printf("[WEB] Rainbow -> speed=%.2f gap=%u..ums\n",
                rainbow.speed, rainbow.min_ms, rainbow.max_ms);

  webpage_handle_state();
}

//* ---------------------- LOGIN ----------------------

void webpage_auth_clear()
{
  preferences.begin(WEBPAGE_AUTH_NAMESPACE, false);
  preferences.remove(WEBPAGE_AUTH_USER_KEY);
  preferences.remove(WEBPAGE_AUTH_PASS_KEY);
  preferences.end();

  webpage_auth_user_value = "";
  webpage_auth_password_value = "";
}

void webpage_auth_load()
{
  webpage_auth_user_value = "";
  webpage_auth_password_value = "";

  //? A namespace that has never been written is not an error here: it is a board
  //* with no login, which is the state this ships in.
  if (preferences.begin(WEBPAGE_AUTH_NAMESPACE, true))
  {
    webpage_auth_user_value = preferences.getString(WEBPAGE_AUTH_USER_KEY, "");
    webpage_auth_password_value = preferences.getString(WEBPAGE_AUTH_PASS_KEY, "");
    preferences.end();
  }

  if (webpage_auth_enabled())
    Serial.printf("[WEB] Login required (user '%s')\n", webpage_auth_user_value.c_str());
  else
    Serial.println("[WEB] No login set - the page answers anyone on this network");
}

//* A stored password is the whole of "on". Nothing else arms the check, so a
//* board that has never been given one behaves exactly as it did before.
bool webpage_auth_enabled()
{
  return webpage_auth_password_value.length() > 0;
}

void webpage_auth_store(const char *user, const char *password)
{
  String name = (user != nullptr) ? String(user) : String("");
  const String pass = (password != nullptr) ? String(password) : String("");
  name.trim();

  if (pass.length() == 0)
  {
    //* Off, and off means the keys are GONE rather than stored empty, which is
    //* what the recovery wipe does to them as well.
    webpage_auth_clear();
    Serial.println("[WEB] Login disabled - the page is open again");
    return;
  }

  if (name.length() == 0)
    name = F(WEBPAGE_AUTH_DEFAULT_USER);

  preferences.begin(WEBPAGE_AUTH_NAMESPACE, false);
  preferences.putString(WEBPAGE_AUTH_USER_KEY, name);
  preferences.putString(WEBPAGE_AUTH_PASS_KEY, pass);
  preferences.end();

  webpage_auth_user_value = name;
  webpage_auth_password_value = pass;
  Serial.printf("[WEB] Login enabled for user '%s'\n", name.c_str());
}

//* The check without a reply, for the one caller that runs while a request body is
//? still arriving - the firmware upload callback, which must refuse to write
//* without also trying to answer a client that is mid-transfer.
bool webpage_auth_allows()
{
  if (!webpage_auth_enabled())
    return true;

  return web_server.authenticate(webpage_auth_user_value.c_str(),
                                 webpage_auth_password_value.c_str());
}

bool webpage_auth_ok()
{
  if (webpage_auth_allows())
    return true;

  //! requestAuthentication() is what actually sends the 401 and the
  //? WWW-Authenticate header a browser turns into its login dialog.
  //! authenticate() only answers the question - without this the client would get
  //* an empty, perfectly successful reply and no reason to offer a password.
  web_server.requestAuthentication(BASIC_AUTH, WEBPAGE_AUTH_REALM);
  return false;
}

//* Wrap a handler so the whole route table is covered from one place. A handler
//? that forgets the check is the failure this exists to make impossible, and it
//* costs one line per route rather than a guard inside every handler body.
WebServer::THandlerFunction webpage_auth_gate(WebServer::THandlerFunction handler)
{
  return [handler]()
  {
    if (webpage_auth_ok())
      handler();
  };
}

//* GET /api/auth: is a login in force, and for which name.
void webpage_handle_auth_get()
{
  String json;
  json.reserve(96);
  json += F("{\"ok\":true,\"enabled\":");
  json += webpage_auth_enabled() ? F("true") : F("false");
  json += F(",\"user\":\"");
  //! The password never leaves the device, and the username only while a login is
  //? on: with the login off this document is readable by anyone who asks, and
  //* there is no name to report anyway.
  json += webpage_json_escape(webpage_auth_enabled() ? webpage_auth_user_value.c_str() : "");
  json += F("\"}\n");
  web_server.send(200, "application/json", json);
}

//* POST /api/auth: user + pass. An empty pass turns the login off.
void webpage_handle_auth_set()
{
  webpage_auth_store(web_server.arg("user").c_str(), web_server.arg("pass").c_str());

  String json;
  json.reserve(96);
  json += F("{\"ok\":true,\"enabled\":");
  json += webpage_auth_enabled() ? F("true") : F("false");
  json += F(",\"user\":\"");
  json += webpage_json_escape(webpage_auth_enabled() ? webpage_auth_user_value.c_str() : "");
  json += F("\"}\n");
  web_server.send(200, "application/json", json);
}

//* ---------------------- WIFI, FROM THE PAGE ----------------------
//* A join the page asked for, deferred to the next pass of webpage_loop().
//! The handler must not start it. Moving to another network tears this page down,
//? so `wifi_join_begin()` reaches `webpage_stop()` - and stopping the server from
//! inside one of its own handlers closes the socket the reply is still travelling
//* on. This is the same reason the firmware upload defers its reboot.
uint8_t webpage_wifi_join_slot = 0;
bool webpage_wifi_join_pending = false;

void webpage_wifi_poll()
{
  if (!webpage_wifi_join_pending)
    return;

  webpage_wifi_join_pending = false;
  wifi_join_begin(webpage_wifi_join_slot);
}

//? The page's own view of the two saved networks. A document of its own rather
//* than a corner of /api/state, for the same reason /api/buttons is: the slots
//? and the last join's outcome only matter while someone is looking at them, and
//* /api/state is polled every three seconds for as long as the board is up.
//!
//? Saved SSIDs are reported here and their passwords never are. An SSID is
//? broadcast over the air by the access point itself, so it is not a secret; the
//* password is the only part of a credential this page must not hand back.

//* GET /api/wifi: the slots, which one is in use, and how the last join the page
//* asked for ended.
void webpage_handle_wifi_get()
{
  String json;
  json.reserve(360);
  json += F("{\"ok\":true,\"state\":\"");
  json += wifi_state_name();
  json += F("\",\"offline\":");
  json += (net_state == NET_OFFLINE) ? F("true") : F("false");
  json += F(",\"join_error\":\"");
  json += webpage_json_escape(wifi_join_error.c_str());
  json += F("\",\"slots\":[");

  for (uint8_t slot = 0; slot < WIFI_CREDENTIAL_SLOTS; slot++)
  {
    if (slot != 0)
      json += ',';

    json += F("{\"slot\":");
    json += String((unsigned)slot);
    json += F(",\"saved\":");
    json += wifi_credential_present(slot) ? F("true") : F("false");
    json += F(",\"active\":");
    json += (slot == wifi_credential_active()) ? F("true") : F("false");
    json += F(",\"ssid\":\"");
    json += webpage_json_escape(wifi_credential_ssid(slot).c_str());
    json += F("\"}");
  }

  json += F("]}\n");
  web_server.send(200, "application/json", json);
}

//* POST /api/wifi: ssid + pass into one slot. An empty ssid forgets the slot.
void webpage_handle_wifi_set()
{
  const uint8_t slot = wifi_credential_clamp((uint8_t)web_server.arg("slot").toInt());
  const String ssid = web_server.arg("ssid");

  //? keep_password is how the page says "this field is empty because the stored
  //! password is never shown, not because the network is open". Without it, saving
  //* an otherwise unchanged network through the form would wipe its password.
  const bool keep_password = web_server.hasArg("keep_password") && wifi_credential_present(slot);

  String json;
  json.reserve(200);
  json += F("{\"ok\":true,\"slot\":");
  json += String((unsigned)slot);

  if (ssid.length() == 0)
  {
    wifi_credential_clear(slot);
    json += F(",\"saved\":false");
  }
  else
  {
    const String password = keep_password ? wifi_credential_password(slot) : web_server.arg("pass");
    wifi_credential_store(slot, ssid.c_str(), password.c_str());

    //* A network typed into this page is a provision, so the offline switch goes
    //? off with it - the same rule the config portal follows, and the reason a
    //* board parked offline can be brought back from here at all.
    wifi_offline_store(false);

    json += F(",\"saved\":true,\"ssid\":\"");
    json += webpage_json_escape(ssid.c_str());
    json += F("\"");
  }

  //* Both slots empty leaves nothing to aim at, and wifi_loop() parks the radio
  //? the moment the current link drops. Reporting it here is the difference
  //* between a setting the user chose and a surprise they did not.
  json += F(",\"has_credentials\":");
  json += wifi_has_credentials() ? F("true") : F("false");
  json += F("}\n");

  web_server.send(200, "application/json", json);

  //! Flushed before the radio can move: a join on a different network takes this
  //? page down with it, and a reply still sitting in the socket buffer would turn
  //* a request that worked into a network error in the browser.
  web_server.client().flush();

  //* Deferred to webpage_wifi_poll() rather than started here: see the note over
  //* it, and the one over wifi_join_begin().
  if (ssid.length() > 0)
  {
    webpage_wifi_join_slot = slot;
    webpage_wifi_join_pending = true;
  }
}

//* POST /api/wifi/scan: start one. A route of its own rather than a flag on the
//* GET, because starting a scan is a change of radio state and the page polls the
//* GET while it runs.
void webpage_handle_wifi_scan_start()
{
  const bool started = wifi_scan_start();

  String json;
  json.reserve(80);
  json += F("{\"ok\":true,\"scanning\":");
  json += started ? F("true") : F("false");
  json += F("}\n");
  web_server.send(200, "application/json", json);
}

//* GET /api/wifi/scan: how the scan is doing, and its results once there are any.
void webpage_handle_wifi_scan()
{
  const int16_t status = wifi_scan_status();

  String json;
  json.reserve(1100);
  json += F("{\"ok\":true,\"scanning\":");
  json += (status == WIFI_SCAN_RUNNING) ? F("true") : F("false");
  json += F(",\"failed\":");
  json += (status == WIFI_SCAN_FAILED) ? F("true") : F("false");
  json += F(",\"networks\":[");

  if (status > 0)
  {
    for (int16_t i = 0; i < status; i++)
    {
      if (i != 0)
        json += ',';

      const String found = WiFi.SSID(i);
      const int8_t saved = wifi_credential_find(found.c_str());

      json += F("{\"ssid\":\"");
      json += webpage_json_escape(found.c_str());
      json += F("\",\"rssi\":");
      json += String(WiFi.RSSI(i));
      json += F(",\"channel\":");
      json += String(WiFi.channel(i));
      json += F(",\"secure\":");
      json += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? F("false") : F("true");
      json += F(",\"saved\":");
      json += (saved >= 0) ? F("true") : F("false");
      json += F("}");
    }

    //* Released as soon as it has been read: this response IS the only copy of the
    //? result, and holding it would keep the driver's buffer alive until the next
    //* scan started.
    wifi_scan_release();
  }

  json += F("]}\n");
  web_server.send(200, "application/json", json);
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
<title>Lightpack &middot; Device Configuration</title>
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
/* The only free-text fields on the page: the login in the Security section. */
input[type=text],input[type=password]{flex:1;min-width:0;font:inherit;padding:.65rem;
  border-radius:.6rem;border:1px solid #3a3450;background:#1e1b29;color:#e8e4f0}
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
/* Upload progress. The bar is fed by the browser's own upload events, not by
   the device: a transfer blocks the device's HTTP service outright, so nothing
   on the board can answer "how far along am I" while it is happening. */
.bar{height:.6rem;border-radius:.4rem;background:#2b2738;overflow:hidden;margin:.7rem 0 0}
.bar div{height:100%;width:0;background:#8a63e8;transition:width .15s linear}
/* Button indicator. The dot is filled while the switch reads down, which is
   only visible at all because the firmware services HTTP during a press. */
.dot{width:.85rem;height:.85rem;border-radius:50%;background:#2b2738;
     border:1px solid #3a3450;flex:0 0 auto;transition:background .1s,box-shadow .1s}
.dot.down{background:#8a63e8;border-color:#a184f0;
          box-shadow:0 0 0 .22rem rgba(138,99,232,.22)}
.btnstate{font-variant-numeric:tabular-nums}
/* One block per switch: the live line, its counters, and what the firmware does
   with it. Built by buildInputs() from the device's own list. */
.inputs{display:grid;gap:.8rem}
.input{border:1px solid #2b2738;border-radius:.6rem;padding:.7rem .8rem}
.input .row{gap:.6rem}
.input h3{flex:1;margin:0;font-size:.85rem;font-weight:600;text-transform:none;
          letter-spacing:0}
.input dl{margin-top:.6rem}
output{min-width:2.75rem;text-align:right;font-variant-numeric:tabular-nums;color:#c9c1e0}
dl{display:grid;grid-template-columns:auto 1fr;gap:.3rem .9rem;margin:0;font-size:.9rem}
dt{color:#9d93b8}
dd{margin:0;text-align:right;font-variant-numeric:tabular-nums;word-break:break-all}
.banner{background:#4a1f2b;border:1px solid #7d3348;color:#ffd9e2;padding:.6rem .8rem;
        border-radius:.6rem;font-size:.85rem;margin-top:.6rem;display:none}
/* The "no login is set" prompt. Amber, not red: nothing has gone wrong, the page
   is just telling the truth about who can reach it. */
.banner.warn{background:#3a3320;border-color:#7d6a33;color:#ffe9b8}
.banner a{color:inherit}
</style>
</head>
<body>
<h1>Lightpack</h1>
<p class="sub">Device configuration</p>
<div id="error" class="banner"></div>
<div id="authWarn" class="banner warn">No login is set, so anyone who can reach this
  device can change the light or install firmware. Set a user and a password under
  <a href="#security">Security</a>.</div>

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
  <p class="hint">Drives the <em>Solid color</em> mode and sets the colour of the
     <em>Pulse whip</em> hump and the <em>Step flame</em>. The gyro-driven modes
     and <em>Rainbow flow</em> compute their own colour.</p>
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
     settles at. The colour is the one picked under <strong>Palette</strong>: the
     step sets how bright each LED is, the palette sets what colour that brightness
     is, so a black palette draws no flame.</p>
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
  <p class="hint">Applies to every mode.</p>
</section>

<section>
  <h2>Status</h2>
  <dl id="status"></dl>
</section>

<section>
  <h2>WiFi</h2>
  <div id="wifiSlots" class="inputs"></div>
  <div class="row">
    <button type="button" id="wifiScanGo">Scan for networks</button>
    <span class="hint" id="wifiScanInfo"></span>
  </div>
  <div id="wifiFound" class="inputs"></div>
  <p class="hint" id="wifiInfo">Checking&hellip;</p>
  <p class="hint">Two networks are saved and tried in turn, one attempt a minute, so
     a phone hotspot that is switched on later is still picked up on its own. What
     you type here is written to the radio itself and survives a reboot. The stored
     password is never sent back to this page, so leaving the password field empty
     keeps the one already saved. The C3 is <strong>2.4&nbsp;GHz only</strong>: an
     iPhone hotspot needs <em>Maximise Compatibility</em> on and an Android one needs
     the 2.4&nbsp;GHz band selected, or it will never appear in a scan. Scanning
     makes this page stutter for a second or two, because the radio has to leave its
     channel to look.</p>
</section>

<section>
  <h2>Inputs</h2>
  <div id="inputs" class="inputs"></div>
  <p class="hint" id="inputInfo"></p>
  <p class="hint">Every switch on the board, reported live from the firmware's own
     counters &mdash; a tap that falls between two polls is still counted below even
     when the indicator never lights. Clicks are grouped into ONE gesture: the
     firmware waits <span id="inputGap">0.4</span>&nbsp;s after your last click before
     it acts, which is what lets it tell a single click from a double. What each
     switch <em>does</em> is the firmware's business and is spelled out on its own
     line here, along with the hold it takes to trigger it.</p>
</section>

<section>
  <h2>Firmware</h2>
  <div class="row">
    <label class="filebtn">Choose firmware.bin<input type="file" id="fwFile"
      accept=".bin,application/octet-stream"></label>
    <button type="button" id="fwGo" disabled>Install</button>
  </div>
  <div class="bar" id="fwBarWrap" style="display:none"><div id="fwBar"></div></div>
  <p class="hint" id="fwInfo">Checking the target slot&hellip;</p>
  <p class="hint">Uploads a firmware image to the <em>inactive</em> flash slot and
     reboots into it. Take the file from
     <code>.pio/build/C3-OTA/firmware.bin</code> - a full flash dump or a merged
     factory image is not an app image and will be rejected. The running firmware
     is never overwritten, and a rejected or interrupted upload leaves the device
     on the firmware it is running now. Settings and presets live in NVS and
     survive the update. The strip freezes while the transfer runs: the whole
     upload is parsed inside one pass of the main loop, and the progress bar is
     measuring the browser's send, not the flash write.</p>
</section>

<section id="security">
  <h2>Security</h2>
  <div class="row">
    <input type="text" id="authUser" placeholder="User" autocomplete="username">
    <input type="password" id="authPass" placeholder="Password"
           autocomplete="new-password">
  </div>
  <div class="row">
    <button type="button" id="authSave">Save login</button>
    <button type="button" id="authOff">Turn off</button>
  </div>
  <p class="hint" id="authInfo">Checking&hellip;</p>
  <p class="hint">An <em>empty password</em> leaves the login off and the page open
     to anyone who can reach it. A saved login is asked for with the browser's own
     dialog: there is no session, so the pair is sent with every request and the
     browser will ask again after a reload. Nothing is ever filled in for you here,
     so the browser may still warn that the connection is not secure while you type
     &mdash; this page is served over plain HTTP, and that warning is what plain
     HTTP means. This covers this page and the firmware upload only; the config
     hotspot serves its own page and has no login. Forgetting the password is not a
     dead end: hold <strong>GPIO9</strong> for ten seconds and let go, and the saved
     networks and this login are cleared while every light setting is kept.</p>
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

/* ---------------------- login ---------------------- */
/* Nothing is filled in here, on purpose, for two reasons. A password this page
   invented would be one it had already handed to every other visitor who loaded it
   while the login was off - so the value the user accepted would be a value
   somebody else knows. And a prefilled password field on a page served over plain
   http is what makes Firefox announce that the connection is insecure and offer to
   save the login. The fields start empty and are emptied again on every load. */
function authInfo(msg) { $('#authInfo').textContent = msg; }

/* The banner at the top of the page. Its text is static markup, so the prompt
   costs one display toggle here rather than a sentence in the firmware. */
function authNotice(on) {
  $('#authWarn').style.display = on ? 'block' : 'none';
}

function authRender(a) {
  authNotice(!a.enabled);

  /* Cleared on every load, including the re-read after a save: whatever was in
     these fields was typed in this session, and it has no business sitting in the
     DOM of a document that will be fetched again over an unencrypted connection. */
  $('#authUser').value = '';
  $('#authPass').value = '';

  if (a.enabled) {
    authInfo('Login is ON for "' + a.user + '". The browser asks for it again after ' +
             'a reload; Turn off clears it.');
  } else {
    authInfo('No login is set, so anyone on this network can change the light. Type ' +
             'a user and a password to turn one on; an empty password leaves the page ' +
             'open.');
  }
}

function authLoad() {
  return api('/api/auth').then(authRender).catch(function (e) {
    authInfo('Could not read the login state: ' + e.message);
  });
}

function authSet(disable) {
  var user = $('#authUser').value.trim();
  var pass = disable ? '' : $('#authPass').value;
  return api('/api/auth', { user: user, pass: pass }).then(function (a) {
    showError('');
    authRender(a);
  }).catch(function (e) { showError('Could not save the login: ' + e.message); });
}

$('#authSave').addEventListener('click', function () { authSet(false); });
$('#authOff').addEventListener('click', function () { authSet(true); });

/* ---------------------- wifi ---------------------- */
var netSlotCount = 0;
var netSavedSlots = {};
var netScanTimer = null;

function netInfo(msg) { $('#wifiInfo').textContent = msg; }

function netScanInfo(msg) { $('#wifiScanInfo').textContent = msg; }

/* One card per slot, built from the device's own list rather than from markup:
   how many networks can be saved is the firmware's business, not this page's. */
function netBuild(w) {
  var keep = {};
  var i;

  /* Whatever is half-typed survives a reload. Saving one network re-reads the
     whole list, and without this the password typed into the other slot would be
     wiped by the refresh that the save itself triggered. */
  for (i = 0; i < netSlotCount; i++) {
    var oldSsid = document.getElementById('wifiSsid' + i);
    var oldPass = document.getElementById('wifiPass' + i);
    if (oldSsid && oldPass) { keep[i] = { ssid: oldSsid.value, pass: oldPass.value }; }
  }

  netSlotCount = w.slots.length;
  netSavedSlots = {};
  var box = $('#wifiSlots');
  box.innerHTML = '';

  w.slots.forEach(function (s) {
    netSavedSlots[s.slot] = s.saved;

    var card = document.createElement('div');
    card.className = 'input';

    var head = document.createElement('h3');
    head.textContent = 'Network ' + (s.slot + 1) +
      (s.saved ? (s.active ? ' - in use' : ' - saved') : ' - empty');
    card.appendChild(head);

    var row = document.createElement('div');
    row.className = 'row';

    var ssid = document.createElement('input');
    ssid.type = 'text';
    ssid.id = 'wifiSsid' + s.slot;
    ssid.placeholder = 'SSID';
    ssid.value = s.ssid || '';
    row.appendChild(ssid);

    var pass = document.createElement('input');
    pass.type = 'password';
    pass.id = 'wifiPass' + s.slot;
    pass.placeholder = s.saved ? 'password (blank = keep)' : 'password';
    pass.autocomplete = 'new-password';
    row.appendChild(pass);
    card.appendChild(row);

    if (keep[s.slot]) {
      if (keep[s.slot].ssid) { ssid.value = keep[s.slot].ssid; }
      if (keep[s.slot].pass) { pass.value = keep[s.slot].pass; }
    }

    var buttons = document.createElement('div');
    buttons.className = 'row';

    var save = document.createElement('button');
    save.type = 'button';
    save.textContent = 'Save';
    save.addEventListener('click', function () { netSave(s.slot); });
    buttons.appendChild(save);

    if (s.saved) {
      var forget = document.createElement('button');
      forget.type = 'button';
      forget.textContent = 'Forget';
      forget.addEventListener('click', function () { netForget(s.slot); });
      buttons.appendChild(forget);
    }

    card.appendChild(buttons);
    box.appendChild(card);
  });
}

function netRender(w) {
  netBuild(w);

  if (w.join_error) {
    netInfo('The last join did not succeed: ' + w.join_error + '. A network that is ' +
            'not in a scan below is switched off, out of range, or on 5 GHz; one ' +
            'that is there usually means the password is wrong.');
  } else if (w.offline) {
    netInfo('Offline mode: the radio is off. Saving a network here switches it back on.');
  } else if (!w.slots.some(function (s) { return s.saved; })) {
    netInfo('No network is saved, so the board is running on its own. A 5 s press of ' +
            'GPIO9 opens the config hotspot instead.');
  } else {
    netInfo('Radio: ' + w.state + '.');
  }
}

function netLoad() {
  return api('/api/wifi').then(netRender).catch(function (e) {
    netInfo('Could not read the saved networks: ' + e.message);
  });
}

function netSave(slot) {
  var ssidEl = $('#wifiSsid' + slot);
  var passEl = $('#wifiPass' + slot);
  var body = { slot: slot, ssid: ssidEl.value.trim(), pass: passEl.value };

  /* A blank field on a slot that already holds a network means "keep the stored
     password" - it is the only thing it can mean, because the device never sends
     that password back for the page to show. On an empty slot, blank means an
     open network, which is the truth. */
  if (!passEl.value && netSavedSlots[slot]) { body.keep_password = 1; }

  return api('/api/wifi', body).then(function (r) {
    showError('');
    passEl.value = '';

    if (!body.ssid) {
      netInfo('Network ' + (slot + 1) + ' forgotten.' +
              (r.has_credentials ? '' :
               ' Nothing is saved now, so the radio switches off when this link drops.'));
    } else {
      netInfo('Saved "' + body.ssid + '". Joining now: this page may vanish for a few ' +
              'seconds while the radio moves, and it will not come back if the ' +
              'password is wrong and no other network is saved.');
    }
    return netLoad();
  }).catch(function (e) { showError('Could not save the network: ' + e.message); });
}

function netForget(slot) {
  $('#wifiSsid' + slot).value = '';
  $('#wifiPass' + slot).value = '';
  return netSave(slot);
}

function netScanRender(list) {
  var box = $('#wifiFound');
  box.innerHTML = '';

  list.forEach(function (n) {
    var card = document.createElement('div');
    card.className = 'input';

    var head = document.createElement('h3');
    head.textContent = n.ssid + (n.saved ? ' - saved' : '');
    card.appendChild(head);

    var meta = document.createElement('p');
    meta.className = 'hint';
    meta.textContent = n.rssi + ' dBm, channel ' + n.channel +
                       (n.secure ? ', secured' : ', open');
    card.appendChild(meta);

    var row = document.createElement('div');
    row.className = 'row';

    for (var slot = 0; slot < netSlotCount; slot++) {
      (function (slot) {
        var b = document.createElement('button');
        b.type = 'button';
        b.textContent = 'Use in ' + (slot + 1);
        b.addEventListener('click', function () {
          $('#wifiSsid' + slot).value = n.ssid;
          $('#wifiPass' + slot).focus();
          netInfo('"' + n.ssid + '" copied into network ' + (slot + 1) +
                  '. Type its password, then Save.');
        });
        row.appendChild(b);
      })(slot);
    }

    card.appendChild(row);
    box.appendChild(card);
  });
}

function netScanPoll() {
  api('/api/wifi/scan').then(function (s) {
    if (s.scanning) { return; }

    clearInterval(netScanTimer);
    netScanTimer = null;
    $('#wifiScanGo').disabled = false;

    if (s.failed) {
      netScanInfo('The scan failed. Try again.');
      return;
    }
    netScanInfo(s.networks.length + ' networks heard (2.4 GHz only).');
    netScanRender(s.networks);
  }).catch(function (e) {
    clearInterval(netScanTimer);
    netScanTimer = null;
    $('#wifiScanGo').disabled = false;
    netScanInfo('Could not read the scan: ' + e.message);
  });
}

function netScan() {
  $('#wifiScanGo').disabled = true;
  netScanInfo('Asking the radio...');

  return api('/api/wifi/scan', {}).then(function () {
    /* Polled, not awaited: the scan runs in the background and the device answers
       this route while it sweeps, which is the whole reason it is asynchronous. */
    netScanTimer = setInterval(netScanPoll, 1000);
  }).catch(function (e) {
    $('#wifiScanGo').disabled = false;
    netScanInfo('Could not start a scan: ' + e.message);
  });
}

$('#wifiScanGo').addEventListener('click', netScan);

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
    'Network': state.wifi_ssid || 'none saved',
    'Signal': state.rssi + ' dBm',
    'LEDs': state.active_leds + ' of ' + state.num_leds,
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

  var flames = [
    ['fldec', state.flame_decay, state.flame_decay.toFixed(3)],
    ['flspat', state.flame_spatial, Number(state.flame_spatial).toFixed(1) + ' x'],
    ['flpslope', state.flame_power_slope, state.flame_power_slope.toFixed(1) + ' /mag'],
    ['flpmax', state.flame_power_max, String(state.flame_power_max)],
    ['flpidle', state.flame_power_idle, String(state.flame_power_idle)]
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
      b.disabled = !p.used;
      if (p.used) {
        b.textContent = p.name;
        b.title = 'Apply "' + p.name + '"';
        b.addEventListener('click', function () { presetApply(p.slot); });
      } else if (p.state === 'incompatible') {
        /* Something IS stored here - this firmware just cannot read it, because
           it was written by a build whose preset struct was laid out differently.
           Saying so is the whole point of the state field: before this, such a
           slot looked identical to one that had never been saved, so a preset
           could disappear without a word beyond one error line on the serial
           console. It is disabled, because there is nothing here to apply. */
        b.textContent = 'Slot ' + (p.slot + 1) + ': old format';
        b.title = 'Saved by another firmware version and not readable here. ' +
                  'Save over it, or import the JSON file again.';
      } else {
        b.textContent = 'Slot ' + (p.slot + 1) + ' empty';
        b.title = 'Nothing saved in this slot yet';
      }
      box.appendChild(b);
    });

    var sel = $('#presetSlot');
    sel.innerHTML = '';
    list.forEach(function (p) {
      var o = document.createElement('option');
      o.value = p.slot;
      o.textContent = 'Slot ' + (p.slot + 1) +
                      (p.used ? ': ' + p.name
                              : (p.state === 'incompatible' ? ' (old format)' : ' (empty)'));
      sel.appendChild(o);
    });

    var stale = list.filter(function (p) { return p.state === 'incompatible'; }).length;
    presetInfo(list.filter(function (p) { return p.used; }).length +
               ' of ' + list.length + ' slots used.' +
               (stale ? ' ' + stale + ' slot(s) hold a preset from an older firmware ' +
                        'version and cannot be read - saving over them, or importing ' +
                        'the JSON file again, replaces them.' : ''));
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
        lightpack_presets: 1,
        device: state ? state.hostname : '',
        slots: docs.length,
        presets: docs
      };
      var a = document.createElement('a');
      a.href = URL.createObjectURL(new Blob([JSON.stringify(doc, null, 1)],
                                            { type: 'application/json' }));
      a.download = 'lightpack-presets.json';
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

//* ---------------------- INPUTS ----------------------
//* The input panels are polled on their own timer, not from refresh(): that one
//* rebuilds the whole page (status rows, every slider, every hint) and running it
//* several times a second would fight the user's own input. /api/buttons is under
//* a kilobyte and this handler touches a few elements per switch.
//?
//? Why 250 ms: the shortest thing worth seeing is a press, and a press that is
//? held for the 1 s mode switch spans four polls at that rate, while a deliberate
//? tap of 100-200 ms can still fall entirely between two of them. Going faster
//? does not fix that - it would take ~100 ms polling to catch a quick tap, which
//? is 10 requests/s of airtime and main-loop time for a device that renders the
//? strip at 20 fps - so the counters and the "last press" lines carry the
//? information the indicator can miss. When nothing has happened for a minute the
//? interval backs off to 1 s and returns to 250 ms on any change.
var INPUT_FAST_MS = 250;
var INPUT_SLOW_MS = 1000;
var INPUT_IDLE_MS = 60000;

var inputTimer = null;
var inputPeriod = 0;
var inputIdleAt = Date.now();
var inputSeen = -1;
var inputMode = null;

//* key -> the elements buildInputs() made for that switch.
var inputEls = {};

var INPUT_EVENTS = {
  0: 'no press yet',
  1: 'short press',
  2: 'long press',
  3: 'held past the timeout - press dropped',
  4: 'held to this switch\'s action',
  5: 'triple press (retired)',
  6: 'recovery wipe'
};

function inputArm(ms) {
  if (inputTimer && inputPeriod === ms) { return; }
  inputPeriod = ms;
  clearInterval(inputTimer);
  inputTimer = setInterval(inputPoll, ms);
}

function agoText(ms) {
  if (ms < 0) { return 'never'; }
  if (ms < 1000) { return ms + ' ms ago'; }
  return (ms / 1000).toFixed(1) + ' s ago';
}

//* One block per switch, built from the device's own list rather than from a copy
//* of it here: a switch added to the table in services/readings.h shows up with no
//* change to this page.
function buildInputs(list) {
  var box = $('#inputs');
  list.forEach(function (b) {
    if (inputEls[b.key]) { return; }

    var wrap = document.createElement('div');
    wrap.className = 'input';

    var row = document.createElement('div');
    row.className = 'row';
    var dot = document.createElement('span');
    dot.className = 'dot';
    var name = document.createElement('h3');
    name.textContent = b.label;
    var stateEl = document.createElement('span');
    stateEl.className = 'btnstate';
    stateEl.textContent = 'released';
    var holdEl = document.createElement('output');
    holdEl.textContent = '-';
    row.appendChild(dot);
    row.appendChild(name);
    row.appendChild(stateEl);
    row.appendChild(holdEl);

    var last = document.createElement('dl');
    //* The switch's own role text, which is where the firmware states what it
    //* does about this input. Kept as textContent: it is a sentence, not markup.
    var role = document.createElement('p');
    role.className = 'hint';
    role.textContent = b.role;

    wrap.appendChild(row);
    wrap.appendChild(last);
    wrap.appendChild(role);
    box.appendChild(wrap);

    inputEls[b.key] = { dot: dot, state: stateEl, hold: holdEl, last: last };
  });
}

function renderInput(b) {
  var el = inputEls[b.key];
  if (!el) { return; }

  el.dot.classList.toggle('down', b.pressed);
  el.state.textContent = b.pressed ? 'held - ' + b.held_now_ms + ' ms' : 'released';

  //* While a press is in flight the interesting number is how far it has got
  //* toward this switch's thresholds; between presses it is the last hold. The
  //* thresholds come from the device, and a zero means this switch has no action
  //* at that point at all - which is what long_press_ms is on the boot switch.
  //? The action is named by the device too, because "held to its threshold" means
  //? power-off on one switch and a config portal on the other.
  var pastLong = b.long_press_ms > 0 && b.held_now_ms > b.long_press_ms;
  var pastAction = b.action_hold_ms > 0 && b.held_now_ms >= b.action_hold_ms;
  el.hold.textContent = b.pressed
    ? (pastAction ? b.action : (pastLong ? (b.action_hold_ms ? 'mode on release' : 'no action') : 'holding'))
    : (b.hold_ms > 0 ? b.hold_ms + ' ms' : '-');

  var rows = {
    'Last press': b.last_event === 4
                  ? INPUT_EVENTS[4] + ': ' + b.action
                  : (INPUT_EVENTS[b.last_event] || ('event ' + b.last_event)),
    'When': b.last_event === 0 ? '-' : agoText(b.last_event_age_ms),
    'Presses': b.events + ' since boot',
    'Recent presses': b.counter > 0
                     ? b.counter + ' (counter clears after ' + (b.multi_gap_ms / 1000) + ' s of quiet)'
                     : 'none'
  };
  el.last.innerHTML = Object.keys(rows).map(function (k) {
    return '<dt>' + k + '</dt><dd>' + rows[k] + '</dd>';
  }).join('');
}

function inputPoll() {
  return api('/api/buttons').then(function (r) {
    buildInputs(r.buttons);

    //* Summed across both switches: the fast rate is about "one of them moved",
    //* and the counters only ever go up, so the sum is as good as a flag.
    var total = 0;
    r.buttons.forEach(function (b) {
      renderInput(b);
      total += b.events;
    });

    //* The shared numbers are written from the device's own constants, so a change
    //* to BUTTON_* in services/readings.h cannot leave this text describing
    //* behaviour the firmware no longer has.
    $('#inputGap').textContent = (r.click_gap_ms / 1000).toFixed(1);

    //* Any completed press keeps the panels at full rate for another minute.
    if (total !== inputSeen) {
      inputSeen = total;
      inputIdleAt = Date.now();
      inputArm(INPUT_FAST_MS);
    } else if (inputPeriod !== INPUT_SLOW_MS && Date.now() - inputIdleAt > INPUT_IDLE_MS) {
      inputArm(INPUT_SLOW_MS);
    }

    //* The touch button switches mode on the device. Reuse the state render so the
    //* mode buttons and the status block catch up without waiting for the 3 s poll.
    if (r.mode !== inputMode) {
      inputMode = r.mode;
      if (state && state.mode !== r.mode) { refresh(); }
    }

    //* Say how often the panels are actually asking, so a tap that fell between
    //* two polls is explained rather than mysterious - and so the back-off is
    //* visible instead of looking like a device that stopped answering.
    $('#inputInfo').textContent = 'Polling every ' + inputPeriod + ' ms' +
      (inputPeriod === INPUT_SLOW_MS
       ? ' (nothing has happened for ' + (INPUT_IDLE_MS / 1000) + ' s).'
       : '.');
  }).catch(function () {
    //! Silent on purpose. Losing contact is what the 3 s state poll reports, and
    //! a banner from here would appear and clear four times a second during an
    //! upload, when the device stops answering on purpose.
  });
}

//* Start asking immediately; the first reply builds the panels and fills their
//* thresholds in from the device's own numbers, and the placeholder in the markup
//* only covers the moment before it lands.
inputArm(INPUT_FAST_MS);
inputPoll();
//* ---------------------- FIRMWARE UPLOAD ----------------------
var fwFile = null;
var fwMax = 0;
var fwRebootTimer = null;

function fwInfo(msg) { $('#fwInfo').textContent = msg; }

function fwProgress(frac) {
  $('#fwBarWrap').style.display = 'block';
  $('#fwBar').style.width = Math.round(frac * 100) + '%';
}

function fwBusy(busy) {
  $('#fwGo').disabled = busy || !fwFile;
  $('#fwFile').disabled = busy;
}

function fwSize(bytes) {
  return bytes >= 1048576
    ? (bytes / 1048576).toFixed(2) + ' MB'
    : Math.round(bytes / 1024) + ' KB';
}

function fwPick(file) {
  fwFile = null;
  $('#fwGo').disabled = true;
  if (!file) { return; }

  if (!/\.bin$/i.test(file.name)) {
    fwInfo(file.name + ' is not a .bin file. Pick the firmware.bin built for this board.');
    return;
  }
  if (fwMax && file.size > fwMax) {
    fwInfo(file.name + ' is ' + fwSize(file.size) + ', larger than the ' +
           fwSize(fwMax) + ' slot it would have to fit in.');
    return;
  }

  //* Sniff the app image magic before spending a minute of transfer on it. The
  //* device checks the same byte, but only after the first 4 KB have arrived.
  var reader = new FileReader();
  reader.onload = function () {
    var first = new Uint8Array(reader.result)[0];
    if (first !== 0xE9) {
      fwInfo(file.name + ' does not start with an app image header (0x' +
             first.toString(16) + '). A full flash dump or a merged factory ' +
             'image is not accepted here; use the firmware.bin the build leaves ' +
             'in .pio/build/C3-OTA/.');
      return;
    }
    fwFile = file;
    $('#fwGo').disabled = false;
    fwInfo(file.name + ' - ' + fwSize(file.size) + ', ready to install.');
  };
  reader.readAsArrayBuffer(file.slice(0, 1));
}

function fwWaitForReboot() {
  var deadline = Date.now() + 60000;
  clearInterval(fwRebootTimer);
  fwRebootTimer = setInterval(function () {
    fetch('/api/buttons', { cache: 'no-store' }).then(function (res) {
      if (!res.ok) { throw new Error('HTTP ' + res.status); }
      clearInterval(fwRebootTimer);
      //* Reload rather than patch up the page in place: every cached number on it
      //* (mode, firmware size, uptime) belongs to the previous image.
      location.reload();
    }).catch(function () {
      if (Date.now() > deadline) {
        clearInterval(fwRebootTimer);
        fwInfo('The device has not answered since the upload. If it does not come ' +
               'back, it may be on the new firmware but unable to join WiFi - ' +
               'recover it over USB with the last known-good firmware.bin.');
      }
    });
  }, 1000);
}

$('#fwFile').addEventListener('change', function (e) {
  fwPick(e.target.files[0]);
});

api('/api/update').then(function (u) {
  fwMax = u.max_bytes;
  fwInfo('Running ' + u.running + ' (' + fwSize(u.running_bytes) + '); ' +
         (u.max_bytes
          ? 'the next upload goes to ' + u.partition + ', which holds up to ' + fwSize(u.max_bytes) + '.'
          : 'this board has no inactive OTA slot to write to.'));
}).catch(function (e) {
  fwInfo('Could not read the update target: ' + e.message);
});

$('#fwGo').addEventListener('click', function () {
  if (!fwFile) { return; }

  if (!window.confirm('Install ' + fwFile.name + ' (' + fwSize(fwFile.size) + ')?\n\n' +
                      'The device writes it to the inactive slot and reboots. An ' +
                      'image that boots but cannot join WiFi has to be recovered ' +
                      'over USB.')) {
    return;
  }

  //* Stop both pollers for the duration. The device is inside one handleClient()
  //* call for the whole transfer and cannot answer either of them, and their
  //* timeouts would fire as failed requests that mean nothing.
  clearInterval(refreshTimer);
  clearInterval(inputTimer);
  inputTimer = null;

  fwBusy(true);
  fwProgress(0);
  fwInfo('Sending ' + fwSize(fwFile.size) + '...');

  var fd = new FormData();
  fd.append('firmware', fwFile, fwFile.name);

  var xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/update');

  //* The browser's send progress, which runs ahead of the flash write: the
  //* device reads the body only as fast as it can erase and program the slot.
  xhr.upload.onprogress = function (e) {
    if (e.lengthComputable) {
      fwProgress(e.loaded / e.total);
      fwInfo('Sending ' + Math.round(e.loaded / e.total * 100) + '% (' +
             fwSize(e.loaded) + ' of ' + fwSize(e.total) + ')');
    }
  };

  xhr.onload = function () {
    var r = null;
    try { r = JSON.parse(xhr.responseText); } catch (err) { r = null; }

    if (!r) {
      fwInfo('The device did not answer with a result. It may have restarted ' +
             'already - waiting for it to come back...');
      fwProgress(1);
      fwWaitForReboot();
      return;
    }

    if (!r.ok) {
      fwBusy(false);
      fwProgress(0);
      fwInfo('Install failed after ' + fwSize(r.bytes || 0) + ': ' +
             (r.error || 'unknown error') + '. The device is still running the ' +
             'firmware it booted with.');
      refreshTimer = setInterval(refresh, 3000);
      inputArm(INPUT_FAST_MS);
      refresh();
      return;
    }

    fwProgress(1);
    fwInfo('Installed ' + fwSize(r.bytes) + ' (md5 ' + r.md5 + '). Rebooting...');
    fwWaitForReboot();
  };

  xhr.onerror = function () {
    //! A dropped connection this late is what a successful install can look like
    //! if the reboot wins the race with the reply; the device is asked, not
    //* assumed, by the reboot watcher below.
    fwInfo('The connection dropped. Waiting for the device to come back...');
    fwWaitForReboot();
  };

  xhr.send(fd);
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

//* Read the login state on load: it is what reports whose login is in force, and
//* what clears the two fields so nothing is ever filled in on a fresh document.
authLoad();

//* And the saved networks, which is also what draws the WiFi section - including
//* the join failure of the attempt before this page was loaded.
netLoad();

//* Named so the firmware upload can stop it: the device answers nothing while
//* the body of an upload is being parsed, and these requests would only pile up
//* behind it and time out.
var refreshTimer = setInterval(refresh, 3000);
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
//? Split in two, because the page now comes and goes with the network rather than
//? existing from boot (see the NET_ state machine in services/wifi_settings.h):
//?   setup_webpage() once - the route table and the saved configuration, neither
//?                   of which may run twice, since on() would register a second
//?                   handler for every route and the config load would overwrite
//?                   whatever the button has changed in the meantime.
//*   webpage_start()/webpage_stop() on every connect and disconnect.

//* Whether the route table has been registered yet. Not the same question as
//? webpage_started below, which is whether the listener is up right now.
bool webpage_routes_ready = false;

void webpage_start()
{
  if (webpage_started)
    return;

  if (WiFi.getMode() == WIFI_OFF || WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[WEB] No address to serve on - configuration page not started");
    return;
  }

  //* Friendly .local alias. Harmless when the LAN has no mDNS resolver.
  //* setup_OTA() has already run by the time this is reached (net_enter_connected),
  //? and ArduinoOTA::begin() initialises the responder itself because _mdnsEnabled
  //? defaults to true - so this normally finds mDNS already up and the call is a
  //? no-op that re-sets the same hostname.
  //! The note that used to be here claimed mdns_init() cannot run twice and that
  //! this call fails with ESP_ERR_INVALID_STATE. That is wrong for this framework:
  //! mdns_init() is idempotent and returns success when it has already run, which
  //? the boot log confirms - it shows neither "Failed starting MDNS" nor the
  //? "already running" branch below.
  //* The check is kept because it is still the call that brings the responder up
  //? if ArduinoOTA's own MDNS.begin() ever fails. What must NOT happen is bailing
  //! out of here on a false return, which is what used to leave the HTTP service
  //! unadvertised.
  if (!MDNS.begin(WEBPAGE_MDNS_HOST))
    Serial.println("[WEB] mDNS responder already running - reusing its hostname");
  MDNS.addService("http", "tcp", WEBPAGE_HTTP_PORT);

  //? WiFiManager's provisioning portal has already been torn down here (see
  //? wifi_settings.h), so port 80 is free for the persistent page.
  web_server.begin();
  webpage_started = true;

  Serial.printf("[WEB] Device configuration at %s\n", webpage_url().c_str());
  Serial.printf("[WEB] Also reachable at http://%s.local/\n", WEBPAGE_MDNS_HOST);
}

//* Take the listener down without touching the route table, so webpage_start()
//? can bring the same one back after a drop or an offline round trip.
//! Only the HTTP listener: the mDNS responder is created by ArduinoOTA::begin()
//? and freed by ArduinoOTA.end(), which the network transitions call alongside
//* this one. Freeing it here as well would be the second half of a double free.
void webpage_stop()
{
  if (!webpage_started)
    return;
  web_server.stop();
  webpage_started = false;
  Serial.println("[WEB] Configuration page stopped");
}

void setup_webpage()
{
  if (webpage_routes_ready)
  {
    //? Already built: this is a reconnect, so only the listener comes back.
    webpage_start();
    return;
  }

  webpage_config_load();
  //* The login is read once, here, rather than per request: every route asks
  //* whether one is set, and the input panels poll four times a second.
  webpage_auth_load();
  //* Every route below goes through the gate, which is what makes the login cover
  //? the whole surface - firmware upload included - rather than whichever handlers
  //! someone remembered to guard. See webpage_auth_gate().
  web_server.on("/", HTTP_GET, webpage_auth_gate(webpage_handle_root));
  web_server.on("/api/state", HTTP_GET, webpage_auth_gate(webpage_handle_state));
  //* The input panels poll this one hard, so it is a document of its own
  //* rather than a slower /api/state (see webpage_handle_buttons).
  web_server.on("/api/buttons", HTTP_GET, webpage_auth_gate(webpage_handle_buttons));
  web_server.on("/api/modes", HTTP_GET, webpage_auth_gate(webpage_handle_modes));
  web_server.on("/api/mode", HTTP_POST, webpage_auth_gate(webpage_handle_set_mode));
  web_server.on("/api/config", HTTP_POST, webpage_auth_gate(webpage_handle_set_config));
  //* The login itself, behind the gate like everything else: being able to change
  //? or turn off a login you cannot authenticate against would put the hole back
  //* exactly where it was.
  web_server.on("/api/auth", HTTP_GET, webpage_auth_gate(webpage_handle_auth_get));
  web_server.on("/api/auth", HTTP_POST, webpage_auth_gate(webpage_handle_auth_set));
  //* The saved networks. The scan is two routes on one path on purpose: the POST
  //? starts it and the GET reports it, because a scan takes seconds and nothing in
  //* this firmware may spend them inside a request.
  web_server.on("/api/wifi", HTTP_GET, webpage_auth_gate(webpage_handle_wifi_get));
  web_server.on("/api/wifi", HTTP_POST, webpage_auth_gate(webpage_handle_wifi_set));
  web_server.on("/api/wifi/scan", HTTP_POST, webpage_auth_gate(webpage_handle_wifi_scan_start));
  web_server.on("/api/wifi/scan", HTTP_GET, webpage_auth_gate(webpage_handle_wifi_scan));
  //* Preset slots (services/presets.h). The slot list is what the page draws
  //* its bar from; a single slot carries its values, and is what an export
  //* reads, so the file the browser writes needs no value handling on device.
  web_server.on("/api/presets", HTTP_GET, webpage_auth_gate(webpage_handle_presets));
  web_server.on("/api/preset", HTTP_GET, webpage_auth_gate(webpage_handle_preset));
  web_server.on("/api/preset", HTTP_POST, webpage_auth_gate(webpage_handle_set_preset));
  //* Firmware upload (components/ESP32/features/update_firmware_web.h). The
  //* GET is the page's pre-flight - which slot, and how big an image fits -
  //* and the POST pairs the completion handler with the streaming upload
  //* callback, which is the only form the WebServer parses a file body in.
  web_server.on("/api/update", HTTP_GET, webpage_auth_gate(webpage_handle_update_info));
  //! The streaming callback is deliberately NOT wrapped: it runs while the request
  //? body is still arriving, and answering 401 from inside it would write to a
  //! socket the client is halfway through sending on. It asks the same question
  //? silently and refuses to write a byte (see webpage_handle_update_upload), so
  //* the 401 the client sees comes from the gated completion handler beside it.
  web_server.on("/api/update", HTTP_POST, webpage_auth_gate(webpage_handle_update_done),
                webpage_handle_update_upload);
  web_server.onNotFound(webpage_auth_gate(webpage_handle_not_found));

  //* The route table is complete and must never be built twice: on() appends a
  //! handler, so a second pass would leave two of everything and the first
  //? matching one would answer. Everything after this point is repeatable.
  webpage_routes_ready = true;
  webpage_start();
}

void webpage_loop()
{
  if (!webpage_started)
    return;

  //* A firmware upload that completed sets a restart deadline here rather than
  //* restarting inside the HTTP handler, so the reply reaches the browser
  //* first. Checked before handleClient() so the reboot is not delayed behind
  //* another request.
  webpage_update_poll();

  //* A network the page saved is joined here rather than in the handler that saved
  //? it, for the same reason the reboot above is deferred: the radio is about to
  //* move, and the reply has to be on the wire before it does.
  webpage_wifi_poll();

  web_server.handleClient();
}

#endif // SERVICE_WEBPAGE
