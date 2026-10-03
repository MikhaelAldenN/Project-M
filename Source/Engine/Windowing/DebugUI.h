#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Where a registered panel is placed inside the debug window.
enum class DebugPanelSlot : std::uint8_t
{
    column, // left column, always visible
    tab,    // one tab in the right-hand tab bar
};

// Internal storage shared between DebugUI and its handles. Do not use directly.
namespace DebugUIDetail
{
    struct Panel
    {
        std::uint32_t id{ 0 };
        DebugPanelSlot slot{ DebugPanelSlot::column };
        std::string title{};
        std::function<void()> draw{};
        bool isAlive{ true }; // false = unregistered while a frame was being drawn
    };

    struct Registry
    {
        std::vector<Panel> panels{};
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

// Layout frame of the debug window: a fixed left column plus a tab bar.
// Systems register panels that draw only their contents; they never open
// an ImGui window themselves. Main thread only.
class DebugUI
{
public:
    static DebugUI& Instance();

    // The callback runs once per frame while the panel is visible. It must not
    // call RegisterPanel. Discarding the returned handle removes the panel at once.
    [[nodiscard]] DebugPanelHandle RegisterPanel(DebugPanelSlot slot, std::string title, std::function<void()> draw);

    // Draws the whole frame. Call once per frame, between ImGui NewFrame and Render.
    void Draw();

private:
    DebugUI() = default;
    ~DebugUI() = default;
    DebugUI(const DebugUI&) = delete;
    DebugUI& operator=(const DebugUI&) = delete;

    std::shared_ptr<DebugUIDetail::Registry> m_registry{ std::make_shared<DebugUIDetail::Registry>() };
};