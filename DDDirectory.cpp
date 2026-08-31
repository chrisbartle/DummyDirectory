#include "DDDirectory.h"

DDDirectory::DDDirectory() {
    m_processingStatus = NONE;
}

const std::filesystem::path &DDDirectory::relativePath() const
{
    return m_relativePath;
}

void DDDirectory::setRelativePath(const std::filesystem::path &newRelativePath)
{
    m_relativePath = newRelativePath;
}

/**
 * @brief DDDirectory::getRelativePathDepth
 * Returns the depth of the path, how many levels above the root it is. Root is depth 0.
 * @return Path depth
 */
uint64_t DDDirectory::getRelativePathDepth()
{
    uint64_t pathDepth;
    pathDepth = std::distance(m_relativePath.begin(), m_relativePath.end());
    return pathDepth;
}

void DDDirectory::setProcessingStatus(DirectoryProcessingStatus newProcessingStatus)
{
    m_processingStatus = newProcessingStatus;
    if ((newProcessingStatus != ERROR) && !m_processingError.empty())
        m_processingError.clear();
}

DDDirectory::DirectoryProcessingStatus DDDirectory::processingStatus() const
{
    return m_processingStatus;
}
