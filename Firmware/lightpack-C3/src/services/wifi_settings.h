#if !defined(CONNECT_WIFI)
#define CONNECT_WIFI

#include "shared/dependencies.h"

//* ---------------------- CREDENTIAL SLOTS ----------------------
//? Two saved networks, tried in turn. One slot is not enough for the case this
//? firmware is actually used in: a phone hotspot that is switched on later has to
//? be found without anyone pressing anything, and that means keeping the network
//? that is not on the air right now alongside the one that is.
//! Preferences is the source of truth for both, NOT the driver's STA config: the
//? ESP-IDF driver holds exactly one network and it is only ever the last attempt
//? made, so it can answer "which network is this" but never "which are saved".
bool wifi_credential_present(uint8_t slot);
String wifi_credential_ssid(uint8_t slot);
String wifi_credential_password(uint8_t slot);
uint8_t wifi_credential_count();
uint8_t wifi_credential_active();
uint8_t wifi_credential_next(uint8_t from);
uint8_t wifi_credential_free_slot();
void wifi_credential_set_active(uint8_t slot);
int8_t wifi_credential_find(const char *ssid);
void wifi_credential_store(uint8_t slot, const char *ssid, const char *password);
void wifi_credential_clear(uint8_t slot);
bool wifi_credential_join(uint8_t slot);
bool wifi_credential_join_active();
void wifi_credentials_migrate();
void wifi_recovery_wipe();
void wifi_adopt_join();
void wifi_offline_store(bool offline);
void setup_WIFI(bool portal_enabled = true);
void wifi_loop();
String wifi_saved_ssid();
void wifi_report_status();
void wifi_report_scan();
void wifi_request_portal_on_boot();
bool wifi_portal_requested();
bool wifi_offline_saved();
void wifi_toggle_offline();
const char *wifi_state_name();

//* How long the portal a board with NO SAVED CREDENTIALS opens stays up.
//? This is a fixed window measured from the portal opening, not from your first
//? connection attempt, so a board that boots and sits idle will have closed its AP
//? by the time you go looking for it. Raising this lengthens the no-network boot
//? delay. The countdown restarts while a device is associated with the hotspot
//? (setAPClientCheck below), so this is the idle window, not a hard cap.
//! This portal is only ever opened unprompted on an unprovisioned board. A board
//* that HAS credentials never opens a portal by itself - see setup_WIFI().
#define WIFI_PORTAL_TIMEOUT_S 60

//* How long a portal the BOOT BUTTON asked for stays up.
//? Deliberately longer than the boot-time window: nobody asked for that one, while
//? this one is up because someone stood at the device and held a button down.
//! 120 s, not the 300 s this used to be. The button has to be held for five
//? seconds to ask for the portal in the first place, so anyone who gets this far
//? already knows what they are doing, and two minutes is enough to read an SSID
//? list and type a password on a phone.
#define WIFI_PORTAL_BUTTON_TIMEOUT_S 120

//* How long the boot button has to be held to LEAVE a portal that is already up.
//? Deliberately shorter than the 5 s that OPENS one: outside the window that
//* switch means "let me in", and inside it means "not now" - and the shorter hold
//* is the one a user reaches for when a hotspot has appeared in front of them.
#define WIFI_PORTAL_SKIP_HOLD_MS 3000

//* How long before the skip the pixel starts saying so. The strip is frozen inside
//* setup(), so the onboard pixel is the only thing that can answer "I see it".
#define WIFI_PORTAL_WARN_MS 1000

//* What a portal window can end in.
//? Three outcomes rather than two: a skip has to be its own answer, because "the
//* user stood here and said no" is a different thing from "the clock ran out while
//* they were reading it" - the first is a decision to keep, the second is not.
enum : uint8_t
{
    PORTAL_CONNECTED = 0, // Came back joined: adopt the network and carry on
    PORTAL_CLOSED,        // Out of time, or the credentials were already fine
    PORTAL_SKIPPED        // The button asked to leave it - park the radio instead
};

//* How often an ONLINE board retries a network it is not joined to.
//? Reconnecting is not a one-shot at boot any more: a lamp that boots while the
//? router is still coming up has to find its way onto the network by itself, so
//? the join is retried forever at this interval until it succeeds.
#define WIFI_RETRY_INTERVAL_MS 60000UL

//* How long a joined board has to stay unjoined before the link is treated as
//? gone and the page and the OTA listener are torn down.
//! This is hysteresis, not a timeout on reconnecting: on the -68 dBm link this
//? board actually runs on, a single missed beacon is normal and tearing down a
//? working HTTP server over one would make the page flap. The grace period is
//? only about how long a drop must persist before the teardown is believed.
#define WIFI_DROP_GRACE_MS 10000UL

//* How long a join the PAGE asked for gets before the board calls it failed and
//? says so. Short enough that the page can report the outcome inside a few polls,
//! long enough to cover a slow association on the -68 dBm link this board runs on.
//? The retry loop keeps going afterwards either way, and the rotation in wifi_loop
//* falls back to the other slot, so a network that is merely late is not lost.
#define WIFI_JOIN_TIMEOUT_MS 15000

//* NVS keys. All of them live in the "wifi" namespace, which is the whole set a
//? recovery boot clears: the credential keys below, the active slot pointer, the
//? offline switch and the one-shot portal request. Nothing else uses this
//* namespace, which is what makes scoping a recovery wipe this narrowly correct.
//? WIFI_OFFLINE_KEY is the persisted offline switch; WIFI_PORTAL_REQUEST_KEY is
//? the one-shot "open the portal on the next boot" request.
#define WIFI_OFFLINE_KEY "offline"
#define WIFI_PORTAL_REQUEST_KEY "portalreq"
#define WIFI_CREDENTIAL_ACTIVE_KEY "active"

//* How many networks can be saved at once.
//? Two is what the use case asks for - a phone hotspot and a home network - and
//! the cost of each extra slot is real: the join rotates one slot per retry, so a
//? network that has just appeared is found within WIFI_RETRY_INTERVAL_MS per slot,
//* and one that is gone costs that slot's attempt on every rotation.
#define WIFI_CREDENTIAL_SLOTS 2

//? Per-slot key names, as a table rather than built with snprintf, so the strings
//* live in flash and a slot can never be addressed by a key that was never written.
const char *const WIFI_CREDENTIAL_SSID_KEYS[WIFI_CREDENTIAL_SLOTS] = {"ssid0", "ssid1"};
const char *const WIFI_CREDENTIAL_PASS_KEYS[WIFI_CREDENTIAL_SLOTS] = {"pass0", "pass1"};

//? SSID of that boot-time configuration hotspot. Deliberately NOT the same string
//? as NETWORK_HOSTNAME: the hotspot is a temporary provisioning AP, while the
//? hostname is the name the board keeps on the LAN once it has joined.
#define WIFI_PORTAL_AP_SSID "Lightpack-Devkit"

//? Set to 0 to drop the scan/status reporting from the failure path.
#define WIFI_DIAGNOSTICS 1

//* ---------------------- RADIO POWER SAVE ----------------------
//? With modem sleep on, the radio dozes between access point beacons and the AP
//? holds everything addressed to this board until the next delivery window.
//! That, not packet loss, is what "the connection is very unstable" turns out to
//? mean here. Measured against this board on a -68 dBm link with sleep on:
//? 60 pings came back 23-266 ms with a sawtooth drift and only 1% loss, and a
//? LAN round trip took 200-380 ms to open and 240-600 ms to first byte. Almost
//? nothing was being lost; every packet was simply waiting for the next window.
//? The framework default is modem sleep: WiFiGeneric.cpp sets
//? `_sleepEnabled = WIFI_PS_MIN_MODEM` for every target except the S2 and applies
//! it on every STA_START. Nothing in this firmware turned it off on the path that
//! actually runs: the one WiFi.setSleep(false) that existed lived in
//? get_saved_wifi(), a helper no caller ever reached, and it went with the
//* credential helpers it belonged to. setup_WIFI() applies the value below instead.
//* 1 keeps the doze and the lower idle draw; 0 buys the LAN back its latency.
//? The extra draw of 0 is the radio staying awake between beacons rather than
//? the radio transmitting, so it is tens of milliamps of idle current, not the
//? strip's draw - measure it on this board before shipping a battery-only build.
#define WIFI_MODEM_SLEEP 0

//* ---------------------- BUTTON-REQUESTED PORTAL ----------------------
//? The boot button asks for a portal by leaving a flag in NVS and restarting,
//? rather than opening one where the press happened. Two reasons, both of them
//? about WiFiManager rather than about the button:
//?   1. Once the board has joined a network, the persistent configuration page
//?      owns port 80. WiFiManager's portal needs that port for its own server, so
//?      a portal opened at runtime would come up on a socket it could not bind.
//?   2. The portal is pumped inside setup_WIFI(), so it is a window of the BOOT.
//?      Opened from inside loop() the same window would be the strip, the sensors
//?      and the web page all frozen for as long as the hotspot is up - and the
//!      3 s skip makes coming back out of it a button hold rather than a restart.
//* The flag is consumed (read and cleared) by the boot that opens the portal, so
//! it can never outlive the restart it asked for and reappear on a later power-up.

void wifi_request_portal_on_boot()
{
    preferences.begin("wifi", false);
    preferences.putBool(WIFI_PORTAL_REQUEST_KEY, true);
    preferences.end();
    Serial.println("[WIFI] Config portal requested - it opens on the next boot");
}

bool wifi_portal_requested()
{
    preferences.begin("wifi", false);
    const bool requested = preferences.getBool(WIFI_PORTAL_REQUEST_KEY, false);
    if (requested)
        preferences.remove(WIFI_PORTAL_REQUEST_KEY);
    preferences.end();
    return requested;
}

//* ---------------------- NETWORK STATE ----------------------
//? The board is in exactly one of three states, and which one it is decides what
//? exists: the configuration page and the OTA listener are started and stopped by
//? the transitions below rather than once from setup(), because an online board
//? that cannot reach its access point spends an indefinite amount of time without
//? either of them.
//?
//?   NET_OFFLINE     The radio is off. No join attempts, no hotspot, no page and
//?                   no OTA listener. This is the persisted "offline mode" switch
//?                   and it survives a reboot; a 1 s press of GPIO9 flips it.
//?   NET_CONNECTING  The radio is up, the saved network is not joined. The join is
//?                   retried every WIFI_RETRY_INTERVAL_MS, forever. There is no
//?                   address to serve on, so there is still no page and no OTA.
//?   NET_CONNECTED   Joined. The configuration page and the OTA listener are live.
//!
//! Nothing outside this file may WRITE net_state. The transitions and the actions
//! that drive them are the only place that knows the page and the OTA listener
//! have to be torn down together with the radio, and a write from elsewhere would
//! leave one of them bound to a dead interface - which is exactly the bug that
//! made lightpack.local resolve to nothing (see components/ESP32/setup.h).
//* The state, the enum and its names live in shared/variables.h, because the
//? onboard status blink in services/led_scripts.h reads it too and that header is
//* included first. Reading it from elsewhere is fine; changing it is not.

//* When the next join retry is due, and when the current drop was first seen.
uint32_t wifi_retry_at = 0;
uint32_t wifi_lost_at = 0;

//* The join the page asked for: which slot it targeted, when the window opened,
//? and what to tell the user if it closes without a connection. An attempt time of
//* 0 means nothing is being timed, which is the case for every ordinary reconnect.
uint8_t wifi_join_attempt_slot = 0;
uint32_t wifi_join_attempt_at = 0;
String wifi_join_error = "";

//* Whether the one-shot failure diagnostics have already run this boot.
bool wifi_diagnostics_done = false;

const char *wifi_state_name()
{
    switch (net_state)
    {
    case NET_CONNECTING:
        return "not connected";
    case NET_CONNECTED:
        return "connected";
    default:
        return "offline";
    }
}

//* The persisted offline switch. Read once per boot in setup_WIFI() and written
//? by wifi_toggle_offline(); it lives beside the portal request in the "wifi"
//* namespace because both describe what the radio should do, not what it looks like.
bool wifi_offline_saved()
{
    preferences.begin("wifi", true);
    const bool offline = preferences.getBool(WIFI_OFFLINE_KEY, false);
    preferences.end();
    return offline;
}

void wifi_offline_store(bool offline)
{
    preferences.begin("wifi", false);
    preferences.putBool(WIFI_OFFLINE_KEY, offline);
    preferences.end();
}

//* Is there a network to aim at at all? This is what separates "cannot reach my
//? access point" - which is retried forever and never opens a portal - from
//* "nobody has ever told me about a network", which is the one case that does.
//! Answered from the slots in Preferences, NOT from the driver's STA config the
//? way it used to be. The driver holds one network and only the last attempt, so a
//? board whose other saved network was tried last would report "nothing
//! configured" while holding two of them - and open a portal over a network it can
//* already join perfectly well.
bool wifi_has_credentials()
{
    return wifi_credential_count() > 0;
}

//* The page and the OTA listener, both owned by headers included after this one.
void setup_webpage();
void webpage_stop();

//* ---------------------- TRANSITIONS ----------------------
//? Every one of these is the complete story for entering its state: the callers
//? (setup_WIFI, wifi_loop, wifi_toggle_offline) decide WHEN, and these decide
//* WHAT, so a state can never be entered half-way.

void net_set_state(uint8_t next)
{
    if (net_state == next)
        return;
    net_state = next;

    //* The same colour language the rest of the firmware uses on the onboard
    //? pixel: green means it is on the network, amber means it is trying, red
    //* means the radio is off and only a button will bring it back.
    switch (next)
    {
    case NET_CONNECTED:
        led_notice(0, 45, 0, 700);
        break;
    case NET_CONNECTING:
        led_notice(45, 25, 0, 700);
        break;
    default:
        led_notice(45, 0, 0, 900);
        break;
    }

    Serial.printf("[NET] %s\n", wifi_state_name());
}

//* Radio off, everything that depended on the interface torn down with it.
void net_enter_offline()
{
    webpage_stop();   // HTTP listener first: it is the one holding port 80
    ArduinoOTA.end(); // OTA socket and the mDNS responder it owns

    //! eraseap stays false. WiFi.disconnect(true) would otherwise be read as
    //! "forget the network", and the whole point of offline mode is that the
    //! board comes back to the same network when the switch is flipped again.
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);

    wifi_lost_at = 0;
    wifi_retry_at = 0;
    net_set_state(NET_OFFLINE);
}

//* Radio up against the stored credentials. Nothing is joined yet, so no page.
void net_enter_connecting()
{
    //* WIFI_STA, not the AP_STA setup_ESP32() left behind: an offline round trip
    //? through the portal can leave the AP half-configured, and there is no
    //* portal open here to justify one.
    WiFi.mode(WIFI_STA);

    //* The DHCP name, and the one local DNS and LLMNR/NetBIOS answer to.
    //! This used to be set by WiFiManager.setHostname() on every boot, because
    //! autoConnect() ran on every boot. It now runs only when a portal opens, so
    //? a board that joins straight from its stored credentials would announce
    //? itself to the router as esp32c3-<mac> while still answering to
    //! lightpack.local over mDNS - the exact split identity this firmware warns
    //* about elsewhere. Set here, before begin(), so the DHCP request carries it.
    WiFi.setHostname(NETWORK_HOSTNAME);

    //? WiFi.begin() with no arguments, which used to be here, reconnects from
    //* whatever the driver already holds - and that is only ever the network
    //? attempted last. The active slot is joined explicitly instead, so entering
    //* this state always aims at the network the board currently believes in.
    //! Skipped when the portal just left us joined.
    if (WiFi.status() != WL_CONNECTED)
        wifi_credential_join_active();

    wifi_lost_at = 0;
    wifi_retry_at = millis();
    net_set_state(NET_CONNECTING);
}

//* Joined: bring up the two things that need an address.
void net_enter_connected()
{
    Serial.println("[NET] Connected to WiFi");
    Serial.println(WiFi.localIP());

    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, macAdress);
    if (ret == ESP_OK)
    {
        Serial.printf("MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
                      macAdress[0], macAdress[1], macAdress[2],
                      macAdress[3], macAdress[4], macAdress[5]);
    }

    String current_hostname = WiFi.getHostname();
    Serial.printf("Hostname: %s (LAN name: %s.local)\n",
                  current_hostname.c_str(), NETWORK_HOSTNAME);

    //* The join is the evidence. Adopt the network into a slot (writing back the
    //? password that actually worked), make it the active one, and clear the offline
    //* switch - a board that just came up on a network is not a board parked offline.
    //! Replaces the parallel credential copy that nothing read back.
    wifi_adopt_join();

    //* A page-requested join's window closes as a success only if THIS join is the
    //? one that arrived. Cleared here rather than at the deadline check, so a board
    //! that connects on the last second is never reported as a failure after the
    //? fact - and left set when the connection came from the fallback slot, where
    //* the page still needs to say which network it was that did not answer.
    if (wifi_join_attempt_at != 0)
    {
        wifi_join_attempt_at = 0;
        wifi_join_error = "";
    }

    //! OTA before the page. ArduinoOTA::begin() is what initialises the mDNS
    //? responder (_mdnsEnabled defaults to true), and setup_webpage() only adds
    //! the http service to it. Reversed, the page would advertise into a responder
    //* that does not exist yet.
    setup_OTA();
    setup_webpage();

    wifi_lost_at = 0;
    net_set_state(NET_CONNECTED);
}

//* Moved while currently joined. Teardown only - wifi_loop() owns the retry.
void net_link_lost()
{
    webpage_stop();
    ArduinoOTA.end();
    wifi_lost_at = 0;
    wifi_retry_at = millis();
    net_set_state(NET_CONNECTING);
}

//* Removed: wifi_join_saved(). It was the "portal closed with nothing new, go
//? back to the stored network" fallback, and it existed because the old
//! setup_WIFI() had to leave with an answer - joined or not - before it returned.
//* net_enter_connecting() does the same reconnect without blocking and keeps
//? retrying, so the only thing the old helper could still add was a second,
//* divergent way to join. Deleted rather than left uncalled: a dead setSleep in
//* this same file is what hid the modem-sleep bug for as long as it did.

//* The SSID the next join attempt will use.
//! Read from Preferences, not from the driver's STA config: the driver holds only
//? the network attempted last, so on a board rotating between two saved networks it
//* answers a different question than this one asks.
String wifi_saved_ssid()
{
    return wifi_credential_ssid(wifi_credential_active());
}

void wifi_report_status()
{
    Serial.println("[WIFI] ---------- radio status ----------");
    Serial.printf("[WIFI] mode=%d status=%d channel=%d rssi=%d\n",
                  (int)WiFi.getMode(), (int)WiFi.status(), WiFi.channel(), WiFi.RSSI());
    Serial.printf("[WIFI] sta_ip=%s ap_ip=%s ap_stations=%d\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.softAPIP().toString().c_str(),
                  WiFi.softAPgetStationNum());
    Serial.printf("[WIFI] sta_mac=%s\n", WiFi.macAddress().c_str());
    for (uint8_t slot = 0; slot < WIFI_CREDENTIAL_SLOTS; slot++)
        Serial.printf("[WIFI] saved slot %u: '%s'%s\n", (unsigned)slot,
                      wifi_credential_ssid(slot).c_str(),
                      (slot == wifi_credential_active()) ? "  <== active" : "");
}

void wifi_report_scan()
{
    //? The decisive test for "can this board even see my network". The ESP32-C3
    //? is 2.4 GHz only, so a 5 GHz SSID can never appear here and no amount of
    //? credential juggling will help. Reading nothing at all points at the RF
    //? path instead of the network.
    if (WiFi.getMode() == WIFI_OFF)
        WiFi.mode(WIFI_STA); // The scan needs a live radio

    //? Both saved networks are matched, not just the active one. The whole point of
    //* a second slot is a network that is expected to be absent for now, and a scan
    //? that only reported on the active slot could not tell "my hotspot is not on
    //* yet" apart from "this board cannot see anything at all".
    bool seen[WIFI_CREDENTIAL_SLOTS] = {false};
    bool any_saved = false;
    for (uint8_t slot = 0; slot < WIFI_CREDENTIAL_SLOTS; slot++)
        if (wifi_credential_present(slot))
            any_saved = true;

    Serial.println("[WIFI] ---------- scan (2.4 GHz only) ----------");
    const int found = WiFi.scanNetworks();

    if (found <= 0)
    {
        Serial.printf("[WIFI] scan returned %d (0 or less = nothing heard at all)\n", found);
        Serial.println("[WIFI] Hearing NOTHING is an RF problem, not a credentials problem:");
        Serial.println("[WIFI] check the antenna select resistor / IPEX connector on the C3 board,");
        Serial.println("[WIFI] and check for a brown-out when the radio transmits.");
        return;
    }

    for (int i = 0; i < found; i++)
    {
        const int8_t slot = wifi_credential_find(WiFi.SSID(i).c_str());
        if (slot >= 0)
            seen[(uint8_t)slot] = true;

        Serial.printf("[WIFI] %2d) %-30s rssi=%4d ch=%2d auth=%-2d%s\n",
                      i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                      (int)WiFi.encryptionType(i),
                      (slot >= 0) ? ((slot == (int8_t)wifi_credential_active()) ? "  <== active" : "  <== saved")
                                  : "");
    }
    WiFi.scanDelete();

    Serial.println("[WIFI] auth: 0=open 2=WPA 3=WPA2 4=WPA/WPA2 6=WPA3 7=WPA2/WPA3");
    Serial.printf("[WIFI] %d networks heard\n", found);

    if (!any_saved)
    {
        Serial.println("[WIFI] No saved SSID to match against.");
        return;
    }

    for (uint8_t slot = 0; slot < WIFI_CREDENTIAL_SLOTS; slot++)
    {
        if (!wifi_credential_present(slot))
            continue;

        if (seen[slot])
            Serial.printf("[WIFI] slot %u '%s' IS visible here: a join failure on it is association or credentials, not range.\n",
                          (unsigned)slot, wifi_credential_ssid(slot).c_str());
        else
            Serial.printf("[WIFI] slot %u '%s' NOT seen: switched off, 5 GHz-only, out of range, or a hidden SSID.\n",
                          (unsigned)slot, wifi_credential_ssid(slot).c_str());
    }
}

//* ---------------------- CREDENTIAL SLOTS ----------------------
//? Both networks live in the "wifi" namespace, beside the pointer to the one the
//* next attempt uses. Callers address a slot by number, so the storage and the
//? "which one now" decision cannot drift apart the way the old parallel copy did.

//? An out-of-range slot lands on the last slot rather than reading past the key
//* tables, which is what a corrupt or older NVS value would otherwise do.
uint8_t wifi_credential_clamp(uint8_t slot)
{
    return (slot < WIFI_CREDENTIAL_SLOTS) ? slot : (uint8_t)(WIFI_CREDENTIAL_SLOTS - 1);
}

//* Read one credential string, quietly.
//? Preferences::getString() takes a default for exactly this case, but it calls
//? nvs_get_str() with a null buffer first and the library logs [E] "len fail:
//! NOT_FOUND" when the key is simply absent - which is the ordinary state of an
//? unused slot, not a fault. isKey() answers the same question through getType(),
//? which tries the raw nvs_get_* calls and logs nothing, so an unprovisioned slot
//* stops writing a false error into the log on every status refresh.
String wifi_credential_read(const char *key)
{
    preferences.begin("wifi", true);
    const String value = preferences.isKey(key) ? preferences.getString(key, "") : String();
    preferences.end();
    return value;
}

bool wifi_credential_present(uint8_t slot)
{
    return wifi_credential_read(WIFI_CREDENTIAL_SSID_KEYS[wifi_credential_clamp(slot)]).length() > 0;
}

String wifi_credential_ssid(uint8_t slot)
{
    return wifi_credential_read(WIFI_CREDENTIAL_SSID_KEYS[wifi_credential_clamp(slot)]);
}

String wifi_credential_password(uint8_t slot)
{
    return wifi_credential_read(WIFI_CREDENTIAL_PASS_KEYS[wifi_credential_clamp(slot)]);
}

uint8_t wifi_credential_count()
{
    uint8_t count = 0;
    for (uint8_t slot = 0; slot < WIFI_CREDENTIAL_SLOTS; slot++)
        if (wifi_credential_present(slot))
            count++;
    return count;
}

//* The first slot with nothing in it, or WIFI_CREDENTIAL_SLOTS when both are
//? taken. That sentinel is deliberate: 0 is a valid slot number, so "no free slot"
//* has to be a value no caller can mistake for one.
uint8_t wifi_credential_free_slot()
{
    for (uint8_t slot = 0; slot < WIFI_CREDENTIAL_SLOTS; slot++)
        if (!wifi_credential_present(slot))
            return slot;
    return WIFI_CREDENTIAL_SLOTS;
}

//* Which slot the next attempt uses.
//! An active slot with nothing in it is not an answer: this falls through to the
//? first slot that does hold a network, so no caller is ever aimed at an empty
//* slot while a saved network sits beside it.
uint8_t wifi_credential_active()
{
    preferences.begin("wifi", true);
    uint8_t slot = (uint8_t)preferences.getUChar(WIFI_CREDENTIAL_ACTIVE_KEY, 0);
    preferences.end();

    slot = wifi_credential_clamp(slot);
    if (wifi_credential_present(slot))
        return slot;

    for (uint8_t other = 0; other < WIFI_CREDENTIAL_SLOTS; other++)
        if (wifi_credential_present(other))
            return other;
    return slot;
}

void wifi_credential_set_active(uint8_t slot)
{
    preferences.begin("wifi", false);
    preferences.putUChar(WIFI_CREDENTIAL_ACTIVE_KEY, wifi_credential_clamp(slot));
    preferences.end();
}

//* The slot holding this SSID, or -1. It is how a network the board was just
//? provisioned with is recognised, so its password is written back into the slot
//* that already describes it instead of into whichever slot is active.
int8_t wifi_credential_find(const char *ssid)
{
    if (ssid == nullptr || ssid[0] == '\0')
        return -1;

    for (uint8_t slot = 0; slot < WIFI_CREDENTIAL_SLOTS; slot++)
        if (wifi_credential_ssid(slot) == ssid)
            return (int8_t)slot;
    return -1;
}

//* Write a network into a slot and make it the one the next attempt uses.
void wifi_credential_store(uint8_t slot, const char *ssid, const char *password)
{
    slot = wifi_credential_clamp(slot);
    preferences.begin("wifi", false);
    preferences.putString(WIFI_CREDENTIAL_SSID_KEYS[slot], ssid);
    preferences.putString(WIFI_CREDENTIAL_PASS_KEYS[slot], password != nullptr ? password : "");
    preferences.putUChar(WIFI_CREDENTIAL_ACTIVE_KEY, slot);
    preferences.end();

    Serial.printf("[WIFI] Credential slot %u holds '%s'\n", (unsigned)slot, ssid);
}

//* Forget one slot. The page's "forget" and the recovery boot are the only two
//? things that remove a network, and this is the narrow one: one slot, no reboot.
//? The active pointer is moved to the other slot when there is one, so forgetting
//* the network the board is on does not leave it aiming at an empty slot.
void wifi_credential_clear(uint8_t slot)
{
    slot = wifi_credential_clamp(slot);

    preferences.begin("wifi", true);
    const uint8_t stored_active = (uint8_t)preferences.getUChar(WIFI_CREDENTIAL_ACTIVE_KEY, 0);
    preferences.end();

    preferences.begin("wifi", false);
    preferences.remove(WIFI_CREDENTIAL_SSID_KEYS[slot]);
    preferences.remove(WIFI_CREDENTIAL_PASS_KEYS[slot]);
    preferences.end();

    Serial.printf("[WIFI] Credential slot %u forgotten\n", (unsigned)slot);

    const uint8_t other = (uint8_t)((slot + 1) % WIFI_CREDENTIAL_SLOTS);
    if (stored_active == slot && wifi_credential_present(other))
        wifi_credential_set_active(other);
}

//* The next slot after `from` that holds a network, wrapping around.
//? Returns `from` when every other slot is empty, which is the single-network case
//* and has to mean "try the same one again" rather than "give up".
uint8_t wifi_credential_next(uint8_t from)
{
    from = wifi_credential_clamp(from);
    for (uint8_t step = 1; step <= WIFI_CREDENTIAL_SLOTS; step++)
    {
        const uint8_t candidate = (uint8_t)((from + step) % WIFI_CREDENTIAL_SLOTS);
        if (wifi_credential_present(candidate))
            return candidate;
    }
    return from;
}

//* Point the ESP-IDF driver at one slot and start the attempt.
//! This is the only place the driver's STA config is written, and writing it is
//? what makes two networks possible at all: WiFi.begin() with no arguments
//! reconnects from whatever the driver happens to hold, so a rotation that did not
//* re-point the driver would retry the same network forever.
//! WiFi.disconnect(false, false) first, both arguments false: it abandons an
//? attempt already in flight without powering the radio down and without erasing
//* the driver's stored config.
bool wifi_credential_join(uint8_t slot)
{
    slot = wifi_credential_clamp(slot);

    const String ssid = wifi_credential_ssid(slot);
    if (ssid.length() == 0)
    {
        Serial.printf("[WIFI] Credential slot %u is empty - nothing to join\n", (unsigned)slot);
        return false;
    }
    const String password = wifi_credential_password(slot);

    wifi_credential_set_active(slot);
    WiFi.disconnect(false, false);
    WiFi.begin(ssid.c_str(), password.c_str());

    Serial.printf("[WIFI] Joining slot %u: '%s'\n", (unsigned)slot, ssid.c_str());
    return true;
}

bool wifi_credential_join_active()
{
    return wifi_credential_join(wifi_credential_active());
}

//* One-time adoption of a network configured by a build that had no slots. Back
//? then it lived only in the driver's own config, because WiFiManager wrote it
//* there, so an upgraded board would otherwise come up with two empty slots and
//! open a portal over a network it is already perfectly able to join.
void wifi_credentials_migrate()
{
    if (wifi_credential_count() > 0)
        return;

    //? The driver's config can only be read once the radio exists, which is why
    //* this is called from setup_WIFI() rather than from setup()'s first line.
    wifi_config_t conf = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK || conf.sta.ssid[0] == '\0')
        return;

    wifi_credential_store(0, (const char *)conf.sta.ssid, (const char *)conf.sta.password);
    Serial.println("[WIFI] Adopted the driver's saved network into slot 0");
}

//* The wifi half of a recovery boot: the whole "wifi" namespace, plus the network
//? the ESP-IDF driver keeps in an NVS namespace of its own.
//! Wiped as a namespace rather than key by key, and that is safe for exactly one
//? reason: nothing but this file ever writes to "wifi" (the key list is at the top
//* of it), so the namespace and the wifi service are the same set of things.
//? The offline switch goes with the credentials deliberately. A board left parked
//? offline would come back with no network AND no hotspot, and the only way out of
//* that is another button hold - a poor place to end a recovery.
void wifi_recovery_wipe()
{
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();

    //! eraseap = true erases the driver's own stored STA config. Without it the
    //? board rejoins the network it was just told to forget, because the driver
    //* keeps its own copy in NVS - the very copy wifi_credentials_migrate() reads.
    WiFi.disconnect(true, true);

    Serial.println("[WIFI] Cleared: saved networks, the offline switch, the portal request");
}

//* Whatever the radio is joined to becomes a saved slot, with the password that
//? actually worked written back into it: a completed join is the only evidence
//* that a stored password is the right one.
//! This is also where a successful provision clears the offline switch. A board
//? parked offline, given credentials, and then dropped straight back to offline
//* would make the portal and the 3 s skip into a one-way trip - see setup_WIFI().
void wifi_adopt_join()
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    const String ssid = WiFi.SSID();
    if (ssid.length() == 0)
        return;

    const int8_t known = wifi_credential_find(ssid.c_str());
    uint8_t slot;
    if (known >= 0)
    {
        slot = (uint8_t)known;
    }
    else
    {
        //? A network nobody saved: a portal provision, most likely. The first
        //* empty slot, and only when both are taken the active one - overwriting
        //? the active slot is the least destructive option left, and it is the
        //* network the user has just told the board to use.
        const uint8_t free_slot = wifi_credential_free_slot();
        slot = (free_slot < WIFI_CREDENTIAL_SLOTS) ? free_slot : wifi_credential_active();
    }

    //? Written only when something differs: this runs on every join, and a
    //* needless flash write per boot is a cost with no reader.
    const String password = WiFi.psk();
    if (wifi_credential_ssid(slot) != ssid || wifi_credential_password(slot) != password)
        wifi_credential_store(slot, ssid.c_str(), password.c_str());
    else
        wifi_credential_set_active(slot);

    if (wifi_offline_saved())
    {
        wifi_offline_store(false);
        Serial.println("[NET] The join confirmed a network - offline mode cleared");
    }
}

//* ---------------------- JOIN AND SCAN, DRIVEN BY THE PAGE ----------------------
//? One rule shapes both of these: a request must never block. A scan takes 1.5-3 s
//? on this board and a join takes as long as the association does, so the scan is
//! asynchronous between loop() passes and the join is timed out in wifi_loop()
//* rather than waited on by the handler that asked for it.

//* Start an attempt the page can be told about.
//? The reply to that request has already been sent by the time this runs: a join on
//* a different network takes the page down with it, so the browser must have its
//* answer in hand first.
void wifi_join_begin(uint8_t slot)
{
    slot = wifi_credential_clamp(slot);

    wifi_join_error = "";
    wifi_join_attempt_slot = slot;
    wifi_join_attempt_at = millis();

    if (net_state == NET_OFFLINE || WiFi.getMode() == WIFI_OFF)
    {
        //* Coming back from offline is the whole transition, not a bare begin():
        //* the radio has to come up, the hostname has to be set, and the page and
        //* the OTA listener have to be armed again on the way through.
        net_enter_connecting();
        return;
    }

    //! Joined: this tears the page down NOW rather than after the drop grace
    //? period. The user asked to move to another network, and a board that keeps
    //* serving on a link it is about to leave is a board that lies about its state.
    if (net_state == NET_CONNECTED)
        net_link_lost();

    wifi_credential_join(slot);
}

//* Kick off a scan. Asynchronous on purpose: this returns while the radio is still
//? sweeping and the page polls for the result separately, so loop() - the strip,
//* the gyro, the buttons - keeps running throughout.
bool wifi_scan_start()
{
    if (WiFi.getMode() == WIFI_OFF)
    {
        Serial.println("[WIFI] Scan refused: the radio is off");
        return false;
    }
    if (WiFi.scanComplete() == WIFI_SCAN_RUNNING)
    {
        //? Already sweeping. The driver would refuse a second start as well; saying
        //* so here keeps the log honest about which request did what.
        return false;
    }

    //* Drop the previous result set first. Without this a poll arriving between
    //? the two would read the scan before last and the page would show a list that
    //* does not match the request that asked for it.
    WiFi.scanDelete();

    const int16_t started = WiFi.scanNetworks(true);
    Serial.printf("[WIFI] Scan started from the page (%d)\n", started);
    return started == WIFI_SCAN_RUNNING;
}

//* WIFI_SCAN_RUNNING while it sweeps, WIFI_SCAN_FAILED if it died, else the count.
int16_t wifi_scan_status()
{
    return WiFi.scanComplete();
}

void wifi_scan_release()
{
    WiFi.scanDelete();
}

//* The one blocking window of the boot. Only ever called from setup_WIFI(), so
//? loop() is not running yet and being blocked costs nothing but boot time - which
//! is still why the runtime portal button restarts the board instead of calling
//* this from loop() (see wifi_request_portal_on_boot).
//!
//! What changed: it is no longer a window the user has to sit through. The portal
//? used to run WiFiManager's own loop, so the ONLY way out was the clock - a minute
//! of a board that is neither serving nor listening, with the strip frozen and no
//* way to say "not now". It is pumped one pass at a time here instead, so the boot
//? button can leave it in three seconds, and the countdown is ours to keep.
uint8_t run_config_portal(bool forced, uint16_t timeout_s, bool portal_enabled)
{
    uint8_t result = PORTAL_CLOSED;

    //* The window, in the same shape the old code used: a portal nobody asked for
    //? gets a token timeout when portals are disabled, because the saved
    //* credentials are what that boot is really trying.
    const uint16_t window_s = (forced || portal_enabled) ? timeout_s : 1;

    { // Scoped block so WiFiManager is destroyed before we start our own web server
        WiFiManager wifiManager;

        wifiManager.setAPCallback([window_s](WiFiManager *myWiFiManager)
                                  {
            Serial.println();
            Serial.println("=== CONFIG HOTSPOT IS UP NOW ===");
            Serial.printf("SSID : %s   (open network, no password)\n",
                          myWiFiManager->getConfigPortalSSID().c_str());
            Serial.printf("URL  : http://%s/\n", WiFi.softAPIP().toString().c_str());
            Serial.printf("ALIVE: %u seconds, or until GPIO9 is held for %u s to skip it.\n",
                          (unsigned)window_s, (unsigned)(WIFI_PORTAL_SKIP_HOLD_MS / 1000));
            Serial.println("=== join it now; the boot button can reopen it later ==="); });

        wifiManager.setSaveConfigCallback([]()
                                          { Serial.println("Should save config"); });

        //? Keep the portal alive while a device is actually associated with the
        //? hotspot. WiFiManager's default is false, which means the countdown runs
        //! whether or not anyone is connected - so a client that joins late, or is
        //! halfway through entering credentials, gets cut off when the clock runs
        //! out and the AP silently disappears underneath them.
        wifiManager.setAPClientCheck(true);

        //* The rewrite in one line: with WiFiManager's own loop disabled, the portal
        //? is serviced by process() in the wait below and THIS function decides when
        //! it ends. In that mode its configured timeout is not applied either, so the
        //* countdown further down is not a duplicate - it is the only one there is.
        wifiManager.setConfigPortalBlocking(false);

        String hostname = NETWORK_HOSTNAME;

        //? DHCP hostname: what the router lists this board as, what its local DNS
        //? record says, and what LLMNR/NetBIOS (Windows) answer to. It has to
        //? match the mDNS name in webpage.h, otherwise the board is reachable
        //? under one name over .local and shows up under another in the router.
        wifiManager.setHostname(hostname);

        if (forced)
        {
            //* startConfigPortal(), NOT autoConnect(): this boot exists to hand the
            //? user the portal, whatever state the saved credentials are in - and
            //* this line is how the log tells the two paths apart, because the AP
            //* banner above is identical either way.
            Serial.println("[WIFI] Boot button asked for the portal - opening it now");
            wifiManager.setConfigPortalTimeout(window_s);
            wifiManager.startConfigPortal(WIFI_PORTAL_AP_SSID);
        }
        else
        {
            wifiManager.setConfigPortalTimeout(window_s);

            //? The provisioning hotspot keeps its own descriptive SSID - it is only
            //? visible on an unprovisioned board, and it is not the network name.
            //? autoConnect() with the loop disabled brings the AP up and returns;
            //* the join, if there is one, is what the wait below notices.
            wifiManager.autoConnect(WIFI_PORTAL_AP_SSID);
        }

        const uint32_t opened_at = millis();
        uint32_t window_started_at = opened_at;
        uint32_t skip_blink_at = 0;
        bool portal_seen = false;
        bool skipped = false;

        //* The switch's classified state, so this window can hand it back untouched.
        //? A press that ends while the hotspot is up belongs to the window, not to
        //! loop(): the 3 s offline toggle, the 6 s portal request and the 10 s
        //* recovery wipe are all meaningless while the portal is already being
        //* offered, and acting on one of them after the boot would be a surprise
        //* with no visible cause. Everything below is restored before returning.
        const uint32_t events_before = boot_button.event_count;
        const uint32_t last_event_ms_before = boot_button.last_event_ms;
        const uint16_t hold_ms_before = boot_button.hold_ms;
        const uint8_t last_event_before = boot_button.last_event;
        const uint8_t counter_before = boot_button.counter;

        while (true)
        {
            //* The portal's entire server, serviced once per pass.
            //! Its return value says whether the portal CONNECTED, not whether it is
            //? alive - it answers false on every idle pass - so it is not a liveness
            //* test. See getConfigPortalActive() below.
            wifiManager.process();

            //* Polled rather than waited for. setup_button_interrupt() ran before
            //? setup_WIFI(), so the edge is already latched and the press clock has
            //* been running since the finger went down - only the classification was
            //* idle, and button_poll() is what does it.
            button_poll(boot_button);

            //? Only a press that BEGAN with the portal up can skip it. Without this,
            //* a hold that started during the boot - a hand resting on the switch -
            //* would already be past the threshold the moment the portal opened, and
            //* the hotspot would vanish before it was ever really offered.
            if (boot_button.pressed && boot_button.press_start_ms >= opened_at)
            {
                const uint32_t held = millis() - boot_button.press_start_ms;

                //* The last second before the skip: an amber pulse. The strip is
                //* frozen in setup(), so the onboard pixel is the only thing here
                //* that can confirm the hold is being seen at all.
                if (held >= (WIFI_PORTAL_SKIP_HOLD_MS - WIFI_PORTAL_WARN_MS) &&
                    (millis() - skip_blink_at) >= WIFI_PORTAL_WARN_MS)
                {
                    skip_blink_at = millis();
                    led_notice(45, 25, 0, WIFI_PORTAL_WARN_MS);
                }

                if (held >= WIFI_PORTAL_SKIP_HOLD_MS)
                {
                    skipped = true;
                    break;
                }
            }

            if (WiFi.status() == WL_CONNECTED)
            {
                result = PORTAL_CONNECTED;
                break;
            }

            //* Latched, then watched: the first pass establishes that the portal is
            //? up, so a later "it is down" is a real shutdown - a save that took, or
            //* WiFiManager's own timeout - rather than a reading taken in the instant
            //* before it started. Nothing is left to service either way, and running
            //* out the rest of the window would only be a boot delay.
            if (wifiManager.getConfigPortalActive())
                portal_seen = true;
            else if (portal_seen)
            {
                Serial.println("[WIFI] Portal closed");
                break;
            }

            //? A client that is actually using the hotspot restarts the window. That
            //? is what setAPClientCheck(true) does for WiFiManager's own loop, and
            //! this countdown replaced that loop - so the rule has to be repeated
            //* here, or a user halfway through typing a password would have the AP
            //* disappear underneath them.
            if (WiFi.softAPgetStationNum() > 0)
                window_started_at = millis();

            if ((millis() - window_started_at) >= ((uint32_t)window_s * 1000UL))
            {
                Serial.println("[WIFI] Portal window closed");
                break;
            }

            //* Nothing else to do in this pass. delay() yields to the scheduler, so
            //* the idle task and the radio keep running while the portal is up.
            delay(10);
        }

        //* Hand the switch back exactly as it was found - see the snapshot above.
        boot_button.event_count = events_before;
        boot_button.last_event_ms = last_event_ms_before;
        boot_button.hold_ms = hold_ms_before;
        boot_button.last_event = last_event_before;
        boot_button.counter = counter_before;

        if (skipped)
        {
            //* Red, which is the colour the offline blink will be using a moment
            //? from now: the board says where it is going rather than going quiet.
            led_notice(45, 0, 0, 1200);

            //! The press has been acted on, so it must not be handed back to the
            //? classifier: the same three seconds would read as a five-second portal
            //! request or, past ten, as the recovery wipe - and both of those restart
            //* the board, which would undo the skip on the way back up.
            button_discard_press(boot_button);

            result = PORTAL_SKIPPED;
            Serial.println("[WIFI] Portal skipped - the button asked to leave it");
        }

        wifiManager.stopConfigPortal();
        // wifiManager goes out of scope (its internal web server on port 80 stops here)
    }

    return result;
}

//* Bring the network up, once, from setup(). Which of the three states the board
//? lands in is decided entirely by two stored facts - the offline switch and
//* whether a network has ever been configured - plus the one-shot portal request.
void setup_WIFI(bool portal_enabled)
{
    //* Applied before anything else the radio does, so the join gets the latency
    //? the joined session expects. The driver is already running by now -
    //? setup_ESP32() called WiFi.mode(WIFI_AP_STA) - which is what
    //! esp_wifi_set_ps() needs to take effect immediately. Called before
    //? WiFi.mode() it would only record the value, which is still the right
    //? outcome: the framework re-applies _sleepEnabled on every STA_START, so it
    //* survives every mode change below, including coming back from offline.
    WiFi.setSleep(WIFI_MODEM_SLEEP != 0);

    //* Before anything asks whether a network is configured at all: a board
    //? upgrading from a build without slots has its network only in the driver's
    //* config, and would otherwise look unprovisioned.
    wifi_credentials_migrate();

    const bool forced_portal = wifi_portal_requested();
    const bool offline = wifi_offline_saved();
    const bool have_credentials = wifi_has_credentials();

    //* Which portal, if any, this boot owes the user:
    //?   * the 5 s press outranks everything, including the offline switch. It is
    //?     the documented way back in when the credentials are wrong or the board
    //?     is parked offline, and a board in offline mode has no page to fix
    //!     either from - so this is the only way in, and it always opens.
    //?   * an online board with no saved credentials opens one unprompted. The
    //!     alternative is retrying a network whose name it does not know, forever.
    const bool open_portal = forced_portal || (!offline && !have_credentials);

    if (!open_portal && offline)
    {
        //* Offline: the radio never comes up, so there is no hotspot.
        WiFi.mode(WIFI_OFF);
        net_state = NET_OFFLINE;
        Serial.println("[NET] Offline mode - radio off, no hotspot");
        Serial.println("[NET] Hold GPIO9 for 3 s to go back online");
        return;
    }

    if (open_portal)
    {
        const uint16_t window_s = forced_portal ? WIFI_PORTAL_BUTTON_TIMEOUT_S
                                                : WIFI_PORTAL_TIMEOUT_S;
        const uint8_t outcome = run_config_portal(forced_portal, window_s, portal_enabled);

        //* Skipped: the user stood at the board, watched the hotspot come up and
        //? said no. That is a decision, so it is persisted rather than forgotten -
        //* the radio goes off now and stays off across reboots, and the 5 s hold is
        //* the documented way back in.
        if (outcome == PORTAL_SKIPPED)
        {
            wifi_offline_store(true);
            Serial.println("[NET] Portal skipped - offline mode on, radio off");
            Serial.println("[NET] Hold GPIO9 for 6 s to open the config hotspot");
            net_enter_offline();
            return;
        }

        //* Anything else that came back JOINED is a provision, and a provision is
        //? the user saying they want this network: adopt it into a slot, which also
        //* clears the offline switch.
        //! The switch is re-read below instead of reusing the value read at the top
        //? of this function, and that is the whole point of this call. A board
        //* parked offline, given credentials through the portal, and then dropped
        //! back to offline - because the value read before the portal was still
        //* true - would make the portal a one-way trip.
        wifi_adopt_join();
    }

    if (wifi_offline_saved())
    {
        //* Still offline: either the board was parked and the portal was only
        //! looked at, or the portal closed without joining anything on purpose. A
        //? portal opened FROM offline mode must not promote the board to online
        //* just because someone opened its page.
        net_enter_offline();
        Serial.println("[NET] Portal closed - back to offline mode");
        return;
    }

    if (!wifi_has_credentials())
    {
        //* Nothing configured and nothing stored. This is the state the user
        //? described as "not connected", but without a network name there is
        //! nothing to retry, so the radio goes off rather than beaconing forever.
        //? A 1 s press brings the radio back, and a 5 s press reopens the portal.
        WiFi.mode(WIFI_OFF);
        net_state = NET_OFFLINE;
        Serial.println("[NET] No network configured - radio off until one is");
        Serial.println("[NET] Hold GPIO9 for 5 s to open the config hotspot");
        return;
    }

    //* Online with a network to aim at. Joined or not, this is the only path that
    //* ends here, and wifi_loop() takes it from a join attempt to a live page.
    Serial.printf("[NET] Aiming at '%s'\n", wifi_saved_ssid().c_str());
    net_enter_connecting();
}

//* Run every pass of loop(): drives the retry, the promotion to connected, and
//* the teardown when a joined link is genuinely gone.
void wifi_loop()
{
    switch (net_state)
    {
    case NET_OFFLINE:
        //? Nothing to do and nothing to poll. The radio is off; a press of GPIO9
        //* is what leaves this state, and that is polled elsewhere.
        return;

    case NET_CONNECTING:
        if (WiFi.status() == WL_CONNECTED)
        {
            net_enter_connected();
            return;
        }

        //! Nothing left to aim at. The page can forget the last saved network while
        //? the board is still joined to it, so this state is reachable with both
        //! slots empty as soon as that link goes. Beaconing forever for a network
        //* nobody has named is exactly what the offline state exists to avoid.
        if (!wifi_has_credentials())
        {
            WiFi.mode(WIFI_OFF);
            net_set_state(NET_OFFLINE);
            Serial.println("[NET] No saved network left - radio off");
            Serial.println("[NET] Hold GPIO9 for 6 s to open the config hotspot");
            return;
        }

        //* The window a PAGE-REQUESTED join gets. An ordinary reconnect never arms
        //? it, so an access point that is merely slow keeps being retried without
        //* ever being declared a failure.
        if (wifi_join_attempt_at != 0 && (millis() - wifi_join_attempt_at) >= WIFI_JOIN_TIMEOUT_MS)
        {
            wifi_join_attempt_at = 0;
            const String failed = wifi_credential_ssid(wifi_join_attempt_slot);
            wifi_join_error = failed + " did not answer";

            Serial.printf("[NET] '%s' did not join within %lu ms\n", failed.c_str(),
                          (unsigned long)WIFI_JOIN_TIMEOUT_MS);

            //* Fall back rather than sit on a slot that just failed. The other saved
            //? network is very often the one that was working before this one was
            //! typed in, and a board that goes back to it is a board the user can
            //* still reach in order to fix the mistake.
            const uint8_t other = (uint8_t)((wifi_join_attempt_slot + 1) % WIFI_CREDENTIAL_SLOTS);
            if (wifi_credential_present(other))
            {
                Serial.printf("[NET] Falling back to saved network %u\n", (unsigned)other);
                wifi_credential_join(other);
            }
        }

        if (millis() - wifi_retry_at >= WIFI_RETRY_INTERVAL_MS)
        {
            //* A retry is not a fresh start: WiFi.begin() reconnects from the
            //? stored configuration, so this is safe to repeat forever, and on an
            //? access point that is merely late it costs one attempt per minute.
            wifi_retry_at = millis();

#if WIFI_DIAGNOSTICS
            //* A minute in and still nothing. This is the moment the radio
            //? diagnostics are worth their cost, and the only moment they are
            //! taken: a board that cannot see its own network is the one case a
            //? log can settle on its own, and repeating a blocking scan every
            //* minute would freeze the strip once a minute forever.
            if (!wifi_diagnostics_done)
            {
                wifi_diagnostics_done = true;
                wifi_report_status();
                wifi_report_scan();
            }
#endif

            //* Rotate before retrying. With two saved networks this is what makes
            //? the second one reachable at all: a hotspot switched on after this
            //! board booted is only found when its slot comes round.
            //* One saved network returns the same slot, so the retry below is
            //? exactly what it was before the slots existed.
            const uint8_t next_slot = wifi_credential_next(wifi_credential_active());
            if (next_slot != wifi_credential_active())
                Serial.printf("[NET] Switching to saved network %u\n", (unsigned)next_slot);

            Serial.printf("[NET] Still not connected - retrying '%s'\n",
                          wifi_credential_ssid(next_slot).c_str());
            wifi_credential_join(next_slot);
        }
        return;

    case NET_CONNECTED:
        if (WiFi.status() == WL_CONNECTED)
        {
            wifi_lost_at = 0;
            return;
        }
        //* Dropped. Wait out the grace period before believing it - see
        //* WIFI_DROP_GRACE_MS for why a single missed beacon must not count.
        if (wifi_lost_at == 0)
        {
            wifi_lost_at = millis();
            Serial.printf("[NET] Link dropped - waiting %lu ms before teardown\n",
                          (unsigned long)WIFI_DROP_GRACE_MS);
            return;
        }
        if (millis() - wifi_lost_at >= WIFI_DROP_GRACE_MS)
            net_link_lost();
        return;
    }
}

//* The 1 s press: flip offline mode, live. Persisted, so it is the state the next
//* boot starts from, and applied now, so it is the state this boot ends in.
void wifi_toggle_offline()
{
    const bool going_offline = (net_state != NET_OFFLINE);
    wifi_offline_store(going_offline);

    if (going_offline)
    {
        Serial.println("[BUTTON] Offline mode ON - radio off");
        net_enter_offline();
    }
    else
    {
        Serial.println("[BUTTON] Offline mode OFF - radio on");
        net_enter_connecting();
        //* No portal here on purpose. A board with no saved network that comes
        //? out of offline mode lands in NET_OFFLINE off the back of
        //! setup_WIFI()'s no-credentials rule, and the 5 s press is the way to
        //* give it one - toggling the radio on should not put an AP on the air.
        if (!wifi_has_credentials())
        {
            WiFi.mode(WIFI_OFF);
            net_state = NET_OFFLINE;
            Serial.println("[NET] No network configured - radio off until one is");
        }
    }
}

#endif // CONNECT_WIFI
