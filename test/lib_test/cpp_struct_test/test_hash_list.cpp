#include "cpp_struct/hash_list.hpp"

#include <cstdlib>

int
main()
{
    HashList l{};

    l.access(0);
    l.access(1);
    l.access(2);
    l.access(0);
    std::free(l.extract(0));
    std::free(l.extract(0));
    std::free(l.extract(0));
    std::free(l.extract(0));
    std::free(l.extract_head());
    std::free(l.extract_head());
    std::free(l.extract_head());
    std::free(l.extract_head());

    return 0;
}
