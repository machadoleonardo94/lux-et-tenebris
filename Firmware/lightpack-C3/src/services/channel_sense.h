#if !defined(SERVICE_CHANNEL_SENSE)
#define SERVICE_CHANNEL_SENSE

#include "shared/dependencies.h"

//* ---------------------- CHANNEL RESISTANCE SENSE ----------------------
//? A short, separate bench measurement: light N white LEDs at full brightness,
//? compare V_REG before and during the burst, and report the source resistance.
//? It is deliberately decoupled from the stress ramp so a linearity sweep
//? (1, 10, 20, 50, 100 LEDs) can be run without a long load ramp in between.
//?
//? The 5 s blank is the cable-removal window, same as the stress test. The burst
//? is 200 ms (fast, inside the PMOS pulse rating) or 2000 ms (precise, and above
//? the PMOS 3.6 A continuous rating at 100 LEDs - lab only).
//?
//! The current is the firmware's model (60 mA per white LED), not a measurement.
//? If the cell current plateaus, R will drift with N; that drift is exactly what
//? the sweep is meant to expose.

#define SENSE_FORMAT_VERSION 1
#define SENSE_COUNT_MIN 1
#define SENSE_COUNT_MAX 100
#define SENSE_PER_LED_MA 60
#define SENSE_BURST_FAST_MS 200
#define SENSE_BURST_PRECISE_MS 2000
#define SENSE_UNPLUG_MS 5000UL

typedef struct senseResult
{
    uint8_t version;
    uint8_t count;
    uint16_t burst_ms;
    uint16_t v_off;
    uint16_t v_on;
    uint16_t ma;
    uint16_t r_mohm;
    uint32_t elapsed_ms;
} struct_senseResult;

struct_senseResult sense_last;

bool sense_run_flag = false;
bool sense_pending = false;
bool sense_report_pending = false;
uint8_t sense_pending_count = 20;
uint16_t sense_pending_burst_ms = SENSE_BURST_FAST_MS;

void sense_test_boot();
void sense_test_loop();
bool sense_test_request(uint8_t count, uint16_t burst_ms);

void sense_test_load()
{
    memset(&sense_last, 0, sizeof(sense_last));
    preferences.begin(POWER_NVS_NAMESPACE, true);
    if (preferences.getBytesLength("sense") == sizeof(sense_last))
        preferences.getBytes("sense", &sense_last, sizeof(sense_last));
    preferences.end();
    if (sense_last.version != SENSE_FORMAT_VERSION)
        memset(&sense_last, 0, sizeof(sense_last));
}

void sense_store_result(const struct_senseResult &result)
{
    preferences.begin(POWER_NVS_NAMESPACE, false);
    preferences.putBytes("sense", &result, sizeof(result));
    preferences.end();
}

void sense_test_boot()
{
    sense_test_load();
}

bool sense_abort_requested()
{
    if (!button_is_down(boot_button))
        return false;
    delay(30);
    return button_is_down(boot_button);
}

void sense_fill(uint16_t count, uint8_t brightness)
{
    strip.setBrightness(brightness);
    for (uint16_t i = 0; i < count; i++)
        strip.setPixelColor(i, strip.Color(255, 255, 255));
    strip.show();
}

void sense_blank()
{
    strip.setBrightness(0);
    strip.clear();
    strip.show();
}

void sense_test_run(uint8_t count, uint16_t burst_ms)
{
    if (count < SENSE_COUNT_MIN)
        count = SENSE_COUNT_MIN;
    if (count > SENSE_COUNT_MAX)
        count = SENSE_COUNT_MAX;
    if (count > strip_leds())
        count = (uint8_t)strip_leds();

    const uint16_t ma = (uint16_t)count * SENSE_PER_LED_MA;
    const uint32_t start = millis();

    sense_run_flag = true;
    sense_blank();
    Serial.printf("[SENSE] start: %u white LEDs, %u mA model, %u ms burst\n",
                  (unsigned)count, (unsigned)ma, (unsigned)burst_ms);

    //* Cable-removal window. GPIO9 aborts; the C3 is on USB during the window.
    for (uint8_t i = 0; i < (SENSE_UNPLUG_MS / 100); i++)
    {
        if (sense_abort_requested())
        {
            sense_run_flag = false;
            strip_brightness_dirty = true;
            button_suppress_touch(1000);
            Serial.println("[SENSE] aborted");
            return;
        }
        delay(100);
    }

    delay(1000);
    const uint16_t v_off = (uint16_t)supply_rail_mv();
    sense_fill(count, 255);
    delay(burst_ms);
    const uint16_t v_on = (uint16_t)supply_rail_mv();

    const uint32_t delta = (v_off > v_on) ? (uint32_t)(v_off - v_on) : 0;
    struct_senseResult result;
    memset(&result, 0, sizeof(result));
    result.version = SENSE_FORMAT_VERSION;
    result.count = count;
    result.burst_ms = burst_ms;
    result.v_off = v_off;
    result.v_on = v_on;
    result.ma = ma;
    result.r_mohm = (ma > 0) ? (uint16_t)((delta * 1000UL) / ma) : 0;
    result.elapsed_ms = millis() - start;

    sense_store_result(result);
    sense_last = result;
    sense_run_flag = false;
    sense_report_pending = true;
    strip_brightness_dirty = true;

    Serial.printf("[SENSE] %u LEDs: V_off %u mV, V_on %u mV, %u mA, R %u mOhm\n",
                  (unsigned)count, (unsigned)v_off, (unsigned)v_on,
                  (unsigned)ma, (unsigned)result.r_mohm);

    //* Same one-second pad blanking as the stress test: the burst has just
    //? ended and the capacitive module can latch a false press.
    button_suppress_touch(1000);
}

bool sense_test_request(uint8_t count, uint16_t burst_ms)
{
    if (sense_pending || sense_run_flag)
        return false;
    sense_pending_count = count;
    sense_pending_burst_ms = burst_ms;
    sense_pending = true;
    return true;
}

void sense_test_loop()
{
    if (!sense_pending)
        return;
    sense_pending = false;

    if (net_state != NET_OFFLINE)
    {
        Serial.println("[SENSE] refused: switch the radio off first");
        return;
    }

    sense_test_run(sense_pending_count, sense_pending_burst_ms);
}

#endif // SERVICE_CHANNEL_SENSE
