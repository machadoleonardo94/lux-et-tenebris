#if !defined(PROJECT_GLOBAL_VARIABLES)
#define PROJECT_GLOBAL_VARIABLES

//* ---------------------- NETWORK IDENTITY ----------------------
//? The name this board answers to on the LAN. Three separate advertisements have
//? to agree on it, so it is defined once, here, and referenced everywhere else:
//?   1. the DHCP hostname the router's client list and its DNS record show
//?      (WiFiManager.setHostname, src/services/wifi_settings.h);
//?   2. the mDNS responder that actually answers <name>.local
//?      (ArduinoOTA in setup_OTA, and setup_webpage);
//?   3. the OTA target name listed by the Arduino IDE / espota.
//* The responder is started once per join and torn down with the OTA listener when
//* the board goes offline (see net_enter_offline in wifi_settings.h), so the name
//! has to be stated at each start - nothing re-registers it on its own.
//! The note that used to sit here said a second MDNS.begin() fails with
//! ESP_ERR_INVALID_STATE. That is not true of this framework: mdns_init() is
//! idempotent and returns success when it has already run, and the boot log shows
//! no failure from the second call. The reason to define the name once is simply
//! that three advertisements have to agree.
#define NETWORK_HOSTNAME "lightpack"

//* ---------------------- NETWORK STATE ----------------------
//? The board is in exactly one of these three states. It lives here rather than in
//? services/wifi_settings.h, which owns the transitions, because the onboard
//? status blink in services/led_scripts.h reads it and that header is included
//* first. Nothing outside wifi_settings.h may WRITE it - see the note there.
//?
//?   NET_OFFLINE     The radio is off. No join attempts, no hotspot, no page and
//?                   no OTA listener. This is the persisted "offline mode" switch
//?                   and it survives a reboot; a 1 s press of GPIO9 flips it.
//?   NET_CONNECTING  The radio is up, the saved network is not joined. The join is
//?                   retried every WIFI_RETRY_INTERVAL_MS, forever. There is no
//?                   address to serve on, so there is still no page and no OTA.
//?   NET_CONNECTED   Joined. The configuration page and the OTA listener are live.
//!
//! The numbers are the blink's case labels (services/led_scripts.h) and the wire
//? format of /api/state, so append to this enum rather than reordering it.
enum : uint8_t
{
    NET_OFFLINE = 0,
    NET_CONNECTING,
    NET_CONNECTED
};

uint8_t net_state = NET_OFFLINE;

//* ---------------------- GLOBAL COMPONENT VARIABLES ----------------------

//* OLED display
bool display_started = false;

//* Gyroscope
bool gyro_started = false;
sensors_event_t a, g, temp;

float max_gyro_x = 0;
float max_gyro_y = 0;
float max_gyro_z = 0;

float max_accel_x = 0;
float max_accel_y = 0;
float max_accel_z = 0;

uint32_t loopTimer = 0;
uint32_t globalTimer = 0;
uint32_t loopCounter = 0;

int position = 0;
int power = 5;
int lenght = 0;

int mode = 1;

uint8_t macAdress[6] = {0, 0, 0, 0, 0, 0};

typedef struct ledstrip
{
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    uint8_t color = 0;
    uint8_t brightness = 255; // Applied via setBrightness() in update_strip()
    uint32_t update_time = 50;
    int8_t index = 0;
} struct_ledstrip;

struct_ledstrip status_led;
struct_ledstrip led_strip;

//* ---------------------- STATUS LED NOTICE ----------------------
//? update_onboard_LED() (services/led_scripts.h) repaints the onboard pixel from
//? the gyro every 5 ms, so anything that writes a colour there is overwritten
//? before a human could see it. A notice is a colour plus a deadline: while the
//? deadline is in the future the gyro repaint stands down and the notice stands,
//? and when it expires the gyro display resumes on its own.
//! Deliberately not a blocking delay. The button actions that use this run inside
//? loop(), and blocking there is what the whole input path was rewritten to avoid
//* (see the note over the button table in services/readings.h).
uint32_t led_notice_until_ms = 0;
uint8_t led_notice_r = 0;
uint8_t led_notice_g = 0;
uint8_t led_notice_b = 0;

uint32_t adcReading = 0;
uint32_t adcVoltage = 0;
#define MAX_ADC_SAMPLES 8
#define OVERSAMPLING_RESOLUTION 3 // 2^3 = 8 samples

//* ---------------------- LAST BOOT REPORT ----------------------
//? Why the PREVIOUS run ended, and what the LED rail did while it did so. These
//? live here rather than in main.cpp because services/serial_outputs.h replays
//! them in the periodic telemetry - see the note over boot_report_print() for
//? why printing them at boot alone is not readable on this board.
//?
//? Captured as early in setup() as the reason can be read, before this boot has
//* a chance to overwrite it with a reset of its own.
esp_reset_reason_t boot_reset_reason = ESP_RST_UNKNOWN;

//? V_REG before any LED drew current. The baseline the sag below is measured
//* against.
uint32_t boot_rail_mv = 0;

//? Lowest V_REG seen while the strip self-test lit the strip, on the same boot.
//* 0 means the self-test did not run or never reported - not "zero volts".
uint32_t boot_rail_min_mv = 0;

//* The button state lives with the button code, not here: services/readings.h
//? owns both switches (GPIO3 touch, GPIO9 boot) in one `buttonInput` table, which
//? is what the configuration page enumerates. What used to sit here was a single
//? `buttonState` with a debounce timer and a press counter that the blocking
//? handler maintained by hand; the non-blocking state machine needs press start,
//? release, last event and event count per switch, and keeping a second copy of
//? any of that here would just be a global nobody reads.
//!
//! A second `buttonState` typedef, for a `button_states[4]` array, was removed
//! from this file before that: it was a redefinition error waiting to happen and
//! nothing anywhere indexed the array.

//* ---------------------- USER PALETTE ----------------------
//? The colour selected on the configuration page. Modes read this; only the
//? webpage writes it.
//! Do NOT store the palette in led_strip.red/green/blue. Those three fields are
//! scratch: flame_steps() and fade_strip() overwrite them every frame with a
//! colour computed from the accelerometer/gyro. With the palette kept there,
//! running either mode destroyed the user's choice, the page then read the
//! clobbered value back, and Solid color rendered leftover gyro red (x-axis
//! noise) because the y and z terms truncate to zero.

typedef struct colorRGB
{
    uint8_t red = 255;
    uint8_t green = 0;
    uint8_t blue = 255;
} struct_colorRGB;

struct_colorRGB palette;

//* ---------------------- PULSE WHIP TUNABLES (mode 5) ----------------------
//? Exposed on the device configuration webpage. The dynamics that consume these
//? live in pulse_whip() (services/led_scripts.h); ranges are enforced by
//? whip_sanitize(), which runs on every load and every write.

typedef struct whipSettings
{
    float speed = 4.0f;     // LEDs the hump advances per frame (a frame is LED_SCRIPT_FRAME_MS)
    uint8_t length = 30;    // Width of the half-sine hump, in LEDs
    uint16_t period = 1200; // ms between successive whip launches
    uint8_t peak = 200;     // Amplitude of the hump, 0-255
    uint8_t offset = 0;     // Baseline level the strip rests at between whips, 0-255
} struct_whipSettings;

struct_whipSettings whip;

//* ---------------------- RAINBOW FLOW TUNABLES (mode 6) ----------------------
//? Exposed on the device configuration webpage. The dynamics that consume these
//? live in rainbow_flow() (services/led_scripts.h); ranges are enforced by
//? rainbow_sanitize(), which runs on every load and every write.
//? The requested "min_tick / max_tick steps" are held here as wall-clock
//? milliseconds, because the delays the user reasons about are times, and the
//? flow speed is already expressed in LEDs per frame.

typedef struct rainbowSettings
{
    float speed = 2.0f;     // LEDs the pattern advances per frame (a frame is LED_SCRIPT_FRAME_MS)
    uint16_t min_ms = 400;  // Shortest wait before the starting LED is given a new random colour
    uint16_t max_ms = 2000; // Longest wait before the starting LED is given a new random colour
} struct_rainbowSettings;

struct_rainbowSettings rainbow;

//* ---------------------- STRIP SIZE ----------------------
//? NUM_LEDS (shared/library_objects.h) is the COMPILED size of the NeoPixel
//? buffer. Adafruit_NeoPixel allocates NUM_LEDS * 3 bytes during static
//? initialisation and exposes no API to grow or shrink that allocation at
//? runtime, so "adjustable strip length" in this firmware means the number of
//? LEDs the scripts LIGHT, not the number the driver owns. `active_leds` is that
//? number: anything from 1 to NUM_LEDS, with the pixels above it left dark.
//!
//! Never iterate NUM_LEDS in a mode. That lights the entire buffer whatever the
//! user chose, which is exactly the bug this setting would otherwise introduce.
//! Loop to strip_leds() (services/led_scripts.h) instead. The only place that
//! legitimately walks the whole buffer is the tail-clear in update_strip(),
//! which exists precisely to blank the pixels a shortened strip left behind.
#if !defined(NUM_LEDS)
#error "shared/variables.h needs NUM_LEDS from shared/library_objects.h - include shared/dependencies.h rather than this header directly."
#endif

typedef struct stripSettings
{
    uint16_t active_leds = NUM_LEDS; // LEDs actually driven, 1..NUM_LEDS
} struct_stripSettings;

struct_stripSettings strip_settings;

//* ---------------------- STEP FLAME TUNABLES (mode 2) ----------------------
//? Exposed on the device configuration webpage. The dynamics that consume these
//? live in flame_steps() (services/led_scripts.h); ranges are enforced by
//? flame_sanitize(), which runs on every load and every write.
//?
//? The mode is a flash-and-decay field holding one brightness per LED:
//?   1. A step detection lights the WHOLE strip at once, at a brightness
//?      proportional to the impact: `power_slope` times the magnitude
//?      impact_poll() reported (services/gyros.h), capped by `power_max`.
//?   2. Every frame, each LED keeps a fraction of its own brightness. `decay` is
//?      that fraction at LED 0, and it worsens with distance: LED i keeps
//?      decay^(1 + spatial * i / (leds - 1)). The far end therefore goes dark
//?      first and the flame reads as retreating towards the start of the strip.
//?      A `spatial` of 0 decays the whole strip together.
//?   3. The floor is `power_idle`, which is where the whole strip settles.
//?
//? A step arriving mid-decay raises every LED to at least the new flash
//? brightness, so overlapping steps brighten the strip rather than restarting it
//? dimmer than it already was.
//?
//? The flame is drawn in the PALETTE colour: `power` decides how bright each LED
//? is and the palette decides what colour that brightness is, exactly as Solid
//? color does. One consequence worth knowing: a palette of black draws no flame,
//? because there is no channel intensity in it to scale.
//! Three fixed per-channel gains used to sit here instead, and they are gone
//? rather than kept beside the palette - two controls for one colour is how this
//* mode came to ignore the colour picked on the page and always come out magenta.

typedef struct flameSettings
{
    float decay = 0.90f;      // Fraction of its brightness each LED keeps per frame, at LED 0
    float spatial = 2.0f;     // Extra decay each frame at the far end, spread across the strip
    float power_slope = 5.0f; // Brightness gained per unit of magnitude
    uint8_t power_max = 75;   // Ceiling on the flash brightness, 0-255
    uint8_t power_idle = 5;   // Brightness the strip rests at, 0-255
} struct_flameSettings;

struct_flameSettings flame;

//* ---------------------- IMPACT DETECTION TUNABLES ----------------------
//? Exposed on the device configuration webpage. The detector that consumes these
//? is impact_poll() (services/gyros.h); ranges and the cross-field relations
//? between them are enforced by step_sanitize(), which runs on every load and
//? every write.
//?
//? This began as a step detector for a device that sat in one place. The device is
//? worn and rotated and has to react to knocks as well as footfalls, so the
//? detector is now an impact detector and three things changed:
//?
//?   1. The magnitude is converted to g once, at the input. The sensor event is in
//!      m/s^2, and every one of these fields was documented and labelled as g - so
//!      `threshold` was being applied as 1.2 m/s^2, which is 0.12 g. The detector
//?      was 9.8x more sensitive than its own labels, and the whole reporting scale
//?      saturated on any real impact.
//?   2. It is polled from loop() on a fixed cadence instead of from the render path
//?      of one LED mode. It used to run at 20 Hz and only while the Step Flame mode
//?      was selected, which both missed the peak of a 10 ms impact and stopped the
//?      baseline learning entirely whenever another mode was chosen.
//?   3. A jerk gate was added. Measuring |accel| against a slow baseline already
//?      ignores pure rotation, but rotation about an axis offset from the sensor
//?      produces centripetal acceleration that does move |accel|; a brisk swing is
//?      ~0.9 g. An impact changes the magnitude over a couple of samples, a swing
//?      ramps over hundreds of milliseconds, and `jerk_min` is where that line sits.
//?
//? The state machine is:
//?
//?   magnitude = |accel| / g                                  (orientation free)
//?   jerk      = |magnitude - previous_magnitude| / dt        (g/s)
//?   impact    = |magnitude - baseline|                       (baseline is slow, in g)
//?
//? It ARMS when impact rises above `threshold` AND jerk is at least `jerk_min`,
//? captures the peak over a bounded window, and reports once. The reported value is
//? the peak rescaled from [impact_min, impact_max] onto [magnitude_min,
//? magnitude_max], which is the scale flame_steps() reads.
//!
//! Four relations have to hold, and they are why these are not independent
//! sliders: `release` < `threshold` (or the detector re-arms mid-impact and
//! double-counts one impact), `impact_min` < `impact_max`, `magnitude_min` <
//! `magnitude_max` (both are divisors of the rescale), and `baseline_tau_ms` must
//! stay long enough that the baseline does not track the event it is meant to
//! measure. step_sanitize() pulls the left side of each pair into line rather than
//! rejecting the write.

//* The magnitude scale, declared here because it is part of this settings block
//* rather than a fact about the detector: changing either end changes what every
//* consumer of impact_poll() sees, and the configuration page reports the
//* pair so its sliders and its labels cannot drift from the firmware's.
#define STEP_MAGNITUDE_MIN 3
#define STEP_MAGNITUDE_MAX 30

typedef struct stepSettings
{
    uint16_t baseline_tau_ms = 2000;     // Time constant of the resting-magnitude baseline
    float threshold = 1.20f;             // g above baseline that arms the detector
    float release = 0.45f;               // g at which an armed detector releases, < threshold
    float jerk_min = 10.0f;              // g/s the magnitude must be moving to arm; 0 disables the gate
    uint16_t min_gap_ms = 200;           // Refractory: impacts closer together than this do not re-arm
    float impact_min = 1.5f;             // Softest impact that maps to magnitude_min, in g
    float impact_max = 6.5f;             // Hardest impact that maps to magnitude_max, in g
    uint8_t magnitude_min = STEP_MAGNITUDE_MIN; // Bottom of the scale the modes consume
    uint8_t magnitude_max = STEP_MAGNITUDE_MAX; // Top of the scale
} struct_stepSettings;

struct_stepSettings step;

//* ---------------------- IMPACT DETECTOR OUTPUT ----------------------
//? The detector runs on its own cadence, independent of whichever mode is
//? rendering, so its result has to be handed to the modes rather than called by
//? them. `impact_pending` is the latch: the strongest impact accepted since the
//? last consumer took it, 0 meaning none. impact_take() (services/gyros.h) reads
//? and clears it in one step, so nothing can miss an event or see it twice.
//!
//? A latch rather than a queue is deliberate: two impacts inside one LED frame
//? cannot be drawn as two flashes anyway, and the frame that does draw them only
//? needs the stronger of the two.
uint8_t impact_pending = 0;

//* Diagnostics. impact_samples counts detector passes so the serial report can
//* show the rate the loop actually achieved, which is not something the firmware
//* can assume: with WiFi, a web request or an OTA transfer in flight, loop() can
//* run far slower than the detector's target cadence.
uint32_t impact_samples = 0;
uint8_t impact_last_magnitude = 0;
uint32_t impact_last_ms = 0;
uint32_t impact_count = 0;

#endif // PROJECT_GLOBAL_VARIABLES
