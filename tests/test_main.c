/*
 * Unit tests for the cache engine, parser and statistics.
 * No framework: each test is a function, CHECK records failures.
 */
#include <stdint.h>
#include <stdio.h>

#include "cache.h"
#include "config.h"
#include "parser.h"
#include "statistics.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++checks;                                                            \
        if (!(cond)) {                                                       \
            ++failures;                                                      \
            (void)fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                    \
    } while (0)

/* Builds a small cache: size/block/ways with everything else default. */
static Cache make_cache(size_t size, size_t block, size_t ways, ReplacementPolicy policy)
{
    SimulatorConfig config;
    Cache cache;

    config_set_defaults(&config);
    config.cache_size_bytes = size;
    config.block_size_bytes = block;
    config.associativity = ways;
    config.replacement_policy = policy;
    (void)cache_initialize(&cache, &config, NULL, 0U);
    return cache;
}

static bool hit(Cache *cache, uint32_t address)
{
    return cache_access(cache, ACCESS_READ, address).hit;
}

static void test_config_validation(void)
{
    SimulatorConfig config;

    config_set_defaults(&config);
    CHECK(config_validate(&config, NULL, 0U));

    config.block_size_bytes = 48U;   /* not a power of two */
    CHECK(!config_validate(&config, NULL, 0U));

    config_set_defaults(&config);
    config.cache_size_bytes = 256U;
    config.block_size_bytes = 64U;
    config.associativity = 8U;       /* more ways than the 4 lines */
    CHECK(!config_validate(&config, NULL, 0U));
}

static void test_address_decode(void)
{
    /* 1 KiB, 16-byte blocks, direct-mapped: 64 sets -> 4 offset, 6 index bits. */
    Cache cache = make_cache(1024U, 16U, 1U, REPLACEMENT_POLICY_LRU);
    AddressFields f = cache_decode_address(&cache, 0x12345678U);

    CHECK(cache.offset_bits == 4U);
    CHECK(cache.index_bits == 6U);
    CHECK(f.block_offset == 0x8U);
    CHECK(f.set_index == 0x27U);
    CHECK(f.tag == 0x48D15U);    /* 0x12345678 >> 10 */
    cache_destroy(&cache);
}

static void test_direct_mapped_conflict(void)
{
    /* 0x000 and 0x400 share set 0 in a 1 KiB direct-mapped cache. */
    Cache cache = make_cache(1024U, 64U, 1U, REPLACEMENT_POLICY_LRU);

    CHECK(!hit(&cache, 0x000U));
    CHECK(hit(&cache, 0x004U));      /* same block */
    CHECK(!hit(&cache, 0x400U));     /* conflict evicts 0x000 */
    CHECK(!hit(&cache, 0x000U));
    cache_destroy(&cache);
}

static void test_lru(void)
{
    /* One set of 2 ways: A, B, touch A, then C must evict B. */
    Cache cache = make_cache(128U, 64U, 2U, REPLACEMENT_POLICY_LRU);

    (void)hit(&cache, 0x000U);       /* A */
    (void)hit(&cache, 0x040U);       /* B */
    CHECK(hit(&cache, 0x000U));      /* A is now most recent */
    (void)hit(&cache, 0x080U);       /* C evicts B */
    CHECK(hit(&cache, 0x000U));
    CHECK(!hit(&cache, 0x040U));
    cache_destroy(&cache);
}

static void test_fifo(void)
{
    /* Same pattern, but FIFO ignores the re-use and evicts A (oldest). */
    Cache cache = make_cache(128U, 64U, 2U, REPLACEMENT_POLICY_FIFO);

    (void)hit(&cache, 0x000U);
    (void)hit(&cache, 0x040U);
    CHECK(hit(&cache, 0x000U));
    (void)hit(&cache, 0x080U);
    CHECK(hit(&cache, 0x040U));
    CHECK(!hit(&cache, 0x000U));
    cache_destroy(&cache);
}

static void test_random_is_repeatable(void)
{
    Cache a = make_cache(256U, 64U, 4U, REPLACEMENT_POLICY_RANDOM);
    Cache b = make_cache(256U, 64U, 4U, REPLACEMENT_POLICY_RANDOM);
    uint32_t addr;
    bool same = true;

    for (addr = 0U; addr < 0x4000U; addr += 0x40U) {
        same = same && cache_access(&a, ACCESS_READ, addr * 7U).way ==
                       cache_access(&b, ACCESS_READ, addr * 7U).way;
    }
    CHECK(same);
    cache_destroy(&a);
    cache_destroy(&b);
}

static void test_write_back(void)
{
    Cache cache = make_cache(64U, 64U, 1U, REPLACEMENT_POLICY_LRU);
    CacheAccessResult r;

    r = cache_access(&cache, ACCESS_WRITE, 0x000U);
    CHECK(!r.hit && r.allocated && !r.memory_write);
    CHECK(cache_count_dirty(&cache) == 1U);

    r = cache_access(&cache, ACCESS_READ, 0x040U);   /* evicts the dirty line */
    CHECK(r.evicted && r.writeback);
    CHECK(cache_count_dirty(&cache) == 0U);

    /* The block loaded by a read must start clean, so evicting it is free. */
    r = cache_access(&cache, ACCESS_READ, 0x000U);
    CHECK(r.evicted && !r.writeback);
    cache_destroy(&cache);
}

static void test_write_through_no_allocate(void)
{
    SimulatorConfig config;
    Cache cache;
    CacheAccessResult r;

    config_set_defaults(&config);
    config.cache_size_bytes = 64U;
    config.block_size_bytes = 64U;
    config.write_policy = WRITE_POLICY_WRITE_THROUGH;
    config.write_allocate = false;
    CHECK(cache_initialize(&cache, &config, NULL, 0U));

    r = cache_access(&cache, ACCESS_WRITE, 0x000U);
    CHECK(!r.hit && !r.allocated && r.memory_write);
    CHECK(!hit(&cache, 0x000U));                     /* write did not load it */

    r = cache_access(&cache, ACCESS_WRITE, 0x000U);   /* now a write hit */
    CHECK(r.hit && r.memory_write);
    CHECK(cache_count_dirty(&cache) == 0U);
    cache_destroy(&cache);
}

static void test_parser_lines(void)
{
    AccessType type;
    uint32_t address;
    bool is_entry;

    CHECK(parser_parse_line("0x1F40\n", &type, &address, &is_entry));
    CHECK(type == ACCESS_READ && address == 0x1F40U);

    CHECK(parser_parse_line("  W 0x10", &type, &address, &is_entry));
    CHECK(type == ACCESS_WRITE && address == 0x10U);

    CHECK(parser_parse_line("r ff", &type, &address, &is_entry));
    CHECK(type == ACCESS_READ && address == 0xFFU);

    CHECK(!parser_parse_line("# comment", &type, &address, &is_entry) && !is_entry);
    CHECK(!parser_parse_line("   \n", &type, &address, &is_entry) && !is_entry);
    CHECK(!parser_parse_line("0x1 junk", &type, &address, &is_entry) && is_entry);
    CHECK(!parser_parse_line("0x100000000", &type, &address, &is_entry) && is_entry);
}

static void test_statistics(void)
{
    Statistics s;
    CacheAccessResult hit_result = {0};
    CacheAccessResult miss_result = {0};

    hit_result.hit = true;
    statistics_reset(&s);
    statistics_record(&s, ACCESS_READ, &hit_result);
    statistics_record(&s, ACCESS_READ, &hit_result);
    statistics_record(&s, ACCESS_WRITE, &hit_result);
    statistics_record(&s, ACCESS_READ, &miss_result);

    CHECK(statistics_accesses(&s) == 4U);
    CHECK(statistics_misses(&s) == 1U);
    CHECK(statistics_hit_rate(&s) == 75.0);
    /* 1 + 0.25 * 100 */
    CHECK(statistics_amat(&s, 1U, 100U) == 26.0);
}

/* ---------------------------------------------------------------------
 * Property tests: rules that must hold for every configuration. Each one
 * runs a pseudo-random read/write trace through many cache setups.
 * ------------------------------------------------------------------- */

#define PROPERTY_TRACE_LENGTH 5000U

typedef struct TraceEntry {
    AccessType type;
    uint32_t address;
} TraceEntry;

static TraceEntry property_trace[PROPERTY_TRACE_LENGTH];

static uint32_t test_random(uint32_t *state)
{
    uint32_t x = *state;

    x ^= x << 13U;
    x ^= x >> 17U;
    x ^= x << 5U;
    *state = x;
    return x;
}

/* Mostly nearby addresses (so there are hits) with occasional far jumps. */
static void build_property_trace(uint32_t seed)
{
    uint32_t state = seed;
    uint32_t address = 0U;
    size_t i;

    for (i = 0U; i < PROPERTY_TRACE_LENGTH; ++i) {
        uint32_t r = test_random(&state);

        if (r % 10U < 7U) {
            address += (r >> 8) % 64U;
        } else {
            address = (r >> 4) % (16U * 1024U);
        }
        property_trace[i].address = address % (16U * 1024U);
        property_trace[i].type = (r >> 20) % 4U == 0U ? ACCESS_WRITE : ACCESS_READ;
    }
}

static size_t count_valid(const Cache *cache)
{
    size_t i;
    size_t valid = 0U;

    for (i = 0U; i < cache->set_count * cache->ways; ++i) {
        valid += cache->lines[i].valid ? 1U : 0U;
    }
    return valid;
}

static Statistics run_property_trace(const SimulatorConfig *config, Cache *cache)
{
    Statistics stats;
    size_t i;

    statistics_reset(&stats);
    (void)cache_initialize(cache, config, NULL, 0U);
    for (i = 0U; i < PROPERTY_TRACE_LENGTH; ++i) {
        CacheAccessResult r = cache_access(cache, property_trace[i].type,
                                           property_trace[i].address);
        statistics_record(&stats, property_trace[i].type, &r);
    }
    return stats;
}

static void test_invariants_all_configs(void)
{
    static const size_t sizes[] = {256U, 1024U, 4096U};
    static const size_t blocks[] = {16U, 64U};
    static const size_t ways[] = {1U, 2U, 4U, 64U};
    size_t a, b, w, p, wp, alloc;
    int configs = 0;
    bool all_ok = true;

    build_property_trace(12345U);

    for (a = 0U; a < 3U; ++a)
    for (b = 0U; b < 2U; ++b)
    for (w = 0U; w < 4U; ++w)
    for (p = 0U; p < 3U; ++p)
    for (wp = 0U; wp < 2U; ++wp)
    for (alloc = 0U; alloc < 2U; ++alloc) {
        SimulatorConfig config;
        Cache cache;
        Statistics s;
        size_t lines = sizes[a] / blocks[b];
        uint64_t write_misses;
        bool ok = true;

        if (ways[w] > lines) {
            continue;
        }
        config_set_defaults(&config);
        config.cache_size_bytes = sizes[a];
        config.block_size_bytes = blocks[b];
        config.associativity = ways[w];
        config.replacement_policy = (ReplacementPolicy)p;
        config.write_policy = (WritePolicy)wp;
        config.write_allocate = alloc == 1U;

        s = run_property_trace(&config, &cache);
        write_misses = s.writes - s.write_hits;
        ++configs;

        /* Every access is counted exactly once. */
        ok = ok && statistics_accesses(&s) == PROPERTY_TRACE_LENGTH;
        ok = ok && s.read_hits <= s.reads && s.write_hits <= s.writes;

        /* Each fill either used an empty line or evicted one. */
        ok = ok && s.blocks_fetched == s.evictions + count_valid(&cache);
        ok = ok && count_valid(&cache) <= lines;

        /* Only dirty lines are written back, so never more than evictions. */
        ok = ok && s.writebacks <= s.evictions;

        if (config.write_allocate) {
            /* Every miss loads a block. */
            ok = ok && s.blocks_fetched == statistics_misses(&s);
        } else {
            /* Only read misses load a block; write misses go to memory. */
            ok = ok && s.blocks_fetched == s.reads - s.read_hits;
            ok = ok && s.memory_writes >= write_misses;
        }

        if (config.write_policy == WRITE_POLICY_WRITE_THROUGH) {
            /* Write-through never holds dirty data. */
            ok = ok && s.writebacks == 0U && cache_count_dirty(&cache) == 0U;
            ok = ok && s.memory_writes == s.writes;
        } else {
            /* Write-back: only write misses with no-allocate touch memory directly. */
            ok = ok && s.memory_writes == (config.write_allocate ? 0U : write_misses);
        }

        if (!ok) {
            (void)fprintf(stderr, "  invariant broken: size=%zu block=%zu ways=%zu "
                                  "policy=%zu write=%zu allocate=%zu\n",
                          sizes[a], blocks[b], ways[w], p, wp, alloc);
        }
        all_ok = all_ok && ok;
        cache_destroy(&cache);
    }

    CHECK(all_ok);
    CHECK(configs == 252);
}

static void test_direct_mapped_ignores_policy(void)
{
    /* With one way there is nothing to choose, so every policy must agree. */
    SimulatorConfig config;
    Cache cache;
    Statistics s;
    uint64_t hits[3];
    size_t p;

    build_property_trace(777U);
    for (p = 0U; p < 3U; ++p) {
        config_set_defaults(&config);
        config.cache_size_bytes = 1024U;
        config.replacement_policy = (ReplacementPolicy)p;
        s = run_property_trace(&config, &cache);
        hits[p] = statistics_hits(&s);
        cache_destroy(&cache);
    }
    CHECK(hits[0] == hits[1] && hits[1] == hits[2]);
}

static void test_write_policy_does_not_change_hits(void)
{
    /* With write-allocate, WB and WT keep the same blocks; only traffic differs. */
    SimulatorConfig config;
    Cache cache;
    Statistics wb, wt;

    build_property_trace(4242U);
    config_set_defaults(&config);
    config.cache_size_bytes = 2048U;
    config.associativity = 4U;

    config.write_policy = WRITE_POLICY_WRITE_BACK;
    wb = run_property_trace(&config, &cache);
    cache_destroy(&cache);

    config.write_policy = WRITE_POLICY_WRITE_THROUGH;
    wt = run_property_trace(&config, &cache);
    cache_destroy(&cache);

    CHECK(statistics_hits(&wb) == statistics_hits(&wt));
    CHECK(wb.memory_writes == 0U && wt.memory_writes == wt.writes);
}

static void test_lru_bigger_cache_never_worse(void)
{
    /*
     * LRU is a "stack algorithm": a fully associative LRU cache of N+1 lines
     * always holds everything an N-line one holds, so hits can only go up as
     * the cache grows. FIFO does not have this property (Belady's anomaly).
     */
    SimulatorConfig config;
    Cache cache;
    uint64_t previous = 0U;
    size_t size;
    bool monotonic = true;

    build_property_trace(99U);
    for (size = 128U; size <= 8192U; size *= 2U) {
        Statistics s;

        config_set_defaults(&config);
        config.cache_size_bytes = size;
        config.associativity = size / config.block_size_bytes;
        s = run_property_trace(&config, &cache);
        cache_destroy(&cache);

        monotonic = monotonic && statistics_hits(&s) >= previous;
        previous = statistics_hits(&s);
    }
    CHECK(monotonic);
}

int main(void)
{
    struct {
        const char *name;
        void (*run)(void);
    } tests[] = {
        {"config validation", test_config_validation},
        {"address decode", test_address_decode},
        {"direct-mapped conflict", test_direct_mapped_conflict},
        {"LRU replacement", test_lru},
        {"FIFO replacement", test_fifo},
        {"random is repeatable", test_random_is_repeatable},
        {"write-back", test_write_back},
        {"write-through, no-write-allocate", test_write_through_no_allocate},
        {"parser lines", test_parser_lines},
        {"statistics", test_statistics},
        {"invariants, all configs", test_invariants_all_configs},
        {"direct-mapped ignores policy", test_direct_mapped_ignores_policy},
        {"write policy keeps hits", test_write_policy_does_not_change_hits},
        {"LRU: bigger is never worse", test_lru_bigger_cache_never_worse},
    };
    size_t i;

    for (i = 0U; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        int before = failures;

        tests[i].run();
        (void)printf("%-36s %s\n", tests[i].name, failures == before ? "ok" : "FAILED");
    }

    (void)printf("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
