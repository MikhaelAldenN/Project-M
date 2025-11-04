#include "System/Graphics.h"
#include "SceneTitle.h"
#include "System/Input.h"
#include "SceneGame.h"
#include "SceneManager.h"
#include "SceneLoading.h"

SceneTitle::~SceneTitle()
{

}

//初期化
void SceneTitle::Initialize()
{
	sprite = new Sprite("Data/Sprite/TitleLogo.png");
}

//終了化
void SceneTitle::Finalize()
{
	if (sprite != nullptr)
	{
		delete sprite;
		sprite = nullptr;
	}
}

//更新処理
void SceneTitle::Update(float elapsedTime)
{
	GamePad& gamePad = Input::Instance().GetGamePad();

	const GamePadButton anyButton =
		GamePad::BTN_A
		| GamePad::BTN_B
		| GamePad::BTN_X
		| GamePad::BTN_Y;

	if (gamePad.GetButtonDown() & anyButton)
	{
		SceneManager::Instance().ChangeScene(new SceneLoading(new SceneGame));
	}
}

//描画処理
void SceneTitle::Render()
{
	Graphics& graphics = Graphics::Instance();
	ID3D11DeviceContext* dc = graphics.GetDeviceContext();
	RenderState* renderState = graphics.GetRenderState();

	//書画準備
	RenderContext rc;
	rc.deviceContext = dc;
	rc.renderState = graphics.GetRenderState();

	// 2Dスプライト描画
	{
		float screenWidth = static_cast<float>(graphics.GetScreenWidth());
		float screenHeight = static_cast<float>(graphics.GetScreenHeight());
		sprite->Render(rc,
			screenWidth * 0.5f, screenHeight * 0.5f, 0,
			842, 99,
			0,
			1, 1, 1, 1);
	}
}

//GUI描画
void SceneTitle::DrawGUI()
{

}