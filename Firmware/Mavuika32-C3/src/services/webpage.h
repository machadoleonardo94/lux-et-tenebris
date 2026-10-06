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
#define WEBPAGE_MDNS_HOST "lightpack"
#define WEBPAGE_NVS_NAMESPACE "device"

WebServer web_server(WEBPAGE_HTTP_PORT);
bool webpage_started = false;

void setup_webpage();
void webpage_loop();

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

    led_strip.red = preferences.getUChar("red", led_strip.red);
    led_strip.green = preferences.getUChar("green", led_strip.green);
    led_strip.blue = preferences.getUChar("blue", led_strip.blue);
    led_strip.brightness = preferences.getUChar("bright", led_strip.brightness);

    preferences.end();

    Serial.printf("[WEB] Config loaded: mode=%d rgb=%u,%u,%u bright=%u\n",
                  mode, led_strip.red, led_strip.green, led_strip.blue,
                  led_strip.brightness);
}

void webpage_config_save()
{
    //! Writes only on an explicit POST from the page, never from the loop, so
    //! NVS wear stays proportional to user actions.
    preferences.begin(WEBPAGE_NVS_NAMESPACE, false);
    preferences.putInt("mode", mode);
    preferences.putUChar("red", led_strip.red);
    preferences.putUChar("green", led_strip.green);
    preferences.putUChar("blue", led_strip.blue);
    preferences.putUChar("bright", led_strip.brightness);
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
    json.reserve(320);
    json += F("{\"mode\":");
    json += String(mode);
    json += F(",\"mode_key\":\"");
    json += webpage_json_escape(entry ? entry->key : "unknown");
    json += F("\",\"mode_label\":\"");
    json += webpage_json_escape(entry ? entry->label : "Unknown");
    json += F("\",\"red\":");
    json += String(led_strip.red);
    json += F(",\"green\":");
    json += String(led_strip.green);
    json += F(",\"blue\":");
    json += String(led_strip.blue);
    json += F(",\"brightness\":");
    json += String(led_strip.brightness);
    json += F(",\"num_leds\":");
    json += String(NUM_LEDS);
    json += F(",\"majoras_leds\":");
    json += String(MAJORAS_LEDS);
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
        led_strip.red = (uint8_t)constrain(web_server.arg("red").toInt(), 0, 255);
        changed = true;
    }
    if (web_server.hasArg("green"))
    {
        led_strip.green = (uint8_t)constrain(web_server.arg("green").toInt(), 0, 255);
        changed = true;
    }
    if (web_server.hasArg("blue"))
    {
        led_strip.blue = (uint8_t)constrain(web_server.arg("blue").toInt(), 0, 255);
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

    if (!changed)
    {
        web_server.send(400, "application/json", "{\"ok\":false,\"error\":\"no recognised field\"}\n");
        return;
    }

    webpage_config_save();
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
input[type=color]{flex:0 0 5rem;height:2.75rem;padding:.15rem;border:1px solid #3a3450;
                  border-radius:.6rem;background:#1e1b29;cursor:pointer}
input[type=range]{flex:1;accent-color:#8a63e8}
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
  <h2>Solid colour</h2>
  <div class="row"><input type="color" id="color" value="#ff00ff"></div>
  <p class="hint">Used by the <em>Solid color</em> mode. The gyro-driven modes
     compute their own colour.</p>
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
    'LEDs': state.num_leds + ' strip / ' + state.majoras_leds + ' majora',
    'Gyroscope': state.gyro ? 'ready' : 'not detected',
    'Uptime': Math.floor(state.uptime_ms / 1000) + ' s'
  };
  $('#status').innerHTML = Object.keys(rows).map(function (k) {
    return '<dt>' + k + '</dt><dd>' + rows[k] + '</dd>';
  }).join('');
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

$('#bright').addEventListener('input', function (e) {
  $('#brightVal').textContent = e.target.value;
});

$('#bright').addEventListener('change', function (e) {
  pushConfig({ brightness: e.target.value });
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
  return refresh();
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
    if (WiFi.status() == WL_CONNECTED && MDNS.begin(WEBPAGE_MDNS_HOST))
        MDNS.addService("http", "tcp", WEBPAGE_HTTP_PORT);

    //? WiFiManager's provisioning portal has already been torn down here (see
    //? wifi_settings.h), so port 80 is free for the persistent page.
    web_server.on("/", HTTP_GET, webpage_handle_root);
    web_server.on("/api/state", HTTP_GET, webpage_handle_state);
    web_server.on("/api/modes", HTTP_GET, webpage_handle_modes);
    web_server.on("/api/mode", HTTP_POST, webpage_handle_set_mode);
    web_server.on("/api/config", HTTP_POST, webpage_handle_set_config);
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
