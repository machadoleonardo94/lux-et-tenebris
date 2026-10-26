//? ---------------------- BUTTON PROBE ----------------------
//? A diagnostics-only sketch for the Lux et Portabilis V2 button net.
//? Build and flash it with, from the firmware directory:
//?     pio run -d tools/button_probe -t upload --upload-port COM6
//? It is NOT part of the firmware: this directory is its own PlatformIO project
//* (see platformio.ini here), so the real firmware is not compiled into it.

//! WHY THIS EXISTS
//! The touch pad is an EXTERNAL module on TOUCH1, and the V2 board puts no pull
//! resistor on that net. Measured off the board's own net table:
//!
//!   TOUCH1 pin 1 -> +BATT          (the raw battery rail, NOT 3.3 V)
//!   TOUCH1 pin 2 -> Button_GPIO    (the module's OUT)
//!   TOUCH1 pin 3 -> GND
//!   Button_GPIO  -> ESP32-C3 pad 12 = GPIO3, and nothing else drives it:
//!                   C7  100 nF to GND   (filter)
//!                   R12 2.2 k to BT1    (the on-board power button shares this net)
//!                   D6  1N4148W         (clamp, anode on this net)
//!
//! So the idle level of GPIO3 is entirely the module's business. The firmware
//! assumes ACTIVE HIGH - button_input_configure() gives it INPUT_PULLDOWN and
//* setup_button_interrupt() waits for a RISING edge. A module that is active-LOW
//! or open-drain can never produce that rising edge, and the button then looks
//* completely dead while being perfectly wired.

//! HOW TO READ THIS
//! Each pin is sampled for PROBE_WINDOW_MS under three internal pull
//! configurations in turn. Every window reports the digital levels seen, how many
//! times the level changed, and the average ADC reading in millivolts.
//!
//! Touch the module during EVERY window, then compare:
//!
//!   Only the PULLDOWN window shows edges, min=0 max=1
//!       -> active HIGH, push-pull. The firmware's assumption is correct and the
//!          fault is elsewhere (power to the module, or the module itself).
//!
//!   Only the PULLUP window shows edges, min=0 max=1
//!       -> active LOW / open drain. The firmware needs active_high = false,
//!          which also switches it to INPUT_PULLUP (see services/readings.h).
//!
//!   Edges in NO window, level pinned at 0 or 1
//!       -> the pin never moves. The module is unpowered, its OUT is not on
//!          TOUCH1 pin 2, or it is dead. Check VCC at TOUCH1 pin 1 - remember it
//!          is +BATT, so on USB with no cell fitted it may not be what you expect.
//!
//!   Every window wanders between 0 and 1 with no touch, and the average ADC
//!   reading drifts when your hand approaches
//!       -> nothing is attached to that pin. It is floating.
//!
//!   A fixed average near 1650 mV that never reaches 0 or 3.3 V
//!       -> two resistors are fighting over the line. That is the signature of an
//!          external pull meeting the internal one, and it explains a pin that
//!          reads a constant HIGH or LOW no matter what the module does.
//!
//! GPIO2 is probed as well, as a CONTROL. On this board pad 11 is Servo_out, so
//? it is expected to look like nothing or like a floating pin - it is there to
//* show what "no button here" looks like next to the real pin.

//! SAFETY - GPIO4
//! GPIO4 is latch_enable: it holds the board's own power latch closed. The real
//! firmware raises it in the first statements of setup(), and this sketch does the
//! same. Without it the latch opens and the board switches itself off as soon as
//* the power button is released, which looks exactly like a crash.

#include <Arduino.h>

#define LATCH_ENABLE_PIN 4
#define BUTTON_PIN 3 // TOUCH1 pin 2, the module's OUT, shared with BT1 through R12
#define CONTROL_PIN 2 // Servo_out on V2 - no button here, for contrast

//* How long each pull configuration is held before the report moves on.
#define PROBE_WINDOW_MS 2000
//* Sampling period inside a window. 2 ms is well under any module's output
//* transition and gives ~1000 samples per window.
#define PROBE_SAMPLE_MS 2
//* Discarded after a configuration change: the pin and the 100 nF cap on it need
//! a moment to settle, and those samples would be counted as edges otherwise.
#define PROBE_SETTLE_MS 40

enum probePull : uint8_t
{
    PULL_FLOAT = 0,
    PULL_UP,
    PULL_DOWN,
    PULL_COUNT
};

const char *pull_name(uint8_t cfg)
{
    switch (cfg)
    {
    case PULL_FLOAT:
        return "float   ";
    case PULL_UP:
        return "pullup  ";
    default:
        return "pulldown";
    }
}

void probe_apply_pull(uint8_t pin, uint8_t cfg)
{
    switch (cfg)
    {
    case PULL_UP:
        pinMode(pin, INPUT_PULLUP);
        break;
    case PULL_DOWN:
        pinMode(pin, INPUT_PULLDOWN);
        break;
    default:
        //? INPUT, not INPUT_PULLUP: this window has to show what the outside
        //* world does with the pin when nothing on the chip helps it.
        pinMode(pin, INPUT);
        break;
    }
}

//* One window: hold one pull configuration and report what the pin did.
//? `edges` is the count of digital level changes, so a pin that follows the
//? module produces edges in the window whose pull matches the module's idle
//* state, and a pin that never moves produces none anywhere.
void probe_window(uint8_t pin, uint8_t cfg)
{
    probe_apply_pull(pin, cfg);
    delay(PROBE_SETTLE_MS);

    int first = digitalRead(pin);
    int sawLow = (first == LOW) ? 1 : 0;
    int sawHigh = (first == HIGH) ? 1 : 0;
    int edges = 0;
    int previous = first;

    uint32_t samples = 0;
    uint32_t mv_sum = 0;

    const uint32_t start = millis();
    while (millis() - start < (uint32_t)PROBE_WINDOW_MS)
    {
        const int level = digitalRead(pin);
        if (level != previous)
        {
            edges++;
            previous = level;
        }
        if (level == LOW)
            sawLow = 1;
        else
            sawHigh = 1;

        //? analogReadMilliVolts() applies the core's own eFuse calibration, so
        //* this figure is directly comparable to a multimeter on the pin - which
        //? is what makes a "stuck at 1.65 V" divider visible as such instead of
        //* just reading as a solid HIGH.
        mv_sum += (uint32_t)analogReadMilliVolts(pin);
        samples++;

        delay(PROBE_SAMPLE_MS);
    }

    const uint32_t avg_mv = (samples > 0) ? (mv_sum / samples) : 0;

    Serial.printf("[PROBE] GPIO%-2u %s  low=%d high=%d edges=%-4d avg=%4u mV\n",
                  (unsigned)pin, pull_name(cfg), sawLow, sawHigh, edges,
                  (unsigned)avg_mv);
}

void probe_pin(uint8_t pin, const char *note)
{
    Serial.printf("\n[PROBE] ===== GPIO%u %s =====\n", (unsigned)pin, note);
    for (uint8_t cfg = 0; cfg < PULL_COUNT; cfg++)
        probe_window(pin, cfg);
}

void setup()
{
    //* First, for the reason in the header: the power latch is the board's own
    //* supply and nothing else in this sketch would keep it closed.
    pinMode(LATCH_ENABLE_PIN, OUTPUT);
    digitalWrite(LATCH_ENABLE_PIN, HIGH);

    Serial.begin(115200);
    delay(300);

    Serial.println("\n\n================ BUTTON PROBE ================");
    Serial.println("Lux et Portabilis V2 - touch module polarity test");
    Serial.println("TOUCH1: pin1=+BATT  pin2=signal  pin3=GND");
    Serial.println("GPIO3 is the button net. GPIO2 is Servo_out, used as a control.");
    Serial.println("");
    Serial.println("Touch the module during EVERY window below.");
    Serial.println("  edges in PULLDOWN only -> active HIGH (firmware's assumption)");
    Serial.println("  edges in PULLUP only   -> active LOW  (firmware must invert)");
    Serial.println("  edges nowhere          -> module unpowered / not on pin 2 / dead");
    Serial.println("  wanders with no touch  -> nothing attached to that pin");
    Serial.printf("\nEach window is %u ms.\n", (unsigned)PROBE_WINDOW_MS);
}

void loop()
{
    probe_pin(BUTTON_PIN, "(TOUCH1 pin 2, the button)");
    probe_pin(CONTROL_PIN, "(Servo_out, control - no button expected)");

    Serial.println("\n[PROBE] ---- cycle complete, repeating ----");
}
