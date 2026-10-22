#include "UIComponents.h"
#include "System/Input.h" // only the .cpp needs Input

// ================= STATUS PANEL =================
StatusPanel::StatusPanel(float x, float y, const std::string& startText)
    : UIElement(x, y), m_text(startText)
{}

void StatusPanel::SetStatus(const std::string& newText) {
    m_text = newText;
}

void StatusPanel::Draw(BitmapFont* font) {
    if (!font) return;
    font->Draw(m_text.c_str(), x, y, 0.625f, 1, 1, 1, 1);
}

// ================= LOG PANEL =================
LogPanel::LogPanel(float x, float y, int max)
    : UIElement(x, y), m_maxLines(max), m_lineSpacing(35.0f)
{}

void LogPanel::AddLog(const std::string& msg) {
    m_logs.push_back(msg);
    if (m_logs.size() > m_maxLines) {
        m_logs.pop_front();
    }
}

void LogPanel::Draw(BitmapFont* font) {
    if (!font) return;
    float currentY = y;
    for (const auto& line : m_logs) {
        font->Draw(line.c_str(), x, currentY, 0.625f, 1, 1, 1, 1);
        currentY += m_lineSpacing;
    }
}

// ================= DIRECTORY PANEL =================
DirectoryPanel::DirectoryPanel(float x, float y)
    : UIElement(x, y), m_selectedIndex(0), m_lineSpacing(40.0f)
{}

void DirectoryPanel::AddFile(std::string name, int size, bool isDir, std::function<void()> action) {
    m_files.push_back({ name, size, isDir, action });
}

void DirectoryPanel::Update(float dt) {
    Input& input = Input::Instance();

    // Keyboard navigation, wrapping at both ends
    if (input.GetKeyboard().IsTriggered(VK_UP)) {
        m_selectedIndex--;
        if (m_selectedIndex < 0) m_selectedIndex = (int)m_files.size() - 1;
    }
    if (input.GetKeyboard().IsTriggered(VK_DOWN)) {
        m_selectedIndex++;
        if (m_selectedIndex >= (int)m_files.size()) m_selectedIndex = 0;
    }
    if (input.GetKeyboard().IsTriggered(VK_RETURN)) {
        if (m_selectedIndex >= 0 && m_selectedIndex < m_files.size()) {
            if (m_files[m_selectedIndex].onExecute) {
                m_files[m_selectedIndex].onExecute();
            }
        }
    }
}

void DirectoryPanel::Draw(BitmapFont* font) {
    if (!font) return;
    float currentY = y;
    for (int i = 0; i < m_files.size(); i++) {
        // Highlight logic
        if (i == m_selectedIndex) {
            // selected: yellow
            font->Draw(m_files[i].name.c_str(), x, currentY, 0.625f, 1.0f, 1.0f, 0.0f, 1.0f);
        }
        else {
            // white
            font->Draw(m_files[i].name.c_str(), x, currentY, 0.625f, 1.0f, 1.0f, 1.0f, 1.0f);
        }
        currentY += m_lineSpacing;
    }
}