#pragma once

#include "app_types.h"

bool simulationEnabled();
void beginSimulation();
void updateSimulation();
void tareSimulationScale(float tareOffsetGrams);
const SimulatedSensorValues& getSimulatedSensorValues();
