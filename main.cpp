#include <iostream>

#include "cmake_vals.h"
#include "DDParameters.h"

using namespace std;

int main(int argc, char *argv[])
{
    //Load the parameters
    DDParameters mainParameters;
    mainParameters.LoadFromCommandLine(argc, argv);

    cout << "Dummy Directory version " << APP_VERSION_STRING << endl;
    cout << "Written by Chris Bartle" << endl;
    return 0;
}
