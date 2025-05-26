#include "cpp_lib/cache_metadata.hpp"
#include "cpp_lib/format_measurement.hpp"
#include <cstdint>
#include <string>

class WhichEvictionQueue {
private:
    static inline bool
    is_bit_set(uint64_t const bits, uint64_t const idx)
    {
        // I am just needlessly verbose out of paranoia. I don't understand
        // the C++ standard well enough.
        return 1ULL << idx & bits ? 1 : 0;
    }

    static inline void
    set_bit(uint64_t &bits, uint64_t const idx, bool const value)
    {
        if (value) {
            bits |= (uint64_t)1 << idx;
        } else {
            bits &= ~((uint64_t)1 << idx);
        }
    }

public:
    void
    reset()
    {
        queue_ = 0;
    }

    std::string
    str() const
    {
        return format_binary(queue_);
    }

    void
    set_ttl()
    {
        set_bit(queue_, 0, true);
    }

    void
    set_lru()
    {
        set_bit(queue_, 1, true);
    }

    void
    unset_ttl()
    {
        set_bit(queue_, 0, false);
    }

    void
    unset_lru()
    {
        set_bit(queue_, 1, false);
    }

    bool
    uses_ttl() const
    {
        return is_bit_set(queue_, 0);
    }

    bool
    uses_lru() const
    {
        return is_bit_set(queue_, 1);
    }

private:
    uint64_t queue_ = 0;
};

struct CachePredictiveMetadata : public CacheMetadata {
    void
    set_ttl()
    {
        which.set_ttl();
    }
    void
    set_lru()
    {
        which.set_lru();
    }

    void
    unset_ttl()
    {
        which.unset_ttl();
    }
    void
    unset_lru()
    {
        which.unset_lru();
    }

    bool
    uses_ttl() const
    {
        return which.uses_ttl();
    }
    bool
    uses_lru() const
    {
        return which.uses_lru();
    }

private:
    WhichEvictionQueue which;
};
