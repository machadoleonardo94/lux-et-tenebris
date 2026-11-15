#if !defined(SERVICE_SAFE_MODE)
#define SERVICE_SAFE_MODE

#include "shared/dependencies.h"

//* ---------------------- SAFE MODE ----------------------
//? What happens after the previous run ended in a brown-out. The flag is set the
//? moment the reset reason is read (main.cpp), before the strip self-test, and it
//? survives every reboot until the user deliberately clears it:
//?
//?   * a deliberate power-off (5 clicks) clears it before the latch opens, so the
//?     next start is a normal one;
//?   * the USB console can clear it with @SAFE OFF without a power cycle.
//?
//? The boot warning is a BLOCKING function at the end of the hardware setup, just
//? before setup_WIFI(). It owns the strip and the status pixel for its whole life:
//? nothing else is serviced, so no network, gyro or LED state can be disturbed by
//? it, and the click that exits it cannot leak into the gesture machinery.
//!
//! Do NOT put this after setup_WIFI(): a board that lands in the config portal
//? would show its power warning only after the 60 s window.
//?
//? The 70% is applied by update_strip(), not here: this file only decides whether
//? the warning is shown. That keeps the cap in one place for every mode, the UI
//? flashes and the next boot.

void safe_mode_load_flag();
void safe_mode_warning();
void safe_mode_clear();

//* Read the flag and fold in this boot's reset reason.
//? Called at the top of setup(), immediately after esp_reset_reason(), because the
//? strip self-test runs long before the warning and must already know whether to
//* paint the soft red power indication instead of the R/G/B channel walk.
void safe_mode_load_flag()
{
    preferences.begin(POWER_NVS_NAMESPACE, false);

    //* A brown-out is the event that arms safe mode. Writing the same value again
    //? is free: Preferences compares before writing (see webpage_config_save()).
    if (boot_reset_reason == ESP_RST_BROWNOUT)
        preferences.putUChar(POWER_SAFE_KEY, 1);

    safe_boot_active = preferences.getUChar(POWER_SAFE_KEY, 0) != 0;
    preferences.end();

    if (safe_boot_active)
        Serial.println("[SAFE] Safe mode is armed: the strip runs at 70% until cleared");
}

//* Clear the flag in NVS and in RAM. Safe to call when it is already clear.
void safe_mode_clear()
{
    preferences.begin(POWER_NVS_NAMESPACE, false);
    preferences.putUChar(POWER_SAFE_KEY, 0);
    preferences.end();

    const bool was_active = safe_boot_active;
    safe_boot_active = false;

    if (was_active)
        Serial.println("[SAFE] Cleared: the configured brightness is back in force");
}

//* One completed click on either switch, consumed by the caller.
//? The button pollers only start in loop(), so this is the only reader while the
//? warning blocks. The ISR may have latched the press edge; discard it and zero
//* the click counters so the first loop() pass cannot turn it into a gesture.
//!
//! The timeout is not cosmetic. The USB console resets the C3 when it opens the
//! port, so a blocking wait that only a physical switch can end leaves the console
//! unable to reach @SAFE OFF or even PING: every reconnect lands in this warning
//! again. The wait therefore gives up after `timeout_ms` and lets setup() finish;
//? the flag stays set, the strip stays at 70%, and the console can clear it.
#define SAFE_ACK_TIMEOUT_MS 5000UL
#define SAFE_RELEASE_TIMEOUT_MS 2000UL

bool safe_mode_click_wait(uint32_t timeout_ms)
{
    const uint32_t start = millis();
    bool pressed = false;

    while (!pressed && (millis() - start) < timeout_ms)
    {
        if (button_is_down(touch_button) || button_is_down(boot_button))
        {
            delay(40); // debounce
            if (button_is_down(touch_button) || button_is_down(boot_button))
                pressed = true;
        }
        else
        {
            delay(10);
        }
    }

    if (!pressed)
    {
        Serial.printf("[SAFE] no acknowledgement in %lu ms - continuing at %u%%\n",
                      (unsigned long)timeout_ms, (unsigned)POWER_SAFE_PERCENT);
        return false;
    }

    //? The action happens on RELEASE, like every other gesture on this board: a
    //? finger resting on the pad must not skip the warning by accident. The cap
    //* is for a pad stuck high, which would otherwise wedge the boot again.
    const uint32_t release_start = millis();
    while ((button_is_down(touch_button) || button_is_down(boot_button)) &&
           (millis() - release_start) < SAFE_RELEASE_TIMEOUT_MS)
        delay(10);

    button_discard_press(touch_button);
    button_discard_press(boot_button);

    for (uint8_t i = 0; i < BUTTON_INPUT_COUNT; i++)
    {
        struct_buttonInput &b = *button_inputs[i];
        b.click_count = 0;
        b.counter = 0;
        b.click_last_ms = 0;
        b.press_start_ms = 0;
        b.release_seen_ms = 0;
        b.hold_ms = 0;
    }

    return true;
}

//* The blocking warning at the end of the hardware setup.
//? Three full-red flashes, then a steady yellow pixel and a soft red strip. The
//? strip is deliberately not the channel test and not the selected mode: a board
//? that just lost its supply should say so with one unambiguous colour.
void safe_mode_warning()
{
    if (!safe_boot_active)
        return;

    Serial.println("[SAFE] Previous run ended in a brown-out");
    Serial.printf("[SAFE] Strip output at %u%% until cleared; click either switch to continue\n",
                  (unsigned)POWER_SAFE_PERCENT);

    //* 1. Three red flashes. The status pixel is the only thing visible with the
    //? case fitted, and red is the established "about to lose power" colour.
    for (uint8_t i = 0; i < 3; i++)
    {
        onboard_led.setPixelColor(0, strip.Color(255, 0, 0));
        onboard_led.show();
        delay(150);
        onboard_led.setPixelColor(0, strip.Color(0, 0, 0));
        onboard_led.show();
        delay(150);
    }

    //* 2. Steady yellow while the board waits. Yellow, not the amber of the
    //? recovery countdown: this one is steady, that one pulses.
    onboard_led.setPixelColor(0, strip.Color(40, 40, 0));
    onboard_led.show();

    //* 3. Soft red on the strip. It is low enough to be safe on a weak cell and
    //? visible across a room, and it is not a mode's output.
    strip.setBrightness(255);
    for (uint16_t i = 0; i < strip_leds(); i++)
        strip.setPixelColor(i, strip.Color(20, 0, 0));
    strip.show();

    //* 4. Wait for the acknowledgement. The timeout is what keeps this blocking
    //? warning from wedging the USB console: without it, every reconnect lands
    //* here and the page can never reach @SAFE OFF.
    const bool acknowledged = safe_mode_click_wait(SAFE_ACK_TIMEOUT_MS);
    delay(500);

    if (acknowledged)
        Serial.println("[SAFE] Acknowledged - continuing the boot at the safe brightness");
    else
        Serial.println("[SAFE] No click - continuing the boot at the safe brightness");
}

#endif // SERVICE_SAFE_MODE
