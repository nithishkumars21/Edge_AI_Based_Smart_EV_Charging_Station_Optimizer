#include "edge_ai.h"
#include "model.h"

// ---- Live state (defaults; main.cpp updates these every loop) ----
int   g_hourOfDay         = 9;
int   g_dayOfWeek         = 1;
bool  g_bayOccupied       = false;
float g_recentAvgCurrent  = 0.0f;
float g_sessionElapsedMin = 0.0f;

float g_arrivalProbability   = 0.0f;
float g_predictedDurationMin = 0.0f;

// Offline-derived hourly arrival-rate table (fraction of days an EV arrived
// in that hour, from the same historical dataset used to train the models
// in tools/train_models.py). Two peaks: morning drop-off, evening return.
static const float kHourlyArrivalRate[24] = {
    0.02f, 0.01f, 0.01f, 0.01f, 0.02f, 0.05f, // 0-5
    0.10f, 0.22f, 0.35f, 0.28f, 0.15f, 0.10f, // 6-11
    0.08f, 0.07f, 0.07f, 0.09f, 0.14f, 0.24f, // 12-17
    0.38f, 0.42f, 0.30f, 0.18f, 0.09f, 0.04f  // 18-23
};

float historicalArrivalRate(int hour) {
    if (hour < 0 || hour > 23) return 0.0f;
    return kHourlyArrivalRate[hour];
}

void runEdgeAIInference() {
    float histRate = historicalArrivalRate(g_hourOfDay);

    g_arrivalProbability = predictArrival(
        (float)g_hourOfDay,
        (float)g_dayOfWeek,
        g_bayOccupied ? 1.0f : 0.0f,
        g_recentAvgCurrent,
        g_sessionElapsedMin,
        histRate
    );

    g_predictedDurationMin = predictDuration(
        (float)g_hourOfDay,
        (float)g_dayOfWeek,
        g_bayOccupied ? 1.0f : 0.0f,
        g_recentAvgCurrent,
        g_sessionElapsedMin,
        histRate
    );
}
