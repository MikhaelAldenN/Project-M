#pragma once

#include "Scene.h"
#include "Stage.h"
#include "Player.h"
#include "CameraController.h"

/// SceneGame
/// - Main game scene: manages stage, player, camera, and updates
class SceneGame : public Scene
{
public:
    SceneGame() {};
    ~SceneGame() override {};

    void Initialize() override;                
    void Finalize() override;                  
    void Update(float elapsedTime) override;   
    void Render() override;                    
    void DrawGUI() override;                   

private:
    // Scene Data Members
    Stage* stage = nullptr;
    Player* player = nullptr;
    CameraController* cameraController = nullptr;
};