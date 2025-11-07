#include "SceneManager.h"

void SceneManager::Update(float elapsedTime)
{
    if (nextScene != nullptr)
    {
		// Old scene cleanup
        if (currentScene != nullptr)
        {
            currentScene->Finalize();
            delete currentScene;
        }

		// Set new scene
        currentScene = nextScene;
        nextScene = nullptr;

		// Initialize new scene
        if (!currentScene->IsReady())
        {
            currentScene->Initialize();
        }
    }

    if (currentScene != nullptr)
    {
        currentScene->Update(elapsedTime);
    }
}

void SceneManager::Render()
{
    if (currentScene != nullptr)
    {
        currentScene->Render();
    }
}

void SceneManager::DrawGUI()
{
    if (currentScene != nullptr)
    {
        currentScene->DrawGUI();
    }
}

void SceneManager::Clear()
{
    if (currentScene != nullptr)
    {
        currentScene->Finalize();
        delete currentScene;
        currentScene = nullptr;
    }
}

void SceneManager::ChangeScene(Scene* scene)
{
    nextScene = scene;
}