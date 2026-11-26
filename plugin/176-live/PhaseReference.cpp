#include "Live176.h"
#include <cstdlib>

int main(int argc, char** argv)
{
    return argc == 3 && ua176::Live176::exportPhaseReference(argv[1], std::atof(argv[2])) ? 0 : 1;
}
