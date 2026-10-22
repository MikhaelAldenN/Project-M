#include "ResourceManager.h"

void ResourceManager::LoadFont(const std::string& keyName, const std::string& texturePath, const std::string& dataPath)
{
    if (m_fonts.find(keyName) != m_fonts.end()) return;
    m_fonts[keyName] = std::make_unique<BitmapFont>(texturePath, dataPath);
}

BitmapFont* ResourceManager::GetFont(const std::string& keyName)
{
    auto it = m_fonts.find(keyName);

    if (it != m_fonts.end())
    {
        return it->second.get();
    }

    return nullptr;
}

void ResourceManager::UnloadAll()
{
    m_fonts.clear();
}