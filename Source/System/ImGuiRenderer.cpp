#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <ImGuizmo.h>
#include "ImGuiRenderer.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
	// Neutral dark-gray editor theme, modelled on the Unreal Engine 5 Details panel.
	// Global: it also restyles the DrawGUI() windows drawn in Release.
	void ApplyEditorTheme()
	{
		// Base palette first, so every ImGuiCol_ not overridden below keeps a valid value.
		ImGui::StyleColorsDark();

		const auto gray = [](float value, float alpha = 1.0f) { return ImVec4{ value, value, value, alpha }; };

		// USULAN DESAIN: Unreal-style blue accent. To change the accent, edit only these two lines.
		const ImVec4 accent{ 0.00f, 0.44f, 0.88f, 1.00f };
		const ImVec4 accentBright{ 0.15f, 0.55f, 1.00f, 1.00f };

		ImVec4* colors{ ImGui::GetStyle().Colors };

		// Text
		colors[ImGuiCol_Text] = gray(0.78f);
		colors[ImGuiCol_TextDisabled] = gray(0.47f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4{ accent.x, accent.y, accent.z, 0.45f };

		// Surfaces
		colors[ImGuiCol_WindowBg] = gray(0.14f);
		colors[ImGuiCol_ChildBg] = gray(0.00f, 0.00f);
		colors[ImGuiCol_PopupBg] = gray(0.10f, 0.98f);
		colors[ImGuiCol_Border] = gray(0.24f);
		colors[ImGuiCol_BorderShadow] = gray(0.00f, 0.00f);
		colors[ImGuiCol_DockingEmptyBg] = gray(0.06f);

		// Input fields: darker than the panel they sit on
		colors[ImGuiCol_FrameBg] = gray(0.06f);
		colors[ImGuiCol_FrameBgHovered] = gray(0.10f);
		colors[ImGuiCol_FrameBgActive] = gray(0.12f);

		// Title bar, menu bar and tabs share one color; the active tab merges with the panel
		colors[ImGuiCol_TitleBg] = gray(0.08f);
		colors[ImGuiCol_TitleBgActive] = gray(0.08f);
		colors[ImGuiCol_TitleBgCollapsed] = gray(0.08f);
		colors[ImGuiCol_MenuBarBg] = gray(0.08f);
		colors[ImGuiCol_Tab] = gray(0.08f);
		colors[ImGuiCol_TabHovered] = gray(0.20f);
		colors[ImGuiCol_TabActive] = gray(0.14f);
		colors[ImGuiCol_TabUnfocused] = gray(0.08f);
		colors[ImGuiCol_TabUnfocusedActive] = gray(0.14f);

		// Category headers: a neutral bar. Header* is also used by Selectable and MenuItem.
		colors[ImGuiCol_Header] = gray(0.20f);
		colors[ImGuiCol_HeaderHovered] = gray(0.26f);
		colors[ImGuiCol_HeaderActive] = gray(0.30f);

		// Buttons and scrollbars stay neutral
		colors[ImGuiCol_Button] = gray(0.22f);
		colors[ImGuiCol_ButtonHovered] = gray(0.29f);
		colors[ImGuiCol_ButtonActive] = gray(0.35f);
		colors[ImGuiCol_ScrollbarBg] = gray(0.10f);
		colors[ImGuiCol_ScrollbarGrab] = gray(0.28f);
		colors[ImGuiCol_ScrollbarGrabHovered] = gray(0.34f);
		colors[ImGuiCol_ScrollbarGrabActive] = gray(0.40f);
		colors[ImGuiCol_Separator] = gray(0.24f);
		colors[ImGuiCol_ResizeGrip] = gray(0.24f, 0.50f);

		// Accent: the only non-gray colors in the theme
		colors[ImGuiCol_CheckMark] = accentBright;
		colors[ImGuiCol_SliderGrab] = accent;
		colors[ImGuiCol_SliderGrabActive] = accentBright;
		colors[ImGuiCol_SeparatorHovered] = accent;
		colors[ImGuiCol_SeparatorActive] = accentBright;
		colors[ImGuiCol_ResizeGripHovered] = accent;
		colors[ImGuiCol_ResizeGripActive] = accentBright;
		colors[ImGuiCol_DockingPreview] = ImVec4{ accent.x, accent.y, accent.z, 0.50f };
		colors[ImGuiCol_DragDropTarget] = accentBright;
		colors[ImGuiCol_NavHighlight] = accent;

		// Sizes, in raw pixels (not DPI-scaled). Tighter than the ImGui defaults.
		ImGuiStyle& style{ ImGui::GetStyle() };

		// Square panels with a thin outline
		style.WindowPadding = ImVec2{ 6.0f, 6.0f };
		style.WindowRounding = 0.0f;
		style.WindowBorderSize = 1.0f;
		style.ChildRounding = 0.0f;
		style.PopupRounding = 2.0f;

		// Fields: small rounded corners and a 1 px outline so dark fields keep an edge
		style.FramePadding = ImVec2{ 4.0f, 2.0f };
		style.FrameRounding = 3.0f;
		style.FrameBorderSize = 1.0f;
		style.GrabMinSize = 8.0f;
		style.GrabRounding = 2.0f;

		// Denser rows
		style.ItemSpacing = ImVec2{ 6.0f, 3.0f };
		style.ItemInnerSpacing = ImVec2{ 4.0f, 3.0f };
		style.IndentSpacing = 16.0f;

		style.ScrollbarSize = 12.0f;
		style.ScrollbarRounding = 3.0f;
		style.TabRounding = 2.0f;
	}
}

// 初期化
void ImGuiRenderer::Initialize(HWND hWnd, ID3D11Device* device, ID3D11DeviceContext* dc)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
	//io.ConfigViewportsNoAutoMerge = true;
	//io.ConfigViewportsNoTaskBarIcon = true;
	//io.ConfigViewportsNoDefaultParent = true;
	//io.ConfigDockingAlwaysTabBar = true;
	//io.ConfigDockingTransparentPayload = true;
#if 1
	io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;     // FIXME-DPI: THIS CURRENTLY DOESN'T WORK AS EXPECTED. DON'T USE IN USER APP!
	io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports; // FIXME-DPI
#endif

	// Setup Dear ImGui style
	//ImGui::StyleColorsDark();
	ApplyEditorTheme();
	//ImGui::StyleColorsClassic();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device, dc);

	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
	// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
	// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
	// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	//io.Fonts->AddFontDefault();
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
	//io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
	ImFont* font = io.Fonts->AddFontFromFileTTF("Data/Font/ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	IM_ASSERT(font != NULL);
}

// 終了化
void ImGuiRenderer::Finalize()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

// フレーム開始処理
void ImGuiRenderer::NewFrame()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();

	ImVec2 pos = ImGui::GetMainViewport()->GetWorkPos();
	ImVec2 size = ImGui::GetIO().DisplaySize;
	ImGuizmo::BeginFrame();
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);

#if 0
	// Docking
	const ImGuiWindowFlags window_flags = ImGuiWindowFlags_None
		| ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoBringToFrontOnFocus
		| ImGuiWindowFlags_NoNavFocus
		| ImGuiWindowFlags_NoBackground
		;
	const ImGuiDockNodeFlags docspace_flags = ImGuiDockNodeFlags_None
		//| ImGuiDockNodeFlags_KeepAliveOnly
		| ImGuiDockNodeFlags_PassthruCentralNode
		;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	bool dock_open = true;
	if (ImGui::Begin("MainDockspace", &dock_open, window_flags))
	{
		ImGui::PopStyleVar(3);

		ImGuiID dockspaceId = ImGui::GetID("MyDockspace");
		ImGui::DockSpace(dockspaceId, ImVec2(0, 0), docspace_flags);
	}
	ImGui::End();
#endif
}

// 描画
void ImGuiRenderer::Render(ID3D11DeviceContext* context)
{
	// Rendering
	ImGui::Render();

	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	//// Update and Render additional Platform Windows
	//ImGuiIO& io = ImGui::GetIO();
	//if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	//{
	//	ImGui::UpdatePlatformWindows();
	//	ImGui::RenderPlatformWindowsDefault();
	//}
}

// WIN32メッセージハンドラー
LRESULT ImGuiRenderer::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}
