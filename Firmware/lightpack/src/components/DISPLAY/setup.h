#if !defined(SETUP_DISPLAY)
#define SETUP_DISPLAY

#include "shared/dependencies.h"
#include "components/DISPLAY/constants.h"

void setup_DISPLAY()
{
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println("SSD1306 allocation failed");
    return;
  }
  display.clearDisplay();
  display_started = true;
  return;
}
#endif // SETUP_DISPLAY
