#ifndef DDMANIFEST_H
#define DDMANIFEST_H

#include <filesystem>
#include <vector>

#include "DDDirectory.h"
#include "DDFile.h"

class DDManifest
{
public:
    DDManifest();

    void SetFilepath(std::filesystem::path inFilepath);
    bool LoadFromFile();
    void SaveToFile();

    uint64_t getTotalSize() { return m_totalSize; };
    uint64_t getTotalFileCount() { return m_files.size(); };
    uint64_t getTotalDirectoryCount() { return m_directories.max_size(); };

    DDFile& getFileByPos(uint64_t inPos) { return m_files[inPos]; };
    DDFile& addFile() { m_files.emplace_back(); return m_files.back(); };
//    void removeFileByPos(uint64_t in Pos) { m_files.}

    DDDirectory& getDirectoryByPos(uint64_t inPos) { return m_directories[inPos]; };
    DDDirectory& addDirectory() { m_directories.emplace_back(); return m_directories.back(); };

private:
    std::filesystem::path m_absoluteManifestPath;

    std::vector<DDFile> m_files;
    std::vector<DDDirectory> m_directories;
    std::atomic_uint64_t m_totalSize;
};

#endif // DDMANIFEST_H
