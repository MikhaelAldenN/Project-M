#include "JuiceEngine.h"
#include <cstdlib> // rand()

using namespace DirectX;

// Random value in [-1, 1]
static float GetRandomFloat()
{
    return ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
}

JuiceEngine& JuiceEngine::Instance()
{
    static JuiceEngine instance;
    return instance;
}

void JuiceEngine::Reset()
{
    m_shakeTrauma = 0.0f;
    m_currentGlitchIntensity = 0.0f;
    m_shakeResultPos = { 0,0,0 };
    m_shakeResultRot = { 0,0,0 };
}

void JuiceEngine::TriggerShake(const ShakeSettings& settings, float intensityMultiplier)
{
    m_activeShake = settings;

    // Apply multiplier
    m_activeShake.AmplitudePos *= intensityMultiplier;
    m_activeShake.AmplitudeRot *= intensityMultiplier;

    // add trauma, capped at 1
    m_shakeTrauma = std::clamp(m_shakeTrauma + 1.0f, 0.0f, 1.0f);
}

void JuiceEngine::TriggerGlitchKick(float power, float decaySpeed)
{
    // The kick replaces the current intensity; it does not stack.
    m_currentGlitchIntensity = power;
    m_glitchDecaySpeed = decaySpeed;
}

void JuiceEngine::Update(float deltaTime)
{
    // =========================================================
    // 1. UPDATE CAMERA SHAKE (Trauma Based)
    // =========================================================
    if (m_shakeTrauma > 0.0f)
    {
        // A. Trauma decay
        // trauma -= dt * (falloff / duration)
        float decay = (deltaTime / m_activeShake.Duration) * m_activeShake.TraumaFalloff;
        m_shakeTrauma -= decay;

        if (m_shakeTrauma < 0.0f) m_shakeTrauma = 0.0f;

        // B. Shake power = trauma squared, which makes the fade-out smooth
        float shakePower = m_shakeTrauma * m_shakeTrauma;

        // C. Generate Noise Offset
        m_shakeTimeCounter += deltaTime * m_activeShake.Frequency;

        // Position
        float maxPos = m_activeShake.AmplitudePos * shakePower;
        m_shakeResultPos.x = maxPos * GetRandomFloat();
        m_shakeResultPos.y = maxPos * GetRandomFloat();
        m_shakeResultPos.z = maxPos * GetRandomFloat();

        // Rotation (pitch, yaw, roll)
        float maxRot = XMConvertToRadians(m_activeShake.AmplitudeRot) * shakePower;
        m_shakeResultRot.x = maxRot * GetRandomFloat();
        m_shakeResultRot.y = maxRot * GetRandomFloat();
        m_shakeResultRot.z = maxRot * GetRandomFloat();
    }
    else
    {
        m_shakeResultPos = { 0,0,0 };
        m_shakeResultRot = { 0,0,0 };
    }

    // =========================================================
    // 2. UPDATE GLITCH DISTORTION (Kick & Decay)
    // =========================================================
    if (m_currentGlitchIntensity > 0.0f)
    {
        m_currentGlitchIntensity -= deltaTime * m_glitchDecaySpeed;
        if (m_currentGlitchIntensity < 0.0f) m_currentGlitchIntensity = 0.0f;
    }
}