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
//* Its inverse, for the double-click. Declared for the same reason as the walk
//* above: the registry lives in a header that is included after this one.
void led_mode_prev();

//* Same ordering problem for the WiFi service: both boot-button actions are
//* implemented in services/wifi_settings.h, which is included after this header.
//? The portal is requested by leaving a flag in NVS and restarting, so that is
//? all the button side needs; the offline toggle is a live transition the WiFi
//* service owns, so the name is all it needs either.
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
//? Two switches on this board, both reported live on the configuration page (the
//? table below is what webpage_handle_buttons() walks):
//?
//?   touch_button  GPIO3  The touch module. It drives the pin HIGH while it is
//!                        touched. HARDWARE DAMAGED on this board: the pad still
//?                        measures a clean 3.3 V but the pin reads LOW and never
//!                        latches an edge, so nothing here fires any more. Kept
//*                        for the repair that clamps BT1 below 3.3 V.
//?                          1 s hold - switch to the next LED mode (on release)
//?                          5 s hold - cut the battery latch and power down
//?   boot_button   GPIO9  The C3 Supermini's BOOT switch, HIGH while idle and
//?                        pulled LOW while it is held. This is now the whole user
//!                        interface, because it is on the module itself and so is
//*                        unaffected by anything done to the carrier board:
//?                          1 click   - next LED mode
//?                          2 clicks  - previous LED mode
//?                          5 clicks  - cut the battery latch and power down
//?                          3 s hold  - toggle offline mode (on release)
//?                          6 s hold  - reopen the WiFi config hotspot (on release)
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
//! user stops clicking, so button_finalize() only reports SHORT and
//* check_boot_button() groups them across BUTTON_CLICK_GAP_MS.
//!
//! An ISR only latches an edge. The classification, the actions and every number
//! the page draws are polled from loop(), so a press never sits inside a wait
//! loop. This used to block: check_button() waited for the release with delay(10)
//! in a while loop, which stopped web_server.handleClient() for the whole press -
//! the page could only be answered after the finger came off, a 1 s press froze
//! the strip and the HTTP service alike, and a held switch was invisible to
//! /api/buttons however fast the page polled.

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

#define BUTTON_LONG_PRESS_MS 1000  // Touch held past this: next LED mode
#define BUTTON_POWER_HOLD_MS 5000  // Touch held past this: latch off, power down

//* ---------------------- BOOT BUTTON GESTURES ----------------------
//? Every user-facing gesture now lives on ONE switch, GPIO9, because GPIO3 is
//! hardware-damaged on this board (the input buffer stopped responding while the
//? pad still measured a clean 3.3 V). GPIO9 is on the SuperMini module itself, so
//* it survives any repair done to the carrier board.
//?
//?   1 click    -> next LED mode      (fires BUTTON_CLICK_GAP_MS after the release)
//?   2 clicks   -> previous LED mode
//?   5 clicks   -> cut the power latch
//?   >= 3 s     -> toggle the radio: offline mode on/off
//?   >= 6 s     -> reboot into the WiFi config hotspot
//?   >= 10 s    -> recovery wipe: saved networks and page login
//?
//? Clicks and holds are told apart by HOW LONG THE PRESS WAS, not by two separate
//* counters racing each other: a press that outlasts BUTTON_CLICK_MAX_MS is a hold
//* candidate and can never be counted as a click.
#define BUTTON_WIFI_HOLD_MS 3000   // Boot: released past this -> radio on/off
#define BUTTON_PORTAL_HOLD_MS 6000 // Boot: released past this -> the config portal

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

#define BUTTON_RECOVERY_WARN_MS 3000 // Countdown blink before the recovery hold is reached
#define BUTTON_RECOVERY_BLINK_MS 400 // One pulse of that countdown

//* The boot button's longest hold: the recovery wipe. It is the way back in when
//* the login has been forgotten or the saved networks are all gone, without a
//? cable and without a reflash, and it is deliberately the only thing in this
//* firmware that erases settings.
#define BUTTON_BOOT_RECOVERY_MS 10000

//! The boot button's actions fire on RELEASE, not at the threshold, and two of
//! them restart the board: a C3 reset with GPIO9 still held enters the ROM's
//? download mode instead of running the firmware, so the board would look bricked
//! until the button was let go. That is why this give-up sits ABOVE every
//? threshold that acts, and why a hold long enough to be beyond doubt is not a
//* press at all - it is classified and dropped instead of firing anything.
//? It is also what makes the wipe safe to hang off a long hold: a switch shorted
//* to ground never releases, so a jammed GPIO9 can never confirm one.
#define BUTTON_BOOT_GIVE_UP_MS 15000
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
//* text, because the same "held to its threshold" means power-off on one switch
//* and a config portal on the other.
enum : uint8_t
{
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_SHORT,  // Released before long_press_ms
    BUTTON_EVENT_LONG,   // Held past long_press_ms
    BUTTON_EVENT_STUCK,  // Still down at monitor_timeout_ms: classified and let go
    BUTTON_EVENT_ACTION, // Held past action_hold_ms: this switch's action ran
    BUTTON_EVENT_TRIPLE, // Legacy: no longer produced. Clicks are grouped by the
                         // caller now (check_boot_button), so three presses are just
                         // three SHORT events and the gesture layer decides.
    //! Appended, never inserted: the page maps these numbers to names of its own,
    //* and a reordering would relabel every press already on its way out.
    BUTTON_EVENT_RECOVERY // Held to recovery_hold_ms and released: the wipe ran
};

typedef struct buttonInput
{
    uint8_t pin;
    bool active_high;     // True when the switch drives the pin HIGH while pressed
    const char *key;      // Stable slug for the web API
    const char *label;    // Human readable name shown in the UI
    const char *role;     // What the firmware does with it, shown in the UI
    const char *action;   // Short name of the action_hold_ms action, shown in the UI
    uint32_t long_press_ms;    // 0 = a long hold means nothing here
    uint32_t action_hold_ms;   // 0 = this switch has no held action
    uint32_t recovery_hold_ms; // 0 = this switch has no recovery hold
    uint32_t monitor_timeout_ms;

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
        return "long press";
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
                            const char *action, uint32_t long_press_ms,
                            uint32_t action_hold_ms, uint32_t recovery_hold_ms,
                            uint32_t monitor_timeout_ms)
{
    b.pin = pin;
    b.active_high = active_high;
    b.key = key;
    b.label = label;
    b.role = role;
    b.action = action;
    b.long_press_ms = long_press_ms;
    b.action_hold_ms = action_hold_ms;
    b.recovery_hold_ms = recovery_hold_ms;
    b.monitor_timeout_ms = monitor_timeout_ms;
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
    //! kind) would read as one endless press here and power the board down at
    //! BUTTON_POWER_HOLD_MS - this firmware assumes the momentary behaviour, where
    //! the active level lasts exactly as long as the touch does.
    //? Its monitor timeout IS its power-off threshold: the power-off fires while
    //? the pad is still held, because a finger resting on it after the latch drops
    //? is not a press anyone needs to see the end of.
    //? The touch button has no recovery hold: the 0 below says so, and the pair
    //* that follows is its action threshold and its monitor timeout, which are the
    //* same number by design.
    button_input_configure(touch_button, button_pin, true, "touch", "Touch button (GPIO3)",
                           "1 s press: next LED mode. 5 s press: power off at the latch.",
                           "power off",
                           BUTTON_LONG_PRESS_MS, BUTTON_POWER_HOLD_MS, 0,
                           BUTTON_POWER_HOLD_MS);
    attachInterrupt(digitalPinToInterrupt(touch_button.pin), touch_button_isr,
                    touch_button.active_high ? RISING : FALLING);

    //! GPIO9 is the C3's BOOT strap pin, not GPIO0: the comment that used to sit
    //! here described the classic ESP32 board this firmware was ported from. The
    //! Supermini pulls it up, so the switch reads HIGH when idle and LOW while it
    //! is held - which is what the pressed/released test and the flag-derived
    //* falling edge below both assume.
    //? Four holds, and their thresholds are what separate them: 1 s..5 s is the
    //? offline toggle (BUTTON_EVENT_LONG), 5 s..10 s is the portal
    //? (BUTTON_EVENT_ACTION), 10 s and over, once released, is the recovery wipe
    //? (BUTTON_EVENT_RECOVERY). Anything under a second is only counted, and a
    //! hold that outlasts the give-up is dropped rather than fired.
    button_input_configure(boot_button, (uint8_t)prog_switch, false, "boot", "Boot button (GPIO9)",
                           "1 click: next mode. 2 clicks: previous mode. "
                           "5 clicks: power off. "
                           "3 s hold, then release: toggle the radio (offline mode). "
                           "6 s hold, then release: reboot into the WiFi config hotspot. "
                           "10 s hold, then release: wipe the saved networks and the page login.",
                           "config portal",
                           BUTTON_WIFI_HOLD_MS, BUTTON_PORTAL_HOLD_MS, BUTTON_BOOT_RECOVERY_MS,
                           BUTTON_BOOT_GIVE_UP_MS);
    //? The edge follows the flag here too, for the same reason as the touch button:
    //* active_high = false is what makes this FALLING, so the two cannot disagree.
    attachInterrupt(digitalPinToInterrupt(boot_button.pin), boot_button_isr,
                    boot_button.active_high ? RISING : FALLING);
}

//* Cut the battery latch and restart. The latch holds the board's own supply, so
//* this is the only power switch the device has.
//? Moved here from loop() in main.cpp, where it fired on any press of this same
//! switch once five seconds had passed since boot - which would have collided
//! with the press the mode switch now uses. The hold is what tells the two apart,
//! and only the state machine below measures it.
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

    Serial.printf("[POWER] Off at %lu ms\n", (unsigned long)millis());
    onboard_led.setPixelColor(0, strip.Color(55, 0, 30));
    onboard_led.show();
    delay(1000);                     // Long enough to see it, short enough not to be a wait
    digitalWrite(latch_enable, LOW); // Deactivate latch to shutdown
    delay(100);                      // Wait for the latch to settle
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

    //* Magenta, and held long enough to read: this is the last thing the board
    //? shows before it comes back with no network and no login, and a user who
    //* did not mean it has exactly this moment to pull the power.
    onboard_led.setPixelColor(0, strip.Color(45, 0, 45));
    onboard_led.show();
    delay(1200);

    Serial.println("[REC] Rebooting - the config hotspot opens on the way up");
    Serial.flush();
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
    //? while still held, and on the touch button it takes the board away, so no
    //* later branch could matter.
    //? A press that was given up on still resolves to the action only when the
    //? action threshold IS the monitor timeout - the touch button's power-off,
    //! which is meant to fire with the pad still under a finger. The boot
    //* button's thresholds are not the same number, so a jammed GPIO9 now stops
    //? at STUCK instead of requesting a config portal on every single boot.
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
        //! (check_boot_button), because only the caller knows the click window and
        //? can tell whether another press is still coming. This function classifies
        //* ONE press and has no idea what follows it.
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
    //* the press. For the touch button this is where the power-off fires.
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

//* The touch button: mode switching and the power latch.
void check_button()
{
    const uint32_t before = touch_button.event_count;
    button_poll(touch_button);

    //? event_count is the "a press completed on this pass" signal: last_event
    //! describes the previous press too, so acting on it directly would repeat the
    //! action on every pass of loop() until the next press.
    if (touch_button.event_count == before)
        return;

    if (touch_button.last_event == BUTTON_EVENT_ACTION)
    {
        power_off_sequence();
        return; // Not reached: the board restarts
    }

    if (touch_button.last_event == BUTTON_EVENT_LONG)
    {
        //* Switch to next mode
        //? The registry walk, the wrap-around and the NVS save all live in
        //? led_mode_next() (services/led_scripts.h), so the button path and the
        //? webpage's POST /api/mode change the mode the same way.
        led_mode_next();
    }

    //* BUTTON_EVENT_SHORT needs no action on this switch: it is only counted and
    //* reported. The touch button has no click gestures - the boot button owns
    //* them, and grouping a click into a gesture needs the click window that
    //* check_boot_button() keeps.
}

//* Clicks waiting to be resolved into a gesture, and when the last one landed.
//? loop()-owned, like the rest of the button state. The count is only ever
//* reported ONCE, when the window closes, so a gesture cannot fire twice.
uint8_t boot_click_count = 0;
uint32_t boot_click_last_ms = 0;

//* Act on a group of clicks that is now known to be complete.
//? 3 and 4 clicks have no gesture of their own. They are dropped rather than
//! mapped onto a neighbour: a half-finished 5-click shutdown must not switch the
//? mode on its way past, and the user's intent there is simply unknown.
void boot_gesture_resolve(uint8_t clicks)
{
    switch (clicks)
    {
    case 1:
        //* Same path as the page's POST /api/mode, so the registry walk, the
        //* wrap-around and the NVS save all stay in one place.
        Serial.println("[BUTTON] boot: 1 click - next mode");
        led_mode_next();
        break;
    case 2:
        Serial.println("[BUTTON] boot: 2 clicks - previous mode");
        led_mode_prev();
        break;
    case 5:
        //* Five deliberate clicks is hard to do by accident, which is why the
        //! power latch is allowed to hang off it at all. power_off_sequence()
        //* still refuses for the first POWER_OFF_ARM_MS of wall clock.
        Serial.println("[BUTTON] boot: 5 clicks - power off");
        power_off_sequence();
        break;
    default:
        Serial.printf("[BUTTON] boot: %u clicks - no gesture, ignored\n", (unsigned)clicks);
        break;
    }
}

//* The boot button, and now the ONLY user input this board still has.
//?   1 click  next mode, 2 clicks previous mode, 5 clicks power off,
//*   >= 3 s   radio on/off, >= 6 s config hotspot, >= 10 s recovery wipe.
//! Every hold resolves on RELEASE. Two of them restart the board, and a C3 reset
//? with GPIO9 still held lands in the ROM's download mode instead of the
//* firmware - the board would look bricked until the button was let go.
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
            boot_click_count = 0;
            recovery_boot();
            return; // Not reached: the board restarts

        case BUTTON_EVENT_ACTION: // >= 6 s
            //* Reopen the config portal by restarting into it. See the note over
            //? wifi_request_portal_on_boot() in services/wifi_settings.h for why
            //? this is not a call to WiFiManager from here: port 80 belongs to the
            //! configuration page once the board has joined a network, and
            //! startConfigPortal() would block loop() for the whole window on top
            //* of that.
            boot_click_count = 0;
            wifi_request_portal_on_boot();

            Serial.println("[BUTTON] boot: held to the hotspot threshold - rebooting into the config hotspot");
            onboard_led.setPixelColor(0, strip.Color(0, 40, 60)); // Blue: portal, not power
            onboard_led.show();
            delay(400); // Long enough to see it; loop() is about to end anyway
            ESP.restart();
            return;

        case BUTTON_EVENT_LONG: // >= 3 s
            boot_click_count = 0;
            //! This is the only switch that changes the radio, so it is also the
            //* only way out of offline mode without a reflash - it always acts.
            Serial.println("[BUTTON] boot: held to the wifi threshold - toggling the radio");
            wifi_toggle_offline();
            return;

        case BUTTON_EVENT_SHORT:
            //! A press that outlasted BUTTON_CLICK_MAX_MS is not a tap, even
            //? though it is far too short to reach any hold threshold. Counting it
            //* as a click would let a slow, hesitant press switch the mode.
            if (boot_button.hold_ms <= BUTTON_CLICK_MAX_MS)
            {
                boot_click_count++;
                boot_click_last_ms = millis();

                //* Fire the shutdown the instant the count arrives instead of
                //? waiting the window out: the count cannot grow past it and the
                //* user has already said everything there is to say.
                if (boot_click_count >= 5)
                {
                    const uint8_t clicks = boot_click_count;
                    boot_click_count = 0;
                    boot_gesture_resolve(clicks);
                }
                return;
            }

            //* Too long to be a click: it cancels whatever clicks were pending,
            //* because the user has visibly moved on to a different intention.
            boot_click_count = 0;
            Serial.printf("[BUTTON] boot: released after %u ms - too long for a click, "
                          "too short for a hold\n",
                          (unsigned)boot_button.hold_ms);
            return;

        default: // STUCK: given up on, never a gesture
            boot_click_count = 0;
            return;
        }
    }

    //* The window closed with clicks still pending: the gesture is complete.
    //! `!boot_button.pressed` matters. Without it a click followed straight away
    //? by a hold would resolve its count DURING the hold - switching the mode at
    //* 400 ms and then toggling the radio on release, two actions from one
    //? intention. While a press is in flight the count is simply left pending; if
    //! that press turns into a hold, the branches above clear it, so the hold wins.
    if (boot_click_count > 0 && !boot_button.pressed &&
        (millis() - boot_click_last_ms) >= BUTTON_CLICK_GAP_MS)
    {
        const uint8_t clicks = boot_click_count;
        boot_click_count = 0;
        boot_gesture_resolve(clicks);
    }
}
#endif // SERVICE_READINGS
