#pragma once

#include <cstdint>

// シーン基底
class Scene
{
public:
	// Identifies the concrete scene without a cast. none means "no scene";
	// no scene returns it from GetKind.
	enum class Kind
	{
		none,
		intro,
		title,
		game,
		sandbox,
		boss,
	};

	Scene() = default;
	virtual ~Scene() = default;

	// 更新処理
	virtual void Update(float elapsedTime) {}

	// 描画処理
	virtual void Render(float dt, class Camera* camera = nullptr) = 0;
	// GUI描画処理
	virtual void DrawGUI() {}

	[[nodiscard]] virtual Kind GetKind() const = 0;

	// Called when the user closes an OS window other than the main window.
	// sdlWindowID is SDL's ID of that window. Scenes that spawn no windows ignore it.
	virtual void OnSubWindowClosed(std::uint32_t /*sdlWindowID*/) {}
};
