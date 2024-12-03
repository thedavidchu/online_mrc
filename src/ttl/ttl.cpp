#include <iostream>

#include "trace/reader.h"
#include "trace/trace.h"

struct CommandLineArguments {
    char const *const path;
};

int
main(int argc, char *argv[])
{
    std::cout << "Hello, World!" << std::endl;
    for (int i = 0; i < argc && *argv != NULL; ++i, ++argv) {
        std::cout << "Arg " << i << ": '" << *argv << "'" << std::endl;
    }
    return 0;
}
