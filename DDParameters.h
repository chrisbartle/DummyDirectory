#ifndef DDPARAMETERS_H
#define DDPARAMETERS_H

#include <string>
#include <unordered_map>
#include <filesystem>

class DDParameters
{
public:
    DDParameters();

    void LoadFromCommandLine(int argc, char* argv[]);

    bool isFlag(std::string inFlagName);
    std::string getFlag(std::string inFlagName);
    void setFlag(std::string inFlagName, std::string inFlag);
    std::string validateFlags();

    std::string getOperation();
    std::string getDirectoryPath();
    std::filesystem::path getAbsoluteDirectoryPath() {return std::filesystem::absolute(getDirectoryPath()).lexically_normal(); };
    std::filesystem::path ConvertToAbsolutePath(std::filesystem::path inPath) {return getAbsoluteDirectoryPath() / inPath; };

private:
    std::unordered_map<std::string, std::string> m_flags;
    std::vector<std::string> m_arguments;
};

#endif // DDPARAMETERS_H
