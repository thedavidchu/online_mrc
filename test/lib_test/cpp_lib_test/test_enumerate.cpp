#include "cpp_lib/enumerate.hpp"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int
main()
{
    std::vector<std::string> v{"Hello!", "Bye!", "Yes please", "No thanks"};
    std::stringstream ss;
    std::string oracle{
        "0: \"Hello!\"\n1: \"Bye!\"\n2: \"Yes please\"\n3: \"No thanks\"\n"};

    for (auto [i, s] : enumerate(v)) {
        ss << i << ": " << std::quoted(s) << "" << std::endl;
    }
    std::cout << ">>> OUTPUT:\n";
    std::cout << ss.str();
    std::cout << ">>> ORACLE:\n";
    std::cout << oracle;
    assert(ss.str() == oracle);

    return 0;
}
