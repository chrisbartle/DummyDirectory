#include "DDManifest.h"
#include <fstream>
#include <iostream>

#include "cmake_vals.h"

using namespace std;

DDManifest::DDManifest()
{
    //The directory list should always include the root directory
    m_directories.emplace_back(std::make_unique<DDDirectory>());
    m_directories.back()->setRelativePath("");
    m_totalSize = 0;
}

void DDManifest::SetFilepath(std::filesystem::path inFilepath)
{
    m_absoluteManifestPath = inFilepath;
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
        throw std::filesystem::filesystem_error("Failed to open manifest file for read", m_absoluteManifestPath, ec);
    }

    //Loop through all of the lines
    std::string lineString;
    uint64_t lineNumber = 0;
    while (std::getline(manifestFile, lineString))
    {
        lineNumber++;
        try
        {
            //Any line that starts with a # should be ignored.
            if (lineString.starts_with('#'))
                continue;

            std::stringstream ss(lineString);
            //Get the first two items in the string, the filename and the file size
            std::string filename;
            std::string filesize;
            std::string filehash;
            ss >> std::quoted(filename) >> filesize;
            //Now the second item normally contains the file's size but it might just contain the word "directory"
            //If this is seen then the item is a directory, not a file
            if (filesize == "directory")
            {
                m_directories.emplace_back(std::make_unique<DDDirectory>());
                m_directories.back()->setRelativePath(filename);
            }
            else
            {
                //Add a new file to the list and fill in its properties
                m_files.emplace_back(std::make_unique<DDFile>());
                m_files.back()->setRelativePathname(filename);
                uint64_t filesizei = std::stoull(filesize);
                m_files.back()->setSize(filesizei);
                m_totalSize += filesizei;
                //It's a file so grab the hash as well
                ss >> filehash;
                m_files.back()->setHash(filehash);
            }
        }
        catch(...)
        {
            throw std::runtime_error("Unable to read line " + std::to_string(lineNumber) + " in " + m_absoluteManifestPath.string());
        }

    }

    return true;
}

void DDManifest::SaveToFile()
{
    //Open a stream and start writing
    std::ofstream manifestFile(m_absoluteManifestPath, std::ios::trunc);
    if (!manifestFile.is_open())
    {
        std::error_code ec = std::make_error_code(std::errc::no_such_file_or_directory);
        throw std::filesystem::filesystem_error("Failed to open manifest file for write", m_absoluteManifestPath, ec);
    }

    //First we write out some comments to identify the file
    manifestFile << "# Dummy Directory manifest file" << endl;
    manifestFile << "# Version " << APP_VERSION_STRING << endl;

    //Next we write out all of our directories
    for (const auto& directory : m_directories)
    {
        //We never need to write out the root directory
        if (directory->relativePath() == "")
            continue;
        //Encase the directory name in quotes
        manifestFile << std::quoted(directory->relativePath().string()) << " directory" << endl;
    }

    //Finally we write out the list of all files
    for (const auto& file : m_files)
    {
        manifestFile << std::quoted(file->relativePathname().string()) << " " << to_string(file->size()) << " " << file->hash() << endl;
    }

}

/**
 * @brief DDManifest::PostOperationCleanup
 * Updates the manifest's total size and clears up the various status flags.
 * This should be called in between operations but it can destroy run information.
 */
void DDManifest::PostOperationCleanup()
{
    uint64_t totalFileSize = 0;
    //Iterate through the files
    for (const auto& file : m_files)
    {
        if (file->processingStatus() != DDFile::NONE)
            file->setProcessingStatus(DDFile::NONE);
        totalFileSize += file->size();
    }
    m_totalSize = totalFileSize;
}