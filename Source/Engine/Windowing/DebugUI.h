#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Where a registered panel is placed inside the debug window.
enum class DebugPanelSlot : std::uint8_t
{
    menuBar, // the one-line menu bar; callback may only submit BeginMenu, MenuItem and text
    tab,     // one dockable window inside the debug window, below the menu bar
    // (unchanged)
};

// Internal storage shared between DebugUI and its handles. Do not use directly.
namespace DebugUIDetail
{
    struct Panel
    {
        std::uint32_t id{ 0 };
        DebugPanelSlot slot{ DebugPanelSlot::menuBar };
        std::string title{};
        std::function<void()> draw{};
        bool isAlive{ true }; // false = unregistered while a frame was being drawn
    };

    struct Registry
    {
        std::vector<Panel> panels{};
        std::vector<std::string> closedWindowIds{};
        std::uint32_t nextId{ 1 };
        bool isDrawing{ false };
        bool hasDeadPanels{ false };
    };
}

// Keeps one panel registered for as long as it lives. Destroying it removes
// the panel. Store it as a member of the object the draw callback captures.
class DebugPanelHandle
{
public:
    DebugPanelHandle() = default;
    ~DebugPanelHandle();

    DebugPanelHandle(DebugPanelHandle&& other) noexcept;
    DebugPanelHandle& operator=(DebugPanelHandle&& other) noexcept;
    DebugPanelHandle(const DebugPanelHandle&) = delete;
    DebugPanelHandle& operator=(const DebugPanelHandle&) = delete;

private:
    friend class DebugUI;
    DebugPanelHandle(std::weak_ptr<DebugUIDetail::Registry> registry, std::uint32_t id);

    void Release();

    // Weak: a handle may outlive DebugUI during static destruction.
    std::weak_ptr<DebugUIDetail::Registry> m_registry{};
    std::uint32_t m_id{ 0 }; // 0 = not registered
};

// Layout frame of the debug window: a one-line menu bar above a dockspace.
// Tab-slot panels are windows docked inside it, opened and closed from the Panels menu.
// The title of a menuBar panel is not drawn.
// // Systems register panels that draw only their contents; they never open
// an ImGui window themselves. Main thread only.
class DebugUI
{
public:
    static DebugUI& Instance();

    // The callback runs once per frame while the panel is visible. It must not
    // call RegisterPanel. Discarding the returned handle removes the panel at once.
    [[nodiscard]] DebugPanelHandle RegisterPanel(DebugPanelSlot slot, std::string title, std::function<void()> draw);

    // Draws the whole frame. Call once per frame, between ImGui NewFrame and Render.
    // `layoutScope` names the current context (the active scene). Dock layout is kept
    // per scope, so a panel that exists in several scenes has one position in each.
    // Must not be null.
    void Draw(const char* layoutScope);

private:
    DebugUI() = default;
    ~DebugUI() = default;
    DebugUI(const DebugUI&) = delete;
    DebugUI& operator=(const DebugUI&) = delete;

    std::shared_ptr<DebugUIDetail::Registry> m_registry{ std::make_shared<DebugUIDetail::Registry>() };
};

// Property rows for panel contents: label on the left, control on the right
// filling the remaining width. `label` is also the row's ImGui ID, so it must be
// unique within the enclosing ID scope (wrap each category in PushID / PopID).
// `isModified` draws an amber dot left of the label (value differs from its loaded baseline).
// Every function returns true on the frame the value was edited.
namespace DebugProperty
{
    bool SliderFloat(const char* label, float& value, float min, float max,
        const char* format = "%.3f", bool isModified = false);
    bool SliderInt(const char* label, int& value, int min, int max, bool isModified = false);
    bool DragFloat(const char* label, float& value, float speed, float min, float max,
        const char* format = "%.3f", bool isModified = false);

    // Two independent values on one row (for example X and Z).
    bool DragFloat2(const char* label, float& x, float& y, float speed, float min, float max,
        const char* format = "%.3f", bool isModified = false);

    // `xyz` points at 3 consecutive floats (for example &v.x of an XMFLOAT3). No range limit.
    bool DragFloat3(const char* label, float* xyz, float speed,
        const char* format = "%.3f", bool isModified = false);

    // A low / high pair on one row; the control keeps low <= high.
    bool DragFloatRange(const char* label, float& low, float& high, float speed, float min, float max,
        const char* format = "%.3f", bool isModified = false);

    bool Checkbox(const char* label, bool& value, bool isModified = false);
    bool InputInt(const char* label, int& value, bool isModified = false);

    // `rgba` points at 4 consecutive floats (for example &color.x of an XMFLOAT4).
    bool ColorEdit4(const char* label, float* rgba, bool isModified = false);

    // Read-only row: printf-style text in the disabled color.
    void Text(const char* label, const char* format, ...);
}