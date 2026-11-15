#if !defined(SERVICE_STRESS_TEST)
#define SERVICE_STRESS_TEST

#include "shared/dependencies.h"

//* ---------------------- POWER STRESS TEST ----------------------
//? A bench characterisation of the supply, run from the USB console's
//? Diagnostics page. It answers one question the firmware cannot answer by
//? itself: how much strip current can THIS cell, cable and strip length carry
//? before the rail collapses?
//?
//? It is deliberately a blocking sequence in loop(), not a state machine spread
//? over loop() passes: the ramp must not be disturbed by a web request, a WiFi
//? TX burst or an OTA poll, and the user is told the board is busy for its
//? duration. The radio must be off before it starts (the 200 mA MCU reserve
//? already accounts for radio peaks; including them here would only muddle the
//? strip measurement).
//?
//? The run is a three-part sequence:
//?   1. blank the strip and wait STRESS_UNPLUG_MS so the user can unplug USB;
//?   2. measure V_REG with the strip blank, then with a small known load
//?      (STRESS_PROBE_LEDS) to estimate the source resistance;
//?   3. ramp the chosen colour from brightness 0 to 255 in +5 steps at the
//?      selected cadence (500 ms fast, 2 s precise), stopping at the selected
//?      ceiling, at full brightness for a strip too short to reach it, on a
//?      brown-out, on a rail collapse, or on a GPIO9 press.
//?
//! The ceiling is test-only. 2 A is the continuous benchmark; 3 A and 4 A exist
//? to force a collapse in the lab, in 500 ms bursts. The normal strip budget and
//? the 10 Hz guard are separate and must never read this value.
//?
//! Every step is written to NVS BEFORE it is applied. That ordering is the whole
//? reason the failing step survives a brown-out: the write happens while the rail
//? is still healthy, and the reset cannot lose an entry that is already stored.
//? The stored V_REG minimum is one step behind (it is updated during the step and
//? persisted on the next one), which the report states rather than hides.
//?
//! Results are a state document field, not a one-shot acknowledgement. Reconnecting
//? the console resets the C3, so a result that only existed in a reply would be
//? lost; a result in NVS is re-read on every boot and survives the reconnect.
//? Only live progress is lost when the cable is pulled.

#define STRESS_FORMAT_VERSION 4

//* Test-only ceiling. The wires and traces cannot take 3 A continuous, so 2 A is
//? the normal benchmark; the lab can select up to 6 A for a forced brown-out.
//? 500 mA is the floor, for a deliberately small load on a weak cell.
#define STRESS_CEILING_MIN_MA 500UL
#define STRESS_CEILING_DEFAULT_MA 2000UL
#define STRESS_CEILING_MAX_MA 6000UL
//* 500 ms is 66 RC time constants of the 300k/100k/100nF divider; the 2 s step
//? exists for the precise point where cell polarization has settled further.
#define STRESS_STEP_FAST_MS 500UL
#define STRESS_STEP_PRECISE_MS 2000UL
#define STRESS_UNPLUG_MS 5000UL
#define STRESS_STEP 5
#define STRESS_PROBE_LEDS 20
//* The rail below which the test stops, used only for the USB-powered case where
//? the module keeps running from the 5 V pin and cannot brown out. 0 disables it:
//! that is the lab "find the absolute limit" setting and must never be the normal
//* guard. The real 0.6 guard is the 10 Hz V_REG floor, not this test safety.
#define STRESS_RAIL_FLOOR_DEFAULT_MV 3000
#define STRESS_RAIL_FLOOR_MAX_MV 3500
#define STRESS_RAIL_FLOOR_MS 500
#define STRESS_TIMEOUT_MS 180000UL

//* The colour is a 3-bit mask so the console can mix channels: R=1, G=2, B=4.
//? 7 is white, 3 yellow, 5 magenta, 6 cyan, 1/2/4 a single channel. The current
//* model follows the popcount: 20 mA per selected channel.
enum : uint8_t
{
    STRESS_RED_BIT = 1,
    STRESS_GREEN_BIT = 2,
    STRESS_BLUE_BIT = 4
};

uint8_t stress_channel_count(uint8_t mask)
{
    uint8_t count = 0;
    if (mask & STRESS_RED_BIT)
        count++;
    if (mask & STRESS_GREEN_BIT)
        count++;
    if (mask & STRESS_BLUE_BIT)
        count++;
    return count;
}

enum : uint8_t
{
    STRESS_RESULT_NONE = 0,
    STRESS_RESULT_BROWNOUT,
    STRESS_RESULT_INTERRUPTED,
    STRESS_RESULT_CEILING,
    STRESS_RESULT_FULL,
    STRESS_RESULT_RAIL_COLLAPSE,
    STRESS_RESULT_ABORTED
};

enum : uint8_t
{
    STRESS_PHASE_NONE = 0,
    STRESS_PHASE_PROBE,
    STRESS_PHASE_RAMP
};

typedef struct stressStep
{
    uint8_t version;
    uint8_t phase;
    uint8_t color;
    uint8_t brightness;
    uint16_t leds;
    uint16_t est_ma;
    uint16_t v_start;
    uint16_t v_off;
    uint16_t v_on;
    uint16_t v_min;
    uint16_t r_mohm;
    uint16_t ceiling_ma;
    uint16_t step_ms;
    uint16_t rail_floor_mv;
    uint32_t elapsed_ms;
} struct_stressStep;

typedef struct stressResult
{
    uint8_t version;
    uint8_t result;
    uint8_t phase; // Which part of the sequence the value belongs to
    uint8_t color;
    uint8_t brightness;
    uint16_t leds;
    uint16_t est_ma;
    uint16_t v_start;
    uint16_t v_off;
    uint16_t v_on;
    uint16_t v_min;
    uint16_t sag_mv;
    uint16_t r_mohm;
    uint16_t ceiling_ma;
    uint16_t step_ms;
    uint16_t rail_floor_mv;
    uint32_t elapsed_ms;
} struct_stressResult;

struct_stressStep stress_step;
struct_stressResult stress_last;

bool stress_run_flag = false;      // NVS "a test is in flight" flag
bool stress_pending = false;       // A request is waiting for the next loop pass
bool stress_report_pending = false; // A finished run should push a state frame
uint8_t stress_pending_color = 0;
uint16_t stress_pending_ceiling_ma = STRESS_CEILING_DEFAULT_MA;
uint16_t stress_pending_step_ms = STRESS_STEP_FAST_MS;
uint16_t stress_pending_rail_floor_mv = STRESS_RAIL_FLOOR_DEFAULT_MV;

void stress_test_boot();
void stress_test_loop();
bool stress_test_request(uint8_t color, uint16_t ceiling_ma, uint16_t step_ms, uint16_t rail_floor_mv);

//* ---------------------- NVS ----------------------

void stress_test_load()
{
    memset(&stress_step, 0, sizeof(stress_step));
    memset(&stress_last, 0, sizeof(stress_last));

    preferences.begin(POWER_NVS_NAMESPACE, true);
    stress_run_flag = preferences.getUChar("run", 0) != 0;
    if (preferences.getBytesLength("step") == sizeof(stress_step))
        preferences.getBytes("step", &stress_step, sizeof(stress_step));
    if (preferences.getBytesLength("res") == sizeof(stress_last))
        preferences.getBytes("res", &stress_last, sizeof(stress_last));
    preferences.end();

    if (stress_step.version != STRESS_FORMAT_VERSION)
        memset(&stress_step, 0, sizeof(stress_step));
    if (stress_last.version != STRESS_FORMAT_VERSION)
        memset(&stress_last, 0, sizeof(stress_last));
}

void stress_persist_step(const struct_stressStep &step)
{
    //! Open, write, close. The power rail is healthy when this runs: it is called
    //? BEFORE the step is applied, never while the strip is dragging the rail down.
    preferences.begin(POWER_NVS_NAMESPACE, false);
    preferences.putBytes("step", &step, sizeof(step));
    preferences.putUChar("run", 1);
    preferences.end();
}

void stress_store_result(const struct_stressResult &result)
{
    preferences.begin(POWER_NVS_NAMESPACE, false);
    preferences.putBytes("res", &result, sizeof(result));
    preferences.putUChar("run", 0);
    preferences.end();
}

//* Finalise a run that a reset ended: a brown-out is the interesting one, any
//? other reset during the ramp is reported as interrupted rather than guessed at.
void stress_test_boot()
{
    stress_test_load();

    if (!stress_run_flag)
        return;

    struct_stressResult result;
    memset(&result, 0, sizeof(result));
    result.version = STRESS_FORMAT_VERSION;
    result.result = (boot_reset_reason == ESP_RST_BROWNOUT)
                        ? STRESS_RESULT_BROWNOUT
                        : STRESS_RESULT_INTERRUPTED;
    result.phase = stress_step.phase;
    result.color = stress_step.color;
    result.brightness = stress_step.brightness;
    result.leds = stress_step.leds;
    result.est_ma = stress_step.est_ma;
    result.v_start = stress_step.v_start;
    result.v_off = stress_step.v_off;
    result.v_on = stress_step.v_on;
    result.v_min = stress_step.v_min;
    result.sag_mv = (stress_step.v_start > stress_step.v_min)
                        ? (uint16_t)(stress_step.v_start - stress_step.v_min)
                        : 0;
    result.r_mohm = stress_step.r_mohm;
    result.ceiling_ma = stress_step.ceiling_ma;
    result.step_ms = stress_step.step_ms;
    result.rail_floor_mv = stress_step.rail_floor_mv;
    result.elapsed_ms = stress_step.elapsed_ms;

    stress_store_result(result);
    stress_last = result;
    stress_run_flag = false;

    Serial.printf("[STRESS] previous run ended %s at brightness %u (%u mA, %u LEDs)\n",
                  (result.result == STRESS_RESULT_BROWNOUT) ? "in a brown-out" : "interrupted",
                  (unsigned)result.brightness, (unsigned)result.est_ma, (unsigned)result.leds);
}

//* ---------------------- THE SEQUENCE ----------------------

void stress_blank()
{
    strip.setBrightness(0);
    strip.clear();
    strip.show();
}

void stress_fill(uint8_t mask, uint8_t brightness, uint16_t count)
{
    const uint8_t red = (mask & STRESS_RED_BIT) ? 255 : 0;
    const uint8_t green = (mask & STRESS_GREEN_BIT) ? 255 : 0;
    const uint8_t blue = (mask & STRESS_BLUE_BIT) ? 255 : 0;

    strip.setBrightness(brightness);
    for (uint16_t i = 0; i < count; i++)
        strip.setPixelColor(i, strip.Color(red, green, blue));
    strip.show();
}

//! GPIO9 only. The capacitive pad on GPIO3 can fire from a knock or a hand near
//? the prop, and aborting a test on that is worse than making the user press the
//* hard switch that is part of the module.
bool stress_abort_requested()
{
    if (!button_is_down(boot_button))
        return false;
    delay(30);
    return button_is_down(boot_button);
}

void stress_discard_buttons()
{
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
}

void stress_finish(uint8_t code, const struct_stressStep &step, uint32_t elapsed)
{
    struct_stressResult result;
    memset(&result, 0, sizeof(result));
    result.version = STRESS_FORMAT_VERSION;
    result.result = code;
    result.phase = step.phase;
    result.color = step.color;
    result.brightness = step.brightness;
    result.leds = step.leds;
    result.est_ma = step.est_ma;
    result.v_start = step.v_start;
    result.v_off = step.v_off;
    result.v_on = step.v_on;
    result.v_min = step.v_min;
    result.sag_mv = (step.v_start > step.v_min)
                        ? (uint16_t)(step.v_start - step.v_min)
                        : 0;
    result.r_mohm = step.r_mohm;
    result.ceiling_ma = step.ceiling_ma;
    result.step_ms = step.step_ms;
    result.rail_floor_mv = step.rail_floor_mv;
    result.elapsed_ms = elapsed;

    stress_store_result(result);
    stress_last = result;
    stress_run_flag = false;

    //* A run that finished while USB was still attached should reach the console
    //? without a reconnect: the control loop pushes one state frame when it next
    //* runs. When the cable was pulled this flag is simply lost with the boot.
    stress_report_pending = true;

    //* The test wrote the driver brightness directly; make update_strip() push the
    //? configured value again on the next frame instead of trusting its cache.
    strip_brightness_dirty = true;

    stress_discard_buttons();

    Serial.printf("[STRESS] done: result %u, brightness %u, est %u mA, Vmin %u mV, sag %u mV, R %u mOhm\n",
                  (unsigned)result.result, (unsigned)result.brightness,
                  (unsigned)result.est_ma, (unsigned)result.v_min,
                  (unsigned)result.sag_mv, (unsigned)result.r_mohm);

    //* Ignore the capacitive pad for a second: the burst just ended and the
    //? module can couple a false press. GPIO9 stays live for an abort/recovery.
    button_suppress_touch(1000);
}

void stress_test_run(uint8_t color, uint16_t ceiling_ma, uint16_t step_ms, uint16_t rail_floor_mv)
{
    const uint16_t leds = strip_leds();
    const uint8_t channels = stress_channel_count(color);
    const uint16_t per_led_ma = 20U * ((channels == 0) ? 1 : channels);
    const uint16_t probe_leds = (leds < STRESS_PROBE_LEDS) ? leds : STRESS_PROBE_LEDS;

    //* The brightness that reaches the selected ceiling at this strip length and
    //? colour, and the last brightness the ramp is allowed to use. A short strip
    //? cannot reach the ceiling and simply runs to 255.
    uint32_t ceiling_b = ((uint32_t)ceiling_ma * 255UL) / ((uint32_t)leds * per_led_ma);
    if (ceiling_b > 255)
        ceiling_b = 255;
    if (ceiling_b < 1)
        ceiling_b = 1;

    struct_stressStep step;
    memset(&step, 0, sizeof(step));
    step.version = STRESS_FORMAT_VERSION;
    step.color = color;
    step.leds = leds;
    step.ceiling_ma = ceiling_ma;
    step.step_ms = step_ms;
    step.rail_floor_mv = rail_floor_mv;

    stress_run_flag = true;
    stress_persist_step(step);

    Serial.printf("[STRESS] start: %u LEDs, colour %u, ceiling %u mA, step %u ms, rail floor %u mV, ceiling brightness %lu\n",
                  (unsigned)leds, (unsigned)color, (unsigned)ceiling_ma,
                  (unsigned)step_ms, (unsigned)rail_floor_mv, (unsigned long)ceiling_b);

    //* 1. Blank, then give the user the unplug window. The test does not depend on
    //? the USB link from here on: the console may disconnect and the ramp continues.
    stress_blank();
    for (uint8_t i = 0; i < (STRESS_UNPLUG_MS / 100); i++)
    {
        if (stress_abort_requested())
        {
            stress_finish(STRESS_RESULT_ABORTED, step, 0);
            return;
        }
        delay(100);
    }

    //* 2. The open-circuit point and the known-load probe.
    delay(1000);
    step.phase = STRESS_PHASE_PROBE;
    step.v_off = (uint16_t)supply_rail_mv();
    step.v_start = step.v_off;
    step.v_min = step.v_off;
    //* Scale the probe so it never exceeds the selected ceiling: a 500 mA run
    //? must not probe at 1.2 A. The 2 A and 4 A runs stay at 255 and keep the
    //* familiar 20-LED / 1.2 A point.
    const uint32_t probe_b_full = ((uint32_t)ceiling_ma * 255UL) / ((uint32_t)probe_leds * per_led_ma);
    const uint8_t probe_b = (probe_b_full > 255UL) ? 255 : (uint8_t)probe_b_full;
    const uint16_t probe_est = (uint16_t)(((uint32_t)probe_leds * per_led_ma * probe_b) / 255UL);

    step.brightness = probe_b;
    step.est_ma = probe_est;

    stress_persist_step(step);

    Serial.printf("[STRESS] V_off %u mV with the strip blank\n", (unsigned)step.v_off);

    stress_fill(color, probe_b, probe_leds);
    delay(step_ms);
    step.v_on = (uint16_t)supply_rail_mv();
    if (step.v_on < step.v_min)
        step.v_min = step.v_on;

    const uint32_t delta_mv = (step.v_off > step.v_on) ? (uint32_t)(step.v_off - step.v_on) : 0;
    step.r_mohm = (probe_est > 0) ? (uint16_t)((delta_mv * 1000UL) / probe_est) : 0;

    Serial.printf("[STRESS] probe %u LEDs at %u/255 (%u mA): V_on %u mV, R %u mOhm\n",
                  (unsigned)probe_leds, (unsigned)probe_b, (unsigned)probe_est,
                  (unsigned)step.v_on, (unsigned)step.r_mohm);
    stress_persist_step(step);

    stress_blank();
    delay(500);

    //* 3. The ramp. `b` is the driver brightness; the pixels carry the chosen
    //? colour at full value, so the per-LED current is per_led_ma * b / 255.
    const uint32_t start = millis();
    uint8_t result = STRESS_RESULT_NONE;
    uint16_t brightness = STRESS_STEP;

    for (;;)
    {
        if (brightness > ceiling_b)
            brightness = (uint16_t)ceiling_b;

        const uint32_t est_ma = ((uint32_t)leds * per_led_ma * brightness) / 255UL;
        step.phase = STRESS_PHASE_RAMP;
        step.brightness = (uint8_t)brightness;
        step.est_ma = (uint16_t)((est_ma > 65535UL) ? 65535UL : est_ma);
        step.elapsed_ms = millis() - start;

        //! Persist BEFORE applying: a brown-out during this step must leave this
        //? step on record, and the rail is still healthy when this write lands.
        stress_persist_step(step);
        stress_fill(color, (uint8_t)brightness, leds);

        Serial.printf("[STRESS] b=%u est=%lu mA", (unsigned)brightness, (unsigned long)est_ma);

        bool aborted = false;
        bool collapsed = false;
        uint16_t below_ms = 0;

        for (uint8_t tick = 0; tick < (step_ms / 100); tick++)
        {
            delay(100);
            if (stress_abort_requested())
            {
                aborted = true;
                break;
            }

            const uint16_t rail_mv = (uint16_t)supply_rail_mv();
            if (rail_mv < step.v_min)
                step.v_min = rail_mv;

            if (rail_floor_mv > 0 && rail_mv < rail_floor_mv)
            {
                below_ms += 100;
                if (below_ms >= STRESS_RAIL_FLOOR_MS)
                {
                    collapsed = true;
                    break;
                }
            }
            else
            {
                below_ms = 0;
            }
        }

        Serial.printf(" Vmin=%u mV\n", (unsigned)step.v_min);

        if (aborted)
        {
            result = STRESS_RESULT_ABORTED;
            break;
        }
        if (collapsed)
        {
            result = STRESS_RESULT_RAIL_COLLAPSE;
            break;
        }
        if (est_ma >= ceiling_ma || brightness >= ceiling_b)
        {
            result = (brightness >= 255 && est_ma < ceiling_ma)
                         ? STRESS_RESULT_FULL
                         : STRESS_RESULT_CEILING;
            break;
        }
        if (millis() - start > STRESS_TIMEOUT_MS)
        {
            result = STRESS_RESULT_ABORTED;
            break;
        }

        brightness += STRESS_STEP;
    }

    step.elapsed_ms = millis() - start;
    stress_finish(result, step, step.elapsed_ms);
}

//* ---------------------- ENTRY POINTS ----------------------

bool stress_test_request(uint8_t color, uint16_t ceiling_ma, uint16_t step_ms, uint16_t rail_floor_mv)
{
    if (stress_pending || stress_run_flag)
        return false;

    stress_pending_color = color;
    stress_pending_ceiling_ma = ceiling_ma;
    stress_pending_step_ms = step_ms;
    stress_pending_rail_floor_mv = rail_floor_mv;
    stress_pending = true;
    return true;
}

void stress_test_loop()
{
    if (!stress_pending)
        return;

    stress_pending = false;

    //? The console already refuses to send a request with the radio on; this is
    //* the backstop for a hand-written line.
    if (net_state != NET_OFFLINE)
    {
        Serial.println("[STRESS] refused: switch the radio off before a stress test");
        return;
    }

    stress_test_run(stress_pending_color, stress_pending_ceiling_ma, stress_pending_step_ms,
                    stress_pending_rail_floor_mv);
}

#endif // SERVICE_STRESS_TEST
