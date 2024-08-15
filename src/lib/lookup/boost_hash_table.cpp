#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdio.h> // This is for FILE so that it matches the header.

#include "lookup/boost_hash_table.h"
#include "lookup/lookup.h"
#include "types/key_type.h"
#include "types/value_type.h"

struct BoostHashTable {
    boost::unordered::unordered_flat_map<std::uint64_t, std::uint64_t>
        hash_table;
};

struct BoostHashTable *
BoostHashTable__new(void)
{
    std::unique_ptr<struct BoostHashTable> me =
        std::make_unique<struct BoostHashTable>();
    std::cout << me.get() << std::endl;
    struct BoostHashTable *raw_ptr = me.release();
    std::cout << raw_ptr << std::endl;
    return raw_ptr;
}

void
BoostHashTable__free(struct BoostHashTable *const me)
{
    std::unique_ptr<struct BoostHashTable> ptr{me};
}

struct LookupReturn
BoostHashTable__lookup(struct BoostHashTable const *const me, KeyType const key)
{
    if (me == NULL || !me->hash_table.contains(key)) {
        return {false, 0};
    }
    return {true, me->hash_table.at(key)};
}

enum PutUniqueStatus
BoostHashTable__put(struct BoostHashTable *const me,
                    KeyType const key,
                    ValueType const value)
{
    if (me == NULL) {
        return LOOKUP_PUTUNIQUE_ERROR;
    }
    if (!me->hash_table.contains(key)) {
        me->hash_table[key] = value;
        return LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE;
    }
    me->hash_table[key] = value;
    return LOOKUP_PUTUNIQUE_REPLACE_VALUE;
}

struct LookupReturn
BoostHashTable__remove(struct BoostHashTable *const me, KeyType const key)
{
    if (me == NULL || !me->hash_table.contains(key)) {
        return {false, 0};
    }
    ValueType value = me->hash_table.at(key);
    me->hash_table.erase(key);
    return {true, value};
}

bool
BoostHashTable__write(struct BoostHashTable const *const me,
                      FILE *const stream,
                      bool const newline)
{
    if (me == NULL || stream == NULL)
        return false;
    fprintf(stream, "{");
    for (auto [key, value] : me->hash_table) {
        // NOTE We could remove the trailing comma if we keep track of
        //      how many items we are printing versus how many we have
        //      already printed. When we see the last item, then we do
        //      not print the trailing comma. Alternatively, we could
        //      print the first item (if it exists) without the trailing
        //      comma and then print all other items with a leading
        //      comma. I don't feel like doing these things though.
        fprintf(stream, "%" PRIu64 ": %" PRIu64 ", ", key, value);
    }
    fprintf(stream, "}%s", newline ? "\n" : "");
    return true;
}
