#ifndef DDMANIFEST_H
#define DDMANIFEST_H

#include <string>
#include <filesystem>

class DDManifest
{
public:
    DDManifest();

    void SetFilename(std::string inFilename);
    bool LoadFromFile();
    void SaveToFile();

private:
    std::filesystem::path m_absoluteManifestPath;
};

#endif // DDMANIFEST_H
