#if !defined(CONNECT_WIFI)
#define CONNECT_WIFI

#include "shared/dependencies.h"

void saveCredentials(const char *ssid, const char *password);
void loadCredentials(char *ssid, char *password);
void get_saved_wifi();
bool setup_WIFI(bool portal_enabled = true);
String wifi_saved_ssid();
void wifi_report_status();
void wifi_report_scan();

//* How long the boot-time config hotspot stays up before it is torn down.
//? This is a fixed window measured from boot, not from your first connection
//? attempt, so a board that boots and sits idle will have closed its AP by the
//? time you go looking for it. Raising this lengthens the no-network boot delay.
#define WIFI_PORTAL_TIMEOUT_S 60

//? SSID of that boot-time configuration hotspot. Deliberately NOT the same string
//? as NETWORK_HOSTNAME: the hotspot is a temporary provisioning AP, while the
//? hostname is the name the board keeps on the LAN once it has joined.
#define WIFI_PORTAL_AP_SSID "Lightpack-Devkit"

//? Set to 0 to drop the scan/status reporting from the failure path.
#define WIFI_DIAGNOSTICS 1

String wifi_saved_ssid()
{
    //? The SSID this board actually tries to join lives in the ESP-IDF WiFi
    //? driver's own NVS, because WiFiManager persists through WiFi.persistent().
    //! It is NOT in the "wifi" Preferences namespace used by the two helpers
    //! above: saveCredentials() writes a parallel copy that nothing reads back,
    //! and loadCredentials() is never called. Do not use them to answer "which
    //! network is this board aiming at" - they will report the wrong thing.
    wifi_config_t conf = {};
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) != ESP_OK)
        return String("");
    return String((const char *)conf.sta.ssid);
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
    Serial.printf("[WIFI] target_ssid='%s'\n", wifi_saved_ssid().c_str());
}

void wifi_report_scan()
{
    //? The decisive test for "can this board even see my network". The ESP32-C3
    //? is 2.4 GHz only, so a 5 GHz SSID can never appear here and no amount of
    //? credential juggling will help. Reading nothing at all points at the RF
    //? path instead of the network.
    if (WiFi.getMode() == WIFI_OFF)
        WiFi.mode(WIFI_STA); // The scan needs a live radio

    const String target = wifi_saved_ssid();

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

    bool matched = false;
    for (int i = 0; i < found; i++)
    {
        const bool is_target = (target.length() > 0) && (WiFi.SSID(i) == target);
        if (is_target)
            matched = true;

        Serial.printf("[WIFI] %2d) %-30s rssi=%4d ch=%2d auth=%-2d%s\n",
                      i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                      (int)WiFi.encryptionType(i), is_target ? "  <== target" : "");
    }
    WiFi.scanDelete();

    Serial.println("[WIFI] auth: 0=open 2=WPA 3=WPA2 4=WPA/WPA2 6=WPA3 7=WPA2/WPA3");
    Serial.printf("[WIFI] %d networks heard\n", found);

    if (target.length() == 0)
        Serial.println("[WIFI] No saved SSID to match against.");
    else if (!matched)
        Serial.printf("[WIFI] '%s' NOT seen: 5 GHz-only, out of range, or a hidden SSID.\n", target.c_str());
    else
        Serial.printf("[WIFI] '%s' IS visible here: the join failure is association or credentials, not range.\n", target.c_str());
}

void saveCredentials(const char *ssid, const char *password)
{
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
}

void loadCredentials(char *ssid = nullptr, char *password = nullptr)
{
    preferences.begin("wifi", true);

    if (ssid != nullptr)
    {
        String savedSSID = preferences.getString("ssid", "");
        strncpy(ssid, savedSSID.c_str(), 32);
        ssid[32] = '\0';
    }
    if (password != nullptr)
    {
        String savedPassword = preferences.getString("password", "");
        strncpy(password, savedPassword.c_str(), 32);
        password[32] = '\0';
    }
    preferences.end();
}

void get_saved_wifi()
{
    char *ssid;
    char *password;

    // Gets saved values on preferences
    Serial.println("Getting WiFi data...\n");

    // Tries to connect to saved network
    WiFi.setSleep(false);
    WiFi.mode(WIFI_AP_STA);
    Serial.print("Connecting to WiFi ..");
    int dropCounter = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print('.');
        dropCounter++;
        delay(500);
        if (dropCounter > 8)
            break;
    }
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println(WiFi.localIP());
        Serial.println("Connected");
        return;
    }
    else
    {
        WiFi.mode(WIFI_OFF);
    }
    return;
}

bool setup_WIFI(bool portal_enabled)
{
    bool connected = false;

    { // Scoped block so WiFiManager is destroyed before we start our own web server
        WiFiManager wifiManager;

        wifiManager.setAPCallback([](WiFiManager *myWiFiManager)
                                  {
            Serial.println();
            Serial.println("=== CONFIG HOTSPOT IS UP NOW ===");
            Serial.printf("SSID : %s   (open network, no password)\n",
                          myWiFiManager->getConfigPortalSSID().c_str());
            Serial.printf("URL  : http://%s/\n", WiFi.softAPIP().toString().c_str());
            Serial.printf("ALIVE: %d seconds from boot, then it shuts down by itself.\n",
                          WIFI_PORTAL_TIMEOUT_S);
            Serial.println("=== join it now, this window does not reopen ==="); });

        wifiManager.setSaveConfigCallback([]()
                                          { Serial.println("Should save config"); });

        wifiManager.setConfigPortalTimeout(portal_enabled ? WIFI_PORTAL_TIMEOUT_S : 1);

        //? Keep the portal alive while a device is actually associated with the
        //? hotspot. WiFiManager's default is false, which means the countdown runs
        //! from BOOT whether or not anyone is connected - so a client that joins
        //! late, or is halfway through entering credentials, gets cut off when the
        //! clock runs out and the AP silently disappears underneath them.
        wifiManager.setAPClientCheck(true);

        String hostname = NETWORK_HOSTNAME;

        //? DHCP hostname: what the router lists this board as, what its local DNS
        //? record says, and what LLMNR/NetBIOS (Windows) answer to. It has to
        //? match the mDNS name in webpage.h, otherwise the board is reachable
        //? under one name over .local and shows up under another in the router.
        wifiManager.setHostname(hostname);

        //? The provisioning hotspot keeps its own descriptive SSID - it is only
        //? visible on an unprovisioned board, and it is not the network name.
        if (!wifiManager.autoConnect(WIFI_PORTAL_AP_SSID))
        {
            Serial.println("Failed to connect and hit timeout");
            connected = false;
        }
        else
        {
            connected = (WiFi.status() == WL_CONNECTED);
        }
        // wifiManager goes out of scope (its internal web server on port 80 stops here)
    }

    if (!connected)
    {
#if WIFI_DIAGNOSTICS
        //! The scan has to run BEFORE the radio is switched off below, and this
        //! is exactly the moment the information is worth having.
        wifi_report_status();
        wifi_report_scan();
#endif
        WiFi.mode(WIFI_OFF); // Turns off wifi for power saving
        return false;
    }

    Serial.println("Connected to WiFi");
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

    // Save credentials
    saveCredentials(WiFi.SSID().c_str(), WiFi.psk().c_str());

    // Start our persistent device portal (available at http://<localIP>/ )
    // startWebPortal();
    return true;
}

#endif // CONNECT_WIFI
