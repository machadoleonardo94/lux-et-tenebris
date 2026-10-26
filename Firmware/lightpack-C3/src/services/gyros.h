#if !defined(SERVICE_GYRO_READINGS)
#define SERVICE_GYRO_READINGS

#include "shared/dependencies.h"

void impact_poll();
uint8_t impact_take();
uint8_t impact_magnitude(float impact_g);
void step_sanitize();
uint16_t step_cadence_max_sps();

//* Impact detector cadence and window.
//? The detector is polled from loop(), NOT from a mode's render path. It used to be
//? sampled inside flame_steps() at LED_SCRIPT_FRAME_MS (50 ms) and only while the
//? Step Flame mode was selected: 20 Hz, which straddles an impact lasting 10 ms and
//? reports whatever the nearest sample happened to catch.
//! IMPACT_SAMPLE_US has to stay well below the shortest impact the detector is meant
//! to size, and IMPACT_PEEK_US has to outlast the rise of one. 2000 us is 500 Hz.
#define IMPACT_SAMPLE_US 2000
#define IMPACT_PEEK_US 30000

//* Working units. The Adafruit event carries m/s^2 (fillAccelEvent multiplies by
//* SENSORS_GRAVITY_STANDARD), while every setting here and every label on the
//* configuration page is in g. The division happens once, at the input, so
//* `threshold` finally means the 1.2 g its slider has always claimed rather than
//* 1.2 m/s^2 - which is 0.12 g, a factor of 9.8 in the wrong direction.
#define IMPACT_G SENSORS_GRAVITY_STANDARD

//* Impact detection limits. These bound the fields of struct_stepSettings
//* (shared/variables.h); step_sanitize() applies them on NVS load and on every
//* web write, and the configuration page receives them so its sliders cannot ask
//* for values the firmware would only clamp.
//!
//! The magnitude scale itself (STEP_MAGNITUDE_MIN/MAX) lives with the settings
//! block, because changing either end changes what every consumer of
//! impact_poll() sees - see the note in shared/variables.h.
#define STEP_BASELINE_TAU_MIN_MS 100
#define STEP_BASELINE_TAU_MAX_MS 20000
#define STEP_THRESHOLD_MIN 0.05f
#define STEP_THRESHOLD_MAX 20.0f
//* `release` has no explicit ceiling: its ceiling is `threshold`, enforced as a
//* relation in step_sanitize().
#define STEP_RELEASE_MIN 0.01f
#define STEP_JERK_MIN 0.0f
#define STEP_JERK_MAX 500.0f
#define STEP_GAP_MIN_MS 20
#define STEP_GAP_MAX_MS 10000
#define STEP_IMPACT_MIN 0.0f
#define STEP_IMPACT_MAX 50.0f

void step_sanitize()
{
    //? Ranges first, then the relations between them - clamping a field without
    //? re-checking its partner is how a sanitiser lets a division by zero
    //! through. Called on NVS load and on every web write.
    if (step.baseline_tau_ms < STEP_BASELINE_TAU_MIN_MS)
        step.baseline_tau_ms = STEP_BASELINE_TAU_MIN_MS;
    if (step.baseline_tau_ms > STEP_BASELINE_TAU_MAX_MS)
        step.baseline_tau_ms = STEP_BASELINE_TAU_MAX_MS;

    if (step.threshold < STEP_THRESHOLD_MIN)
        step.threshold = STEP_THRESHOLD_MIN;
    if (step.threshold > STEP_THRESHOLD_MAX)
        step.threshold = STEP_THRESHOLD_MAX;

    if (step.jerk_min < STEP_JERK_MIN)
        step.jerk_min = STEP_JERK_MIN;
    if (step.jerk_min > STEP_JERK_MAX)
        step.jerk_min = STEP_JERK_MAX;

    if (step.min_gap_ms < STEP_GAP_MIN_MS)
        step.min_gap_ms = STEP_GAP_MIN_MS;
    if (step.min_gap_ms > STEP_GAP_MAX_MS)
        step.min_gap_ms = STEP_GAP_MAX_MS;

    if (step.impact_min < STEP_IMPACT_MIN)
        step.impact_min = STEP_IMPACT_MIN;
    if (step.impact_min > STEP_IMPACT_MAX)
        step.impact_min = STEP_IMPACT_MAX;

    if (step.impact_max < STEP_IMPACT_MIN)
        step.impact_max = STEP_IMPACT_MIN;
    if (step.impact_max > STEP_IMPACT_MAX)
        step.impact_max = STEP_IMPACT_MAX;

    //* Relations. Each one pulls the left-hand field into line rather than
    //* rejecting the write, so a slider that has been dragged to the end of its
    //* travel simply stops moving rather than being ignored.
    //! release < threshold: an armed detector releases at `release`, so a release
    //! at or above the arming threshold would leave it armed forever and one
    //! impact would be counted repeatedly.
    if (step.release >= step.threshold)
        step.release = step.threshold * 0.5f;
    if (step.release < STEP_RELEASE_MIN)
        step.release = STEP_RELEASE_MIN;

    //! impact_max divides the rescale: impact_max == impact_min is a division by
    //! zero, and an inverted pair maps every impact to the top of the scale.
    if (step.impact_max <= step.impact_min)
        step.impact_max = step.impact_min + 0.1f;

    //! Same for the magnitude scale, and its ends are integers, so "one apart"
    //! is the narrowest legal window. magnitude_min stops at 254 to leave room
    //! for max to be strictly greater inside a uint8_t.
    if (step.magnitude_min > 254)
        step.magnitude_min = 254;
    if (step.magnitude_max < 1)
        step.magnitude_max = 1;
    if (step.magnitude_max <= step.magnitude_min)
        step.magnitude_max = (uint8_t)(step.magnitude_min + 1);
}

//? The fastest impact rate the refractory permits, in impacts per second.
uint16_t step_cadence_max_sps()
{
    //? Reported on the configuration page so a refractory that is obviously too
    //! long for the intended cadence is visible as a number rather than as
    //! "some impacts are missing".
    if (step.min_gap_ms == 0)
        return 0; // Unreachable while step_sanitize() runs, but it is a divisor
    return (uint16_t)(1000UL / (uint32_t)step.min_gap_ms);
}

//? Map a measured impact, in g, onto the reporting scale the modes consume.
uint8_t impact_magnitude(float impact_g)
{
    //! step_sanitize() guarantees impact_max > impact_min, so this division cannot
    //! be by zero or by a negative.
    float norm = (impact_g - step.impact_min) / (step.impact_max - step.impact_min);
    if (norm < 0.0f)
        norm = 0.0f;
    if (norm > 1.0f)
        norm = 1.0f;
    return (uint8_t)((int)step.magnitude_min +
                     (int)(norm * (float)(step.magnitude_max - step.magnitude_min)));
}

//* Detector state. File-scope statics because this is one state machine with one
//* sampler, and nothing outside should be able to poke at its baseline.
static bool impact_initialised = false;
static bool impact_armed = false;
static float impact_baseline = 1.0f;   // In g: 1.0 is the resting magnitude
static float impact_previous = 0.0f;   // Previous magnitude, for the jerk term
static float impact_peak = 0.0f;       // Highest impact seen in the current event
static uint32_t impact_last_us = 0;    // When impact_poll() last sampled
static uint32_t impact_event_us = 0;   // When the current event armed
static uint32_t impact_accept_us = 0;  // When an event was last accepted, for the refractory

//? One pass of the impact detector. Called from loop(): it rate-limits itself and
//? returns immediately when the cadence has not elapsed.
void impact_poll()
{
    const uint32_t now_us = micros();
    const uint32_t dt_us = now_us - impact_last_us;
    //? Rate limit rather than trust the loop to be fast enough. With WiFi, a web
    //! request or an OTA transfer in flight, loop() can run far slower than
    //! IMPACT_SAMPLE_US; the detector has to degrade to a coarser sample rate rather
    //! than stop. The rate actually achieved is reported on serial (impact_samples).
    if (dt_us < IMPACT_SAMPLE_US)
        return;
    impact_last_us = now_us;
    impact_samples++;

    //? The measured interval, not the nominal one. Everything below is either a
    //? rate (the jerk term) or a time constant (the baseline), and both would be
    //! wrong if this assumed the cadence had been honoured.
    const float dt_s = (float)dt_us * 1e-6f;

    //* 1. Orientation-free magnitude, in g. Rotating the device moves gravity
    //* between the axes without changing this, which is why the detector does not
    //* need to know how the device is worn.
    const float ax = a.acceleration.x;
    const float ay = a.acceleration.y;
    const float az = a.acceleration.z;
    const float magnitude = sqrtf(ax * ax + ay * ay + az * az) / IMPACT_G;

    //* 2. Jerk: how fast the magnitude itself is moving, in g/s. This is the term
    //* that separates an impact from a swing: an impact changes the magnitude over
    //* one or two samples, a swing ramps over hundreds of milliseconds.
    //! Pure rotation does not move |accel| at all, which is why the magnitude test
    //! alone already ignores it - but rotation about an axis offset from the sensor
    //! adds centripetal acceleration, and a brisk swing reaches ~0.9 g slowly.
    //! That is exactly what an amplitude test cannot reject and a jerk gate can.
    const float jerk = fabsf(magnitude - impact_previous) / dt_s;
    impact_previous = magnitude;

    if (!impact_initialised)
    {
        impact_baseline = magnitude;
        impact_initialised = true;
    }

    //* 3. Slow baseline of the resting magnitude, in g. Held still while an event is
    //! in progress: adapting through an impact drags the baseline towards the impact
    //! itself and desensitises the detector for whatever follows it.
    if (!impact_armed)
    {
        //? One-pole coefficient derived from a TIME constant, so the setting keeps
        //! its meaning when the sample cadence changes. It used to be a per-sample
        //! fraction, which silently became ~25x faster the moment the cadence was
        //! raised - fast enough for the baseline to eat the impact it was measuring.
        const float tau_s = (float)step.baseline_tau_ms * 0.001f;
        const float alpha = dt_s / (tau_s + dt_s);
        impact_baseline += alpha * (magnitude - impact_baseline);
    }

    //* 4. Impact: how far the magnitude has departed from rest.
    const float impact = fabsf(magnitude - impact_baseline);

    if (!impact_armed)
    {
        //* 4a. Arm on a crossing that is both large enough and sharp enough.
        if (impact <= step.threshold || jerk < step.jerk_min)
            return;

        impact_armed = true;
        impact_peak = impact;
        impact_event_us = now_us;
        return;
    }

    //* 5. Armed: hold the peak of this event.
    if (impact > impact_peak)
        impact_peak = impact;

    //* 6. The event ends on ring-down, or when the peek window expires.
    //! Reporting on the ring-down alone has no bound on latency: a sustained shake
    //! never falls back under `release`, so the event would stay armed forever and
    //! every impact inside it would be swallowed. The window is what bounds it.
    const bool released = impact < step.release;
    const bool window_over = (uint32_t)(now_us - impact_event_us) >= IMPACT_PEEK_US;
    if (!released && !window_over)
        return;

    impact_armed = false;

    const uint8_t scale = impact_magnitude(impact_peak);

    //* Latch for the next consumer, whatever the refractory says. The strongest
    //* impact since the last take wins, because two impacts inside one LED frame
    //* cannot be drawn as two flashes - and a knock landing inside the refractory
    //! still has to move the strip. Gating the latch on the refractory would drop
    //! its brightness on the floor, since the only thing known at the crossing is
    //! that the impact had just cleared the threshold.
    if (scale > impact_pending)
        impact_pending = scale;

    //* The refractory gates the COUNT, not the measurement. Anything arriving
    //! inside it is measured and drawn, it is simply not counted as a new impact,
    //! so one knock's ring-down cannot be reported as several.
    if ((impact_accept_us != 0) &&
        ((uint32_t)((now_us - impact_accept_us) / 1000UL) < (uint32_t)step.min_gap_ms))
        return;

    impact_accept_us = now_us;
    impact_last_magnitude = scale;
    impact_last_ms = millis();
    impact_count++;

    Serial.printf("[IMPACT] peak=%.2fg jerk=%.1fg/s magnitude=%u\n", impact_peak, jerk, scale);
}

//? Read and clear the latched impact. Returns 0 when nothing is pending, so a
//? caller can treat "no impact" and "no impact this frame" identically.
uint8_t impact_take()
{
    const uint8_t taken = impact_pending;
    impact_pending = 0;
    return taken;
}

#endif