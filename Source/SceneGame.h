#pragma once

#include "Scene.h"
#include "Stage.h"
#include "Player.h"
#include "CameraController.h"

/// =============================================================
/// SceneGame
/// - Main game scene: manages stage, player, camera, and updates
/// =============================================================
class SceneGame : public Scene
{
public:
    // =================================================================
    // Constructor/Destructor
    // =================================================================
    SceneGame() {};
    ~SceneGame() override {};

    // =================================================================
    // Main Scene Logic
    // =================================================================
    void Initialize() override;                 // Initialize scene// <-- 3. Tambahkan 'override' (opsional, tapi sangat disarankan)
    void Finalize() override;                   // Cleanup scene
    void Update(float elapsedTime) override;    // Update all game objects
    void Render() override;                     // Render all visuals
    void DrawGUI() override;                    // Draw GUI

private:
    // =================================================================
    // Scene Data Members
    // =================================================================
    Stage* stage = nullptr;
    Player* player = nullptr;
    CameraController* cameraController = nullptr;
};