//? ------------------------------------------------------------------------------------------
//*! This file contains all the hardware pinout definitions for the project.
//? ------------------------------------------------------------------------------------------

#ifndef PROJECT_GLOBAL_PINOUT
#define PROJECT_GLOBAL_PINOUT

//* ADC:
#define adc_0 0

//* Channels:
#define strip_out 1
#define majora_out 2

//* Touch:
#define button 3

//* Latch power on
#define latch_enable 4

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
