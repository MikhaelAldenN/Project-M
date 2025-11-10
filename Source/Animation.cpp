#include "Animation.h"
#include <cmath> // Diperlukan untuk fmod

void Animation::PlayAnimation(Model* model, int index, bool loop)
{
    animationPlaying = true;
    animationLoop = loop;
    animationIndex = index;

    // [BARU] Reset sistem blending
    isBlending = true;
    blendTime = 0.0f;

    // [BARU] "Smart Start": Langsung lompat ke waktu awal animasi yang sebenarnya
    animationSeconds = 0.0f; // Default
    if (model)
    {
        const auto& anims = model->GetResource()->GetAnimations();
        if (index >= 0 && index < static_cast<int>(anims.size()))
        {
            const auto& anim = anims.at(index);
            if (!anim.keyframes.empty())
            {
                // Set waktu ke keyframe pertama agar tidak ada delay di awal
                animationSeconds = anim.keyframes.front().seconds;
            }
        }
        // Opsional: Paksa update pose seketika itu juga
        UpdateAnimation(model, 0.0f);
    }
}

void Animation::PlayAnimation(Model* model, const char* name, bool loop)
{
    int index = 0;
    const auto& animations = model->GetResource()->GetAnimations();
    for (const auto& animation : animations)
    {
        if (animation.name == name)
        {
            PlayAnimation(model, index, loop);
            return;
        }
        ++index;
    }
}

void Animation::UpdateAnimation(Model* model, float elapsedTime)
{
    if (!animationPlaying || !model) return;

    const auto& animations = model->GetResource()->GetAnimations();
    if (animationIndex < 0 || animationIndex >= static_cast<int>(animations.size())) return;

    const auto& animation = animations.at(animationIndex);
    const auto& keyframes = animation.keyframes;
    if (keyframes.empty()) return;

    // --- 1. TIME MANAGEMENT ---
    float startTime = keyframes.front().seconds;
    float endTime = keyframes.back().seconds;
    float duration = endTime - startTime;

    // Update waktu dengan playbackSpeed
    animationSeconds += elapsedTime * playbackSpeed;

    // Logika Looping yang Tangguh
    if (animationSeconds >= endTime)
    {
        if (animationLoop)
        {
            // Jika durasi valid, gunakan fmod agar sisa waktu tidak terbuang.
            // Loop kembali ke startTime, BUKAN ke 0.0f!
            if (duration > 0.0001f)
                animationSeconds = startTime + fmod(animationSeconds - startTime, duration);
            else
                animationSeconds = startTime; // Fallback untuk animasi 1 frame
        }
        else
        {
            animationSeconds = endTime;
            animationPlaying = false;
        }
    }

    // Penjaga gawang: Pastikan waktu tidak pernah kurang dari startTime
    if (animationSeconds < startTime) animationSeconds = startTime;

    // --- 2. BLENDING LOGIC ---
    float blendRate = 1.0f;
    if (isBlending)
    {
        blendTime += elapsedTime;
        if (blendTime >= blendDuration)
        {
            blendTime = blendDuration;
            isBlending = false;
        }

        blendRate = blendTime / blendDuration;
        blendRate = blendRate * blendRate * (3.0f - 2.0f * blendRate); // Smoothstep easing (lebih mulus dari kuadrat biasa)
    }

    // --- 3. POSE INTERPOLATION ---
    // Cari segmen keyframe yang aktif saat ini
    int keyCount = static_cast<int>(keyframes.size());
    for (int i = 0; i < keyCount - 1; ++i)
    {
        const auto& k0 = keyframes[i];
        const auto& k1 = keyframes[i + 1];

        if (animationSeconds >= k0.seconds && animationSeconds <= k1.seconds)
        {
            float t = 0.0f;
            if ((k1.seconds - k0.seconds) > 0.00001f)
                t = (animationSeconds - k0.seconds) / (k1.seconds - k0.seconds);

            // Loop semua node untuk interpolasi
            auto& nodes = model->GetNodes();
            int nodeCount = static_cast<int>(nodes.size());
            for (int n = 0; n < nodeCount; ++n)
            {
                auto& node = nodes[n];
                const auto& key0 = k0.nodeKeys.at(n);
                const auto& key1 = k1.nodeKeys.at(n);

                DirectX::XMVECTOR S0, R0, T0, S1, R1, T1;

                if (blendRate < 1.0f) // Sedang blending dari pose lama
                {
                    S0 = DirectX::XMLoadFloat3(&node.scale);
                    R0 = DirectX::XMLoadFloat4(&node.rotate);
                    T0 = DirectX::XMLoadFloat3(&node.translate);
                }
                else // Tidak blending, gunakan keyframe sebelumnya sebagai basis
                {
                    S0 = DirectX::XMLoadFloat3(&key0.scale);
                    R0 = DirectX::XMLoadFloat4(&key0.rotate);
                    T0 = DirectX::XMLoadFloat3(&key0.translate);
                }

                S1 = DirectX::XMLoadFloat3(&key1.scale);
                R1 = DirectX::XMLoadFloat4(&key1.rotate);
                T1 = DirectX::XMLoadFloat3(&key1.translate);

                // Jika tidak blending, interpolasi S0->S1 dengan rate 't'.
                // Jika blending, interpolasi PoseLama(S0)->PoseBaru(S1) dengan 'blendRate'.
                // Catatan: Logika blending asli di base code agak simplistik,
                // tapi kita pertahankan strukturnya agar tidak merombak terlalu banyak.
                // Versi idealnya akan melakukan interpolasi keyframe dulu (dapat Pose Target),
                // baru di-blend dengan Pose Lama.
                // Kode di bawah ini adalah kompromi yang "cukup oke" untuk sekarang.
                float finalRate = (blendRate < 1.0f) ? blendRate : t;
                if (blendRate < 1.0f)
                {
                    // Saat blending, target kita adalah hasil interpolasi keyframe saat ini
                    DirectX::XMVECTOR targetS = DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&key0.scale), S1, t);
                    DirectX::XMVECTOR targetR = DirectX::XMQuaternionSlerp(DirectX::XMLoadFloat4(&key0.rotate), R1, t);
                    DirectX::XMVECTOR targetT = DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&key0.translate), T1, t);

                    // Blend dari pose lama ke target
                    S1 = targetS; R1 = targetR; T1 = targetT;
                }

                DirectX::XMStoreFloat3(&node.scale, DirectX::XMVectorLerp(S0, S1, finalRate));
                DirectX::XMStoreFloat4(&node.rotate, DirectX::XMQuaternionSlerp(R0, R1, finalRate));
                DirectX::XMStoreFloat3(&node.translate, DirectX::XMVectorLerp(T0, T1, finalRate));
            }
            break; // Keyframe ditemukan, keluar loop
        }
    }

    // Jangan lupa update matriks final
    model->UpdateTransform();
}