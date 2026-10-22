#pragma once
#include <algorithm>

// Global time scale for hit stop
class TimeManager {
public:
    static TimeManager& Instance() {
        static TimeManager instance;
        return instance;
    }

    // Call with unscaled delta time.
    void Update(float unscaledDt) {
        if (m_hitStopTimer > 0.0f) {
            m_hitStopTimer -= unscaledDt;
            if (m_hitStopTimer <= 0.0f) {
                m_hitStopMultiplier = 1.0f; // back to normal speed
            }
        }
    }

    // Callable from anywhere (for example on damage, parry or an explosion).
    void TriggerHitStop(float durationInSeconds, float timeScaleDuringHitStop = 0.0f) {
        m_hitStopTimer = durationInSeconds;
        m_hitStopMultiplier = timeScaleDuringHitStop;
    }

    float GetHitStopMultiplier() const { return m_hitStopMultiplier; }

private:
    TimeManager() = default;
    float m_hitStopMultiplier = 1.0f;
    float m_hitStopTimer = 0.0f;
};