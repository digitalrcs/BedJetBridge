/*
  BedJet Matter Bridge (Arduino IDE) - STA + Thermostat/Fan (two logical devices)

  This is a split-project wrapper.
  All implementation has been moved into .h/.cpp modules for easier debugging.
*/

#include "App.h"

void setup() { App::setup(); }
void loop()  { App::loop();  }
