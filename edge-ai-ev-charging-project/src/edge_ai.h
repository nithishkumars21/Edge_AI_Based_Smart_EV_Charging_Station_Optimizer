#ifndef EDGE_AI_H
#define EDGE_AI_H

// Looks up the historical EV-arrival rate for a given hour (0-23) from a
// fixed table trained offline. Used as one of the 6 model input features.
float historicalArrivalRate(int hour);

// Runs both on-device models (predictArrival, predictDuration) using the
// live sensor/session state below, and stores results in
// g_arrivalProbability / g_predictedDurationMin.
void runEdgeAIInference();

// ---- Live inputs, set by main.cpp before each runEdgeAIInference() call ----
extern int   g_hourOfDay;          // 0-23, from millis()-based fake RTC or NTP
extern int   g_dayOfWeek;          // 0-6
extern bool  g_bayOccupied;        // true while an EV session is active
extern float g_recentAvgCurrent;   // smoothed current reading (A)
extern float g_sessionElapsedMin;  // minutes since current session started

// ---- Outputs, filled by runEdgeAIInference() ----
extern float g_arrivalProbability;   // 0..1 — likelihood of next EV arrival soon
extern float g_predictedDurationMin; // minutes — expected remaining/total session length

#endif
