#pragma once
#include "System/ModelRenderer.h"
#include "System/Model.h"

class Animation
{
public:
    // Memulai animasi baru.
    // [PENTING] Parameter 'model' diperlukan untuk "Smart Start" (langsung ke frame pertama).
    void PlayAnimation(Model* model, int index, bool loop);
    void PlayAnimation(Model* model, const char* name, bool loop);

    // Update status animasi setiap frame.
    void UpdateAnimation(Model* model, float elapsedTime);

    // Mengatur kecepatan playback (1.0 = normal, 0.5 = setengah, 2.0 = dua kali lipat).
    void SetPlaybackSpeed(float speed) { playbackSpeed = speed; }

    // Getter untuk debugging atau logika lain
    bool IsPlaying() const { return animationPlaying; }
    bool IsLooping() const { return animationLoop; }
    int GetCurrentIndex() const { return animationIndex; }

private:
    int   animationIndex = -1;
    float animationSeconds = 0.0f;
    bool  animationLoop = false;
    bool  animationPlaying = false;

    // [BARU] Kontrol Kecepatan
    float playbackSpeed = 1.0f;

    // [BARU] Sistem Blending Terpisah
    bool  isBlending = false;
    float blendTime = 0.0f;
    float blendDuration = 0.2f; // Durasi transisi (detik)
};