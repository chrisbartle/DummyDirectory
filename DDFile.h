#ifndef DDFILE_H
#define DDFILE_H

#include <cstdint>
#include <string>
#include <filesystem>

class DDFile
{
public:
    DDFile();

    //Returned by reference rather than by value. This accessor is read by the manifest's sort
    //comparators and by the duplicate scan in PostOperationCleanup, so on a million-file
    //manifest it runs tens of millions of times per operation - returning a path by value
    //there means a heap allocation on every single read.
    const std::filesystem::path &relativePathname() const;
    void setRelativePathname(const std::filesystem::path &newRelativePathname);

    uint64_t size() const;
    void setSize(uint64_t newSize);

    std::string hash() const;
    void setHash(const std::string &newHash);

//    enum FileExistanceStatus { NOEXIST, EXISTS, NOTDD };
//    FileExistanceStatus existsOnFilesystem();

    enum FileProcessingStatus { NONE, QUEUED, STARTED, COMPLETE, ERROR, DELETED, CONFLICT, MISSING, DIFFERENT };
    FileProcessingStatus processingStatus() const;
    void setProcessingStatus(FileProcessingStatus newProcessingStatus);
    void recordProcessingError(std::string inError) {m_processingStatus = ERROR; m_processingError = inError; };
    std::string getProcessingError() { return m_processingError; };

private:
    std::filesystem::path m_relativePathname;
    uint64_t m_size;
    std::string m_hash;
    //These processing variables are ephemeral and only apply to the current
    //operation
//    std::string m_processingOperation;
//    std::string m_processingFileType;
//    std::string m_processingModificationType;
    FileProcessingStatus m_processingStatus;
//    uint64_t m_processingSize;
    std::string m_processingError;
};

#endif // DDFILE_H
