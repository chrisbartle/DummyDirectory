#ifndef DDDIRECTORY_H
#define DDDIRECTORY_H

#include <filesystem>

class DDDirectory
{
public:
    DDDirectory();

    std::filesystem::path relativePath() const;
    void setRelativePath(const std::filesystem::path &newRelativePath);

private:
    std::filesystem::path m_relativePath;
};

#endif // DDDIRECTORY_H
