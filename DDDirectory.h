#ifndef DDDIRECTORY_H
#define DDDIRECTORY_H

#include <filesystem>

class DDDirectory
{
public:
    DDDirectory();

    //Returned by reference rather than by value. These accessors sit inside loops that run
    //hundreds of millions of times on a large manifest, and returning a path by value there
    //means constructing (and heap-allocating) a brand new path object on every single read.
    const std::filesystem::path &relativePath() const;
    void setRelativePath(const std::filesystem::path &newRelativePath);
    uint64_t getRelativePathDepth();

    enum DirectoryProcessingStatus { NONE, PROCESSING, COMPLETE, ERROR, CONFLICT, DELETED };
    DirectoryProcessingStatus processingStatus() const;
    void setProcessingStatus(DirectoryProcessingStatus newProcessingStatus);
    void recordProcessingError(std::string inError) {m_processingStatus = ERROR; m_processingError = inError; };
    std::string getProcessingError() { return m_processingError; };
private:
    std::filesystem::path m_relativePath;

    DirectoryProcessingStatus m_processingStatus;
    std::string m_processingError;
};

#endif // DDDIRECTORY_H
