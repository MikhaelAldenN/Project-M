#pragma once

#include "System/Sprite.h"
#include "Scene.h"

class SceneTitle : public Scene
{
public:
	SceneTitle() {}
	~SceneTitle() override;

	void Initialize() override;
	void Finalize() override;
	void Update(float elapsedTime) override;
	void Render() override;
	void DrawGUI() override;

private:
	Sprite* sprite = nullptr;
};