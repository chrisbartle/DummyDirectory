#ifndef DDMANIFEST_H
#define DDMANIFEST_H

#include <string>
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

private:
    std::filesystem::path m_absoluteManifestPath;

    std::vector<DDFile> m_files;
    std::vector<DDDirectory> m_directories;
};

#endif // DDMANIFEST_H
