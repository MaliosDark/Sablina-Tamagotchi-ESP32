#include <Arduino.h>

void setup();
void loop();

extern "C" void app_main(void) {
  initArduino();
  setup();

  while (true) {
    loop();
    if (serialEventRun) {
      serialEventRun();
    }
    delay(1);
  }
}
