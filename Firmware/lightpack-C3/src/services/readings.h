#if !defined(SERVICE_READINGS)
#define SERVICE_READINGS

#include "shared/dependencies.h"

void read_adc();
void check_button();
void check_boot_button();
void setup_button_interrupt();

//* The mode registry and its accessors live in services/led_scripts.h, which is
//* included after this header (see shared/dependencies.h). The button handler
//* below only needs the name, so it is declared here rather than reordering the
//* service includes - the same reason services/webpage.h forward-declares the
//* helpers of services/presets.h.
void led_mode_next();
//* Its inverse, for the touch pad's double-click and the BOOT switch's single
//* click. Declared for the same reason as the walk above: the registry lives in a
//* header that is included after this one.
void led_mode_prev();

//* Same ordering problem for the WiFi service: the radio toggle and the config
//? portal are implemented in services/wifi_settings.h, which is included after
//* this header. The portal is requested by leaving a flag in NVS and restarting,
//? so that is all the button side needs; the offline toggle is a live transition
//* the WiFi service owns, so the name is all it needs either.
void wifi_request_portal_on_boot();
void wifi_toggle_offline();

//* The recovery wipe reaches into two other services, both included after this
//? header for the same reason: the credentials belong to the WiFi service and the
//* page login to the web service, and the button side only needs their names.
void wifi_recovery_wipe();
void webpage_auth_clear();

//* The onboard pixel, for the recovery countdown. services/led_scripts.h is
//* included after this header too - see the note above about led_mode_next().
void led_notice(uint8_t r, uint8_t g, uint8_t b, uint16_t ms);

//* What a completed gesture means. The vocabulary is in shared/variables.h and
//? the colours live in services/led_scripts.h, so the input layer only names the
//* action and every switch draws the same confirmation for the same action.
void ui_action_feedback(uint8_t action);

//* Safe mode lives in services/safe_mode.h, included after this header. A
//? five-click power-off is the deliberate way OUT of it, so the flag has to be
//* cleared before the latch opens and the board loses the chance to write it.
void safe_mode_clear();

//* Each switch owns its click map. Declared before setup_button_interrupt() stores
//* them in the wiring table; defined with the rest of the gesture code below.
void touch_click_resolve(uint8_t clicks);
void boot_click_resolve(uint8_t clicks);

/*
ADC_ATTEN_DB_2_5
V(ADC)<1050mV
V_reg(max) = 4.2V
V(ADC)/V_reg <= 0.25
*/

void read_adc()
{
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate < 10000)
        return;
    lastUpdate = millis();
    //! `analog_channel_0` is both the ADC1 channel index and the GPIO number: on
    //! the C3 that pin is GPIO0 = ADC1_CH0. analogRead() wants the PIN, and here
    //! the two agree. This call used to name `adc_input`, which is defined
    //! nowhere.
    adcReading = analogRead(analog_channel_0);
    //? analogReadMilliVolts() is the same pin's calibrated millivolts, and it is
    //? what replaces the old esp_adc_cal_raw_to_voltage(adcReading, &adc_cal):
    //? the Arduino core runs its own curve-fitting calibration against the
    //! oneshot driver. Reinstating the legacy esp_adc_cal_* API here would abort
    //! the board - see the note in components/ESP32/setup.h.
    //* The board divides the rail by 4 before it reaches the pin (pinout.h:
    //* V(ADC)/V_reg <= 0.25), so 4x the pin voltage is the rail.
    adcVoltage = 4 * analogReadMilliVolts(analog_channel_0);
    Serial.printf("[ADC] Reading: %d, Voltage: %d mV\n", adcReading, adcVoltage);
}

//* The rail voltage in millivolts, read on demand rather than on read_adc()'s
//* ten-second tick.
//? Same pin and same divider as read_adc() above, but the boot path needs the
//? number BEFORE the strip is driven, so it cannot wait for that tick. Nothing
//? here is rate-limited and nothing here touches read_adc()'s static timer, so
//* the two cannot disturb each other's cadence.
//! Read what this actually measures before trusting the number. The R16-R19
//? divider is tapped from V_REG - the switched LED rail - so this is NOT the
//? cell voltage: it excludes Q3's drop, and it reads ~0 whenever the latch is
//! open. It also cannot tell USB from battery, because V_REG sits at the battery
//? voltage in both cases. What it is, is the LED rail, which is precisely the
//* node that matters here: the external strip, the onboard pixel and the
//? module's own 5V pin all hang off V_REG, separated only by the SS14 at D1.
//* Conversions averaged per supply_rail_mv() call.
#define SUPPLY_RAIL_SAMPLES 8

uint32_t supply_rail_mv()
{
    //! Prime once. The first analogReadMilliVolts() of a boot also creates the
    //? ADC oneshot unit and its curve-fitting calibration scheme lazily, and that
    //* first conversion is not representative - it is discarded here rather than
    //? averaged in.
    (void)analogReadMilliVolts(analog_channel_0);

    //* Averaged, and deliberately NOT a single sample. The load on this rail
    //! swings by hundreds of milliamps within one boot - the radio coming up, the
    //? CPU waking from a delay - so a single conversion can land inside an RF
    //* transmit burst and report the radio's duty cycle rather than the rail.
    //? Eight conversions is a few hundred microseconds, cheap enough to call
    //* from anywhere in the boot path.
    uint32_t total = 0;
    for (uint8_t i = 0; i < SUPPLY_RAIL_SAMPLES; i++)
        total += analogReadMilliVolts(analog_channel_0);

    return 4 * (total / SUPPLY_RAIL_SAMPLES);
}

//* Latch a rail reading as the baseline for this boot, if it has none yet.
//? "At rest" means before the strip has been given anything but black, so the
//? first caller wins and every later call is a no-op.
void boot_rail_baseline(uint32_t rail_mv)
{
    if (boot_rail_mv == 0)
        boot_rail_mv = rail_mv;
}

//* Fold a rail reading into the lowest seen this boot.
//? The gap between boot_rail_mv and boot_rail_min_mv is the number the boot
//! report is built on: it is how far the strip's own current drags the rail the
//? module's regulator is fed from. See boot_report_print().
//! 0 is the "nothing measured yet" sentinel, so a genuine 0 mV reading is not
//? distinguishable from no reading - which is fine, since a 0 mV rail means the
//* board is off and nothing is reading anything.
void boot_rail_track(uint32_t rail_mv)
{
    if (boot_rail_min_mv == 0 || rail_mv < boot_rail_min_mv)
        boot_rail_min_mv = rail_mv;
}

//* Read the rail with one named self-test step still lit, fold it into this
//* boot's minimum, and name the step it was read under.
//? Wrapped up here rather than spelled out three times in the self-test so the
//* measure-then-track order cannot be got wrong at one of the steps.
void boot_rail_note_step(const char *step)
{
    const uint32_t rail_mv = supply_rail_mv();
    boot_rail_track(rail_mv);
    Serial.printf("[STRIP]   rail %lu mV under %s\n", (unsigned long)rail_mv, step);
}

//* ---------------------- USER INPUTS ----------------------
//? Two switches, both reported live on the configuration page (the table below is
//? what webpage_handle_buttons() walks). They are deliberately not two copies of
//? the same interface:
//?
//?   touch_button  GPIO3  The touch module on the carrier board. It drives the pin
//?                        HIGH while it is touched. This is the DAILY button, and
//?                        on the current unit the module works as intended:
//?                          1 click   - next LED mode
//?                          2 clicks  - previous LED mode
//?                          5 clicks  - cut the battery latch and power down
//?                          3 s hold  - toggle offline mode (on release)
//?                          6 s hold  - reboot into the WiFi config hotspot (on release)
//?                        Its 1 s hold no longer changes the mode and its 5 s hold no
//?                        longer powers down: those gestures moved to the click
//?                        ladder, which is what the 1/2/5-click window is for.
//?                        A repaired board that still cannot see this pad loses
//?                        nothing - GPIO9 below carries a complete fallback.
//?   boot_button   GPIO9  The C3 Supermini's BOOT switch, HIGH while idle and
//?                        pulled LOW while it is held. It is the SECURITY and
//?                        RECOVERY button: it lives on the module itself, so it
//?                        survives anything done to the carrier board, and it is
//?                        the one switch that can wipe the saved networks.
//?                          1 click   - previous LED mode (fallback navigation)
//?                          5 clicks  - cut the battery latch and power down
//?                          3 s hold  - toggle offline mode (on release)
//?                          6 s hold  - reboot into the WiFi config hotspot (on release)
//?                          10 s hold - wipe the saved networks and the page login
//?                        Holding it through a reset is still the ROM's
//?                        download-mode request, which no sketch can see or use.
//!
//! The two holds cannot both fire from one press, and that is not a timing
//! accident: button_finalize() below tests action_hold_ms (the portal) BEFORE
//! long_press_ms (the radio toggle) and the branches are exclusive, so a hold long
//! enough for the portal never also toggles. Nothing extra measures the two.
//!
//! Clicks are a different problem, and they are NOT solved here: whether a short
//! press is the start of a 1-, 2- or 5-click gesture cannot be known until the
//! user stops clicking, so button_finalize() only reports SHORT and the two
//* check functions group clicks across BUTTON_CLICK_GAP_MS.
//!
//! An ISR only latches an edge. The classification, the actions and every number
//! the page draws are polled from loop(), so a press never sits inside a wait
//! loop. This used to block: check_button() waited for the release with delay(10)
//! in a while loop, which stopped web_server.handleClient() for the whole press -
//! the page could only be answered after the finger came off, a 1 s press froze
//! the strip and the HTTP service alike, and a held switch was invisible to
//! /api/buttons however fast the page polled.
//!
//! Both switches pay the click window on every tap, because both now carry a
//* 5-click gesture. A mode change therefore lands BUTTON_CLICK_GAP_MS after the
//? finger lifts - the same 400 ms tax the single-button firmware already paid.

//* ---------------------- TOUCH MODULE POLARITY ----------------------
//! The pad is an EXTERNAL module plugged into TOUCH1, not the chip's own sensing.
//! The V2 board's net table reads:
//?
//?   TOUCH1 pin 1 -> +BATT          the raw battery rail, NOT 3.3 V
//?   TOUCH1 pin 2 -> Button_GPIO    the module's OUT
//?   TOUCH1 pin 3 -> GND
//?   Button_GPIO  -> GPIO3 (SuperMini pad 12), shared with the on-board power
//?                   button BT1 through R12 (2.2k), filtered by C7 (100nF to GND)
//?
//! There is NO pull-up and NO pull-down anywhere on that net. The idle level is
//! whatever the module makes it, which means the firmware's own pull is the only
//! thing defining it, and the polarity has to match the module or the button is
//! silently dead: INPUT_PULLDOWN plus a RISING edge (what the call below asks for)
//! is correct ONLY for a push-pull, active-HIGH module. An active-LOW or
//! open-drain module drives the line down and can never produce a rising edge, so
//! every press goes unnoticed and the hardware looks broken while being wired
//* exactly right.
//?
//? Flipping the polarity is ONE argument: pass active_high = false to
//? button_input_configure(), which switches the pull to INPUT_PULLUP by itself,
//? and the ISR edge follows that same flag in setup_button_interrupt().
//* tools/button_probe/ (environment C3-PROBE) measures which one this module is.

//* ---------------------- SHARED HOLD THRESHOLDS ----------------------
//? Both switches now carry the same two holds, so the threshold is a property of
//? the gesture rather than of the switch:
//?
//?   >= 3 s, then release -> toggle the radio: offline mode on/off
//?   >= 6 s, then release -> reboot into the WiFi config hotspot
//?   >= 10 s, then release -> recovery wipe, BOOT SWITCH ONLY
//?
//? Clicks and holds are told apart by HOW LONG THE PRESS WAS, not by two separate
//* counters racing each other: a press that outlasts BUTTON_CLICK_MAX_MS is a hold
//* candidate and can never be counted as a click.
#define BUTTON_WIFI_HOLD_MS 3000   // Released past this -> radio on/off
#define BUTTON_PORTAL_HOLD_MS 6000 // Released past this -> the config portal

//* The gesture window for the click counts.
//? Telling one click from two - and two from five - cannot be done on a release,
//? because the firmware does not yet know whether another press is coming. This is
//! that wait, and it is paid on EVERY click: the mode changes this long after the
//* finger lifts. 400 ms is the usual compromise - short enough to feel immediate,
//? long enough that an unhurried double-tap lands inside one window.
#define BUTTON_CLICK_GAP_MS 400
//* A press longer than this is never a click.
//! Without it, a press too short for any hold threshold but too long to be a tap
//* (say 1.5 s) is classified SHORT and silently counted as one click of a gesture
//? the user never intended.
#define BUTTON_CLICK_MAX_MS 600
//* Five clicks is the most any switch recognises, and the count that fires the
//* power latch. A sixth tap in the same burst is simply the next gesture.
#define BUTTON_CLICK_MAX_COUNT 5

#define BUTTON_RECOVERY_WARN_MS 3000 // Countdown blink before the recovery hold is reached
#define BUTTON_RECOVERY_BLINK_MS 400 // One pulse of that countdown

//* The boot button's longest hold: the recovery wipe. It is the way back in when
//* the login has been forgotten or the saved networks are all gone, without a
//? cable and without a reflash, and it is deliberately the only thing in this
//* firmware that erases settings.
//! Touch deliberately has no recovery hold (0 in its wiring entry). The wipe is a
//* security action and stays on the switch that is part of the module itself.
#define BUTTON_BOOT_RECOVERY_MS 10000

//! Both switches' actions fire on RELEASE, not at the threshold, and two of them
//! restart the board: a C3 reset with GPIO9 still held enters the ROM's download
//? mode instead of running the firmware, so the board would look bricked until the
//! button was let go. That is why this give-up sits ABOVE every threshold that
//? acts, and why a hold long enough to be beyond doubt is not a press at all - it
//* is classified and dropped instead of firing anything.
//? It is also what makes the wipe safe to hang off a long hold: a switch shorted
//* to ground never releases, so a jammed GPIO9 can never confirm one.
#define BUTTON_GIVE_UP_MS 15000
#define BUTTON_IRQ_DEBOUNCE_MS 50      // ISR-level: ignore edges closer together than this
#define BUTTON_RELEASE_DEBOUNCE_MS 30  // The pin must read released this long to count as released
#define BUTTON_MULTI_PRESS_GAP_MS 5000 // Gap that restarts the running press counter shown on the page

//* Power-off is barred for this long after boot.
//? The latch cut is the device's only power switch and it is irreversible
//! without a physical press of BT1, so a spurious classification here costs the
//? user a walk back to the board. The window has to cover the whole of setup(),
//* and the reason it is this long is in button_poll(): the ISR latches an edge
//? but never timestamps it, so the press clock starts at the FIRST loop() poll.
//? A level that is already high at power-on is therefore measured from the end
//! of the boot, not from power-on - the five-second threshold is reached at
//? boot_end + 5 s, which on this board lands around 8-10 s because the strip
//* self-test alone is ~2.8 s and the IMU blink another 1 s. Ten seconds is the
//* first round number that clears that, with margin for a slower boot.
#define POWER_OFF_ARM_MS 10000UL

//* Event codes, reported to the page as last_event. They describe what the
//? SWITCH did; what the firmware DOES about it is the button's `action`/`role`
//* text, because the same "held to its threshold" means the radio on the 3 s
//* threshold and the config portal on the 6 s one.
enum : uint8_t
{
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_SHORT,  // Released before long_press_ms
    BUTTON_EVENT_LONG,   // Held past long_press_ms
    BUTTON_EVENT_STUCK,  // Still down at monitor_timeout_ms: classified and let go
    BUTTON_EVENT_ACTION, // Held past action_hold_ms: this switch's action ran
    BUTTON_EVENT_TRIPLE, // Legacy: no longer produced. Clicks are grouped by the
                         // caller now (check_button / check_boot_button), so three
                         // presses are just three SHORT events and the gesture
                         // layer decides.
    //! Appended, never inserted: the page maps these numbers to names of its own,
    //* and a reordering would relabel every press already on its way out.
    BUTTON_EVENT_RECOVERY // Held to recovery_hold_ms and released: the wipe ran
};

//* What a completed group of clicks means on one switch. Each switch supplies its
//? own resolver, so the same 1-click event can be "next" on the touch pad and
//* "previous" on the BOOT switch without either caller knowing about the other.
typedef void (*button_click_handler)(uint8_t clicks);

typedef struct buttonInput
{
    uint8_t pin;
    bool active_high;     // True when the switch drives the pin HIGH while pressed
    const char *key;      // Stable slug for the web API
    const char *label;    // Human readable name shown in the UI
    const char *role;     // What the firmware does with it, shown in the UI
    const char *action;   // Short name of the action_hold_ms action, shown in the UI
    const char *long_action; // Short name of the long_press_ms action, shown in the UI
    uint32_t long_press_ms;    // 0 = a long hold means nothing here
    uint32_t action_hold_ms;   // 0 = this switch has no held action
    uint32_t recovery_hold_ms; // 0 = this switch has no recovery hold
    uint32_t monitor_timeout_ms;
    button_click_handler click_resolve; // Called when a click burst is complete

    //* ISR-owned. Nothing else touches these two.
    volatile bool irq_pending;
    volatile uint32_t irq_time;

    //* Loop-owned, all of it read by webpage_handle_buttons() from the HTTP
    //* service. They are not volatile because only loop() ever writes them.
    bool pressed;                // Switch is down right now
    uint32_t press_start_ms;     // When the current press began
    uint32_t release_seen_ms;    // First poll that saw it released, 0 while down
    uint16_t hold_ms;            // Duration of the last completed press
    uint32_t last_event_ms;      // When the last press was classified, 0 before the first
    uint32_t event_count;        // Completed presses since boot
    uint8_t last_event;
    uint8_t counter;             // Multi-press count in the current window
    uint8_t click_count;         // Clicks accumulated toward the 1/2/5 gesture
    uint32_t click_last_ms;      // When the last click of that burst landed
} struct_buttonInput;

struct_buttonInput touch_button;
struct_buttonInput boot_button;

//* Every switch the firmware watches, in one table: the page enumerates it, so
//* a third one means one more row here and one more ISR below.
struct_buttonInput *const button_inputs[] = {&touch_button, &boot_button};
#define BUTTON_INPUT_COUNT (sizeof(button_inputs) / sizeof(button_inputs[0]))

bool button_is_down(const struct_buttonInput &b)
{
    const bool level = digitalRead(b.pin);
    return b.active_high ? level : !level;
}

//* Drop the press in flight WITHOUT classifying it.
//? Used by the config-portal pump, which is the one place outside loop() that acts
//! on a hold. Handing that press back to the classifier afterwards would read the
//* same 3 s as a 5 s portal request or, worse, as the 10 s recovery wipe - and
//! both of those restart the board, so the skip would undo itself on the way up.
//? Clearing `pressed` is enough to hide the rest of the hold: the ISR latches a
//* FALLING edge only, so nothing new is registered until the switch comes up.
void button_discard_press(struct_buttonInput &b)
{
    b.pressed = false;
    b.release_seen_ms = 0;
    b.irq_pending = false;
}

const char *button_event_name(const struct_buttonInput &b)
{
    switch (b.last_event)
    {
    case BUTTON_EVENT_SHORT:
        return "short press";
    case BUTTON_EVENT_LONG:
        return (b.long_action != nullptr) ? b.long_action : "long press";
    case BUTTON_EVENT_STUCK:
        return "held past the timeout";
    case BUTTON_EVENT_ACTION:
        return b.action; // "power off", "config portal", ...
    case BUTTON_EVENT_TRIPLE:
        return "triple press";
    case BUTTON_EVENT_RECOVERY:
        return "recovery wipe";
    default:
        return "no press yet";
    }
}

//* Latch one edge. This is the whole ISR body for both switches.
void IRAM_ATTR button_latch(struct_buttonInput *b)
{
    uint32_t now = millis();
    if (now - b->irq_time >= BUTTON_IRQ_DEBOUNCE_MS)
    {
        b->irq_time = now;
        b->irq_pending = true;
    }
}

void IRAM_ATTR touch_button_isr() { button_latch(&touch_button); }
void IRAM_ATTR boot_button_isr() { button_latch(&boot_button); }

//* Zero a switch's counters and arm its pin.
//? `active_high` also picks the pull, because the pull has to hold the pin at
//? the idle level when the switch is open: the touch module drives the pin high,
//? so idle is LOW and the pulldown defines it; GPIO9's switch shorts to ground,
//? so idle is HIGH and the pullup defines it.
void button_input_configure(struct_buttonInput &b, uint8_t pin, bool active_high,
                            const char *key, const char *label, const char *role,
                            const char *action, const char *long_action,
                            uint32_t long_press_ms, uint32_t action_hold_ms,
                            uint32_t recovery_hold_ms, uint32_t monitor_timeout_ms,
                            button_click_handler click_resolve)
{
    b.pin = pin;
    b.active_high = active_high;
    b.key = key;
    b.label = label;
    b.role = role;
    b.action = action;
    b.long_action = long_action;
    b.long_press_ms = long_press_ms;
    b.action_hold_ms = action_hold_ms;
    b.recovery_hold_ms = recovery_hold_ms;
    b.monitor_timeout_ms = monitor_timeout_ms;
    b.click_resolve = click_resolve;
    b.irq_pending = false;
    b.irq_time = 0;
    b.pressed = false;
    b.press_start_ms = 0;
    b.release_seen_ms = 0;
    b.hold_ms = 0;
    b.last_event_ms = 0;
    b.event_count = 0;
    b.last_event = BUTTON_EVENT_NONE;
    b.counter = 0;
    b.click_count = 0;
    b.click_last_ms = 0;

    pinMode(b.pin, b.active_high ? INPUT_PULLDOWN : INPUT_PULLUP);
}

void setup_button_interrupt()
{
    //* GPIO3, the touch module. The edge is derived from the flag below instead of
    //? being fixed at RISING, because active_high already owns the pull and
    //? button_is_down() - and an ISR watching the wrong edge is the one failure
    //! that is indistinguishable from dead hardware (see TOUCH MODULE POLARITY
    //? above). A push-pull active-HIGH module, which is what this call declares,
    //* produces a rising edge on touch.
    //! A module that holds its output active after a tap (the latching, toggle-wired
    //! kind) would read as one endless press here and reach the give-up timeout.
    //* This firmware assumes the momentary behaviour, where the active level lasts
    //? exactly as long as the touch does.
    //? Its monitor timeout is the give-up, not an action: a hold that outlasts it is
    //* classified and dropped. The 0 in the recovery slot says this switch has no
    //? recovery wipe - that security action stays on GPIO9, which is part of the
    //* module itself.
    button_input_configure(touch_button, button_pin, true, "touch", "Touch button (GPIO3)",
                           "1 click: next mode. 2 clicks: previous mode. "
                           "5 clicks: power off. "
                           "3 s hold, then release: toggle the radio (offline mode). "
                           "6 s hold, then release: reboot into the WiFi config hotspot.",
                           "config portal", "radio on/off",
                           BUTTON_WIFI_HOLD_MS, BUTTON_PORTAL_HOLD_MS, 0,
                           BUTTON_GIVE_UP_MS, touch_click_resolve);
    attachInterrupt(digitalPinToInterrupt(touch_button.pin), touch_button_isr,
                    touch_button.active_high ? RISING : FALLING);

    //! GPIO9 is the C3's BOOT strap pin, not GPIO0: the comment that used to sit
    //! here described the classic ESP32 board this firmware was ported from. The
    //! Supermini pulls it up, so the switch reads HIGH when idle and LOW while it
    //! is held - which is what the pressed/released test and the flag-derived
    //* falling edge below both assume.
    //? This is the security and recovery switch, and its gestures are the fallback
    //? navigation when the touch pad is unavailable: 1 click goes BACK one mode
    //? (repeated clicks can walk the whole registry), 5 clicks power off, 3 s
    //? toggles the radio, 6 s opens the portal, 10 s wipes the saved networks and
    //? the page login. A tap under BUTTON_CLICK_MAX_MS is counted toward the 1/2/5
    //* ladder; a press between that and 3 s is dropped, and a hold past the
    //* give-up is dropped too.
    button_input_configure(boot_button, (uint8_t)prog_switch, false, "boot", "Boot button (GPIO9)",
                           "1 click: previous mode (fallback navigation). "
                           "5 clicks: power off. "
                           "3 s hold, then release: toggle the radio (offline mode). "
                           "6 s hold, then release: reboot into the WiFi config hotspot. "
                           "10 s hold, then release: wipe the saved networks and the page login.",
                           "config portal", "radio on/off",
                           BUTTON_WIFI_HOLD_MS, BUTTON_PORTAL_HOLD_MS, BUTTON_BOOT_RECOVERY_MS,
                           BUTTON_GIVE_UP_MS, boot_click_resolve);
    //? The edge follows the flag here too, for the same reason as the touch button:
    //* active_high = false is what makes this FALLING, so the two cannot disagree.
    attachInterrupt(digitalPinToInterrupt(boot_button.pin), boot_button_isr,
                    boot_button.active_high ? RISING : FALLING);
}

//* Cut the battery latch and restart. The latch holds the board's own supply, so
//* this is the only power switch the device has.
//? Fired by five clicks on either switch. It used to be a five-second hold on the
//! touch pad; moving it to the click ladder freed the holds for the radio and the
//* portal, which are the two things a user standing at the board actually needs.
//!
//! Refused for the first POWER_OFF_ARM_MS of wall clock. The check lives here,
//? at the single choke point, rather than at the caller, so a future second
//? caller inherits the guard instead of having to remember it.
//? Being refused does not re-arm anything: button_poll() has already classified
//* and consumed the press, and a pin that is still held produces no new edge
//* until it is released and pressed again.
void power_off_sequence()
{
    if (millis() < POWER_OFF_ARM_MS)
    {
        Serial.printf("[POWER] Refused at %lu ms - power-off is armed %lu ms after boot\n",
                      (unsigned long)millis(), (unsigned long)POWER_OFF_ARM_MS);
        return;
    }

    Serial.printf("[POWER] Latch cut at %lu ms\n", (unsigned long)millis());

    //* A deliberate power-off also means "start clean next time": clear the
    //? safe-mode flag before the latch opens, or the next boot would warn again
    //* about a supply the user has already decided to retry.
    safe_mode_clear();

    //* Red, steady, on both WS2812B channels. This is the last thing the board
    //? shows, so it has to be readable in one glance and it has to be red: the
    //* user is about to lose the device and has one second to change their mind.
    ui_action_feedback(UI_ACTION_POWER_OFF);
    delay(1000);                     // Long enough to see it, short enough not to be a wait
    digitalWrite(latch_enable, LOW); // Deactivate latch to shutdown
    delay(100);                      // Wait for the latch to settle
    //! A board on USB power stays alive after the latch opens, so what follows is
    //? a restart, not a shutdown. Say so: the serial log would otherwise look like
    //* a power-off that failed. On battery the rail collapses before this prints.
    Serial.println("[POWER] Still powered after the latch cut - USB supply, restarting");
    Serial.flush();
    ESP.restart();
}

//* ---------------------- RECOVERY BOOT ----------------------
//? Countdown state for the long hold. Two plain globals rather than statics, to
//* match the rest of this file - and both are owned by loop() alone.
uint32_t boot_recovery_blink_ms = 0;
bool boot_recovery_warned = false;

//* Erase the saved networks and the page login, keep everything else, reboot.
//! Exactly two namespaces are cleared - "wifi" and "auth" - and that is the whole
//? of it. Mode, palette, every tunable and every preset live in "device", which is
//* never opened here, so a recovery costs the user their way back in and nothing
//* at all about the light itself.
//? The reboot is not decoration. setup_WIFI() decides portal-or-not from what it
//? reads at boot, and the page arms its login when it starts, so both halves of
//* the wipe only take effect on the way back up.
void recovery_boot()
{
    Serial.println("[REC] Recovery: clearing the saved networks and the page login");
    Serial.println("[REC] Mode, palette, tunables and presets are NOT touched");

    wifi_recovery_wipe();
    webpage_auth_clear();

    //* Magenta, held long enough to read: this is the last thing the board shows
    //? before it comes back with no network and no login, and a user who did not
    //* mean it has exactly this moment to pull the power.
    ui_action_feedback(UI_ACTION_RECOVERY_WIPE);
    delay(1200);

    Serial.println("[REC] Rebooting - the config hotspot opens on the way up");
    Serial.flush();
    ESP.restart();
}

//* ---------------------- HELD ACTIONS ----------------------
//? Two holds are shared by both switches, so their feedback is shared too. The
//? colour for each one lives in services/led_scripts.h; these functions only say
//* which action happened.

//* The 3 s hold: flip the radio, live and persistently.
//? The colour follows where the radio ENDED UP, not which direction the switch
//? was flipped: a board with no credentials falls straight back to offline even
//? when the user asked for radio on, and painting that green would be a lie.
void radio_toggle_action(const char *source)
{
    Serial.printf("[BUTTON] %s: held to the wifi threshold - toggling the radio\n", source);
    wifi_toggle_offline();
    ui_action_feedback(net_state == NET_OFFLINE ? UI_ACTION_RADIO_OFF : UI_ACTION_RADIO_ON);
}

//* The 6 s hold: leave a portal request in NVS and restart into it.
//? This is a restart rather than a live portal because port 80 belongs to the
//! configuration page once the board has joined a network, and WiFiManager's
//? portal needs that port for its own server. setup_WIFI() consumes the request
//? on the way up; the reboot also means the feedback below is the last frame the
//* user sees, so it is allowed to block for 400 ms.
void config_portal_action(const char *source)
{
    Serial.printf("[BUTTON] %s: held to the hotspot threshold - rebooting into the config hotspot\n", source);
    wifi_request_portal_on_boot();
    ui_action_feedback(UI_ACTION_CONFIG_PORTAL);
    delay(400); // Long enough to see it; loop() is about to end anyway
    ESP.restart();
}

//* Classify one completed press and put the state machine back to idle.
//? Called only from button_poll(). `stuck` means the switch was still down at
//? monitor_timeout_ms, which is not a release but has to be treated as the end of
//! the press all the same: `stuck` is what stops a shorted switch from latching
//! `pressed` forever and hiding every later press.
void button_finalize(struct_buttonInput &b, uint32_t held_ms, bool stuck)
{
    b.pressed = false;
    b.release_seen_ms = 0;
    b.hold_ms = (held_ms > 0xFFFF) ? 0xFFFF : (uint16_t)held_ms;
    b.last_event_ms = millis();
    b.event_count++;

    //* The recovery hold comes first, and it is the one branch that insists on a
    //? real release: `stuck` means the switch was still down at the monitor
    //! timeout, which is what a pin shorted to ground looks like, and a wiped
    //* board is not something to hand to a hardware fault.
    if (!stuck && b.recovery_hold_ms != 0 && held_ms >= b.recovery_hold_ms)
    {
        b.last_event = BUTTON_EVENT_RECOVERY;
        b.counter = 0;
    }
    //* This switch's action threshold next: it is the longest hold that fires
    //? while still held, and on both switches that is the config portal (which
    //* restarts the board).
    //? A press that was given up on resolves to the action only when the action
    //? threshold IS the monitor timeout. Neither switch has that arrangement any
    //! more, so a jammed switch now stops at STUCK instead of requesting a config
    //* portal on every single boot.
    else if (b.action_hold_ms != 0 && held_ms >= b.action_hold_ms &&
             (!stuck || b.action_hold_ms == b.monitor_timeout_ms))
    {
        b.last_event = BUTTON_EVENT_ACTION;
        b.counter = 0;
    }
    else if (stuck)
    {
        b.last_event = BUTTON_EVENT_STUCK;
        b.counter = 0;
    }
    else if (b.long_press_ms != 0 && held_ms > b.long_press_ms)
    {
        b.last_event = BUTTON_EVENT_LONG;
        b.counter = 0;
    }
    else
    {
        //* A short press is a CLICK CANDIDATE and nothing more.
        //? Grouping clicks into the 1/2/5 gestures belongs to the caller
        //! (check_button / check_boot_button), because only the caller knows the
        //? click window and can tell whether another press is still coming. This
        //* function classifies ONE press and has no idea what follows it.
        b.last_event = BUTTON_EVENT_SHORT;
        b.counter++;
    }

    //* One line per classified press, in the same shape whatever it turned out to
    //* be, so the log reads as a timeline of the switch rather than of this
    //* function.
    Serial.printf("[BUTTON] %s: %s (%u ms, count %u)\n", b.key, button_event_name(b),
                  (unsigned)held_ms, (unsigned)b.counter);
}

//* The whole state machine, for one switch. It classifies; it never acts.
void button_poll(struct_buttonInput &b)
{
    const uint32_t now = millis();

    //* Press. The ISR latched the edge (with its own debounce), so this only has
    //* to start the press clock.
    if (b.irq_pending)
    {
        b.irq_pending = false;

        //! A second edge while a press is already in flight is bounce, not a new
        //! press. The old code could count it as one, because it re-read the ISR
        //! flag after the release wait and measured the hold from the bounce.
        if (!b.pressed)
        {
            //* Reset the multi-press count when this press arrives more than
            //* BUTTON_MULTI_PRESS_GAP_MS after the previous one was classified.
            if (b.last_event_ms != 0 && (now - b.last_event_ms) > BUTTON_MULTI_PRESS_GAP_MS)
                b.counter = 0;

            b.pressed = true;
            b.press_start_ms = now;
            b.release_seen_ms = 0;
            //* last_event/last_event_ms/hold_ms keep describing the previous
            //* press until this one is classified, so the page does not blank its
            //* readout the moment a new press starts - `pressed` is what says a
            //* press is in flight.
        }
    }

    if (!b.pressed)
        return;

    const uint32_t held = now - b.press_start_ms;

    //* Held past the point where waiting can teach us anything: classify and drop
    //* the press. Neither switch acts at this point; it is the jammed-switch guard.
    if (held >= b.monitor_timeout_ms)
    {
        button_finalize(b, held, true);
        return;
    }

    if (button_is_down(b))
    {
        //* Still held: the press clock keeps running and loop() stays free.
        b.release_seen_ms = 0;
        return;
    }

    //* Released. The pin has to read released for BUTTON_RELEASE_DEBOUNCE_MS
    //* before the press counts as over - a bouncing contact that drops back
    //* inside that window is the same press continuing.
    if (b.release_seen_ms == 0)
    {
        b.release_seen_ms = now;
        return;
    }
    if ((now - b.release_seen_ms) < BUTTON_RELEASE_DEBOUNCE_MS)
        return;

    button_finalize(b, held, false);
}

//* ---------------------- CLICK GESTURES ----------------------
//? button_poll() only says "a short press ended". Whether that press is tap 1 of
//? a 5-click shutdown or a completed single click is decided here, once the click
//* window has closed - or immediately when the count can no longer grow.

//* Count one completed tap and fire a gesture if the burst is already complete.
void button_clicks_add(struct_buttonInput &b)
{
    b.click_count++;
    b.click_last_ms = millis();

    //* Fire the five-click power-off the instant the count arrives instead of
    //? waiting the window out: the count cannot grow past it and the user has
    //* already said everything there is to say.
    if (b.click_count >= BUTTON_CLICK_MAX_COUNT)
    {
        const uint8_t clicks = b.click_count;
        b.click_count = 0;
        b.click_resolve(clicks);
    }
}

//* Resolve a burst once the click window closes.
//! `!b.pressed` matters. Without it a click followed straight away by a hold
//? would resolve during the hold - switching the mode at 400 ms and then doing
//* the hold action on release, two actions from one intention. While a press is
//? in flight the count is left pending; a hold branch clears it, so the hold wins.
void button_clicks_poll(struct_buttonInput &b)
{
    if (b.click_count == 0 || b.pressed)
        return;
    if ((millis() - b.click_last_ms) < BUTTON_CLICK_GAP_MS)
        return;

    const uint8_t clicks = b.click_count;
    b.click_count = 0;
    b.click_resolve(clicks);
}

//* The touch pad's click map: the daily direction, plus the shared power-off.
//? 3 and 4 clicks have no gesture of their own. They are answered with the dim
//! red "nothing here" flash rather than mapped onto a neighbour: a half-finished
//? 5-click shutdown must not switch the mode on its way past.
void touch_click_resolve(uint8_t clicks)
{
    switch (clicks)
    {
    case 1:
        Serial.println("[BUTTON] touch: 1 click - next mode");
        led_mode_next();
        ui_action_feedback(UI_ACTION_MODE_NEXT);
        break;
    case 2:
        Serial.println("[BUTTON] touch: 2 clicks - previous mode");
        led_mode_prev();
        ui_action_feedback(UI_ACTION_MODE_PREV);
        break;
    case 5:
        //* Five deliberate clicks is hard to do by accident, which is why the
        //! power latch is allowed to hang off it at all. power_off_sequence()
        //* still refuses for the first POWER_OFF_ARM_MS of wall clock.
        Serial.println("[BUTTON] touch: 5 clicks - power off");
        power_off_sequence();
        break;
    default:
        Serial.printf("[BUTTON] touch: %u clicks - no gesture, ignored\n", (unsigned)clicks);
        ui_action_feedback(UI_ACTION_IGNORED);
        break;
    }
}

//* The BOOT switch's click map: one step BACK, plus the shared power-off.
//? One click is the whole mode fallback. It only walks backwards, but repeated
//? clicks reach every entry in the registry, which is exactly what a board with a
//* dead touch pad needs. A second click does not add a forward step: the fallback
//? should be one unambiguous gesture, not a second ladder to learn.
void boot_click_resolve(uint8_t clicks)
{
    switch (clicks)
    {
    case 1:
        Serial.println("[BUTTON] boot: 1 click - previous mode");
        led_mode_prev();
        ui_action_feedback(UI_ACTION_MODE_PREV);
        break;
    case 5:
        Serial.println("[BUTTON] boot: 5 clicks - power off");
        power_off_sequence();
        break;
    default:
        Serial.printf("[BUTTON] boot: %u clicks - no gesture, ignored\n", (unsigned)clicks);
        ui_action_feedback(UI_ACTION_IGNORED);
        break;
    }
}

//* Blank the touch pad for a while after a load test.
//? A 3 A burst can couple into the capacitive module and latch a false press;
//! the ISR is real, the gesture is not. The pad is discarded for the window, so
//* the mode cannot change behind the user's back when the test ends.
uint32_t touch_suppress_until_ms = 0;

void button_suppress_touch(uint32_t ms)
{
    touch_suppress_until_ms = millis() + ms;
}

//* The touch pad: the daily interface.
//?   1 click  next mode, 2 clicks previous mode, 5 clicks power off,
//*   >= 3 s   radio on/off, >= 6 s config hotspot.
//! Every hold resolves on RELEASE. Two of them restart the board, and a C3 reset
//? with GPIO9 still held lands in the ROM's download mode instead of the
//* firmware - the board would look bricked until the button was let go.
void check_button()
{
    //* Load-test transient gate. Discard whatever the pad latched, including the
    //? click counters, until the window passes. GPIO9 is a mechanical switch and
    //* stays live; this is only the capacitive pad.
    if ((int32_t)(millis() - touch_suppress_until_ms) < 0)
    {
        button_discard_press(touch_button);
        touch_button.click_count = 0;
        touch_button.counter = 0;
        touch_button.click_last_ms = 0;
        return;
    }

    const uint32_t before = touch_button.event_count;
    button_poll(touch_button);

    //? event_count is the "a press completed on this pass" signal: last_event
    //! describes the previous press too, so acting on it directly would repeat the
    //! action on every pass of loop() until the next press.
    if (touch_button.event_count != before)
    {
        switch (touch_button.last_event)
        {
        case BUTTON_EVENT_ACTION: // >= 6 s
            touch_button.click_count = 0;
            config_portal_action("touch");
            return; // Not reached: the board restarts

        case BUTTON_EVENT_LONG: // >= 3 s
            touch_button.click_count = 0;
            radio_toggle_action("touch");
            return;

        case BUTTON_EVENT_SHORT:
            //! A press that outlasted BUTTON_CLICK_MAX_MS is not a tap, even
            //? though it is far too short to reach any hold threshold. Counting it
            //* as a click would let a slow, hesitant press switch the mode.
            if (touch_button.hold_ms <= BUTTON_CLICK_MAX_MS)
            {
                button_clicks_add(touch_button);
                return;
            }

            //* Too long to be a click: it cancels whatever clicks were pending,
            //* because the user has visibly moved on to a different intention.
            touch_button.click_count = 0;
            Serial.printf("[BUTTON] touch: released after %u ms - too long for a click, "
                          "too short for a hold\n",
                          (unsigned)touch_button.hold_ms);
            return;

        default: // STUCK: given up on, never a gesture
            touch_button.click_count = 0;
            return;
        }
    }

    button_clicks_poll(touch_button);
}

//* The BOOT switch: the security and recovery interface, with mode navigation as
//? the fallback when the touch pad cannot be used.
//?   1 click  previous mode, 5 clicks power off,
//*   >= 3 s   radio on/off, >= 6 s config hotspot, >= 10 s recovery wipe.
//! Every hold resolves on RELEASE, and the two rebooting holds are why: a C3
//? reset with GPIO9 still held lands in the ROM's download mode instead of the
//* firmware, so the board would look bricked until the button was let go.
void check_boot_button()
{
    const uint32_t before = boot_button.event_count;
    button_poll(boot_button);

    //* The countdown to the recovery hold, drawn from the LIVE press rather than
    //? from the classified event: the event does not exist until the switch is let
    //! go, which is far too late to warn anyone about what letting go will do.
    //* One amber pulse per period over the last BUTTON_RECOVERY_WARN_MS of the hold.
    if (boot_button.pressed)
    {
        const uint32_t held = millis() - boot_button.press_start_ms;

        if (held >= (BUTTON_BOOT_RECOVERY_MS - BUTTON_RECOVERY_WARN_MS))
        {
            if (!boot_recovery_warned)
            {
                boot_recovery_warned = true;
                Serial.printf("[BUTTON] boot: %u ms more and the release wipes the "
                              "saved networks and the page login\n",
                              (unsigned)BUTTON_RECOVERY_WARN_MS);
            }

            if ((millis() - boot_recovery_blink_ms) >= BUTTON_RECOVERY_BLINK_MS)
            {
                boot_recovery_blink_ms = millis();
                //? Amber, on the pixel the network blink otherwise owns. Each
                //* notice lasts exactly one period, so the next one lands as this
                //? expires and the countdown reads as a pulse, not as a colour.
                led_notice(45, 18, 0, BUTTON_RECOVERY_BLINK_MS);
            }
        }
    }
    else
    {
        boot_recovery_warned = false;
        boot_recovery_blink_ms = 0;
    }

    //* A completed press. Holds act immediately (each is unambiguous on its own);
    //? a SHORT press is only ever ACCUMULATED, because the gesture is not known
    //* until the click window closes or the count reaches its maximum.
    if (boot_button.event_count != before)
    {
        switch (boot_button.last_event)
        {
        case BUTTON_EVENT_RECOVERY:
            boot_button.click_count = 0;
            recovery_boot();
            return; // Not reached: the board restarts

        case BUTTON_EVENT_ACTION: // >= 6 s
            boot_button.click_count = 0;
            config_portal_action("boot");
            return; // Not reached: the board restarts

        case BUTTON_EVENT_LONG: // >= 3 s
            boot_button.click_count = 0;
            //! This is the only switch that changes the radio from a board whose
            //* touch pad has failed, so it always acts.
            radio_toggle_action("boot");
            return;

        case BUTTON_EVENT_SHORT:
            //! A press that outlasted BUTTON_CLICK_MAX_MS is not a tap, even
            //? though it is far too short to reach any hold threshold. Counting it
            //* as a click would let a slow, hesitant press switch the mode.
            if (boot_button.hold_ms <= BUTTON_CLICK_MAX_MS)
            {
                button_clicks_add(boot_button);
                return;
            }

            //* Too long to be a click: it cancels whatever clicks were pending,
            //* because the user has visibly moved on to a different intention.
            boot_button.click_count = 0;
            Serial.printf("[BUTTON] boot: released after %u ms - too long for a click, "
                          "too short for a hold\n",
                          (unsigned)boot_button.hold_ms);
            return;

        default: // STUCK: given up on, never a gesture
            boot_button.click_count = 0;
            return;
        }
    }

    button_clicks_poll(boot_button);
}
#endif // SERVICE_READINGS
