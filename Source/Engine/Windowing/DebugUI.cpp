#include "DebugUI.h"

#include <algorithm>
#include <cassert>
#include <utility>
#include <imgui.h>

namespace
{
    using DebugUIDetail::Panel;
    using DebugUIDetail::Registry;

    constexpr float kColumnWidth{ 220.0f };

    // Root fills the client area, cannot be moved, and stays behind any
    // floating window so legacy ImGui windows remain reachable.
    constexpr ImGuiWindowFlags kRootFlags{
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

    void DrawColumnPanels(const Registry& registry)
    {
        for (const Panel& panel : registry.panels)
        {
            if (!panel.isAlive || panel.slot != DebugPanelSlot::column) continue;

            ImGui::PushID(static_cast<int>(panel.id)); // two panels may share a title
            ImGui::TextUnformatted(panel.title.c_str());
            ImGui::Separator();
            panel.draw();
            ImGui::Spacing();
            ImGui::PopID();
        }
    }

    void DrawTabPanels(const Registry& registry)
    {
        for (const Panel& panel : registry.panels)
        {
            if (!panel.isAlive || panel.slot != DebugPanelSlot::tab) continue;

            ImGui::PushID(static_cast<int>(panel.id));
            if (ImGui::BeginTabItem(panel.title.c_str()))
            {
                panel.draw();
                ImGui::EndTabItem();
            }
            ImGui::PopID();
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

void DebugUI::Draw()
{
    Registry& registry{ *m_registry };

    const ImGuiViewport* viewport{ ImGui::GetMainViewport() };
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    const bool isOpen{ ImGui::Begin("##DebugUIRoot", nullptr, kRootFlags) };
    ImGui::PopStyleVar(2);

    if (isOpen)
    {
        registry.isDrawing = true;

        ImGui::BeginChild("##Column", ImVec2{ kColumnWidth, 0.0f }, true);
        DrawColumnPanels(registry);
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##Tabs", ImVec2{ 0.0f, 0.0f }, false);
        if (ImGui::BeginTabBar("##DebugTabs"))
        {
            DrawTabPanels(registry);
            ImGui::EndTabBar();
        }
        ImGui::EndChild();

        registry.isDrawing = false;
    }
    ImGui::End();

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