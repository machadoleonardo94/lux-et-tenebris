#if !defined(SETUP_ESP32)
#define SETUP_ESP32

#include "shared/dependencies.h"
#include "components/ESP32/features/update_firmware_ota.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

esp_adc_cal_characteristics_t adc_cal;

void setup_ESP32()
{
    Serial.println("[ESP32] SETUP STARTED!");

    //* Setup Watchdog
    esp_task_wdt_init(180, true);
    esp_task_wdt_add(NULL);

    Serial.println("[ESP32] SETUP FINISHED!");
}

void setup_GPIOs()
{
    Serial.println("[GPIO] SETUP STARTED!");

    //* Digital Inputs
    pinMode(prog_switch, INPUT); // Programming mode switch

    Serial.println("[GPIO] SETUP FINISHED!");
}

#endif // SETUP_ESP32