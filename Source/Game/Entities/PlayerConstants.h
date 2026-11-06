#pragma once

// ============================================================
// PLAYER CONSTANTS
// All tunable values for Player, PlayerStates, and related
// systems live here. Change a number once, it applies everywhere.
// ============================================================

namespace PlayerConst
{
    // --- Movement ---
    inline constexpr float MoveSpeed = 15.0f;  // Default walk speed (units/s)
    inline constexpr float Acceleration = 8.0f;   // Input ramp-up rate
    inline constexpr float Deceleration = 12.0f;  // Input ramp-down rate
    inline constexpr float RotSmoothSpeed = 15.0f;  // Foot yaw lerp speed (rad/s)

    // --- Stage collision (KinematicMover, Y stays fixed) ---
    inline constexpr float CollisionRadius = 0.5f;      // Swept sphere radius on the XZ plane
    inline constexpr float StepOffset = 0.5f;           // Stage geometry lower than this above the feet does not block
    inline constexpr float SkinWidth = 0.01f;           // Gap kept from walls
    inline constexpr float GroundSnapDistance = 10.0f;  // How far below the spawn point the floor is searched

    // --- Dash ---
    inline constexpr float DashSpeed = 45.0f;  // Burst speed during dash (units/s)
    inline constexpr float DashDuration = 0.15f;  // How long the dash lasts (s)
    inline constexpr float DashIFrameDuration = 0.2f;  // Invincibility granted by a full dash (s)
    inline constexpr int   DashCharges = 2;      // Dashes available before a press fails
    inline constexpr float DashRecoveryTime = 0.8f;   // USULAN DESAIN: time without dashing that restores all charges (s)

    // --- Health ---
    inline constexpr int   MaxHits = 6;               // USULAN DESAIN: hits that kill the player; every bullet is one hit
    inline constexpr float HitIFrameDuration = 1.0f;  // Invincibility after a hit (s)
    inline constexpr float HitBlinkInterval = 0.1f;   // USULAN DESAIN: model hidden, then shown, every this many seconds during it

    // --- Shoot ---
    inline constexpr float ShootInterval = 0.15f;           // USULAN DESAIN: seconds between shots while held
    inline constexpr float OverdriveShootInterval = 0.05f;  // Seconds between shots during overdrive
    inline constexpr float MinShootInterval = 1.0f / 60.0f; // Floor: an interval of 0 (Windowkill cage) fires 60 shots/s at any frame rate

    // --- Aiming ---
    inline constexpr float AimMinDistSq = 0.0f;   // Minimum sq-distance to aim target before clamping
    inline constexpr float MaxTorsoAngle = 1.5707963f; // XM_PIDIV2 — max upper-body twist (rad)

    // --- Projectile (player-fired) ---
    inline constexpr float BulletSpeed = 50.0f;  // Speed of player bullets (units/s)
    inline constexpr float BulletSpawnFwd = 1.5f;   // Spawn offset forward from player center
    inline constexpr float BulletSpawnY = 1.0f;   // Spawn height offset (chest level)
    inline constexpr int   MaxBullets = 150;      // Hard cap on active player projectiles

    // --- Animation blend times ---
    inline constexpr float AnimBlendDefault = 0.2f;   // Standard cross-fade duration (s)
}