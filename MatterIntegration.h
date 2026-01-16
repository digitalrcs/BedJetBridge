#pragma once
#include "Globals.h"

// Matter endpoints + glue to BedJet command queue.
void startMatterEndpoints();
void checkForThermostatWritesFromController();

void checkForFanWritesFromController();
