#ifndef DDFILE_H
#define DDFILE_H

#include <cstdint>
#include <string>
#include <filesystem>

class DDFile
{
public:
    DDFile();

    static std::string getApplicationIDString();

    std::filesystem::path relativePathname() const;
    void setRelativePathname(const std::filesystem::path &newRelativePathname);

    uint64_t size() const;
    void setSize(uint64_t newSize);

    std::string hash() const;
    void setHash(const std::string &newHash);

private:
    std::filesystem::path m_relativePathname;
    uint64_t m_size;
    std::string m_hash;
};

#endif // DDFILE_H
