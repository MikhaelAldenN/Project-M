#pragma once

#include <memory>
#include "System/GamePad.h"
#include "System/Mouse.h"

// インプット
class Input
{
private:
	Input() = default;
	~Input() = default;

public:
	// インスタンス取得
	static Input& Instance()
	{
		static Input instance;
		return instance;
	}

	// 初期化
	void Initialize(HWND hWnd);

	// 更新処理
	void Update();

	// ゲームパッド取得
	GamePad& GetGamePad() { return *gamePad; }

	// マウス取得
	Mouse& GetMouse() { return *mouse; }

private:
	std::unique_ptr<GamePad>	gamePad;
	std::unique_ptr<Mouse>		mouse;
};
