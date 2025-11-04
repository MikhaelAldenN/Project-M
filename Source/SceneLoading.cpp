#include "System/Graphics.h"
#include "System/Input.h"
#include "SceneLoading.h"
#include "SceneManager.h"

void SceneLoading::Initialize()
{
	sprite = new Sprite("Data/Sprite/LoadingIcon.png");

	thread = new std::thread(LoadingThread, this);
}

void SceneLoading::Finalize()
{
	if (sprite != nullptr)
	{
		delete sprite;
		sprite = nullptr;
		thread = nullptr;
	}
}

void SceneLoading::Update(float elapsedTime)
{
	constexpr float speed = 180;
	angle += speed * elapsedTime;
	if (nextScene->IsReady())
	{
		SceneManager::Instance().ChangeScene(nextScene);
		nextScene = nullptr;
	}
}

void SceneLoading::Render()
{
	Graphics& graphics = Graphics::Instance();
	ID3D11DeviceContext* dc = graphics.GetDeviceContext();
	RenderState* renderState = graphics.GetRenderState();

	RenderContext rc;
	rc.deviceContext = dc;
	rc.renderState = graphics.GetRenderState();

	{
		float screenWidth = static_cast<float>(graphics.GetScreenWidth());
		float screenHeight = static_cast<float>(graphics.GetScreenHeight());
		float spriteWidth = 256;
		float spriteHeight = 256;
		float positionX = screenWidth - spriteWidth;
		float positionY = screenHeight - spriteHeight;

		sprite->Render(rc,
			positionX, positionY, 0, spriteWidth, spriteHeight,
			angle,
			1, 1, 1, 1);
	}
}


void SceneLoading::DrawGUI()
{

}

void SceneLoading::LoadingThread(SceneLoading* scene)
{
	// COM関連の初期化でスレッド毎に呼ぶ必要がある
	CoInitialize(nullptr);

	// 次のシーンの初期化を行う
	scene->nextScene->Initialize();

	// スレッドが終わる前にCOM関連の終了化
	CoUninitialize();

	// 次のシーンの準備完了設定
	scene->nextScene->SetReady();
}