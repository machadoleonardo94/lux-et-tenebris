#if !defined(CONNECT_WIFI)
#define CONNECT_WIFI

#include "shared/dependencies.h"

#include "shared/constants.h"

WiFiClientSecure wifi_client;

void saveCredentials(const char *ssid, const char *password);
void loadCredentials(char *ssid, char *password);
void get_saved_wifi();
bool setup_WIFI(bool portal_enabled = true);

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
            Serial.println("Entered config mode");
            Serial.println(WiFi.softAPIP());
            Serial.println(myWiFiManager->getConfigPortalSSID()); });

        wifiManager.setSaveConfigCallback([]()
                                          { Serial.println("Should save config"); });

        wifiManager.setConfigPortalTimeout(portal_enabled ? 60 : 1);

        String hostname = "Lightpack Devkit";

        wifiManager.setHostname(hostname);

        if (!wifiManager.autoConnect(hostname.c_str()))
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

    // Save credentials
    saveCredentials(WiFi.SSID().c_str(), WiFi.psk().c_str());

    // Start our persistent device portal (available at http://<localIP>/ )
    // startWebPortal();

    return true;
}

#endif // CONNECT_WIFI
