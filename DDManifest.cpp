#include "DDManifest.h"
#include <fstream>

DDManifest::DDManifest() {}

void DDManifest::SetFilename(std::string inFilename)
{
    m_absoluteManifestPath = inFilename;
}

bool DDManifest::LoadFromFile()
{
    //If file doesn't exist then nothing to load
    if (!std::filesystem::is_regular_file(m_absoluteManifestPath))
        return false;

    //Open a stream and start reading
    std::ifstream manifestFile(m_absoluteManifestPath);
    if (!manifestFile.is_open())
    {
        std::error_code ec = std::make_error_code(std::errc::no_such_file_or_directory);
        throw std::filesystem::filesystem_error("Failed to open manifest file", m_absoluteManifestPath, ec);
    }

    //Loop through all of the lines
    std::string lineString;
    while (std::getline(manifestFile, lineString))
    {
        //Any line that starts with a # should be ignored.
        if (lineString.starts_with('#'))
            continue;

        std::stringstream ss;
        //Get the first two items in the string, the filename and the file size
        std::string filename;
        std::string filesize;
        std::string filehash;
        ss >> std::quoted(filename) >> filesize;
        //Now the second item normally contains the file's size but it might just contain the word "directory"
        //If this is seen then the item is a directory, not a file
        if (filesize == "directory")
            void();
        else
        {
            //It's a file so grab the hash as well
            ss >> filehash;
        }

    }

    return true;
}

void DDManifest::SaveToFile()
{

}