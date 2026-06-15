#include "DDDirectory.h"

DDDirectory::DDDirectory() {}

std::filesystem::path DDDirectory::relativePath() const
{
    return m_relativePath;
}

void DDDirectory::setRelativePath(const std::filesystem::path &newRelativePath)
{
    m_relativePath = newRelativePath;
}
