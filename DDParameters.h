#ifndef DDPARAMETERS_H
#define DDPARAMETERS_H

#include <string>
#include <unordered_map>

class DDParameters
{
public:
    DDParameters();

    void LoadFromCommandLine(int argc, char* argv[]);

    bool isFlag(std::string inFlagName);
    std::string getFlag(std::string inFlagName);
    void setFlag(std::string inFlagName, std::string inFlag);

    std::string getOperation();
    std::string getDirectoryPath();

private:
    std::unordered_map<std::string, std::string> _flags;
    std::vector<std::string> _arguments;
};

#endif // DDPARAMETERS_H
