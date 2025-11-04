#include "SceneManager.h"

void SceneManager::Update(float elapsedTime)
{
    if (nextScene != nullptr)
    {
        // Cleanup scene lama
        if (currentScene != nullptr)
        {
            currentScene->Finalize();
            delete currentScene;
        }

        // Set scene baru
        currentScene = nextScene;
        nextScene = nullptr;

        // Inisialisasi hanya jika scene tidak dalam keadaan loading
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