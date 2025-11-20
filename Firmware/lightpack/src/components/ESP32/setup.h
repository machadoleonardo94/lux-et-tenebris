#if !defined(SETUP_ESP32)
#define SETUP_ESP32

#include "shared/dependencies.h"
#include "components/ESP32/features/update_firmware_ota.h"

void setup_ESP32()
{
    Serial.println("[ESP32] SETUP STARTED!");

    //* Setup bluetooth
    btStop();

    //* Setup Watchdog
    esp_task_wdt_init(180, true);
    esp_task_wdt_add(NULL);

    WiFi.mode(WIFI_AP_STA);

    //* Configures buttons
    pinMode(buttonA, INPUT);
    pinMode(buttonB, INPUT);
    pinMode(prog_switch, INPUT);
    // pinMode(CC_output, OUTPUT);
    pinMode(openDrain_output1, OUTPUT);
    pinMode(openDrain_output2, OUTPUT);
    // pinMode(openDrain_output3, OUTPUT);
    // digitalWrite(CC_output, LOW);
    digitalWrite(openDrain_output1, LOW);
    digitalWrite(openDrain_output2, LOW);
    // digitalWrite(openDrain_output3, LOW);

    esp_sleep_enable_ext0_wakeup(prog_switch, LOW);

    setup_OTA();

    Serial.println("[ESP32] SETUP FINISHED!");
}

#endif // SETUP_ESP32