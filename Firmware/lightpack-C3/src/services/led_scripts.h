#if !defined(SERVICE_LED_SCRIPTS)
#define SERVICE_LED_SCRIPTS

#include "shared/dependencies.h"

void update_onboard_LED();
void update_strip(int mode);
void flame_steps();
void flame_sanitize();
uint8_t flame_power_for(float magnitude);
uint8_t flame_shade(uint8_t power, uint8_t channel);
void fade_strip();
void constant_brightness();
void strip_sanitize();
uint16_t strip_leds();
void pulse_whip();
void whip_sanitize();
uint16_t whip_travel_ms();
uint8_t whip_in_flight();
void rainbow_flow();
void rainbow_sanitize();
uint16_t rainbow_next_gap_ms();
uint16_t rainbow_travel_ms();
void rainbow_hue_to_rgb(uint16_t hue, uint8_t &red, uint8_t &green, uint8_t &blue);
void strip_channel_test();
void strip_report_config();

//* Frame period of the strip service. update_strip() gates on this and
//* pulse_whip() derives its crossing time from it, so the two cannot drift
//* apart if the cadence is ever retuned.
#define LED_SCRIPT_FRAME_MS 50

//* Strip size. STRIP_LEDS_MIN of 1 rather than 0: the active count is used as a
//* loop bound and in travel-time divisions, and a zero-length strip would make
//* the tail-clear in update_strip() walk the whole buffer on every frame.
#define STRIP_LEDS_MIN 1

//* Step flame limits. FLAME_DECAY_MAX stays below 1.0 deliberately: a decay of
//* 1.0 or more means nothing ever fades, so one step would leave the strip
//* burning forever and every later step would be invisible behind it.
#define FLAME_DECAY_MIN 0.0f
#define FLAME_DECAY_MAX 0.995f
//* Spatial decay is the extra decay applied across the strip, as an exponent on
//* top of `decay`: the far end keeps decay^(1 + spatial) per frame while LED 0
//* keeps decay. 0 fades the whole strip together; the top of the range is far
//* enough that the far end is dark before the near end has visibly moved.
#define FLAME_SPATIAL_MIN 0.0f
#define FLAME_SPATIAL_MAX 10.0f
#define FLAME_POWER_SLOPE_MAX 25.0f

//* Pulse whip limits. WHIP_MAX_HEADS caps how many whips may share the strip;
//* launches that find no free head are dropped rather than queued.
#define WHIP_MAX_HEADS 12
#define WHIP_SPEED_MIN 0.1f
#define WHIP_SPEED_MAX 20.0f
#define WHIP_LENGTH_MIN 1
#define WHIP_PERIOD_MIN 200
#define WHIP_PERIOD_MAX 5000

//* Rainbow flow limits. The gap bounds are the [min_tick]..[max_tick] window of
//* the original request, expressed in ms; RAINBOW_GAP_MAX_MS must stay within
//* uint16_t, since the drawn gap is stored as one.
#define RAINBOW_SPEED_MIN 0.1f
#define RAINBOW_SPEED_MAX 20.0f
#define RAINBOW_GAP_MIN_MS 50
#define RAINBOW_GAP_MAX_MS 10000

//* ---------------------- STRIP SIZE ----------------------
//? See the note on `active_leds` in shared/variables.h: the NeoPixel buffer is
//? fixed at NUM_LEDS bytes, and these two functions are how a mode finds out how
//? much of it to use.

uint16_t strip_leds()
{
    //? The number of LEDs the modes should light. strip_sanitize() guarantees
    //? 1..NUM_LEDS, so a caller can use the result as a loop bound without a
    //? floor of its own. Unlike NUM_LEDS this is what the user actually asked
    //! for - reading NUM_LEDS anywhere in a render loop is the bug to avoid.
    return strip_settings.active_leds;
}

void strip_sanitize()
{
    //! The floor is 1, not 0. Several derived quantities divide by the strip
    //! length, and the tail-clear in update_strip() would otherwise repaint the
    //! entire buffer on every frame.
    if (strip_settings.active_leds < STRIP_LEDS_MIN)
        strip_settings.active_leds = STRIP_LEDS_MIN;
    if (strip_settings.active_leds > NUM_LEDS)
        strip_settings.active_leds = NUM_LEDS;

    //* Anything measured in LEDs has to follow the strip, or a shortened strip
    //* would keep a whip length that cannot be drawn. The rule is uniform:
    //! shortening the strip pulls it down, and raising it again does not bring
    //! it back - the same contract whip.length always had. whip.length is a
    //! uint8_t, so this clamp also caps it at 255 whatever NUM_LEDS becomes.
    if (whip.length > strip_settings.active_leds)
        whip.length = (uint8_t)strip_settings.active_leds;
}

//* ---------------------- MODE REGISTRY ----------------------
//? Single source of truth for the selectable LED scripts.
//? The webpage service enumerates this table to build its mode picker,
//? so adding an entry below makes the mode available over HTTP with no
//? change to services/webpage.h.

typedef void (*led_mode_fn)();

typedef struct ledMode
{
    uint8_t id;        // Stable numeric id, persisted to NVS
    const char *key;   // Stable slug for the web API
    const char *label; // Human readable name shown in the UI
    led_mode_fn run;   // Frame function, gated by update_strip()
} struct_ledMode;

const struct_ledMode *find_led_mode(uint8_t id);
const struct_ledMode *find_led_mode_by_key(const char *key);
const struct_ledMode *led_mode_at(uint8_t index);
uint8_t led_mode_count();

//* Select the next entry in the table above, wrapping at the end. This is what
//* the user button drives (see check_button() in services/readings.h): a 1 s
//* press walks the strip to the next script without a phone in hand.
void led_mode_next();

//* Persistence lives in services/webpage.h, which is included after this header
//* (see shared/dependencies.h). A mode picked with the button has to survive a
//* reboot exactly like one picked on the page, so this one name is declared
//* ahead of its definition - the same arrangement services/webpage.h uses for
//* the preset handlers.
void webpage_config_save();

//* Show one colour on the onboard pixel for a moment, over the top of the steady
//? display. Used by the network transitions in services/wifi_settings.h to mark a
//* state CHANGE; the blink below is what reports the state it settled into.
void led_notice(uint8_t r, uint8_t g, uint8_t b, uint16_t ms);

void led_notice(uint8_t r, uint8_t g, uint8_t b, uint16_t ms)
{
    led_notice_r = r;
    led_notice_g = g;
    led_notice_b = b;
    led_notice_until_ms = millis() + ms;
    onboard_led.setPixelColor(0, strip.Color(r, g, b));
    onboard_led.show();
}

//* ---------------------- ONBOARD LED ----------------------
//? What the single onboard pixel is for. It used to mirror the gyro, which looked
//? like nothing: the channel assignment was `abs(g.gyro.x)` straight into a
//? uint8_t, and the gyro sits at 0.01-0.3 rad/s at rest, so every channel
//! truncated to 0 and the pixel was black at all times. It is a status light now:
//* one blink per period, whose colour is the network state.
//* 1 = the network state blink; 0 = the old gyro display.
#define ONBOARD_LED_NETWORK_BLINK 1

//* One blink every LED_BLINK_PERIOD_MS, lit for LED_BLINK_ON_MS.
//? 200 ms out of 5 s catches the eye in peripheral vision and leaves the pixel
//? dark 96% of the time, which is what makes it read as a signal rather than as a
//* faint glow you have to check for.
#define LED_BLINK_PERIOD_MS 5000
#define LED_BLINK_ON_MS 200

//? Shared with the transition notices above, so a state change and the steady
//* state that follows it are the same brightness.
#define LED_BLINK_LEVEL 45

//* ---------------------- BUTTON FEEDBACK ----------------------
//? Solid colours that report the two switches directly, so a press can be seen on
//? the board without a serial console attached. They exist because the onboard
//* pixel is otherwise only ever the network blink, which says nothing about input.
//! Level, not the classified press: this reads the PIN, so it lights for any touch
//? at all - including a 20 ms tap that is too short to become a BUTTON_EVENT_SHORT
//* and would therefore look like a dead button with nothing on screen to say why.
//? Notices (the recovery countdown, the portal warning) still own the pixel, so
//* this is checked AFTER them and never hides a countdown.
#define ONBOARD_LED_BUTTON_FEEDBACK 1
#define BUTTON_LED_TOUCH_R 0 // green: GPIO3 is HIGH, the touch net is asserted
#define BUTTON_LED_TOUCH_G 10
#define BUTTON_LED_TOUCH_B 0
#define BUTTON_LED_BOOT_R 7 // purple: GPIO9 is held (active LOW)
#define BUTTON_LED_BOOT_G 0
#define BUTTON_LED_BOOT_B 7

//* Colour: green joined, cyan not joined, red radio off. One blink, then dark.
void update_onboard_LED()
{
    //* A notice owns the pixel until its deadline. Checked before the repaint
    //? gate below, so a notice is drawn on the very next pass rather than up to
    //* 5 ms later.
    if (led_notice_until_ms != 0)
    {
        if ((int32_t)(millis() - led_notice_until_ms) < 0)
            return;
        led_notice_until_ms = 0; // Expired: fall through and let the steady display retake it
    }

    if (millis() - status_led.update_time < 5)
        return;
    status_led.update_time = millis();

#if ONBOARD_LED_BUTTON_FEEDBACK
    //* Switch state outranks the blink: while a switch is asserted the pixel is a
    //? level indicator rather than a status report. The boot button is tested
    //* second so a touch and a BOOT hold at once resolve to the touch net.
    if (button_is_down(touch_button))
    {
        onboard_led.setPixelColor(0, strip.Color(BUTTON_LED_TOUCH_R, BUTTON_LED_TOUCH_G, BUTTON_LED_TOUCH_B));
        onboard_led.show();
        return;
    }
    if (button_is_down(boot_button))
    {
        //* Holds act on RELEASE (a reset with GPIO9 still held lands in the ROM's
        //! download mode), so without a progress colour the user gets no feedback
        //? at all while holding - they cannot tell 2 s from 5 s. These three
        //* colours are that feedback: they change AT each threshold, so releasing
        //? now would do exactly what the colour already announces.
        //? Purple below 3 s, amber from 3 s (radio), blue from 6 s (hotspot).
        uint32_t held = 0;
        if (boot_button.pressed)
            held = millis() - boot_button.press_start_ms;

        uint8_t r = BUTTON_LED_BOOT_R;
        uint8_t g = BUTTON_LED_BOOT_G;
        uint8_t b = BUTTON_LED_BOOT_B;

        if (held >= BUTTON_PORTAL_HOLD_MS)
        {
            r = 0;
            g = 16;
            b = 25;
        }
        else if (held >= BUTTON_WIFI_HOLD_MS)
        {
            r = 25;
            g = 15;
            b = 0;
        }

        onboard_led.setPixelColor(0, strip.Color(r, g, b));
        onboard_led.show();
        return;
    }
#endif

#if ONBOARD_LED_NETWORK_BLINK
    //* millis() % PERIOD is the position in the current blink, so the pixel is lit
    //? for the first LED_BLINK_ON_MS of every period and dark for the remainder.
    const bool lit = (millis() % LED_BLINK_PERIOD_MS) < LED_BLINK_ON_MS;

    status_led.red = 0;
    status_led.green = 0;
    status_led.blue = 0;

    if (lit)
    {
        switch (net_state)
        {
        case NET_CONNECTED:
            status_led.green = LED_BLINK_LEVEL;
            break;
        case NET_CONNECTING:
            //? Green plus blue: cyan, the "radio is up but nothing is joined yet"
            //* state, which is the one a user is most likely to be staring at.
            status_led.green = LED_BLINK_LEVEL;
            status_led.blue = LED_BLINK_LEVEL;
            break;
        default: // NET_OFFLINE
            status_led.red = LED_BLINK_LEVEL;
            break;
        }
    }
#else
    //* The old gyro display, kept behind the flag above. Note that these values
    //! truncate to 0 for any realistic reading - see the note over the flag.
    status_led.red = abs(g.gyro.x);
    status_led.green = abs(g.gyro.y);
    status_led.blue = abs(g.gyro.z);
#endif

    onboard_led.setPixelColor(0, strip.Color(status_led.red, status_led.green, status_led.blue));
    onboard_led.show();
    /*
    Serial.printf(">Gyro_X: %.2f \n", g.gyro.x);
    Serial.printf(">Gyro_Y: %.2f \n", g.gyro.y);
    Serial.printf(">Gyro_Z: %.2f \n", g.gyro.z);
    Serial.printf(">Accel_X: %.2f \n", a.acceleration.x);
    Serial.printf(">Accel_Y: %.2f \n", a.acceleration.y);
    Serial.printf(">Accel_Z: %.2f \n", a.acceleration.z);
    */
}

//* ---------------------- STEP FLAME (mode 2) ----------------------
//? A step lights the whole strip at a brightness set by the impact, then every
//? LED decays its own brightness towards the idle floor. The decay is worse the
//? further an LED sits from the start of the strip, so the far end goes dark
//? first and the flame reads as retreating towards LED 0. Every knob is exposed
//? on the configuration page; see the comment on struct_flameSettings in
//? shared/variables.h for what each one means.
//!
//! The per-LED level is the mode's state, held here rather than recomputed from a
//! timestamp: two steps that overlap have to brighten each other, and a
//! closed-form decay cannot express "whichever of these two flashes is still the
//! brighter" without carrying both of them.

void flame_sanitize()
{
    //? Mirrors whip_sanitize(): each check either bounds a value used in the
    //? render maths or keeps a lit count inside the strip. Called on NVS load and
    //! on every web write. The strip-dependent clamps for the other modes live in
    //! strip_sanitize(), which must run BEFORE this one.
    if (flame.decay < FLAME_DECAY_MIN)
        flame.decay = FLAME_DECAY_MIN;
    if (flame.decay > FLAME_DECAY_MAX)
        flame.decay = FLAME_DECAY_MAX;

    //! A negative spatial decay would make the far end brighter than the near end
    //! and grow instead of fading - the flame would expand away from LED 0.
    if (flame.spatial < FLAME_SPATIAL_MIN)
        flame.spatial = FLAME_SPATIAL_MIN;
    if (flame.spatial > FLAME_SPATIAL_MAX)
        flame.spatial = FLAME_SPATIAL_MAX;

    if (flame.power_slope < 0.0f)
        flame.power_slope = 0.0f;
    if (flame.power_slope > FLAME_POWER_SLOPE_MAX)
        flame.power_slope = FLAME_POWER_SLOPE_MAX;

    //* power_max, power_idle and the three gains are uint8_t, so 0..255 is
    //* already enforced by the type.
}

uint8_t flame_power_for(float magnitude)
{
    //? Flash brightness: how bright the whole strip lights when a step of this
    //? magnitude arrives, capped by the user's ceiling. The 255 clamp is not
    //! redundant: power_slope has a slider whose top end can ask for more than a
    //! channel can hold with the hardest step.
    if (magnitude < 0.0f)
        magnitude = 0.0f;

    float power = flame.power_slope * magnitude;
    if (power > (float)flame.power_max)
        power = (float)flame.power_max;
    if (power > 255.0f)
        power = 255.0f;
    return (uint8_t)power;
}

//* One channel of the flame: the brightness this LED's step maths arrived at,
//? scaled by how much of that channel the PALETTE colour asks for.
//? 255 in a channel means "as much as the step asked for", so a white palette
//* leaves the brightness maths exactly as it was and a saturated one simply
//? removes the other channels. Kept in one function so the three channels cannot
//* drift apart, which is what a per-channel gain used to guard against here.
uint8_t flame_shade(uint8_t power, uint8_t channel)
{
    return (uint8_t)(((uint16_t)power * (uint16_t)channel) / 255);
}

void flame_steps()
{
    //! Do not gate on led_strip.update_time here. update_strip() has already
    //! rate-limited on that field and stamped it with the current millis(), so
    //! a check here compares against ~0 and returns on every frame. That is
    //! exactly what this mode used to do, which left it rendering nothing.
    const uint16_t leds = strip_leds();

    //* One brightness per LED, in 0..255. Static and zero-initialised: the decay
    //* below floors every entry it touches at power_idle, so the strip primes
    //* itself to the idle level on the first frame without a separate init pass,
    //* and LEDs revealed by a later increase of active_leds arrive at idle too.
    static float level[NUM_LEDS];

    //* 1. Ignite. A step lights the WHOLE strip at once, at a brightness
    //* proportional to the impact. Taken as a maximum rather than an assignment so
    //! a step landing mid-decay brightens the strip instead of restarting it
    //! dimmer than it already was.
    //? The detector is polled from loop() (services/gyros.h), not from here: it runs
    //? whether or not this mode is selected, and impact_take() hands over the
    //! strongest impact accepted since the last frame.
    const uint8_t detected = impact_take();
    if (detected > 0)
    {
        const uint8_t flash = flame_power_for((float)detected);
        for (uint16_t i = 0; i < leds; i++)
        {
            if (level[i] < (float)flash)
                level[i] = (float)flash;
        }
        Serial.printf("[FLAME] step: magnitude=%u flash=%u\n", detected, flash);
    }

    //* 2. Decay, worst at the far end. LED i keeps decay^(1 + spatial * i/(leds-1)),
    //* which is decay * (decay^(spatial/(leds-1)))^i. Building the exponent with
    //* one multiply per LED off a single powf per frame is what keeps this off the
    //* hot path: the direct form would be a powf for every LED, every frame.
    const float rel_step = (leds > 1) ? (flame.spatial / (float)(leds - 1)) : 0.0f;
    const float keep_step = powf(flame.decay, rel_step);
    const float idle = (float)flame.power_idle;

    float keep = flame.decay; // LED 0, which decays slowest
    //! Decays the whole buffer, not just the active LEDs. Entries above
    //! active_leds are not drawn, but they are still state: decaying them keeps a
    //! strip that is shortened and later lengthened from revealing the brightness
    //! an LED happened to be at before it went dark.
    for (uint16_t i = 0; i < NUM_LEDS; i++)
    {
        level[i] *= keep;
        //! The floor is applied to the stored value, not only at draw time, so a
        //! settled strip holds a flat idle field rather than an exponential that
        //! approaches it forever and keeps re-drawing ever-smaller changes.
        if (level[i] < idle)
            level[i] = idle;
        keep *= keep_step;
    }

    //* 3. Draw, in the palette colour. `power` is how bright this LED is; the
    //? palette is what colour that brightness is, so the mode follows the colour
    //* picked on the page like every other mode does.
    //! led_strip.* stays this mode's scratch colour - see the note in
    //* shared/variables.h about never keeping the palette there.
    for (uint16_t i = 0; i < leds; i++)
    {
        float value = level[i];
        if (value > 255.0f)
            value = 255.0f;
        const uint8_t power = (uint8_t)value;

        led_strip.red = flame_shade(power, palette.red);
        led_strip.green = flame_shade(power, palette.green);
        led_strip.blue = flame_shade(power, palette.blue);

        strip.setPixelColor(i, strip.Color(led_strip.red, led_strip.green, led_strip.blue));
    }
    strip.show();
}

void fade_strip()

{
    max_gyro_x *= 0.9; // Decay over time
    max_gyro_y *= 0.9;
    max_gyro_z *= 0.9;
    // Update hold values
    max_gyro_x = max(max_gyro_x, abs(g.gyro.x));
    max_gyro_y = max(max_gyro_y, abs(g.gyro.y));
    max_gyro_z = max(max_gyro_z, abs(g.gyro.z));

    if (max_gyro_z > 0.2)
    {
        lenght = 20 * max_gyro_x + 20 * max_gyro_y + 15 * max_gyro_z + 10;
        power = 2 * max_gyro_x + 2 * max_gyro_y + 5;
        //! led_strip.* here is per-frame scratch owned by this mode. It is NOT
        //! the user's palette; keep the palette in `palette` (variables.h).
        led_strip.red = max_gyro_x * 7;
        led_strip.green = max_gyro_y * 7;
        led_strip.blue = max_gyro_z * 7;
    }
    else
    {
        lenght = 5; // Dim when stationary
        power = 5;  // Dim when stationary
    }

    for (int i = 0; i < lenght; i++)
    {
        strip.setPixelColor(i, strip.Color(led_strip.red, led_strip.green, led_strip.blue)); // Purple
    }
    for (int i = lenght; i < strip_leds(); i++)
    {
        strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
    }
    strip.show();
    return;
    //* Sine wave brightness
    /*
    strip.setPixelColor(position, strip.Color(power, 0, 0)); // Red
    strip.show();
    position++;
    position %= NUM_LEDS;
    */
}

void constant_brightness()
{
    //? Renders the palette the user picked, not led_strip.* (which is scratch
    //? that the gyro modes own - see the note in variables.h).
    for (int i = 0; i < strip_leds(); i++)
    {
        strip.setPixelColor(i, strip.Color(palette.red, palette.green, palette.blue));
    }
    strip.show();
}

//* ---------------------- PULSE WHIP (mode 5) ----------------------
//? A half-sine hump of light that leaves position 0 and travels toward the far
//? end of the strip. Each launch creates an independent "head" whose leading
//? edge marches outward at `speed` LEDs per frame; behind it the intensity
//? rolls off as peak * sin(pi * u), u being the normalised distance back from
//? the leading edge, so a head lights a window `length` LEDs wide.
//?
//? Because `period` can be shorter than the time a whip needs to cross, several
//? heads coexist and their contributions sum. Set period above the crossing
//? time for discrete, separated pulses.

void whip_sanitize()
{
    //? Every range check here exists to protect the maths in pulse_whip():
    //? length and speed are divisors, and a zero period would try to launch a
    //? whip on every frame. Called on NVS load and on every web write.
    if (whip.speed < WHIP_SPEED_MIN)
        whip.speed = WHIP_SPEED_MIN;
    if (whip.speed > WHIP_SPEED_MAX)
        whip.speed = WHIP_SPEED_MAX;

    if (whip.length < WHIP_LENGTH_MIN)
        whip.length = WHIP_LENGTH_MIN;
    //? Against the LIVE strip, not NUM_LEDS: a hump wider than the number of LEDs
    //! the user lights can never be drawn, and the page would offer a length the
    //! firmware then clamps away.
    if (whip.length > strip_leds())
        whip.length = (uint8_t)strip_leds();

    if (whip.period < WHIP_PERIOD_MIN)
        whip.period = WHIP_PERIOD_MIN;
    if (whip.period > WHIP_PERIOD_MAX)
        whip.period = WHIP_PERIOD_MAX;

    //* peak and offset are uint8_t, so 0-255 is already enforced by the type.
}

uint16_t whip_travel_ms()
{
    //? How long one whip takes to clear the strip at the current speed. This is
    //? the same distance pulse_whip() retires on, so the two stay consistent.
    float frames = ((float)strip_leds() + (float)whip.length) / whip.speed;
    float ms = frames * (float)LED_SCRIPT_FRAME_MS;
    if (ms > 65535.0f)
        ms = 65535.0f;
    return (uint16_t)ms;
}

uint8_t whip_in_flight()
{
    //? Expected number of whips on the strip at once. Reported on the
    //? configuration page so an over-aggressive period is visible rather than
    //? mysterious, and used there to warn when launches start being dropped.
    uint16_t travel = whip_travel_ms();
    int count = (travel + whip.period - 1) / whip.period; // Ceiling divide
    if (count < 1)
        count = 1;
    if (count > WHIP_MAX_HEADS)
        count = WHIP_MAX_HEADS;
    return (uint8_t)count;
}

void pulse_whip()
{
    //? Called once per update_strip() tick, i.e. every LED_SCRIPT_FRAME_MS.
    //? Heads advance a fixed number of LEDs per call, so motion is deliberately
    //? frame-rate coupled: if the main loop stalls the whip visibly slows
    //? instead of jumping. The launch rhythm is wall-clock so it cannot drift.

    static struct
    {
        bool live;
        float pos; // Leading edge, in LEDs
    } heads[WHIP_MAX_HEADS];

    static uint32_t last_launch = 0;
    static bool launched_once = false;
    static uint32_t last_frame = 0;
    static bool primed = false;

    const float kPi = 3.14159265358979f;
    const float inv_length = 1.0f / (float)whip.length; // whip_sanitize() keeps length >= 1
    const uint32_t now = millis();

    //* 0. Heads keep their position in statics, so after a spell in another mode
    //* they would resume mid-flight and read as a glitch. The same guard covers
    //* a stalled loop. Both cases restart cleanly instead of teleporting.
    //! `primed` rather than a last_frame == 0 sentinel: millis() can legitimately
    //! be 0 on the first call and would then reset on every subsequent frame.
    if (!primed || (now - last_frame) > (uint32_t)(LED_SCRIPT_FRAME_MS * 10))
    {
        for (uint8_t h = 0; h < WHIP_MAX_HEADS; h++)
            heads[h].live = false;
        launched_once = false;
        primed = true;
    }
    last_frame = now;

    //* 1. Launch. The first call always fires so the strip starts lit rather
    //* than waiting one full period.
    if (!launched_once || (now - last_launch) >= (uint32_t)whip.period)
    {
        launched_once = true;
        last_launch = now;

        for (uint8_t h = 0; h < WHIP_MAX_HEADS; h++)
        {
            if (heads[h].live)
                continue;
            heads[h].live = true;
            heads[h].pos = 0.0f;
            //! No free head => this launch is dropped. whip_in_flight() reports
            //! the pressure so the page can tell the user to raise the period.
            break;
        }
    }

    //* 2. Advance, then retire any head whose tail has left the strip.
    for (uint8_t h = 0; h < WHIP_MAX_HEADS; h++)
    {
        if (!heads[h].live)
            continue;
        heads[h].pos += whip.speed;
        if (heads[h].pos - (float)whip.length >= (float)strip_leds())
            heads[h].live = false;
    }

    //* 3. Composite: the baseline offset, plus every hump covering each pixel.
    for (int i = 0; i < strip_leds(); i++)
    {
        uint16_t intensity = whip.offset;

        for (uint8_t h = 0; h < WHIP_MAX_HEADS; h++)
        {
            if (!heads[h].live)
                continue;

            const float u = (heads[h].pos - (float)i) * inv_length; // 0 at the head, 1 at the tail
            if (u <= 0.0f || u >= 1.0f)
                continue; // Outside this head's window

            intensity += (uint16_t)((float)whip.peak * sinf(kPi * u));
        }

        if (intensity > 255)
            intensity = 255; // Overlapping whips saturate instead of wrapping

        //* The hump is tinted with the user's palette colour.
        strip.setPixelColor(i, strip.Color(
                                   (uint8_t)(((uint16_t)palette.red * intensity) / 255),
                                   (uint8_t)(((uint16_t)palette.green * intensity) / 255),
                                   (uint8_t)(((uint16_t)palette.blue * intensity) / 255)));
    }

    strip.show();
}

//* ---------------------- RAINBOW FLOW (mode 6) ----------------------
//? LED 0 is the "starting LED". Every `min_ms`..`max_ms` it is handed a fresh
//? random colour, and each LED takes the colour of its upstream neighbour, so the
//? drawn colour is laid down at the start and marches toward the far end at
//? `speed` LEDs per frame. The strip therefore shows one flat band of colour per
//? draw, and the bands are as long as the distance the flow covers between two
//? draws: speed * gap.
//?
//! The bands are deliberately FLAT - no gradient is superimposed on them. An
//? earlier revision rendered a full colour wheel across the strip and slid that,
//? which reads as a rainbow, but it welds the two knobs together: the wheel is
//! periodic with the strip, so any LED cycles through every hue once per
//! NUM_LEDS / speed frames. Changing the speed then changed the colour-change
//? rate (3.75 s per cycle at speed 2, 0.375 s at speed 20) no matter what the
//? wait was set to. With flat bands, a LED changes colour exactly once per draw -
//! every min_ms..max_ms, whatever the speed is - and `speed` only decides how far
//? a colour gets and so how many colours share the strip.
//?
//? As in pulse_whip(): the flow is frame-rate coupled (a stalled loop visibly
//? slows the march instead of jumping it), while the draw rhythm is wall-clock so
//? it cannot drift.

void rainbow_sanitize()
{
    //? Mirrors whip_sanitize(): every check either protects the render maths or
    //? keeps the gap draw non-empty. Called on NVS load and on every web write.
    if (rainbow.speed < RAINBOW_SPEED_MIN)
        rainbow.speed = RAINBOW_SPEED_MIN;
    if (rainbow.speed > RAINBOW_SPEED_MAX)
        rainbow.speed = RAINBOW_SPEED_MAX;

    if (rainbow.min_ms < RAINBOW_GAP_MIN_MS)
        rainbow.min_ms = RAINBOW_GAP_MIN_MS;
    if (rainbow.min_ms > RAINBOW_GAP_MAX_MS)
        rainbow.min_ms = RAINBOW_GAP_MAX_MS;

    if (rainbow.max_ms < RAINBOW_GAP_MIN_MS)
        rainbow.max_ms = RAINBOW_GAP_MIN_MS;
    if (rainbow.max_ms > RAINBOW_GAP_MAX_MS)
        rainbow.max_ms = RAINBOW_GAP_MAX_MS;

    //! The gap is drawn as min + random(max - min + 1), so an inverted pair would
    //! ask for a negative range. Pulling max up to min keeps the draw valid and
    //! simply makes the colour change perfectly regular.
    if (rainbow.max_ms < rainbow.min_ms)
        rainbow.max_ms = rainbow.min_ms;
}

uint16_t rainbow_next_gap_ms()
{
    //? Uniform over [min_ms, max_ms] inclusive. rainbow_sanitize() has already
    //? guaranteed max >= min, so the +1 can never create an empty range.
    return (uint16_t)random((long)rainbow.min_ms, (long)rainbow.max_ms + 1);
}

uint16_t rainbow_travel_ms()
{
    //? How long a newly drawn colour takes to reach the far end of the strip at
    //? the current speed - the same relationship whip_travel_ms() reports.
    float frames = (float)strip_leds() / rainbow.speed;
    float ms = frames * (float)LED_SCRIPT_FRAME_MS;
    if (ms > 65535.0f)
        ms = 65535.0f;
    return (uint16_t)ms;
}

uint16_t rainbow_band_leds(uint16_t gap_ms)
{
    //? How many LEDs one drawn colour covers before the next one is laid down:
    //? the distance the flow travels during that colour's gap. Reported on the
    //? configuration page so the speed and the wait can be related to what the
    //! strip will actually look like - a band shorter than one LED cannot be
    //! resolved by a strip that moves in whole LEDs.
    float frames = (float)gap_ms / (float)LED_SCRIPT_FRAME_MS;
    float leds = frames * rainbow.speed;
    if (leds > 65535.0f)
        leds = 65535.0f;
    return (uint16_t)leds;
}

void rainbow_hue_to_rgb(uint16_t hue, uint8_t &red, uint8_t &green, uint8_t &blue)
{
    //? Full saturation, full value, six 60-degree ramps. The drawn colour is a
    //? 16-bit wheel position, mapped onto a 0..1535 scale (6 segments of 256)
    //! rather than the usual 0..255 so each channel ramp keeps all 8 bits - on a
    //! 0..255 wheel the ramps would step three units at a time and the bands
    //! would quantise visibly when a colour is drawn next to a similar one.
    const uint16_t pos = (uint16_t)(((uint32_t)hue * 1536UL) >> 16); // 0..1535
    const uint8_t segment = (uint8_t)(pos >> 8);                     // 0..5
    const uint8_t ramp = (uint8_t)(pos & 0xFF);                      // 0..255
    const uint8_t down = (uint8_t)(255 - ramp);

    switch (segment)
    {
    case 0: // Red -> yellow
        red = 255;
        green = ramp;
        blue = 0;
        break;
    case 1: // Yellow -> green
        red = down;
        green = 255;
        blue = 0;
        break;
    case 2: // Green -> cyan
        red = 0;
        green = 255;
        blue = ramp;
        break;
    case 3: // Cyan -> blue
        red = 0;
        green = down;
        blue = 255;
        break;
    case 4: // Blue -> magenta
        red = ramp;
        green = 0;
        blue = 255;
        break;
    default: // Magenta -> red
        red = 255;
        green = 0;
        blue = down;
        break;
    }
}

void rainbow_flow()
{
    //? Called once per update_strip() tick, i.e. every LED_SCRIPT_FRAME_MS.
    //! Sized by NUM_LEDS, the BUFFER size, not by the live strip: the array is
    //! static and its extent is fixed at compile time. Only the entries below
    //! strip_leds() are ever shifted, drawn or read back.
    static uint16_t hues[NUM_LEDS]; // Colour of the band sitting on each LED
    static uint16_t head = 0;       // Colour currently being laid down at LED 0
    static float carry = 0.0f;      // Sub-LED travel owed to the next whole shift
    static uint32_t last_frame = 0;
    static uint32_t last_pick = 0;
    static uint16_t gap = RAINBOW_GAP_MIN_MS;
    static bool primed = false;
    static uint16_t primed_leds = 0; // Strip length the pattern above was built for

    const uint32_t now = millis();
    const uint16_t leds = strip_leds();

    //* 0. Entry / recovery reset, as in pulse_whip(): the array is static, so
    //* after a spell in another mode it would resume a stale pattern (and after a
    //* stall, a stale phase). Start flat in one colour rather than lit with
    //* whatever the previous mode left behind.
    //? A change of strip length re-primes too. The shift below moves colours
    //? between neighbouring LEDs, so extending the strip would otherwise reveal
    //! old hues above the previous end, and shortening it would splice an unseen
    //! gap into the middle of the pattern.
    if (!primed || primed_leds != leds || (now - last_frame) > (uint32_t)(LED_SCRIPT_FRAME_MS * 10))
    {
        head = (uint16_t)random(65536L);
        for (int i = 0; i < NUM_LEDS; i++)
            hues[i] = head;
        carry = 0.0f;
        last_pick = now;
        gap = rainbow_next_gap_ms();
        primed = true;
        primed_leds = leds;
    }
    last_frame = now;

    //* 1. Flow. A frame can cover less than one LED, so the fraction is carried
    //* into the next frame; motion stays smooth at the bottom of the speed range
    //! instead of quantising to whole LEDs. Bounded by RAINBOW_SPEED_MAX shifts
    //! per frame, so this cannot spin even if a caller skips rainbow_sanitize().
    //* Refilling the head with the current colour each shift is what makes a band
    //* lengthen as the flow carries it away from the start of the strip.
    carry += rainbow.speed;
    while (carry >= 1.0f)
    {
        carry -= 1.0f;
        for (int i = leds - 1; i > 0; i--)
            hues[i] = hues[i - 1];
        hues[0] = head;
    }

    //* 2. Draw. Done after the shift so the freshly drawn colour sits at LED 0
    //* for the rest of this frame before the flow carries it along - it is the
    //* only pixel that changes on the spot.
    //! The gap is wall-clock and independent of `speed`: this is the only thing
    //! that decides how often any LED on the strip changes colour, and it must
    //! stay that way (see the header comment).
    if ((uint32_t)(now - last_pick) >= (uint32_t)gap)
    {
        head = (uint16_t)random(65536L);
        hues[0] = head;
        last_pick = now;
        gap = rainbow_next_gap_ms();
    }

    //* 3. Paint the bands.
    for (int i = 0; i < leds; i++)
    {
        uint8_t red = 0;
        uint8_t green = 0;
        uint8_t blue = 0;
        rainbow_hue_to_rgb(hues[i], red, green, blue);
        strip.setPixelColor(i, strip.Color(red, green, blue));
    }
    strip.show();
}

//* ---------------------- DIAGNOSTICS ----------------------
//? Set to 0 once the strip is known good; it costs ~3 s of boot time.
#define STRIP_SELFTEST_ON_BOOT 1

//? Lights pure red, green and blue in turn, naming each on serial.
//? This deliberately bypasses every mode, every stored setting and the palette,
//? so it answers one question with no ambiguity: can this strip physically show
//? green and blue at all? If only the RED step lights, the fault is wiring,
//? colour order or power - and no firmware setting can fix it.
void strip_channel_test()
{

    //! Neutralise brightness scaling first: a stored brightness of 0 or 1 would
    //! render every channel as black and make a healthy strip look dead.
    strip.setBrightness(50);
    Serial.printf("[STRIP] channel test\n");

    //* Baseline for the sag reported in the boot report: V_REG with the strip
    //? powered but still black. Taken here rather than in main.cpp because this
    //* is the first place that knows it is about to load the rail, and because
    //! these three steps are the only LED current the boot draws before loop().
    boot_rail_baseline(supply_rail_mv());

    for (int i = 0; i < strip_leds(); i++)
        strip.setPixelColor(i, strip.Color(25, 0, 0));
    strip.show();
    delay(500);
    boot_rail_note_step("RED");

    for (int i = 0; i < strip_leds(); i++)
        strip.setPixelColor(i, strip.Color(0, 25, 0));
    strip.show();
    delay(500);
    boot_rail_note_step("GREEN");

    for (int i = 0; i < strip_leds(); i++)
        strip.setPixelColor(i, strip.Color(0, 0, 25));
    strip.show();
    delay(500);
    boot_rail_note_step("BLUE");

    strip.clear();
    strip.show();
    delay(500);

    Serial.println("[STRIP] channel test done.");
}

void strip_report_config()
{
    Serial.printf("[STRIP] mode=%d palette=%u,%u,%u brightness=%u\n",
                  mode, palette.red, palette.green, palette.blue, led_strip.brightness);
    Serial.printf("[STRIP] size=%u of %d LEDs\n", strip_leds(), NUM_LEDS);
    Serial.printf("[STRIP] flame: decay=%.3f spatial=%.2f power=%.1f/mag max=%u idle=%u (drawn in the palette)\n",
                  flame.decay, flame.spatial, flame.power_slope, flame.power_max,
                  flame.power_idle);
    const struct_ledMode *entry = find_led_mode((uint8_t)mode);
    if (entry != nullptr)
        Serial.printf("[STRIP] active mode '%s' (%s)\n", entry->key, entry->label);
    //? Modes that generate their own colours: the palette is inert for them, and
    //? saying so here saves a hunt through the source when it "does nothing".
    if (mode == 2 || mode == 4)
    {
        Serial.println("[STRIP] NOTE: this mode derives its colour from the sensor and");
        Serial.println("[STRIP] NOTE: ignores the palette - changing it will do nothing here.");
    }
    else if (mode == 6)
    {
        Serial.println("[STRIP] NOTE: this mode generates its own wheel and ignores the palette.");
    }
}

//* ---------------------- MODE TABLE ----------------------
//? The ids are STABLE and persisted: the selected mode and every saved preset
//? store the numeric id from this table (see webpage_config_load() and
//? presets.h), so an entry may be removed but the surviving entries must keep
//? the id they already had. The gap below is one such retired mode - do not
//! renumber to close it, or every device in the field silently switches mode
//! and every stored preset starts naming a different script.
//? A mode whose stored id is no longer here is not an error: the config load
//? falls back to the first entry, so an old device boots lit rather than dark.
const struct_ledMode led_modes[] = {
    {1, "solid", "Solid color", constant_brightness},
    {2, "flame", "Step flame", flame_steps},
    {4, "fade", "Gyro fade", fade_strip},
    {5, "whip", "Pulse whip", pulse_whip},
    {6, "rainbow", "Rainbow flow", rainbow_flow},
};

uint8_t led_mode_count()
{
    return (uint8_t)(sizeof(led_modes) / sizeof(led_modes[0]));
}

const struct_ledMode *led_mode_at(uint8_t index)
{
    if (index >= led_mode_count())
        return nullptr;
    return &led_modes[index];
}

const struct_ledMode *find_led_mode(uint8_t id)
{
    for (uint8_t i = 0; i < led_mode_count(); i++)
        if (led_modes[i].id == id)
            return &led_modes[i];
    return nullptr;
}

const struct_ledMode *find_led_mode_by_key(const char *key)
{
    if (key == nullptr)
        return nullptr;
    for (uint8_t i = 0; i < led_mode_count(); i++)
        if (strcmp(led_modes[i].key, key) == 0)
            return &led_modes[i];
    return nullptr;
}

void led_mode_next()
{
    //? Find where the current mode sits in the table, then step one position.
    //! Step by POSITION, never by `mode + 1`: the ids have gaps on purpose (see
    //! the note above the table), so an id-plus-one walk would land on an id that
    //! is not in the registry, update_strip() would refuse to draw it, and the
    //! strip would sit frozen on its last frame.
    uint8_t index = 0;
    const struct_ledMode *current = find_led_mode((uint8_t)mode);
    if (current != nullptr)
        index = (uint8_t)(current - led_modes);

    index = (uint8_t)((index + 1) % led_mode_count());

    const struct_ledMode *entry = led_mode_at(index);
    if (entry == nullptr)
        return; // Empty table: nothing to switch to

    mode = entry->id;
    webpage_config_save();

    Serial.printf("[MODE] Button -> %u (%s)\n", entry->id, entry->key);
}

//* The same walk, one step the other way. Added for the boot button's double
//? click, which is the only caller; it is a separate function rather than a
//* signed argument so neither direction has to reason about unsigned wrap-around.
//? Stepping backwards off the front of the table lands on the LAST entry, so the
//* two directions are exact inverses of each other.
void led_mode_prev()
{
    uint8_t index = 0;
    const struct_ledMode *current = find_led_mode((uint8_t)mode);
    if (current != nullptr)
        index = (uint8_t)(current - led_modes);

    //? `+ count` before the modulo: the count is small enough that the addition
    //! cannot overflow a uint8_t, and doing it this way avoids relying on the
    //* signedness of the intermediate.
    index = (uint8_t)((index + led_mode_count() - 1) % led_mode_count());

    const struct_ledMode *entry = led_mode_at(index);
    if (entry == nullptr)
        return; // Empty table: nothing to switch to

    mode = entry->id;
    webpage_config_save();

    Serial.printf("[MODE] Button -> %u (%s) [backwards]\n", entry->id, entry->key);
}

void update_strip(int mode)
{
    if (millis() - led_strip.update_time < LED_SCRIPT_FRAME_MS)
        return;
    led_strip.update_time = millis();

    //* Push the configured brightness to the NeoPixel drivers only when it
    //* actually changes. Default of 255 (see variables.h) is a no-op, so the
    //* scripts' own colour maths are unaffected until the user dims the strip.
    static uint8_t applied_brightness = 0;
    if (applied_brightness != led_strip.brightness)
    {
        applied_brightness = led_strip.brightness;
        strip.setBrightness(applied_brightness);
    }

    //* Blank the pixels a shortened strip just gave up. This is the ONE place
    //* that walks the full NUM_LEDS buffer, and it has to: the driver still owns
    //* all of them, no mode loops past strip_leds() any more, so without this the
    //* LEDs above the new count would stay frozen in their last colour forever.
    static uint16_t applied_leds = 0;
    const uint16_t leds = strip_leds();
    if (applied_leds != leds)
    {
        if (applied_leds > leds)
        {
            for (int i = leds; i < NUM_LEDS; i++)
                strip.setPixelColor(i, strip.Color(0, 0, 0));
            strip.show();
        }
        applied_leds = leds;
    }

    const struct_ledMode *entry = find_led_mode((uint8_t)mode);
    if (entry == nullptr || entry->run == nullptr)
        return; // Unknown mode: leave the strip as-is
    entry->run();
}

#endif // SERVICE_LED_SCRIPTS
