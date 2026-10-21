#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <deque>
#include <DirectXMath.h>
#include "System/Sprite.h"
#include "Primitive.h"

enum class ImpactType {
    None,
    Super,
    Hot,
    MashSpace,
    Ready,
    Go,
    Nigero,
    SpaceKeyJP,  // Sprite_Text_SPACEKEY_jp.png
    RendaSeyo,
    Bougyo,
    Kougeki
};

// One step of an impact sequence.
struct ImpactEvent {
    ImpactType type;
    float duration;
};

class UIImpactDisplay
{
public:
    UIImpactDisplay();
    ~UIImpactDisplay();

    void Initialize(ID3D11Device* device);

    // Plays one impact.
    void Show(ImpactType type, float duration = 1.5f);

    // Plays the events in order.
    void ShowSequence(const std::vector<ImpactEvent>& sequence);

    void Update(float elapsedTime);
    void Render(ID3D11DeviceContext* dc);
    void OnResize(float screenW, float screenH);
    bool IsActive() const;

    float GetDistortionKick() const;
private:
    struct AnimationState {
        bool isActive = false;
        float timer = 0.0f;
        float totalDuration = 0.0f;
        ImpactType currentType = ImpactType::None;

        float currentScale = 1.0f;
        float currentAlpha = 1.0f;
        float currentRotation = 0.0f;
        float flashAlpha = 0.0f;
    };

    std::unordered_map<ImpactType, std::unique_ptr<Sprite>> m_sprites;
    struct SpriteDim { float w, h; };
    std::unordered_map<ImpactType, SpriteDim> m_spriteDims;

    std::unique_ptr<Primitive> m_primitive;
    AnimationState m_anim;

    // Pending events of the current sequence.
    std::deque<ImpactEvent> m_eventQueue;

    float m_screenW = 1920.0f;
    float m_screenH = 1080.0f;

    void ResetAnim();
};