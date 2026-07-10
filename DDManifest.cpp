#include "DDManifest.h"
#include <fstream>
#include <iostream>
#include <numeric>
#include <algorithm>
#include <format>

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
    //If there are no directories or files then not only do we not want to write out a file but
    //we want to delete any prior manifest file.
    if ((m_directories.size() <= 1) && (m_files.size() == 0) && std::filesystem::exists(m_absoluteManifestPath))
    {
        std::filesystem::remove(m_absoluteManifestPath);
        return;
    }

    //Sort the vectors. We do this to guarantee that every manifest file is identical
    sort(m_directories.begin(), m_directories.end(), [](const unique_ptr<DDDirectory>& a, const unique_ptr<DDDirectory>& b) {
        return a->relativePath() < b->relativePath();
    });
    sort(m_files.begin(), m_files.end(), [](const unique_ptr<DDFile>& a, const unique_ptr<DDFile>& b) {
        return a->relativePathname() < b->relativePathname();
    });

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
        manifestFile << std::quoted(directory->relativePath().generic_string()) << " directory\n";  //Avoiding endl for performance
    }

    //Finally we write out the list of all files
    for (const auto& file : m_files)
    {
        manifestFile << std::quoted(file->relativePathname().generic_string()) << " " << to_string(file->size()) << " " << file->hash() << "\n";    //Avoiding endl for performance
    }
    manifestFile.close();
}

/**
 * @brief DDManifest::PostOperationCleanup
 * Updates the manifest's total size and clears up the various status flags.
 * This should be called in between operations but it can destroy run information.
 */
void DDManifest::PostOperationCleanup()
{
    //Remove deleted directories
    erase_if(m_directories, [](const unique_ptr<DDDirectory>& d) { return d->processingStatus() == DDDirectory::DELETED; });

    //Clean up the directory errors
    for (const auto& directory : m_directories)
    {
        if (directory->processingStatus() != DDDirectory::NONE)
            directory->setProcessingStatus(DDDirectory::NONE);
        directory->recordProcessingError("");
    }

    //Remove deleted files
    erase_if(m_files, [](const unique_ptr<DDFile>& d) { return d->processingStatus() == DDFile::DELETED; });

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

DDDirectory &DDManifest::getRandomDirectory(DDDeterministicPCGPRNG &inRNG, uint64_t inMaxDepth)
{
    //If the max depth is 0 then the only answer is the root directory
    if (inMaxDepth == 0)
        return *(m_directories[0]);

    uint64_t size = m_directories.size();
    uint64_t dirPos = inRNG.getFromRange(0, size-1);
    if (m_directories[dirPos]->getRelativePathDepth() <= inMaxDepth)
        return *(m_directories[dirPos]);

    //This directory is invalid because it's too deep. We'll need to iterate through
    //the list until we find one that isn't too deep. We'll use the coprime stride method
    //which will let us move through the list in somewhat random order while guaranteeing that
    //we eventually hit every directory (including the root).
    uint64_t stride = (size==1) ? 1 : inRNG.getFromRange(1, size-1);
    //The stride must not have a common denominator compared to the size of the list
    while (std::gcd(stride, size) != 1) {
        stride = inRNG.getFromRange(1, size-1);
    }
    //Now we can loop through the list and expect to hit every item
    for (size_t i = 0; i < size; ++i) {
        // Jump forward by the stride and wrap around using modulo
        dirPos = (dirPos + stride) % size;
        //We are only picking directories that don't have a strange processing status and are within
        //the desired depth
        if ((m_directories[dirPos]->processingStatus() == DDDirectory::NONE)
                && (m_directories[dirPos]->getRelativePathDepth() <= inMaxDepth))
            return *(m_directories[dirPos]);
    }

    //The above should have worked but if it didn't just return the root directory which
    //is always the first one on the vector.
    return *(m_directories[0]);
}

string DDManifest::GetManifestSummation()
{
    string summation;
    summation = format(std::locale(""), "Manifest contains {:L} directories and {:L} files.\n{:L} bytes total.",
                            getTotalDirectoryCount(), getTotalFileCount(), getTotalSize());
    return summation;
}