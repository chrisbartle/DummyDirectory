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
    void PostOperationCleanup();

    uint64_t getTotalSize() { return m_totalSize; };
    uint64_t getTotalFileCount() { return m_files.size(); };
    uint64_t getTotalDirectoryCount() { return m_directories.size(); };

    DDFile& getFileByPos(uint64_t inPos) { return *(m_files[inPos]); };
    DDFile& addFile() { m_files.emplace_back(std::make_unique<DDFile>()); return *(m_files.back()); };
//    void removeFileByPos(uint64_t in Pos) { m_files.}

    DDDirectory& getDirectoryByPos(uint64_t inPos) { return *(m_directories[inPos]); };
    DDDirectory& addDirectory() { m_directories.emplace_back(std::make_unique<DDDirectory>()); return *(m_directories.back()); };

private:
    std::filesystem::path m_absoluteManifestPath;

    //We are using unique_ptr to prevent these objects from being copied and destroyed when
    //the vectors are resized. This way they remain available for the other threads.
    std::vector<std::unique_ptr<DDFile>> m_files;
    std::vector<std::unique_ptr<DDDirectory>> m_directories;
    std::atomic_uint64_t m_totalSize;
};

#endif // DDMANIFEST_H
