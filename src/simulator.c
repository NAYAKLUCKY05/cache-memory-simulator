#include "simulator.h"

#include <inttypes.h>

#include "parser.h"

static void set_error(char *buffer, size_t size, const char *message)
{
    if (buffer != NULL && size > 0U) {
        (void)snprintf(buffer, size, "%s", message);
    }
}

static void print_access(FILE *out, AccessType type, uint32_t address,
                         const CacheAccessResult *r)
{
    (void)fprintf(out, "%c 0x%08" PRIX32 "  tag=0x%-6" PRIX32 " set=%-5" PRIu32
                       " off=%-3" PRIu32 " %-4s",
                  type == ACCESS_WRITE ? 'W' : 'R', address, r->address.tag,
                  r->address.set_index, r->address.block_offset,
                  r->hit ? "HIT" : "MISS");

    if (r->evicted) {
        (void)fprintf(out, "  evict tag=0x%" PRIX32 "%s", r->evicted_tag,
                      r->writeback ? " (write-back)" : "");
    }
    (void)fputc('\n', out);
}

static bool on_trace_entry(AccessType type, uint32_t address,
                           size_t line_number, void *context)
{
    Simulator *sim = context;
    CacheAccessResult result;

    (void)line_number;
    result = cache_access(&sim->cache, type, address);
    statistics_record(&sim->stats, type, &result);

    if (sim->config.verbose && sim->trace_output != NULL) {
        print_access(sim->trace_output, type, address, &result);
    }
    return true;
}

bool simulator_initialize(Simulator *sim, const SimulatorConfig *config,
                          char *error_message, size_t error_message_size)
{
    if (sim == NULL || config == NULL) {
        set_error(error_message, error_message_size,
                  "Simulator and configuration must not be NULL.");
        return false;
    }

    if (!cache_initialize(&sim->cache, config, error_message, error_message_size)) {
        return false;
    }

    sim->config = *config;
    /* Keep CSV/JSON on stdout clean so it can be piped into other tools. */
    sim->trace_output = config->output_format == OUTPUT_FORMAT_TEXT ? stdout : stderr;
    statistics_reset(&sim->stats);
    return true;
}

void simulator_destroy(Simulator *sim)
{
    if (sim == NULL) {
        return;
    }
    cache_destroy(&sim->cache);
    statistics_reset(&sim->stats);
}

bool simulator_run_trace(Simulator *sim, const char *trace_path,
                         char *error_message, size_t error_message_size)
{
    if (sim == NULL || sim->cache.lines == NULL) {
        set_error(error_message, error_message_size,
                  "Simulator has not been initialized.");
        return false;
    }

    statistics_reset(&sim->stats);
    return parser_read_trace(trace_path, on_trace_entry, sim, error_message,
                             error_message_size);
}

static void print_text(const Simulator *sim, FILE *out)
{
    const SimulatorConfig *c = &sim->config;
    const Statistics *s = &sim->stats;

    (void)fprintf(out, "\nCache Memory Simulator Report\n");
    (void)fprintf(out, "=============================\n");
    (void)fprintf(out, "Configuration\n");
    (void)fprintf(out, "  Organization:    %s\n",
                  config_organization_name(config_get_organization(c)));
    (void)fprintf(out, "  Cache size:      %zu bytes\n", c->cache_size_bytes);
    (void)fprintf(out, "  Block size:      %zu bytes\n", c->block_size_bytes);
    (void)fprintf(out, "  Associativity:   %zu-way (%zu sets)\n", c->associativity,
                  sim->cache.set_count);
    (void)fprintf(out, "  Address split:   tag %u | index %u | offset %u bits\n",
                  32U - sim->cache.index_bits - sim->cache.offset_bits,
                  (unsigned int)sim->cache.index_bits,
                  (unsigned int)sim->cache.offset_bits);
    (void)fprintf(out, "  Replacement:     %s",
                  config_replacement_policy_name(c->replacement_policy));
    if (c->replacement_policy == REPLACEMENT_POLICY_RANDOM) {
        (void)fprintf(out, " (seed %" PRIu32 ")", c->random_seed);
    }
    (void)fprintf(out, "\n  Write policy:    %s, %s\n",
                  config_write_policy_name(c->write_policy),
                  c->write_allocate ? "write-allocate" : "no-write-allocate");
    (void)fprintf(out, "  Timing:          hit %u cycles, miss penalty %u cycles\n",
                  c->hit_time_cycles, c->miss_penalty_cycles);

    (void)fprintf(out, "\nResults\n");
    (void)fprintf(out, "  Accesses:        %" PRIu64 " (%" PRIu64 " reads, %" PRIu64
                       " writes)\n",
                  statistics_accesses(s), s->reads, s->writes);
    (void)fprintf(out, "  Hits:            %" PRIu64 " (%" PRIu64 " read, %" PRIu64
                       " write)\n",
                  statistics_hits(s), s->read_hits, s->write_hits);
    (void)fprintf(out, "  Misses:          %" PRIu64 "\n", statistics_misses(s));
    (void)fprintf(out, "  Hit rate:        %.2f%%\n", statistics_hit_rate(s));
    (void)fprintf(out, "  Miss rate:       %.2f%%\n", statistics_miss_rate(s));
    (void)fprintf(out, "  Evictions:       %" PRIu64 "\n", s->evictions);
    (void)fprintf(out, "  AMAT:            %.2f cycles\n",
                  statistics_amat(s, c->hit_time_cycles, c->miss_penalty_cycles));

    (void)fprintf(out, "\nMemory traffic\n");
    (void)fprintf(out, "  Blocks fetched:  %" PRIu64 "\n", s->blocks_fetched);
    (void)fprintf(out, "  Write-backs:     %" PRIu64 "\n", s->writebacks);
    (void)fprintf(out, "  Direct writes:   %" PRIu64 "\n", s->memory_writes);
    (void)fprintf(out, "  Dirty at end:    %zu\n", cache_count_dirty(&sim->cache));
}

static void print_csv(const Simulator *sim, FILE *out)
{
    const SimulatorConfig *c = &sim->config;
    const Statistics *s = &sim->stats;

    (void)fprintf(out, "cache_size,block_size,associativity,replacement,write_policy,"
                       "write_allocate,accesses,reads,writes,hits,misses,hit_rate,"
                       "miss_rate,evictions,writebacks,memory_writes,blocks_fetched,dirty_at_end,amat\n");
    (void)fprintf(out, "%zu,%zu,%zu,%s,%s,%d,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
                       ",%" PRIu64 ",%.4f,%.4f,%" PRIu64 ",%" PRIu64 ",%" PRIu64
                       ",%" PRIu64 ",%zu,%.4f\n",
                  c->cache_size_bytes, c->block_size_bytes, c->associativity,
                  config_replacement_policy_name(c->replacement_policy),
                  config_write_policy_name(c->write_policy), c->write_allocate ? 1 : 0,
                  statistics_accesses(s), s->reads, s->writes, statistics_hits(s),
                  statistics_misses(s), statistics_hit_rate(s), statistics_miss_rate(s),
                  s->evictions, s->writebacks, s->memory_writes, s->blocks_fetched,
                  cache_count_dirty(&sim->cache),
                  statistics_amat(s, c->hit_time_cycles, c->miss_penalty_cycles));
}

static void print_json(const Simulator *sim, FILE *out)
{
    const SimulatorConfig *c = &sim->config;
    const Statistics *s = &sim->stats;

    (void)fprintf(out, "{\n");
    (void)fprintf(out, "  \"config\": {\n");
    (void)fprintf(out, "    \"organization\": \"%s\",\n",
                  config_organization_name(config_get_organization(c)));
    (void)fprintf(out, "    \"cache_size\": %zu,\n", c->cache_size_bytes);
    (void)fprintf(out, "    \"block_size\": %zu,\n", c->block_size_bytes);
    (void)fprintf(out, "    \"associativity\": %zu,\n", c->associativity);
    (void)fprintf(out, "    \"sets\": %zu,\n", sim->cache.set_count);
    (void)fprintf(out, "    \"replacement\": \"%s\",\n",
                  config_replacement_policy_name(c->replacement_policy));
    (void)fprintf(out, "    \"write_policy\": \"%s\",\n",
                  config_write_policy_name(c->write_policy));
    (void)fprintf(out, "    \"write_allocate\": %s,\n", c->write_allocate ? "true" : "false");
    (void)fprintf(out, "    \"hit_time\": %u,\n", c->hit_time_cycles);
    (void)fprintf(out, "    \"miss_penalty\": %u\n", c->miss_penalty_cycles);
    (void)fprintf(out, "  },\n");
    (void)fprintf(out, "  \"results\": {\n");
    (void)fprintf(out, "    \"accesses\": %" PRIu64 ",\n", statistics_accesses(s));
    (void)fprintf(out, "    \"reads\": %" PRIu64 ",\n", s->reads);
    (void)fprintf(out, "    \"writes\": %" PRIu64 ",\n", s->writes);
    (void)fprintf(out, "    \"hits\": %" PRIu64 ",\n", statistics_hits(s));
    (void)fprintf(out, "    \"misses\": %" PRIu64 ",\n", statistics_misses(s));
    (void)fprintf(out, "    \"hit_rate\": %.4f,\n", statistics_hit_rate(s));
    (void)fprintf(out, "    \"miss_rate\": %.4f,\n", statistics_miss_rate(s));
    (void)fprintf(out, "    \"evictions\": %" PRIu64 ",\n", s->evictions);
    (void)fprintf(out, "    \"writebacks\": %" PRIu64 ",\n", s->writebacks);
    (void)fprintf(out, "    \"memory_writes\": %" PRIu64 ",\n", s->memory_writes);
    (void)fprintf(out, "    \"blocks_fetched\": %" PRIu64 ",\n", s->blocks_fetched);
    (void)fprintf(out, "    \"dirty_at_end\": %zu,\n", cache_count_dirty(&sim->cache));
    (void)fprintf(out, "    \"amat\": %.4f\n",
                  statistics_amat(s, c->hit_time_cycles, c->miss_penalty_cycles));
    (void)fprintf(out, "  }\n");
    (void)fprintf(out, "}\n");
}

void simulator_print_report(const Simulator *sim, FILE *output)
{
    if (sim == NULL || output == NULL) {
        return;
    }

    switch (sim->config.output_format) {
    case OUTPUT_FORMAT_CSV:
        print_csv(sim, output);
        break;
    case OUTPUT_FORMAT_JSON:
        print_json(sim, output);
        break;
    case OUTPUT_FORMAT_TEXT:
    default:
        print_text(sim, output);
        break;
    }
}
