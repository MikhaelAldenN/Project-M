#pragma once
#include "IBossAttackPattern.h"
#include "ProjectileMesh.h"
#include "ProjectilePool.h"
#include <cstdint>
#include <string>
#include <vector>

class Player;

// Configuration for the Act 3 wall: one thin window, as tall as the screen, sweeping
// from one side to the other with a gapless column of bullets inside it.
struct WallParams {
    int   bulletCount = 16;        // USULAN DESAIN: bullets in the column (27 / 16 = 1.69 units apart)
    float bulletRadius = 0.6f;     // hitbox; above 0.35 the gaps are narrower than the player's body
    float visualScale = 3.6f;
    float speed = 10.0f;           // USULAN DESAIN: sweep speed, units per second
    float warningDuration = 0.7f;  // USULAN DESAIN: seconds the window blinks before the bullets appear
    float blinkInterval = 0.1f;    // seconds per blink half-cycle during the warning
    float windowWidth = 80.0f;     // canvas pixels
    int   damage = 10;
};

class AttackWall : public IBossAttackPattern {
public:
    // The wall starts on the side farther from `target` and sweeps to the other side.
    // `target` is borrowed: the scene owns the player and outlives the attack.
    // With no target the wall starts on the left.
    AttackWall(const WallParams& params, const Player* target);
    ~AttackWall() override = default;

    void Start(CueBoss* boss, ProjectilePool* pool) override;
    void Update(float dt, CueBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* camera, CueBoss* boss) override;
    void Stop(CueBoss* boss) override;

    bool IsFinished() const override;

private:
    enum class Stage : std::uint8_t {
        warning, // window blinks at the start edge, no bullets yet
        sweep,   // bullet column moves with the window
        done,    // window and bullets are gone
    };

    // Spawns the column at the current wall x, spread evenly over the screen height.
    void SpawnColumn();
    // Moves the wall and every remaining bullet of the column along the sweep.
    void MoveColumn(float dt);
    void UpdateWarningBlink(CueBoss* boss);
    void SetWindowAlpha(CueBoss* boss, float alpha);
    void DespawnColumn();
    void Finish(CueBoss* boss);

    WallParams m_params;
    const Player* m_target{ nullptr };
    ProjectilePool* m_pool{ nullptr }; // borrowed from the phase, set in Start
    ProjectileMesh m_mesh;
    std::vector<ProjectileHandle> m_column;
    std::string m_windowName;

    Stage m_stage{ Stage::warning };
    float m_stageTimer{ 0.0f }; // seconds since the current stage began
    float m_wallX{ 0.0f };
    float m_endX{ 0.0f };
    float m_direction{ 1.0f };  // +1 sweeps toward +x, -1 toward -x
};