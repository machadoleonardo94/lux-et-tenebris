//? ------------------------------------------------------------------------------------------
//*! This file contains all the hardware pinout definitions for the project.
//? ------------------------------------------------------------------------------------------

#ifndef PROJECT_GLOBAL_PINOUT
#define PROJECT_GLOBAL_PINOUT

#include "shared/dependencies.h"

#ifdef EQUIPAHA
//* Channels:
// #define WS_channel0 12
#define WS_channel1 12
#define WS_channel2 27
#define WS_channel3 14
// #define CC_output 13
#define openDrain_output1 13 // Buzzer
#define openDrain_output2 26 // Relay
// #define openDrain_output3 25

//* PORT2 (buttons):
#define buttonA 27
#define buttonB 14
#else
//* Channels:
#define WS_channel0 32
#define WS_channel1 33
#define WS_channel2 19
#define WS_channel3 16
#define CC_output 13
#define openDrain_output1 27
#define openDrain_output2 26
#define openDrain_output3 25

//* PORT2 (buttons):
#define buttonA 17
#define buttonB 18
#endif

//* PORT1 (Serial0):
#define tx_0 1
#define rx_0 3

//* PORT3 (I2C pins):
#define i2c_sda 21
#define i2c_scl 22

#define prog_switch 0

#endif // PROJECT_GLOBAL_PINOUT
