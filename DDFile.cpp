#include "DDFile.h"
#include "cmake_vals.h"

DDFile::DDFile()
{
    m_size = 0;
    m_processingStatus = NONE;
}

/**
 * @brief DDFile::getApplicationIDString
 * This function generates the application ID string that is put at the beginning of every
 * generated file. Its format is: DummyDir_v1234 where 1234 is the version number.
 * @return Application ID string
 */
std::string DDFile::getApplicationIDString()
{
    const std::string verCharList = "01234567890abcdefghijklmnopqrstuvwxyz";
    std::string appInfo = "DummyDir_v";
    //Convert the application version to a 4 character code. If any version number ever gets too high
    //then convert it to a ?
    if (APP_VERSION_MAJOR > verCharList.length()-1)
        appInfo += "?";
    else
        appInfo += verCharList[APP_VERSION_MAJOR];
    if (APP_VERSION_MINOR > verCharList.length()-1)
        appInfo += "?";
    else
        appInfo += verCharList[APP_VERSION_MINOR];
    if (APP_VERSION_PATCH > verCharList.length()-1)
        appInfo += "?";
    else
        appInfo += verCharList[APP_VERSION_PATCH];
    if (APP_VERSION_TWEAK > verCharList.length()-1)
        appInfo += "?";
    else
        appInfo += verCharList[APP_VERSION_TWEAK];
    return appInfo;
}

std::filesystem::path DDFile::relativePathname() const
{
    return m_relativePathname;
}

void DDFile::setRelativePathname(const std::filesystem::path &newRelativePathname)
{
    m_relativePathname = newRelativePathname;
}

uint64_t DDFile::size() const
{
    return m_size;
}

void DDFile::setSize(uint64_t newSize)
{
    m_size = newSize;
}

std::string DDFile::hash() const
{
    return m_hash;
}

void DDFile::setHash(const std::string &newHash)
{
    m_hash = newHash;
}

DDFile::FileProcessingStatus DDFile::processingStatus() const
{
    return m_processingStatus;
}

void DDFile::setProcessingStatus(FileProcessingStatus newProcessingStatus)
{
    m_processingStatus = newProcessingStatus;
}


