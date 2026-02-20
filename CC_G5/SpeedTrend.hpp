#pragma once
#include <Arduino.h>
#include <math.h>

class SpeedTrendCalculator {
private:
  float lastRawSpeed;
  float smoothedSpeed;
  unsigned long lastMillis;
  float smoothedAcceleration;
  bool initialized;
  
  float inputSmoothingTau;   // Smooths the speed input
  float trendSmoothingTau;   // Smooths the trend output

public:
  SpeedTrendCalculator() {
    lastRawSpeed = 0.0f;
    smoothedSpeed = 0.0f;
    lastMillis = 0;
    smoothedAcceleration = 0.0f;
    initialized = false;
    inputSmoothingTau = 1000.0f;
    trendSmoothingTau = 2000.0f;  // Changed from 1000ms to slow the response.
  }

  void update(float rawSpeed) {
    unsigned long currentMillis = millis();
    
    if (!initialized) {
      lastRawSpeed = rawSpeed;
      smoothedSpeed = rawSpeed;
      lastMillis = currentMillis;
      initialized = true;
      return;
    }

    float dt = (currentMillis - lastMillis) / 1000.0f;
    
    if (dt <= 0) return;

    // First smoothing stage: smooth the input speed
    float inputAlpha = 1.0f - exp(-dt * 1000.0f / inputSmoothingTau);
    smoothedSpeed = inputAlpha * rawSpeed + (1.0f - inputAlpha) * smoothedSpeed;

    // Calculate acceleration from smoothed speed
    float instantAcceleration = (smoothedSpeed - lastRawSpeed) / dt;

    // Second smoothing stage: smooth the acceleration/trend
    float trendAlpha = 1.0f - exp(-dt * 1000.0f / trendSmoothingTau);
    smoothedAcceleration = trendAlpha * instantAcceleration + 
                          (1.0f - trendAlpha) * smoothedAcceleration;

    lastRawSpeed = smoothedSpeed;
    lastMillis = currentMillis;
  }

  float getTrendValue() {
    return smoothedAcceleration * 6.0f;
  }
  
  void reset() {
    initialized = false;
    smoothedAcceleration = 0.0f;
    smoothedSpeed = 0.0f;
  }
};