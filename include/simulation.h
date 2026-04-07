#pragma once

#include "app_types.h"

bool simulationEnabled();
void beginSimulation();
void updateSimulation();
const SimulatedSensorValues& getSimulatedSensorValues();
