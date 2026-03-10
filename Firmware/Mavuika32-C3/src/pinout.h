//? ------------------------------------------------------------------------------------------
//*! This file contains all the hardware pinout definitions for the project.
//? ------------------------------------------------------------------------------------------

#ifndef PROJECT_GLOBAL_PINOUT
#define PROJECT_GLOBAL_PINOUT

//* Channels:
#define status_led_pin 0
#define strip_pin 1
#define servo_pin 2

//* PORT2 (buttons):
#define buttonA 3
#define buttonB 4

//* PORT3 (free GPIOs):
#define freeGPIOA 5
#define freeGPIOB 6

//* PORT1 (I2C pins):
#define i2c_sda 7
#define i2c_scl 8

#define prog_switch GPIO_NUM_9 // Switch on C3 Supermini devboard

//* Unrouted (Serial0):
#define tx_0 20
#define rx_0 21

#endif // PROJECT_GLOBAL_PINOUT
