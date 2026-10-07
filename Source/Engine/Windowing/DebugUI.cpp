#include "DebugUI.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdarg>
#include <utility>
#include <imgui.h>

namespace
{
    using DebugUIDetail::Panel;
    using DebugUIDetail::Registry;

    constexpr ImGuiWindowFlags kRootFlags{
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking };

    void RemovePanel(Registry& registry, std::uint32_t id)
    {
        if (registry.isDrawing)
        {
            // Why: erasing now could destroy the callback that is currently running.
            for (Panel& panel : registry.panels)
            {
                if (panel.id == id) panel.isAlive = false;
            }
            registry.hasDeadPanels = true;
            return;
        }

        registry.panels.erase(
            std::remove_if(registry.panels.begin(), registry.panels.end(),
                [id](const Panel& panel) { return panel.id == id; }),
            registry.panels.end());
    }

    void DrawSlotPanels(const Registry& registry, DebugPanelSlot slot)
    {
        for (const Panel& panel : registry.panels)
        {
            if (!panel.isAlive || panel.slot != slot) continue;

            ImGui::PushID(static_cast<int>(panel.id)); // two panels may use the same menu label
            panel.draw();
            ImGui::PopID();
        }
    }

    // Identity of a panel window, and the key of its closed state. The same title
    // gets a separate identity in every scope.
    std::string MakeWindowId(const Panel& panel, const char* layoutScope)
    {
        std::string windowId{ panel.title };
        windowId += '@';
        windowId += layoutScope;
        return windowId;
    }

    bool IsPanelOpen(const Registry& registry, const std::string& windowId)
    {
        return std::find(registry.closedWindowIds.begin(), registry.closedWindowIds.end(), windowId)
            == registry.closedWindowIds.end();
    }

    void SetPanelOpen(Registry& registry, const std::string& windowId, bool isOpen)
    {
        const auto found{ std::find(registry.closedWindowIds.begin(), registry.closedWindowIds.end(), windowId) };
        if (isOpen)
        {
            if (found != registry.closedWindowIds.end()) registry.closedWindowIds.erase(found);
        }
        else if (found == registry.closedWindowIds.end())
        {
            registry.closedWindowIds.push_back(windowId);
        }
    }

    // One checkable entry per tab-slot panel of the current scope.
    void DrawPanelsMenu(Registry& registry, const char* layoutScope)
    {
        if (!ImGui::BeginMenu("Panels")) return;

        bool hasPanel{ false };
        for (const Panel& panel : registry.panels)
        {
            if (!panel.isAlive || panel.slot != DebugPanelSlot::tab) continue;
            hasPanel = true;

            const std::string windowId{ MakeWindowId(panel, layoutScope) };
            bool isOpen{ IsPanelOpen(registry, windowId) };

            ImGui::PushID(static_cast<int>(panel.id)); // two panels may share a title
            if (ImGui::MenuItem(panel.title.c_str(), nullptr, &isOpen))
            {
                SetPanelOpen(registry, windowId, isOpen);
            }
            ImGui::PopID();
        }
        if (!hasPanel) ImGui::TextDisabled("No panels in this scene");

        ImGui::EndMenu();
    }

    // Each open tab-slot panel is one dockable ImGui window; the owner draws contents only.
    void DrawPanelWindows(Registry& registry, const char* layoutScope, ImGuiID dockSpaceId)
    {
        for (const Panel& panel : registry.panels)
        {
            if (!panel.isAlive || panel.slot != DebugPanelSlot::tab) continue;

            const std::string windowId{ MakeWindowId(panel, layoutScope) };
            if (!IsPanelOpen(registry, windowId)) continue;

            // "Title###id": only the text after ### is the window's identity.
            const std::string windowName{ panel.title + "###" + windowId };

            // Why FirstUseEver: only a window with no saved layout is put into the
            // dockspace; a position the user chose is never overridden.
            ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_FirstUseEver);

            bool isOpen{ true };
            // Begin returns false while the window is a hidden dock tab; End is still required.
            if (ImGui::Begin(windowName.c_str(), &isOpen))
            {
                panel.draw();
            }
            ImGui::End();

            if (!isOpen) SetPanelOpen(registry, windowId, false); // closed with the X button
        }
    }
}

// ---------------------------------------------------------------------------
// DebugPanelHandle
// ---------------------------------------------------------------------------
DebugPanelHandle::DebugPanelHandle(std::weak_ptr<DebugUIDetail::Registry> registry, std::uint32_t id)
    : m_registry{ std::move(registry) }
    , m_id{ id }
{
}

DebugPanelHandle::~DebugPanelHandle()
{
    Release();
}

DebugPanelHandle::DebugPanelHandle(DebugPanelHandle&& other) noexcept
    : m_registry{ std::move(other.m_registry) }
    , m_id{ std::exchange(other.m_id, 0) }
{
}

DebugPanelHandle& DebugPanelHandle::operator=(DebugPanelHandle&& other) noexcept
{
    if (this != &other)
    {
        Release();
        m_registry = std::move(other.m_registry);
        m_id = std::exchange(other.m_id, 0);
    }
    return *this;
}

void DebugPanelHandle::Release()
{
    if (const auto registry{ m_registry.lock() })
    {
        RemovePanel(*registry, m_id);
    }
    m_registry.reset();
    m_id = 0;
}

// ---------------------------------------------------------------------------
// DebugUI
// ---------------------------------------------------------------------------
DebugUI& DebugUI::Instance()
{
    static DebugUI s_instance;
    return s_instance;
}

DebugPanelHandle DebugUI::RegisterPanel(DebugPanelSlot slot, std::string title, std::function<void()> draw)
{
    assert(draw && "DebugUI::RegisterPanel: draw callback is empty");
    // Why: adding to the list while it is being iterated would invalidate the running callback.
    assert(!m_registry->isDrawing && "DebugUI::RegisterPanel: called from inside a panel callback");

    const std::uint32_t id{ m_registry->nextId++ };
    m_registry->panels.push_back(Panel{ id, slot, std::move(title), std::move(draw) });

    return DebugPanelHandle{ m_registry, id };
}

void DebugUI::Draw(const char* layoutScope)
{
    assert(layoutScope && "DebugUI::Draw: layoutScope is null");
    Registry& registry{ *m_registry };

    const ImGuiViewport* viewport{ ImGui::GetMainViewport() };
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    const bool isOpen{ ImGui::Begin("##DebugUIRoot", nullptr, kRootFlags) };
    ImGui::PopStyleVar(2);

    ImGuiID dockSpaceId{ 0 };
    if (isOpen)
    {
        registry.isDrawing = true;

        if (ImGui::BeginMenuBar())
        {
            DrawSlotPanels(registry, DebugPanelSlot::menuBar);
            DrawPanelsMenu(registry, layoutScope);
            ImGui::EndMenuBar();
        }


        // One dockspace per scope, so every scene keeps its own split tree.
        // Why before the panel windows: a window can only dock into a dockspace
        // that was already submitted this frame.
        ImGui::PushID(layoutScope);
        dockSpaceId = ImGui::GetID("##DockSpace");
        ImGui::PopID();
        ImGui::DockSpace(dockSpaceId, ImVec2{ 0.0f, 0.0f }, ImGuiDockNodeFlags_None);
    }
    ImGui::End();

    if (isOpen)
    {
        // Why after End: panel windows are top-level windows, not children of the root.
        DrawPanelWindows(registry, layoutScope, dockSpaceId);
        registry.isDrawing = false;
    }

    // Panels unregistered during this frame are erased now that no callback is running.
    if (registry.hasDeadPanels)
    {
        registry.panels.erase(
            std::remove_if(registry.panels.begin(), registry.panels.end(),
                [](const Panel& panel) { return !panel.isAlive; }),
            registry.panels.end());
        registry.hasDeadPanels = false;
    }
}

// ---------------------------------------------------------------------------
// DebugProperty
// ---------------------------------------------------------------------------
namespace
{
    // Layout values are relative to the window and font so rows still fit a narrow docked slot.
    constexpr float k_controlColumnFraction{ 0.40f }; // where controls start, as a share of content width
    constexpr float k_markerWidthInEm{ 0.75f };       // gutter left of the label, reserved on every row
    constexpr float k_markerRadiusInEm{ 0.18f };
    const ImVec4 k_modifiedColor{ 1.0f, 0.75f, 0.0f, 1.0f }; // amber

    // Draws the marker gutter and the label, then leaves the cursor at the control column
    // with the next item set to fill the remaining width.
    void BeginRow(const char* label, bool isModified)
    {
        const float fontSize{ ImGui::GetFontSize() };
        const float frameHeight{ ImGui::GetFrameHeight() };
        const float markerWidth{ fontSize * k_markerWidthInEm };
        const float rowStartX{ ImGui::GetCursorPosX() };
        // Why add scroll back: SameLine() takes an offset from the unscrolled window edge.
        const float controlX{ ImGui::GetWindowContentRegionMin().x + ImGui::GetScrollX()
            + ImGui::GetWindowContentRegionWidth() * k_controlColumnFraction };

        ImGui::AlignTextToFramePadding();

        if (isModified)
        {
            const ImVec2 rowPos{ ImGui::GetCursorScreenPos() };
            const ImVec2 center{ rowPos.x + markerWidth * 0.5f, rowPos.y + frameHeight * 0.5f };
            ImGui::GetWindowDrawList()->AddCircleFilled(
                center, fontSize * k_markerRadiusInEm, ImGui::GetColorU32(k_modifiedColor));
        }
        ImGui::SetCursorPosX(rowStartX + markerWidth);

        // Why clip: a label longer than its column must not run underneath the control.
        const ImVec2 labelPos{ ImGui::GetCursorScreenPos() };
        const float labelMaxX{ ImGui::GetWindowPos().x - ImGui::GetScrollX() + controlX
            - ImGui::GetStyle().ItemSpacing.x };
        ImGui::PushClipRect(labelPos, ImVec2{ labelMaxX, labelPos.y + frameHeight }, true);
        ImGui::TextUnformatted(label);
        ImGui::PopClipRect();

        ImGui::SameLine(controlX);
        ImGui::SetNextItemWidth(-1.0f);
    }

    // Shared frame of every editable row. `drawControl` submits one widget and returns "edited".
    template <typename DrawControl>
    bool Row(const char* label, bool isModified, DrawControl drawControl)
    {
        ImGui::PushID(label);
        BeginRow(label, isModified);
        const bool isEdited{ drawControl() };
        ImGui::PopID();
        return isEdited;
    }
}

bool DebugProperty::SliderFloat(const char* label, float& value, float min, float max,
    const char* format, bool isModified)
{
    return Row(label, isModified, [&]() { return ImGui::SliderFloat("##value", &value, min, max, format); });
}

bool DebugProperty::SliderInt(const char* label, int& value, int min, int max, bool isModified)
{
    return Row(label, isModified, [&]() { return ImGui::SliderInt("##value", &value, min, max); });
}

bool DebugProperty::DragFloat(const char* label, float& value, float speed, float min, float max,
    const char* format, bool isModified)
{
    return Row(label, isModified, [&]() { return ImGui::DragFloat("##value", &value, speed, min, max, format); });
}

bool DebugProperty::DragFloat2(const char* label, float& x, float& y, float speed, float min, float max,
    const char* format, bool isModified)
{
    return Row(label, isModified, [&]() {
        // Why a local copy: the two values need not be adjacent members of the caller's struct.
        std::array<float, 2> values{ x, y };
        const bool isEdited{ ImGui::DragFloat2("##value", values.data(), speed, min, max, format) };
        if (isEdited)
        {
            x = values[0];
            y = values[1];
        }
        return isEdited;
        });
}

bool DebugProperty::DragFloat3(const char* label, float* xyz, float speed, const char* format, bool isModified)
{
    assert(xyz && "DebugProperty::DragFloat3: xyz is null");
    return Row(label, isModified, [&]() { return ImGui::DragFloat3("##value", xyz, speed, 0.0f, 0.0f, format); });
}

bool DebugProperty::DragFloatRange(const char* label, float& low, float& high, float speed, float min, float max,
    const char* format, bool isModified)
{
    return Row(label, isModified, [&]() {
        return ImGui::DragFloatRange2("##value", &low, &high, speed, min, max, format);
        });
}

bool DebugProperty::Checkbox(const char* label, bool& value, bool isModified)
{
    return Row(label, isModified, [&]() { return ImGui::Checkbox("##value", &value); });
}

bool DebugProperty::InputInt(const char* label, int& value, bool isModified)
{
    return Row(label, isModified, [&]() { return ImGui::InputInt("##value", &value); });
}

bool DebugProperty::ColorEdit4(const char* label, float* rgba, bool isModified)
{
    assert(rgba && "DebugProperty::ColorEdit4: rgba is null");
    return Row(label, isModified, [&]() { return ImGui::ColorEdit4("##value", rgba); });
}

void DebugProperty::Text(const char* label, const char* format, ...)
{
    BeginRow(label, false);

    va_list args;
    va_start(args, format);
    ImGui::TextDisabledV(format, args);
    va_end(args);
}