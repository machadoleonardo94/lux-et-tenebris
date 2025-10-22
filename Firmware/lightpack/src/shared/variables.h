#if !defined(PROJECT_GLOBAL_VARIABLES)
#define PROJECT_GLOBAL_VARIABLES

//* ---------------------- GLOBAL COMPONENT VARIABLES ----------------------

bool ads_started = false;
//* OLED display
bool display_started = false;
//* SD card
bool sdcard = false;

uint32_t loopTimer = 0;
uint32_t globalTimer = 0;
uint32_t loopCounter = 0;

uint8_t macAdress[6] = {0, 0, 0, 0, 0, 0};

typedef struct ledstrip
{
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    uint8_t color = 0;
    uint8_t brightness = 25;
    uint32_t update_time = 50;
} struct_ledstrip;

struct_ledstrip strip[3];
#define ZONAS 0
#define BAR1 1
#define BAR2 2

#endif // PROJECT_GLOBAL_VARIABLES
