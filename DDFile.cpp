#include "DDFile.h"

DDFile::DDFile() {}

std::filesystem::path DDFile::relativePathname() const
{
    return m_relativePathname;
}

void DDFile::setRelativePathname(const std::filesystem::path &newRelativePathname)
{
    m_relativePathname = newRelativePathname;
}

uint64_t DDFile::size() const
{
    return m_size;
}

void DDFile::setSize(uint64_t newSize)
{
    m_size = newSize;
}

std::string DDFile::hash() const
{
    return m_hash;
}

void DDFile::setHash(const std::string &newHash)
{
    m_hash = newHash;
}
