#include <boost/version.hpp>
// Boost introduced unordered flat maps in version 1.83.0
#if BOOST_VERSION >= 108300
#include <boost/unordered/unordered_flat_map.hpp>
#else
#include <unordered_map>
#endif
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdio.h> // This is for FILE so that it matches the header.

#include "lookup/boost_hash_table.h"
#include "lookup/lookup.h"
#include "types/key_type.h"
#include "types/value_type.h"

struct BoostHashTablePrivate {
#if BOOST_VERSION >= 108300
    boost::unordered::unordered_flat_map<std::uint64_t, std::uint64_t>
        hash_table;
#else
    std::unordered_map<std::uint64_t, std::uint64_t> hash_table;
#endif
};

static struct BoostHashTablePrivate *
new_private_hash_table(void)
{
    std::unique_ptr<struct BoostHashTablePrivate> me =
        std::make_unique<struct BoostHashTablePrivate>();
    struct BoostHashTablePrivate *raw_ptr = me.release();
    return raw_ptr;
}

bool
BoostHashTable__init(struct BoostHashTable *const me)
{
    me->private_ = new_private_hash_table();
    return (me->private_ != NULL);
}

static void
free_private_hash_table(struct BoostHashTablePrivate *const me)
{
    std::unique_ptr<struct BoostHashTablePrivate> ptr{me};
}

void
BoostHashTable__destroy(struct BoostHashTable *const me)
{
    free_private_hash_table(me->private_);
    *me = {0};
}

struct LookupReturn
BoostHashTable__lookup(struct BoostHashTable const *const me, KeyType const key)
{
    if (me == NULL) {
        return {false, 0};
    }
    auto x = me->private_->hash_table.find(key);
    if (x == me->private_->hash_table.end()) {
        return {false, 0};
    }
    return {true, x->second};
}

enum PutUniqueStatus
BoostHashTable__put(struct BoostHashTable *const me,
                    KeyType const key,
                    ValueType const value)
{
    if (me == NULL) {
        return LOOKUP_PUTUNIQUE_ERROR;
    }
    auto x = me->private_->hash_table.find(key);
    if (x == me->private_->hash_table.end()) {
        me->private_->hash_table[key] = value;
        return LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE;
    }
    x->second = value;
    return LOOKUP_PUTUNIQUE_REPLACE_VALUE;
}

struct LookupReturn
BoostHashTable__remove(struct BoostHashTable *const me, KeyType const key)
{
    if (me == NULL || me->private_->hash_table.count(key) == 0) {
        return {false, 0};
    }
    ValueType value = me->private_->hash_table.at(key);
    me->private_->hash_table.erase(key);
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
    for (auto [key, value] : me->private_->hash_table) {
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

size_t
BoostHashTable__get_size(struct BoostHashTable const *const me)
{
    return me->private_->hash_table.size();
}
