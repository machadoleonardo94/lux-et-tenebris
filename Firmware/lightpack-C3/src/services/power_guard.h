#if !defined(SERVICE_POWER_GUARD)
#define SERVICE_POWER_GUARD

#include "shared/dependencies.h"

//* ---------------------- POWER GUARD (0.6) ----------------------
//? A 10 Hz voltage-feedback trim, deliberately separate from the power budget in
//? update_strip(): the budget is a static cap the user chooses, the guard is the
//? dynamic correction when the chosen cap still sags the rail. It never writes the
//? stored brightness - it only scales the frame - so the strip returns to the
//? configured level once V_REG recovers.
//?
//! This is not a brown-out cure. The C3 can still reset; safe mode is the
//? recovery path. The guard simply keeps normal operation away from the edge.

void power_guard_loop()
{
    static uint32_t last = 0;
    const uint32_t now = millis();
    if (now - last < (uint32_t)POWER_GUARD_INTERVAL_MS)
        return;
    last = now;

    const uint16_t rail = (uint16_t)supply_rail_mv();

    if (rail < POWER_GUARD_FLOOR_MV)
    {
        //* Trim 5/255 (~2%) per 100 ms while the rail is under the floor. Full
        //? trim from 255 takes about 5 s, which is slower than a load transient
        //* and faster than a cell can sag further.
        power_guard_scale = (power_guard_scale > POWER_GUARD_STEP)
                                ? (uint8_t)(power_guard_scale - POWER_GUARD_STEP)
                                : 0;
        power_guard_active = true;
    }
    else if (rail > POWER_GUARD_RECOVER_MV)
    {
        //* Recover one step per 100 ms above the hysteresis band: slower than the
        //? cut, so it cannot oscillate around the floor.
        if (power_guard_scale < POWER_GUARD_RAMP)
            power_guard_scale++;
        if (power_guard_scale >= POWER_GUARD_RAMP)
        {
            power_guard_scale = POWER_GUARD_RAMP;
            power_guard_active = false;
        }
    }
}

//* Advisory only: the safe continuous current implied by the last test's
//? open-circuit rail and source resistance. The user asked for a suggestion,
//* not an imposed cap, so this is reported next to the budget slider and never
//* written into it by the firmware.
//! The test cannot know whether the battery was actually disconnected. A
//? USB-assisted run will overestimate; the page says so beside the value.
uint16_t power_suggestion_ma()
{
    uint16_t v_off = 0;
    uint16_t r_mohm = 0;

    if (stress_last.result != STRESS_RESULT_NONE)
    {
        v_off = stress_last.v_off;
        r_mohm = stress_last.r_mohm;
    }
    else if (sense_last.count > 0)
    {
        v_off = sense_last.v_off;
        r_mohm = sense_last.r_mohm;
    }

    if (v_off <= POWER_GUARD_FLOOR_MV || r_mohm == 0)
        return 0;

    uint32_t ma = ((uint32_t)(v_off - POWER_GUARD_FLOOR_MV) * 1000UL) / r_mohm;
    if (ma < POWER_BUDGET_MIN_MA)
        ma = POWER_BUDGET_MIN_MA;
    if (ma > POWER_BUDGET_MAX_MA)
        ma = POWER_BUDGET_MAX_MA;
    return (uint16_t)ma;
}

#endif // SERVICE_POWER_GUARD
