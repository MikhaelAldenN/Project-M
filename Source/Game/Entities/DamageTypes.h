#pragma once

#include <cstdint>
#include <DirectXMath.h>

// One hit, as passed to every damage receiver (Player, Enemy, NaviAlly, boss phases).
struct DamageInfo
{
    int amount{ 0 };                 // HP to remove
    DirectX::XMFLOAT3 hitPosition{}; // world space; receivers that place hit feedback at the impact use it
};

// What a receiver did with a hit. Callers use it to decide on feedback and follow-up.
enum class DamageResult : std::uint8_t
{
    ignored, // receiver was invincible, inactive or already dead; nothing changed
    damaged, // HP was reduced and the receiver is still alive
    killed,  // this hit brought HP to zero
};