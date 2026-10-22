#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

// Shake parameters
struct ShakeSettings {
    float Duration = 0.5f;          // total duration (s)
    float AmplitudePos = 0.5f;      // position amplitude (meters)
    float AmplitudeRot = 2.0f;      // rotation amplitude (degrees)
    float Frequency = 25.0f;        // frequency (Hz)
    float TraumaFalloff = 1.5f;     // decay rate (higher = stops sooner)
};

class JuiceEngine
{
public:
    // Singleton Access
    static JuiceEngine& Instance();

    // Call once per frame from the scene update.
    void Update(float deltaTime);

    // --- Triggers (gameplay code) ---

    // Starts a trauma-based camera shake.
    void TriggerShake(const ShakeSettings& settings, float intensityMultiplier = 1.0f);

    // Starts a screen glitch that decays (kick and decay).
    void TriggerGlitchKick(float power = 1.5f, float decaySpeed = 5.0f);

    // --- Getters (camera and shaders) ---

    // Camera position offset for this frame
    DirectX::XMFLOAT3 GetShakePosOffset() const { return m_shakeResultPos; }

    // Camera rotation offset for this frame (pitch, yaw, roll)
    DirectX::XMFLOAT3 GetShakeRotOffset() const { return m_shakeResultRot; }

    // Current glitch intensity, passed to the shader
    float GetGlitchIntensity() const { return m_currentGlitchIntensity; }

    // Clears every effect (for example on a scene change).
    void Reset();

private:
    JuiceEngine() = default;
    ~JuiceEngine() = default;
    JuiceEngine(const JuiceEngine&) = delete;
    void operator=(const JuiceEngine&) = delete;

    // --- Internal State: Camera Shake ---
    ShakeSettings m_activeShake;
    float m_shakeTrauma = 0.0f;     // 0.0 to 1.0
    float m_shakeTimeCounter = 0.0f;
    DirectX::XMFLOAT3 m_shakeResultPos = { 0,0,0 };
    DirectX::XMFLOAT3 m_shakeResultRot = { 0,0,0 };

    // --- Internal State: Glitch Kick ---
    float m_currentGlitchIntensity = 0.0f;
    float m_glitchDecaySpeed = 5.0f;
};