#include "solver.h"

enum { MIN_BLOCK_BITS = 3, MAX_BLOCK_BITS = 7, MIN_SET_BITS = 1, MAX_SET_BITS = 9 };
enum { MAX_WAYS = 16, POLICY_TRIALS = 64 };

static void load_blocks(uint32_t stride, int count)
{
    for (int block = 0; block < count; ++block)
        cache_load((uint32_t)block * stride);
}

static int blocks_fit(uint32_t stride, int count)
{
    cache_flush();
    load_blocks(stride, count);
    for (int block = 0; block < count; ++block) {
        if (!cache_load((uint32_t)block * stride))
            return 0;
    }
    return 1;
}

static int block_bits(void)
{
    int b = MIN_BLOCK_BITS;
    cache_flush();
    cache_load(0);
    while (b < MAX_BLOCK_BITS && cache_load(1u << b))
        ++b;
    return b;
}

static int associativity(int b)
{
    /* This stride keeps every possible set index at zero. */
    uint32_t stride = 1u << (b + MAX_SET_BITS);
    int low = 1, high = MAX_WAYS;
    while (low < high) {
        int middle = (low + high + 1) / 2;
        if (blocks_fit(stride, middle))
            low = middle;
        else
            high = middle - 1;
    }
    return low;
}

static int set_bits(int b, int ways)
{
    int low = MIN_SET_BITS, high = MAX_SET_BITS;
    while (low < high) {
        int middle = (low + high) / 2;
        /* Smaller strides spread the blocks across at least two sets. */
        if (blocks_fit(1u << (b + middle), ways + 1))
            low = middle + 1;
        else
            high = middle;
    }
    return low;
}

static int replacement_policy(uint32_t stride, int ways)
{
    if (ways == 1)
        return POLICY_LRU;

    int lru_possible = 1, fifo_possible = 1;
    for (int trial = 0; trial < POLICY_TRIALS && (lru_possible || fifo_possible);
         ++trial) {
        cache_flush();
        load_blocks(stride, ways);
        cache_load(0);
        cache_load((uint32_t)ways * stride);

        /* LRU evicts block 1 after the hit on block 0. FIFO evicts block 0. */
        int block = trial % 2;
        int hit = cache_load((uint32_t)block * stride);
        int expected_lru_hit = block == 0;
        if (hit == expected_lru_hit)
            fifo_possible = 0;
        else
            lru_possible = 0;
    }

    if (lru_possible)
        return POLICY_LRU;
    if (fifo_possible)
        return POLICY_FIFO;
    return POLICY_RANDOM;
}

void solve(void)
{
    int b = block_bits();
    int ways = associativity(b);
    int s = set_bits(b, ways);
    int policy = replacement_policy(1u << (b + s), ways);

    cache_flush();
    cache_store(0);
    int write_allocate = cache_load(0);

    report(b, s, ways, policy, write_allocate);
}
