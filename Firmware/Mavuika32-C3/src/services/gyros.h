#if !defined(SERVICE_GYRO_READINGS)
#define SERVICE_GYRO_READINGS

#include "shared/dependencies.h"

uint8_t detectSingleStep(float ax, float ay, float az);

// Returns true only when a new step is detected.
// On detection, litLeds is set from 3..30 based on impact.
uint8_t detectSingleStep(float ax, float ay, float az)
{
    // Tunables
    const float baselineAlpha = 0.03f;    // slightly faster baseline to reject drift
    const float stepThreshold = 1.20f;    // higher threshold reduces false positives
    const float releaseThreshold = 0.45f; // hysteresis release point
    const uint32_t minStepMs = 200;       // longer refractory prevents double counts
    const uint32_t maxStepMs = 2000;      // optional cadence sanity
    const float impactMin = 1.5f;         // map lower bound
    const float impactMax = 6.5f;         // map upper bound (hard stomp)

    static bool initialized = false;
    static float baseline = 9.81f;
    static uint32_t lastStepMs = 0;
    static bool aboveThreshold = false;
    static float peakImpact = 0.0f;

    const uint32_t now = millis();

    // 1) Magnitude (orientation-independent)
    const float amag = sqrtf(ax * ax + ay * ay + az * az);

    // Init baseline once with first reading
    if (!initialized)
    {
        baseline = amag;
        initialized = true;
    }

    // 2) Slow baseline estimation (gravity + posture)
    baseline += baselineAlpha * (amag - baseline);

    // 3) Dynamic impact
    const float impact = fabsf(amag - baseline);

    // 4) One-step detection with refractory and cadence sanity
    const uint32_t dt = now - lastStepMs;
    const bool refractoryOk = dt >= minStepMs;
    const bool cadenceOk = (lastStepMs == 0) || (dt <= maxStepMs) || (dt > 2000);
    // Allow first step, normal cadence, and recovery after long idle.

    if (!aboveThreshold)
    {
        if (impact > stepThreshold)
        {
            aboveThreshold = true;
            peakImpact = impact;
        }
    }
    else
    {
        // Capture one full impact peak and trigger only after release.
        if (impact > peakImpact)
            peakImpact = impact;

        if (impact < releaseThreshold)
        {
            aboveThreshold = false;

            if (refractoryOk && cadenceOk)
            {
                lastStepMs = now;

                // 5) Map impact to 3..30 LEDs
                float norm = (peakImpact - impactMin) / (impactMax - impactMin);
                if (norm < 0.0f)
                    norm = 0.0f;
                if (norm > 1.0f)
                    norm = 1.0f;
                int ledCount = 3 + (int)(norm * (30 - 3));
                return ledCount;
            }
        }
    }

    return 0;
}

#endif