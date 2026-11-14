//! Bench tool, not firmware. Built only by the C3-RF environment, which replaces
//? src/main.cpp with this file (see platformio.ini), so it never reaches a release.
//*
//? What it answers, in order, with nothing else running:
//?   1. What can this board HEAR?  A full sweep with BSSID, channel, security and
//*      RSSI, so the same SSID's several BSSIDs (Silvia / -EXT / -PLUS) can be
//?      compared against what a laptop sees from the same spot.
//?   2. Can anything HEAR this board? A 20 s open SoftAP with a known name. If a
//!      phone or laptop cannot see it, the transmit path or the antenna is the
//*      fault - and that also explains why the router never answers the station's
//?      authentication frames while its beacons are received at -39 dBm.
//*   3. Does the SAVED credential join at all? It reads ssid0/pass0 from the same
//?      NVS namespace the firmware uses and reports every disconnect reason. If the
//!      probe joins and the firmware does not, the fault is in the firmware; if
//?      neither joins, it is the radio, the antenna or the access point.
//*
//? Serial: 115200. AP window: 20 s right after the first sweep.

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <esp_system.h>
#include "shared/pinout.h"

static uint8_t sta_mac[6];

static const char *reason_name(uint8_t r)
{
    switch (r)
    {
    case WIFI_REASON_AUTH_EXPIRE:
        return "auth expired (AP never answered)";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        return "4-way timeout (wrong WPA2 password)";
    case WIFI_REASON_NO_AP_FOUND:
        return "no AP found";
    case WIFI_REASON_AUTH_FAIL:
        return "auth failed (AP refused this station)";
    case WIFI_REASON_ASSOC_FAIL:
        return "association failed";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "handshake timeout";
    case WIFI_REASON_CONNECTION_FAIL:
        return "connection failed";
    default:
        return "?";
    }
}

static void on_wifi_event(arduino_event_id_t event, arduino_event_info_t info)
{
    if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED)
        Serial.printf("[EVT] ASSOCIATED auth=%u ch=%u\n",
                      (unsigned)info.wifi_sta_connected.authmode,
                      (unsigned)info.wifi_sta_connected.channel);
    else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
        Serial.printf("[EVT] disconnected reason=%u (%s)\n",
                      (unsigned)info.wifi_sta_disconnected.reason,
                      reason_name(info.wifi_sta_disconnected.reason));
    else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP)
        Serial.printf("[EVT] GOT IP %s\n",
                      IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
}

static void scan_dump(const char *tag)
{
    WiFi.scanDelete();
    const int n = WiFi.scanNetworks(false, true);
    Serial.printf("\n[SCAN %s] %d network(s)\n", tag, n);
    for (int i = 0; i < n; i++)
    {
        const uint8_t *b = WiFi.BSSID(i);
        Serial.printf("[SCAN %s] %2d) %-28s rssi=%4d ch=%2d auth=%d bssid=%02X:%02X:%02X:%02X:%02X:%02X\n",
                      tag, i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                      (int)WiFi.encryptionType(i),
                      b[0], b[1], b[2], b[3], b[4], b[5]);
    }
}

//* 20 s of transmitting on the exact code path the config portal uses.
//? Repeated from loop() so the network can be looked for at any time, not only in
//* the one window after a reset.
static void ap_test()
{
    Serial.println("\n[AP] ==========================================");
    Serial.println("[AP] 90 s transmit test - look for the network");
    Serial.printf("[AP]   C3-RF-%02X%02X\n", sta_mac[4], sta_mac[5]);
    Serial.println("[AP] on a phone/laptop NOW. It stays up 90 s.");
    Serial.println("[AP] ==========================================");

    WiFi.mode(WIFI_AP);
    String ap_ssid = String("C3-RF-") + String(sta_mac[4], HEX) + String(sta_mac[5], HEX);
    const bool ap_ok = WiFi.softAP(ap_ssid.c_str(), nullptr, 3, 0, 4);
    Serial.printf("[AP] '%s' start=%d ip=%s ch=%u\n", ap_ssid.c_str(), ap_ok,
                  WiFi.softAPIP().toString().c_str(), WiFi.channel());
    for (int i = 1; i <= 90; i++)
    {
        if ((i % 10) == 0)
            Serial.printf("[AP] up %2d/90 s  stations=%u\n", i, WiFi.softAPgetStationNum());
        delay(1000);
    }
    WiFi.softAPdisconnect(true);
    Serial.println("[AP] stopped");
    WiFi.mode(WIFI_STA);
}

void setup()
{
    //! FIRST, before anything can block: this carrier is latched on, and the
    //? power switch only starts it. Same pin and level as main.cpp's setup().
    //* Without this the latch releases as soon as the button is released or the
    //! capacitor drains, so the probe could only ever run tethered to USB - which
    //* is exactly wrong for a transmit test that should not depend on the cable.
    pinMode(latch_enable, OUTPUT);
    digitalWrite(latch_enable, HIGH);

    Serial.begin(115200);
    delay(600);

    Serial.println("\n\n=== C3 RF PROBE ===");
    Serial.printf("[RF] reset reason: %d\n", (int)esp_reset_reason());

    esp_read_mac(sta_mac, ESP_MAC_WIFI_STA);
    Serial.printf("[RF] STA MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  sta_mac[0], sta_mac[1], sta_mac[2], sta_mac[3], sta_mac[4], sta_mac[5]);

    WiFi.onEvent(on_wifi_event);
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    int8_t txp = 0;
    if (esp_wifi_get_max_tx_power(&txp) == ESP_OK)
        Serial.printf("[RF] max TX power: raw=%d (%.2f dBm)\n", txp, txp / 4.0f);
    wifi_bandwidth_t bw;
    if (esp_wifi_get_bandwidth(WIFI_IF_STA, &bw) == ESP_OK)
        Serial.printf("[RF] bandwidth: %d\n", (int)bw);
    Serial.printf("[RF] free heap: %lu\n", (unsigned long)ESP.getFreeHeap());

    scan_dump("baseline");

    //* ---------------------- TX TEST ----------------------
    //? An open AP is the simplest transmitter this board can run, and it is the
    //* same code path the config portal uses. If this name is invisible from a
    //! phone standing next to the board, the portal was never going to appear
    //? either, and the fault is RF - not the button or the portal logic.
    ap_test();

    //* ---------------------- JOIN TEST ----------------------
    //? The same two keys the firmware treats as slot 0. Read only: this probe
    //* never writes NVS, so a failed run costs nothing.
    Preferences prefs;
    prefs.begin("wifi", true);
    const String ssid = prefs.isKey("ssid0") ? prefs.getString("ssid0", "") : String();
    const String pass = prefs.isKey("pass0") ? prefs.getString("pass0", "") : String();
    prefs.end();

    Serial.printf("\n[JOIN] slot0='%s' pass_len=%u\n", ssid.c_str(), (unsigned)pass.length());
    if (ssid.length() == 0)
    {
        Serial.println("[JOIN] nothing saved in slot0 - skipping");
    }
    else
    {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(false);
        WiFi.begin(ssid.c_str(), pass.c_str());
        const uint32_t t0 = millis();
        wl_status_t last = (wl_status_t)-1;
        while (millis() - t0 < 30000)
        {
            const wl_status_t st = WiFi.status();
            if (st != last)
            {
                last = st;
                Serial.printf("[JOIN] %5lu ms status=%d ip=%s rssi=%d\n",
                              (unsigned long)(millis() - t0), (int)st,
                              WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
            }
            delay(200);
        }
        Serial.printf("[JOIN] result: status=%d ip=%s\n", (int)WiFi.status(),
                      WiFi.localIP().toString().c_str());
        WiFi.disconnect(true, false);
    }

    WiFi.mode(WIFI_STA);
    scan_dump("after-join");

    Serial.println("\n[PROBE] idle - AP test + sweep, repeating. Swap the unit when ready.");
}

void loop()
{
    ap_test();          // 90 s transmitting
    scan_dump("periodic");
    delay(5000);        // short gap, then the AP is back
}
