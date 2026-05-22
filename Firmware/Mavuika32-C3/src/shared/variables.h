#if !defined(PROJECT_GLOBAL_VARIABLES)
#define PROJECT_GLOBAL_VARIABLES

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

uint8_t macAdress[6] = {0, 0, 0, 0, 0, 0};

typedef struct ledstrip
{
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    uint8_t color = 0;
    uint8_t brightness = 25;
    uint32_t update_time = 50;
    int8_t index = 0;
} struct_ledstrip;

struct_ledstrip status_led;
struct_ledstrip led_strip;
struct_ledstrip majora_strip;

typedef struct buttonState
{
    bool currentState = HIGH;
    bool lastState = HIGH;
    uint32_t lastDebounceTime = 0;
    uint32_t debounceDelay = 50;
    uint8_t counter = 0;
} struct_buttonState;

struct_buttonState button_states[4]; // Assuming 4 buttons

#endif // PROJECT_GLOBAL_VARIABLES
