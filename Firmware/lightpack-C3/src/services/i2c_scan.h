#if !defined(SERVICE_I2C_SCAN)
#define SERVICE_I2C_SCAN

#include "shared/dependencies.h"

//? ------------------------------------------------------------------------------------------
//* I2C bus diagnostics, sensor address resolution and MPU6xxx bring-up.
//?
//? The gyroscope used to be initialised with a single hard-coded probe, mpu.begin(0x68, &Wire),
//? which is only one of the two addresses an MPU6050 can answer on - and it reported nothing at
//? all when the probe failed. "Failed to find MPU6050 chip" was printed identically for an empty
//? bus, for a sensor sitting on the other legal address, and for a device that is present and
//? correct on the wire but whose WHO_AM_I register Adafruit refuses. Those three cases need
//? different fixes, so this service separates them.
//?
//? The third case turned out to be the real one on this board: the module answers at 0x68 and
//? reports WHO_AM_I 0x70, which is an MPU6500. mpu_begin_family() below is what drives it.
//? ------------------------------------------------------------------------------------------

//* Address range.
//? A 7-bit I2C address is 0x00-0x7F, but the specification reserves the low block and the top
//! block: 0x00-0x07 hold the general call address, the CBUS address and the high-speed master
//! codes, and 0x78-0x7F are the 10-bit address prefixes and reserved space. Probing them yields
//! false positives on some slaves, so the scan covers 0x08-0x77 - the 112 assignable addresses.
#define I2C_SCAN_FIRST 0x08
#define I2C_SCAN_LAST 0x77

//* MPU6050 / MPU6500 family.
//? AD0 is the address LSB, so exactly two addresses are legal for these parts.
//! On this board AD0 is not routed to the MCU: the lux-et-portabilis-V2 MPU6050 footprint is the
//! 4-pin module (VCC / GND / SDA / SCL), so the address is whatever the module's own AD0 strap
//! decides. The firmware cannot choose it, so it has to try both.
#define MPU_ADDR_AD0_LOW 0x68
#define MPU_ADDR_AD0_HIGH 0x69

//? WHO_AM_I lives at 0x75 on every part in this family.
#define MPU_REG_WHO_AM_I 0x75

//* Accelerometer full scale.
//? Adafruit_MPU6050::_init() selects +/-2 g, which is right for reading gravity and
//! useless for measuring an impact: a knock on a worn device is 5-20 g, so every
//! axis rails at 2 g and every impact reads the same clipped value. +/-8 g covers
//! knocks and most drops while keeping 4096 LSB/g, four times coarser than 2 g and
//! still far finer than the impact scale needs. _read() scales from the range
//! register, so the rest of the pipeline follows this automatically.
#define MPU_ACCEL_RANGE MPU6050_RANGE_8_G

//? Sensor configuration applied after bring-up, on BOTH paths below. The genuine
//? MPU6050 path gets it from Adafruit_MPU6050::_init() and would otherwise keep its
//? own +/-2 g default, which is why this is applied again afterwards rather than
//? left to the driver.
void mpu_apply_accel_range()
{
    mpu.setAccelerometerRange(MPU_ACCEL_RANGE);
    Serial.printf("[IMU] accelerometer range set to +/-%d g\n",
                  MPU_ACCEL_RANGE == MPU6050_RANGE_16_G ? 16 :
                  MPU_ACCEL_RANGE == MPU6050_RANGE_8_G  ? 8 :
                  MPU_ACCEL_RANGE == MPU6050_RANGE_4_G  ? 4 : 2);
}

//? Read a single register over the bus. Returns false when the device did not answer, so a
//! failure cannot be mistaken for a device that legitimately returned 0x00 or 0xFF.
bool i2c_read_reg(uint8_t address, uint8_t reg, uint8_t &value)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0)
        return false;

    if (Wire.requestFrom((uint8_t)address, (uint8_t)1) != 1)
        return false;

    value = (uint8_t)Wire.read();
    return true;
}

//? WHO_AM_I values. 0x68 is the only one Adafruit_MPU6050::begin() accepts; the family list in
//? mpu6xxx_registers_compatible() is what mpu_begin_family() drives around that check. The rest
//? are the identities of parts routinely sold as "MPU6050".
const char *i2c_who_am_i_name(uint8_t id)
{
    switch (id)
    {
    case 0x68:
        return "MPU6050 / MPU6000";
    case 0x70:
        return "MPU6500";
    case 0x71:
        return "MPU9250";
    case 0x73:
        return "MPU9255";
    case 0x19:
        return "MPU6886";
    case 0x98:
        return "MPU6500 variant";
    default:
        return "unknown device id";
    }
}

//? Addresses worth naming on this bus. Two of these are libraries the project already depends on
//? (Adafruit SSD1306 and Adafruit ADS1X15), so seeing them here is expected, not a fault.
const char *i2c_address_hint(uint8_t address)
{
    switch (address)
    {
    case 0x19:
        return "MPU6886 family";
    case 0x3C:
    case 0x3D:
        return "SSD1306 OLED (0x3C/0x3D)";
    case 0x48:
    case 0x49:
    case 0x4A:
    case 0x4B:
        return "ADS1015/ADS1115 ADC (0x48-0x4B)";
    case 0x50:
    case 0x51:
    case 0x52:
    case 0x53:
    case 0x54:
    case 0x55:
    case 0x56:
    case 0x57:
        return "AT24Cxx EEPROM (0x50-0x57)";
    case MPU_ADDR_AD0_LOW:
        return "MPU6050 AD0=GND / MPU6000";
    case MPU_ADDR_AD0_HIGH:
        return "MPU6050 AD0=VCC (or DS1307 RTC family)";
    case 0x76:
    case 0x77:
        return "BMP280/BME280 (0x76/0x77)";
    default:
        return "";
    }
}

//? Walk every assignable address and report what answered. Returns the number of devices found.
uint8_t i2c_scan_report()
{
    Serial.printf("[I2C] ---------- bus scan 0x%02X-0x%02X (SDA=%d, SCL=%d) ----------\n",
                  I2C_SCAN_FIRST, I2C_SCAN_LAST, i2c_sda, i2c_scl);

    uint8_t found = 0;
    for (uint8_t address = I2C_SCAN_FIRST; address <= I2C_SCAN_LAST; address++)
    {
        //? A device that ACKs its address is present. This says nothing about whether it is the
        //? device you are looking for - that is what WHO_AM_I is for.
        Wire.beginTransmission(address);
        if (Wire.endTransmission() != 0)
            continue;

        found++;
        const char *hint = i2c_address_hint(address);
        if (hint[0] != '\0')
            Serial.printf("[I2C]   0x%02X ACK  - %s\n", address, hint);
        else
            Serial.printf("[I2C]   0x%02X ACK\n", address);
    }

    if (found == 0)
    {
        //! An entirely empty scan is a wiring or power fault, never an address fault - no address
        //! in the range would have helped. Say so, because this is where the old code sent people
        //! looking at the address instead.
        Serial.println("[I2C]   nothing answered.");
        Serial.println("[I2C]   Check 3V3 at the module, SDA on GPIO7, SCL on GPIO8, common GND,");
        Serial.println("[I2C]   and that the bus has pull-ups (a bare sensor needs them).");
    }

    Serial.printf("[I2C] %u device(s) on the bus\n", found);
    return found;
}

//* Identities Adafruit_MPU6050 refuses but whose register map it can drive.
//? 0x70 MPU6500, 0x71 MPU9250 and 0x73 MPU9255 share the register map for everything getEvent()
//? touches: data at 0x3B, PWR_MGMT_1 at 0x6B, CONFIG at 0x1A, GYRO_CONFIG at 0x1B, ACCEL_CONFIG
//? at 0x1C, SMPLRT_DIV at 0x19, and the same 16384 LSB/g and 65.5 LSB/dps scale factors. The
//? 9250 and 9255 are a 6500 core plus a magnetometer, which this firmware never reads.
//! 0x19 (MPU6886) is deliberately absent: it is a different part whose map only mostly overlaps,
//! so it is reported rather than driven.
bool mpu6xxx_registers_compatible(uint8_t who)
{
    return (who == 0x70) || (who == 0x71) || (who == 0x73);
}

//? Look for a supported inertial sensor on either legal address. Sets address_out and who_out.
bool i2c_find_mpu6050(uint8_t &address_out, uint8_t &who_out)
{
    address_out = 0;
    who_out = 0;
    const uint8_t candidates[2] = {MPU_ADDR_AD0_LOW, MPU_ADDR_AD0_HIGH};

    for (uint8_t i = 0; i < 2; i++)
    {
        const uint8_t address = candidates[i];

        Wire.beginTransmission(address);
        if (Wire.endTransmission() != 0)
        {
            Serial.printf("[IMU] no ACK at 0x%02X\n", address);
            continue;
        }

        uint8_t who = 0;
        if (!i2c_read_reg(address, MPU_REG_WHO_AM_I, who))
        {
            Serial.printf("[IMU] 0x%02X ACKs but WHO_AM_I (0x%02X) did not read back\n",
                          address, MPU_REG_WHO_AM_I);
            continue;
        }

        Serial.printf("[IMU] 0x%02X ACKs, WHO_AM_I = 0x%02X (%s)\n",
                      address, who, i2c_who_am_i_name(who));

        if (who == MPU6050_DEVICE_ID || mpu6xxx_registers_compatible(who))
        {
            address_out = address;
            who_out = who;
            return true;
        }

        //? Present and readable, but not a part this firmware drives. Both addresses are probed
        //! before this is reported, because moving the sensor to the other one would not help.
        Serial.printf("[IMU] 0x%02X is not a part this firmware drives (WHO_AM_I 0x%02X)\n",
                      address, who);
    }

    return false;
}

//? Start the sensor, working around the driver's identity check.
//!
//! Adafruit_MPU6050::begin() reads WHO_AM_I and returns false unless it equals 0x68, so an
//! MPU6500 - which is what many modules sold as "MPU6050" actually carry, and what this board
//! has - can never be started through begin(), at any address. begin() is not virtual and the
//! class is declared final, so the check cannot be relaxed by subclassing either.
//?
//? begin() is still called first: it allocates the I2C device handle and runs the ACK retries,
//? and it can only have reached its WHO_AM_I comparison if that handle exists. The device is then
//? re-probed here, and only that re-probe authorises the driver calls below - a false return from
//? begin() is also exactly what an absent device produces, and the public setters would otherwise
//? dereference a handle that never reached anything. With identity and presence both confirmed,
//? the configuration _init() applies is replayed through the public API.
bool mpu_begin_family(uint8_t address, uint8_t who)
{
    if (mpu.begin(address, &Wire))
    {
        //? A real MPU6050: the driver did the whole job, but with its own +/-2 g
        //! default, so the impact range still has to be applied here.
        mpu_apply_accel_range();
        return true;
    }

    uint8_t confirm = 0;
    if (!i2c_read_reg(address, MPU_REG_WHO_AM_I, confirm) || confirm != who)
    {
        Serial.printf("[IMU] 0x%02X stopped answering on the WHO_AM_I re-read\n", address);
        return false;
    }

    Serial.printf("[IMU] 0x%02X is an %s: replaying the MPU6050 sequence by hand\n",
                  address, i2c_who_am_i_name(who));

    //* The body of Adafruit_MPU6050::_init(), in the public form of each step.
    mpu.reset();
    mpu.setSampleRateDivisor(0);
    mpu.setFilterBandwidth(MPU6050_BAND_260_HZ);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu_apply_accel_range();

    //! reset() restores PWR_MGMT_1 with SLEEP set, and setClock() writes only the CLKSEL bits, so
    //! without this the part stays asleep: getEvent() would still return true and every reading
    //! would be zero. _init() sidesteps that by writing PWR_MGMT_1 in one go; the public API
    //! needs two calls. MPU6050_PLL_GYROX is the 0x01 that write carries.
    mpu.enableSleep(false);
    mpu.setClock(MPU6050_PLL_GYROX);

    delay(100);

    //? getEvent() scales from the range registers it reads back, so it needs none of the sensor
    //? objects _init() constructs at its end - skipping them changes nothing.
    return true;
}

//? Probe both legal addresses and start the first supported sensor found.
bool mpu_begin_auto(uint8_t &address_out)
{
    uint8_t who = 0;
    if (!i2c_find_mpu6050(address_out, who))
        return false;

    return mpu_begin_family(address_out, who);
}

#endif // SERVICE_I2C_SCAN
