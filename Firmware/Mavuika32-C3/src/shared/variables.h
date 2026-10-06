#if !defined(PROJECT_GLOBAL_VARIABLES)
#define PROJECT_GLOBAL_VARIABLES

//* ---------------------- NETWORK IDENTITY ----------------------
//? The name this board answers to on the LAN. Three separate advertisements have
//? to agree on it, so it is defined once, here, and referenced everywhere else:
//?   1. the DHCP hostname the router's client list and its DNS record show
//?      (WiFiManager.setHostname, src/services/wifi_settings.h);
//?   2. the mDNS responder that actually answers <name>.local
//?      (ArduinoOTA in setup_ESP32, and setup_webpage);
//?   3. the OTA target name listed by the Arduino IDE / espota.
//! The mDNS responder can only be initialised ONCE per boot: a second MDNS.begin()
//! calls mdns_init() again, which fails with ESP_ERR_INVALID_STATE and returns
//! false. Whichever name reaches the responder FIRST is therefore the only name
//! that ever resolves, and any mismatch silently pins the board to the wrong
//! .local name. Never hardcode a hostname at one of those sites.
#define NETWORK_HOSTNAME "lightpack"

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
struct_ledstrip majora_strip;

uint32_t adcReading = 0;
uint32_t adcVoltage = 0;
#define MAX_ADC_SAMPLES 8
#define OVERSAMPLING_RESOLUTION 3 // 2^3 = 8 samples

typedef struct buttonState
{
    bool currentState = LOW; // Last confirmed debounced state
    bool lastState = LOW;    // Previous raw reading (used for debounce)
    uint32_t lastDebounceTime = 0;
    uint32_t debounceDelay = 50; // ms
    uint8_t counter = 0;
} struct_buttonState;

struct_buttonState button_states;

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

//! A second `buttonState` typedef used to sit here, for a
//! `button_states[4]` array. It was left behind by the move to a single
//! debounced button: check_button() (services/readings.h) reads
//! `button_states.counter` on the scalar instance declared above, and nothing
//! anywhere indexes the array. Keeping both was a redefinition error, so the
//! array version is gone.

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
//? The three *_gain fields are intensity scalers applied as each LED is drawn.
//? They re-tint the flame - a green or amber one instead of the magenta - with no
//? reflash and without disturbing the brightness maths above them. A gain is a
//? percentage: 100 is unity.

typedef struct flameSettings
{
    float decay = 0.90f;      // Fraction of its brightness each LED keeps per frame, at LED 0
    float spatial = 2.0f;     // Extra decay each frame at the far end, spread across the strip
    float power_slope = 5.0f; // Brightness gained per unit of magnitude
    uint8_t power_max = 75;   // Ceiling on the flash brightness, 0-255
    uint8_t power_idle = 5;   // Brightness the strip rests at, 0-255
    uint8_t red_gain = 100;   // Intensity scaler, per cent, applied to the red channel
    uint8_t green_gain = 0;   // Intensity scaler, per cent, applied to the green channel
    uint8_t blue_gain = 100;  // Intensity scaler, per cent, applied to the blue channel
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
