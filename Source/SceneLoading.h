#pragma once

#include "System/Sprite.h"
#include "Scene.h"
#include <thread>

class SceneLoading : public Scene
{
public:
	SceneLoading(Scene* nextScene) : nextScene(nextScene) {}
	~SceneLoading() {}

	void Initialize() override;
	void Finalize() override;
	void Update(float elapsedTime) override;
	void Render() override;
	void DrawGUI() override;

private:
	static void LoadingThread(SceneLoading* scene);

private:
	Sprite* sprite = nullptr;
	float angle = 0.0f;
	Scene* nextScene = nullptr;
	std::thread* thread = nullptr;
};