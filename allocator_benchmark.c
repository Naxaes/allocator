#include "allocator.h"
#include "pool_allocator.h"
#include "slab_allocator.h"
#include "stack_allocator.h"
#include "system_allocator.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define BENCHMARK_MAX_SIZES 16
#define BENCHMARK_MAX_REPEATS 32
#define BENCHMARK_SCENARIO_COUNT 5

static volatile uint64_t benchmark_sink = 0;

enum BenchmarkFormat {
    BENCHMARK_FORMAT_TEXT,
    BENCHMARK_FORMAT_CSV,
    BENCHMARK_FORMAT_JSON,
};

struct BenchmarkResult {
    const char *name;
    size_t operations;
    uint64_t elapsed_ns;
};

struct BenchmarkSummary {
    const char *name;
    size_t payload_size;
    size_t operations;
    size_t warmup_runs;
    size_t repeat_runs;
    uint64_t samples_ns[BENCHMARK_MAX_REPEATS];
    uint64_t min_ns;
    uint64_t median_ns;
    uint64_t max_ns;
    double average_ns;
    double average_ns_per_op;
};

struct BenchmarkConfig {
    size_t operations;
    size_t warmup_runs;
    size_t repeat_runs;
    size_t size_count;
    size_t sizes[BENCHMARK_MAX_SIZES];
    int include_raw_samples;
    enum BenchmarkFormat format;
    const char *output_path;
};

typedef struct BenchmarkResult (*BenchmarkFunction)(size_t operations, size_t allocation_size);

struct BenchmarkScenario {
    const char *name;
    BenchmarkFunction function;
};

static uint64_t now_ns(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((uint64_t)tv.tv_sec * 1000000000ULL) + ((uint64_t)tv.tv_usec * 1000ULL);
}

static void print_usage(const char *program_name) {
    printf("Usage: %s [operations] [options]\n", program_name);
    puts("Options:");
    puts("  --operations <count>   Requested operation count per scenario (default: 200000)");
    puts("  --sizes <list>         Comma-separated payload sizes in bytes (default: 8,16,32,64,128,256)");
    puts("  --warmup <count>       Number of warmup runs per scenario/size (default: 1)");
    puts("  --repeat <count>       Number of timed runs per scenario/size (default: 5)");
    puts("  --raw-samples          Include individual timed repeats in CSV/JSON output");
    puts("  --format <text|csv|json>  Output format (default: text)");
    puts("  --output <path>        Write results to a file instead of stdout");
    puts("  --help                 Show this help text");
}

static int parse_size_t_value(const char *text, size_t *value) {
    char *end = NULL;
    const unsigned long long parsed = strtoull(text, &end, 10);

    if (text[0] == '\0' || (end != NULL && *end != '\0')) {
        return -1;
    }

    *value = (size_t)parsed;
    return 0;
}

static int parse_format(const char *text, enum BenchmarkFormat *format) {
    if (strcmp(text, "text") == 0) {
        *format = BENCHMARK_FORMAT_TEXT;
        return 0;
    }
    if (strcmp(text, "csv") == 0) {
        *format = BENCHMARK_FORMAT_CSV;
        return 0;
    }
    if (strcmp(text, "json") == 0) {
        *format = BENCHMARK_FORMAT_JSON;
        return 0;
    }

    return -1;
}

static int parse_sizes(const char *text, struct BenchmarkConfig *config) {
    char *cursor = NULL;
    char *copy = strdup(text);

    if (copy == NULL) {
        return -1;
    }

    config->size_count = 0;
    cursor = strtok(copy, ",");
    while (cursor != NULL) {
        size_t size = 0;

        if (config->size_count >= BENCHMARK_MAX_SIZES || parse_size_t_value(cursor, &size) != 0 || size == 0) {
            free(copy);
            return -1;
        }

        config->sizes[config->size_count++] = size;
        cursor = strtok(NULL, ",");
    }

    free(copy);
    return config->size_count == 0 ? -1 : 0;
}

static int read_option_value(int argc, char **argv, int *index, const char **value) {
    if (*index + 1 >= argc) {
        return -1;
    }

    *index += 1;
    *value = argv[*index];
    return 0;
}

static int compare_u64(const void *left, const void *right) {
    const uint64_t lhs = *(const uint64_t *)left;
    const uint64_t rhs = *(const uint64_t *)right;

    if (lhs < rhs) {
        return -1;
    }
    if (lhs > rhs) {
        return 1;
    }
    return 0;
}

static size_t ceil_div_size(size_t numerator, size_t denominator) {
    return (numerator + denominator - 1) / denominator;
}

static void touch_region(struct MemoryRegion region, size_t seed) {
    const size_t bytes_to_touch = region.size < 32 ? region.size : 32;
    memset(region.base, (int)(seed & 0xFF), bytes_to_touch);
    benchmark_sink += ((const unsigned char *)region.base)[0] + region.size;
}

static struct BenchmarkResult benchmark_malloc_baseline(size_t operations, size_t allocation_size) {
    const uint64_t start = now_ns();

    for (size_t i = 0; i < operations; ++i) {
        struct MemoryRegion region = {
            .base = malloc(allocation_size),
            .size = allocation_size,
        };

        if (region.base == NULL) {
            fprintf(stderr, "malloc benchmark failed to allocate %zu bytes\n", allocation_size);
            exit(EXIT_FAILURE);
        }

        touch_region(region, i);
        free(region.base);
    }

    return (struct BenchmarkResult){
        .name = "direct malloc/free",
        .operations = operations,
        .elapsed_ns = now_ns() - start,
    };
}

static struct AllocatorOptions default_system_options(void) {
    return (struct AllocatorOptions){
        .oom_strategy = OOM_STRATEGY_PANIC,
        .alignment = allocator_default_alignment_exponent(),
    };
}

static struct BenchmarkResult benchmark_system_allocator(size_t operations, size_t allocation_size) {
    struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    push_allocator(&system_allocator.allocator);
    const uint64_t start = now_ns();

    for (size_t i = 0; i < operations; ++i) {
        struct MemoryRegion region = allocate(allocation_size);
        touch_region(region, i);
        deallocate(region);
    }

    pop_allocator();
    return (struct BenchmarkResult){
        .name = "allocator api system",
        .operations = operations,
        .elapsed_ns = now_ns() - start,
    };
}

static struct BenchmarkResult benchmark_stack_allocator(size_t operations, size_t allocation_size) {
    const size_t batch_size = 256;
    const size_t batches = ceil_div_size(operations, batch_size);
    struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    push_allocator(&system_allocator.allocator);
    const uint64_t start = now_ns();

    for (size_t batch = 0; batch < batches; ++batch) {
        struct StackAllocator stack_allocator = make_stack_allocator((struct AllocatorOptions){
            .parent = &system_allocator.allocator,
            .oom_strategy = OOM_STRATEGY_GROW,
            .alignment = allocator_default_alignment_exponent(),
        });
        push_allocator(&stack_allocator.allocator);

        for (size_t i = 0; i < batch_size; ++i) {
            const struct MemoryRegion region = allocate(allocation_size);
            touch_region(region, batch * batch_size + i);
        }

        pop_allocator();
    }

    pop_allocator();
    return (struct BenchmarkResult){
        .name = "stack batch allocate",
        .operations = batches * batch_size,
        .elapsed_ns = now_ns() - start,
    };
}

static struct BenchmarkResult benchmark_pool_allocator(size_t operations, size_t allocation_size) {
    const uint32_t capacity = 256;
    const size_t batches = ceil_div_size(operations, capacity);
    struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    push_allocator(&system_allocator.allocator);
    struct PoolAllocator pool_allocator = make_pool_allocator((struct PoolAllocatorOptions){
        .allocator_options = {
            .parent = &system_allocator.allocator,
            .oom_strategy = OOM_STRATEGY_PANIC,
            .alignment = allocator_default_alignment_exponent(),
        },
        .slot_size = allocation_size,
        .capacity = capacity,
    });
    push_allocator(&pool_allocator.allocator);
    struct MemoryRegion *regions = malloc(sizeof(*regions) * capacity);
    const uint64_t start = now_ns();

    if (regions == NULL) {
        fprintf(stderr, "Failed to allocate benchmark scratch buffer\n");
        exit(EXIT_FAILURE);
    }

    for (size_t batch = 0; batch < batches; ++batch) {
        for (uint32_t i = 0; i < capacity; ++i) {
            regions[i] = allocate(allocation_size);
            touch_region(regions[i], batch * capacity + i);
        }
        for (uint32_t i = 0; i < capacity; ++i) {
            deallocate(regions[i]);
        }
    }

    free(regions);
    pop_allocator();
    pop_allocator();
    return (struct BenchmarkResult){
        .name = "pool alloc/free",
        .operations = batches * (size_t)capacity,
        .elapsed_ns = now_ns() - start,
    };
}

static struct BenchmarkResult benchmark_slab_allocator(size_t operations, size_t allocation_size) {
    const uint32_t slots_per_slab = 128;
    const uint32_t working_set = 512;
    const size_t batches = ceil_div_size(operations, working_set);
    struct SystemAllocator system_allocator = make_system_allocator(default_system_options());
    push_allocator(&system_allocator.allocator);
    struct SlabAllocator slab_allocator = make_slab_allocator((struct SlabAllocatorOptions){
        .allocator_options = {
            .parent = &system_allocator.allocator,
            .oom_strategy = OOM_STRATEGY_GROW,
            .alignment = allocator_default_alignment_exponent(),
        },
        .slot_size = allocation_size,
        .slots_per_slab = slots_per_slab,
    });
    push_allocator(&slab_allocator.allocator);
    struct MemoryRegion *regions = malloc(sizeof(*regions) * working_set);
    const uint64_t start = now_ns();

    if (regions == NULL) {
        fprintf(stderr, "Failed to allocate benchmark scratch buffer\n");
        exit(EXIT_FAILURE);
    }

    for (size_t batch = 0; batch < batches; ++batch) {
        for (uint32_t i = 0; i < working_set; ++i) {
            regions[i] = allocate(allocation_size);
            touch_region(regions[i], batch * working_set + i);
        }
        for (uint32_t i = 0; i < working_set; ++i) {
            deallocate(regions[i]);
        }
    }

    free(regions);
    pop_allocator();
    pop_allocator();
    return (struct BenchmarkResult){
        .name = "slab alloc/free",
        .operations = batches * (size_t)working_set,
        .elapsed_ns = now_ns() - start,
    };
}

static struct BenchmarkSummary summarize_samples(
        const char *name,
        size_t payload_size,
        size_t warmup_runs,
        size_t repeat_runs,
        const struct BenchmarkResult *samples) {
    uint64_t sorted[BENCHMARK_MAX_REPEATS];
    uint64_t sum_ns = 0;

    for (size_t i = 0; i < repeat_runs; ++i) {
        sorted[i] = samples[i].elapsed_ns;
        sum_ns += samples[i].elapsed_ns;
    }

    qsort(sorted, repeat_runs, sizeof(sorted[0]), compare_u64);

    {
        struct BenchmarkSummary summary = {
        .name = name,
        .payload_size = payload_size,
        .operations = samples[0].operations,
        .warmup_runs = warmup_runs,
        .repeat_runs = repeat_runs,
        .min_ns = sorted[0],
        .median_ns = sorted[repeat_runs / 2],
        .max_ns = sorted[repeat_runs - 1],
        .average_ns = (double)sum_ns / (double)repeat_runs,
        .average_ns_per_op = (double)sum_ns / ((double)repeat_runs * (double)samples[0].operations),
        };

        for (size_t i = 0; i < repeat_runs; ++i) {
            summary.samples_ns[i] = samples[i].elapsed_ns;
        }

        return summary;
    }
}

static void print_text_output(FILE *stream, const struct BenchmarkConfig *config, const struct BenchmarkSummary *summaries, size_t summary_count) {
    fprintf(stream, "Allocator benchmark\n");
    fprintf(stream, "===================\n");
    fprintf(stream, "requested operations: %zu\n", config->operations);
    fprintf(stream, "warmup runs:          %zu\n", config->warmup_runs);
    fprintf(stream, "timed repeats:        %zu\n\n", config->repeat_runs);

    for (size_t i = 0; i < summary_count; ++i) {
        const struct BenchmarkSummary summary = summaries[i];
        fprintf(stream,
                "%-20s size=%4zu ops=%8zu avg=%10.2f ns/op min=%10" PRIu64 " ns med=%10" PRIu64 " ns max=%10" PRIu64 " ns\n",
                summary.name,
                summary.payload_size,
                summary.operations,
                summary.average_ns_per_op,
                summary.min_ns,
                summary.median_ns,
                summary.max_ns);
    }

    fprintf(stream, "\nbenchmark sink: %" PRIu64 "\n", benchmark_sink);
}

static void print_csv_output(FILE *stream, const struct BenchmarkConfig *config, const struct BenchmarkSummary *summaries, size_t summary_count) {
    fprintf(stream, "row_kind,allocator,payload_size,operations,warmup_runs,repeat_runs,sample_index,elapsed_ns,avg_ns,min_ns,median_ns,max_ns,avg_ns_per_op\n");
    for (size_t i = 0; i < summary_count; ++i) {
        const struct BenchmarkSummary summary = summaries[i];
        fprintf(stream,
                "summary,%s,%zu,%zu,%zu,%zu,,,%0.2f,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%.2f\n",
                summary.name,
                summary.payload_size,
                summary.operations,
                summary.warmup_runs,
                summary.repeat_runs,
                summary.average_ns,
                summary.min_ns,
                summary.median_ns,
                summary.max_ns,
                summary.average_ns_per_op);

        if (!config->include_raw_samples) {
            continue;
        }

        for (size_t sample_index = 0; sample_index < summary.repeat_runs; ++sample_index) {
            fprintf(stream,
                    "sample,%s,%zu,%zu,%zu,%zu,%zu,%" PRIu64 ",,,,,\n",
                    summary.name,
                    summary.payload_size,
                    summary.operations,
                    summary.warmup_runs,
                    summary.repeat_runs,
                    sample_index,
                    summary.samples_ns[sample_index]);
        }
    }
}

static void print_json_output(FILE *stream, const struct BenchmarkConfig *config, const struct BenchmarkSummary *summaries, size_t summary_count) {
    fprintf(stream, "{\n");
    fprintf(stream, "  \"operations_requested\": %zu,\n", config->operations);
    fprintf(stream, "  \"warmup_runs\": %zu,\n", config->warmup_runs);
    fprintf(stream, "  \"repeat_runs\": %zu,\n", config->repeat_runs);
    fprintf(stream, "  \"benchmark_sink\": %" PRIu64 ",\n", benchmark_sink);
    fprintf(stream, "  \"results\": [\n");

    for (size_t i = 0; i < summary_count; ++i) {
        const struct BenchmarkSummary summary = summaries[i];
        fprintf(stream,
                "    {\"allocator\": \"%s\", \"payload_size\": %zu, \"operations\": %zu, \"warmup_runs\": %zu, \"repeat_runs\": %zu, \"avg_ns\": %.2f, \"min_ns\": %" PRIu64 ", \"median_ns\": %" PRIu64 ", \"max_ns\": %" PRIu64 ", \"avg_ns_per_op\": %.2f",
                summary.name,
                summary.payload_size,
                summary.operations,
                summary.warmup_runs,
                summary.repeat_runs,
                summary.average_ns,
                summary.min_ns,
                summary.median_ns,
                summary.max_ns,
                summary.average_ns_per_op);

        if (config->include_raw_samples) {
            fprintf(stream, ", \"samples_ns\": [");
            for (size_t sample_index = 0; sample_index < summary.repeat_runs; ++sample_index) {
                fprintf(stream, "%s%" PRIu64, sample_index == 0 ? "" : ", ", summary.samples_ns[sample_index]);
            }
            fprintf(stream, "]");
        }

        fprintf(stream, "}%s\n", (i + 1 == summary_count) ? "" : ",");
    }

    fprintf(stream, "  ]\n");
    fprintf(stream, "}\n");
}

static int parse_arguments(int argc, char **argv, struct BenchmarkConfig *config) {
    int positional_operations_consumed = 0;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const char *value = NULL;

        if (strcmp(arg, "--help") == 0) {
            return 1;
        }
        if (strcmp(arg, "--raw-samples") == 0) {
            config->include_raw_samples = 1;
            continue;
        }
        if (strcmp(arg, "--operations") == 0) {
            if (read_option_value(argc, argv, &i, &value) != 0 || parse_size_t_value(value, &config->operations) != 0 || config->operations == 0) {
                return -1;
            }
            continue;
        }
        if (strncmp(arg, "--operations=", 13) == 0) {
            value = arg + 13;
            if (parse_size_t_value(value, &config->operations) != 0 || config->operations == 0) {
                return -1;
            }
            continue;
        }
        if (strcmp(arg, "--sizes") == 0) {
            if (read_option_value(argc, argv, &i, &value) != 0 || parse_sizes(value, config) != 0) {
                return -1;
            }
            continue;
        }
        if (strncmp(arg, "--sizes=", 8) == 0) {
            if (parse_sizes(arg + 8, config) != 0) {
                return -1;
            }
            continue;
        }
        if (strcmp(arg, "--warmup") == 0) {
            if (read_option_value(argc, argv, &i, &value) != 0 || parse_size_t_value(value, &config->warmup_runs) != 0) {
                return -1;
            }
            continue;
        }
        if (strncmp(arg, "--warmup=", 9) == 0) {
            if (parse_size_t_value(arg + 9, &config->warmup_runs) != 0) {
                return -1;
            }
            continue;
        }
        if (strcmp(arg, "--repeat") == 0) {
            if (read_option_value(argc, argv, &i, &value) != 0 || parse_size_t_value(value, &config->repeat_runs) != 0 ||
                config->repeat_runs == 0 || config->repeat_runs > BENCHMARK_MAX_REPEATS) {
                return -1;
            }
            continue;
        }
        if (strncmp(arg, "--repeat=", 9) == 0) {
            if (parse_size_t_value(arg + 9, &config->repeat_runs) != 0 ||
                config->repeat_runs == 0 || config->repeat_runs > BENCHMARK_MAX_REPEATS) {
                return -1;
            }
            continue;
        }
        if (strcmp(arg, "--format") == 0) {
            if (read_option_value(argc, argv, &i, &value) != 0 || parse_format(value, &config->format) != 0) {
                return -1;
            }
            continue;
        }
        if (strncmp(arg, "--format=", 9) == 0) {
            if (parse_format(arg + 9, &config->format) != 0) {
                return -1;
            }
            continue;
        }
        if (strcmp(arg, "--output") == 0) {
            if (read_option_value(argc, argv, &i, &value) != 0) {
                return -1;
            }
            config->output_path = value;
            continue;
        }
        if (strncmp(arg, "--output=", 9) == 0) {
            config->output_path = arg + 9;
            continue;
        }
        if (arg[0] != '-' && !positional_operations_consumed) {
            if (parse_size_t_value(arg, &config->operations) != 0 || config->operations == 0) {
                return -1;
            }
            positional_operations_consumed = 1;
            continue;
        }

        return -1;
    }

    return 0;
}

int main(int argc, char **argv) {
    static const size_t default_sizes[] = { 8, 16, 32, 64, 128, 256 };
    static const struct BenchmarkScenario scenarios[BENCHMARK_SCENARIO_COUNT] = {
        { "direct malloc/free", benchmark_malloc_baseline },
        { "allocator api system", benchmark_system_allocator },
        { "stack batch allocate", benchmark_stack_allocator },
        { "pool alloc/free", benchmark_pool_allocator },
        { "slab alloc/free", benchmark_slab_allocator },
    };

    struct BenchmarkConfig config = {
        .operations = 200000,
        .warmup_runs = 1,
        .repeat_runs = 5,
        .size_count = sizeof(default_sizes) / sizeof(default_sizes[0]),
        .include_raw_samples = 0,
        .format = BENCHMARK_FORMAT_TEXT,
        .output_path = NULL,
    };
    struct BenchmarkSummary summaries[BENCHMARK_MAX_SIZES * BENCHMARK_SCENARIO_COUNT];
    size_t summary_count = 0;
    FILE *output_stream = stdout;

    memcpy(config.sizes, default_sizes, sizeof(default_sizes));

    {
        const int parse_result = parse_arguments(argc, argv, &config);
        if (parse_result == 1) {
            print_usage(argv[0]);
            return 0;
        }
        if (parse_result != 0) {
            print_usage(argv[0]);
            return 1;
        }
    }

    if (config.output_path != NULL && strcmp(config.output_path, "-") != 0) {
        output_stream = fopen(config.output_path, "w");
        if (output_stream == NULL) {
            fprintf(stderr, "Failed to open output file: %s\n", config.output_path);
            return 1;
        }
    }

    for (size_t size_index = 0; size_index < config.size_count; ++size_index) {
        const size_t payload_size = config.sizes[size_index];

        for (size_t scenario_index = 0; scenario_index < BENCHMARK_SCENARIO_COUNT; ++scenario_index) {
            const struct BenchmarkScenario scenario = scenarios[scenario_index];
            struct BenchmarkResult samples[BENCHMARK_MAX_REPEATS];

            for (size_t warmup = 0; warmup < config.warmup_runs; ++warmup) {
                (void)scenario.function(config.operations, payload_size);
            }

            for (size_t repeat = 0; repeat < config.repeat_runs; ++repeat) {
                samples[repeat] = scenario.function(config.operations, payload_size);
            }

            summaries[summary_count++] = summarize_samples(
                    scenario.name,
                    payload_size,
                    config.warmup_runs,
                    config.repeat_runs,
                    samples
            );
        }
    }

    switch (config.format) {
        case BENCHMARK_FORMAT_TEXT:
            print_text_output(output_stream, &config, summaries, summary_count);
            break;
        case BENCHMARK_FORMAT_CSV:
            print_csv_output(output_stream, &config, summaries, summary_count);
            break;
        case BENCHMARK_FORMAT_JSON:
            print_json_output(output_stream, &config, summaries, summary_count);
            break;
        default:
            break;
    }

    if (output_stream != stdout) {
        fclose(output_stream);
    }

    return 0;
}
