#pragma once
#include <vector>
#include <string>
#include <deque>
#include <functional>
#include <memory>
#include "BitmapFont.h"

// --- BASE CLASS ---
class UIElement {
public:
    float m_x, m_y;
    UIElement(float _x, float _y) : m_x(_x), m_y(_y) {}
    virtual ~UIElement() = default;

    virtual void Update(float dt) {} // does nothing by default
    virtual void Draw(BitmapFont* font) = 0; // Pure virtual
};

// --- STATUS PANEL ---
class StatusPanel : public UIElement {
private:
    std::string m_text;
public:
    StatusPanel(float x, float y, const std::string& startText);
    void SetStatus(const std::string& newText);
    void Draw(BitmapFont* font) override;
};

// --- LOG PANEL ---
class LogPanel : public UIElement {
private:
    std::deque<std::string> m_logs;
    int m_maxLines;
    float m_lineSpacing;
public:
    LogPanel(float x, float y, int max);
    void AddLog(const std::string& msg);
    void Draw(BitmapFont* font) override;
};

// --- DIRECTORY PANEL ---
struct FileEntry {
    std::string name;
    int size;
    bool isDir;
    std::function<void()> onExecute;
};

class DirectoryPanel : public UIElement {
private:
    std::vector<FileEntry> m_files;
    int m_selectedIndex;
    float m_lineSpacing;
public:
    DirectoryPanel(float x, float y);
    void AddFile(std::string name, int size, bool isDir, std::function<void()> action);
    void Update(float dt) override; // reads keyboard input
    void Draw(BitmapFont* font) override;
};