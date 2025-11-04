#pragma once

#include "Scene.h"

class SceneManager
{
private:
	SceneManager() {}
	~SceneManager() {}

public:
	static SceneManager& Instance()
	{
		static SceneManager instance;
		return instance;
	}

	void Update(float elapsedTime);
	void Render();
	void DrawGUI();
	void Clear();
	void ChangeScene(Scene* scene);

private:
	Scene* currentScene = nullptr;
	Scene* nextScene = nullptr;

};