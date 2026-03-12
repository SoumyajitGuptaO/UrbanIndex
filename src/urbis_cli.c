/**
 * @file urbis_cli.c
 * @brief Command-line utility for the Urbis spatial index
 */

#include "urbis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define URBIS_CLI_OK 0
#define URBIS_CLI_ERR 1

static void print_usage(FILE *out) {
    fprintf(out,
        "Urbis CLI\n"
        "\n"
        "Usage:\n"
        "  urbis <command> [options]\n"
        "\n"
        "Commands:\n"
        "  build         Build an index from GeoJSON or WKT\n"
        "  query-range   Range query against an index\n"
        "  query-knn     K nearest neighbor query\n"
        "  pages         Adjacent page lookup\n"
        "  stats         Print index statistics\n"
        "  version       Show library version\n"
        "\n"
        "Common options:\n"
        "  --index <path>        Index file path\n"
        "  --json                JSON output\n"
        "\n"
        "Build options:\n"
        "  --input <path>        Input file path\n"
        "  --format <geojson|wkt>\n"
        "  --block-size <n>      Objects per block\n"
        "  --page-capacity <n>   Objects per page\n"
        "  --cache-size <n>      Page cache size\n"
        "  --no-quadtree         Disable adjacent-page index\n"
        "\n"
        "Query options:\n"
        "  --mbr <minx,miny,maxx,maxy>\n"
        "  --point <x,y>         Query point (for knn)\n"
        "  --k <n>               Number of neighbors\n"
    );
}

static int parse_size(const char *text, size_t *out) {
    if (!text || !out) return 0;
    char *end = NULL;
    unsigned long long value = strtoull(text, &end, 10);
    if (!end || *end != '\0') return 0;
    *out = (size_t)value;
    return 1;
}

static int parse_point(const char *text, double *x, double *y) {
    if (!text || !x || !y) return 0;
    return sscanf(text, "%lf,%lf", x, y) == 2;
}

static int parse_mbr(const char *text, UrbisMBR *out) {
    if (!text || !out) return 0;
    return sscanf(text, "%lf,%lf,%lf,%lf",
                  &out->min_x, &out->min_y, &out->max_x, &out->max_y) == 4;
}

static const char* geom_type_str(GeomType type) {
    switch (type) {
        case GEOM_POINT: return "point";
        case GEOM_LINESTRING: return "linestring";
        case GEOM_POLYGON: return "polygon";
        default: return "unknown";
    }
}

static int load_index(const char *path, UrbisIndex **out) {
    UrbisStatus status = urbis_index_load(path, out);
    if (status != URBIS_OK) {
        fprintf(stderr, "Failed to load index: %s\n", urbis_status_str(status));
        return URBIS_CLI_ERR;
    }
    return URBIS_CLI_OK;
}

static char* read_text_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size <= 0) {
        fclose(file);
        return NULL;
    }

    char *buffer = (char *)malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)size, file);
    fclose(file);

    buffer[read] = '\0';
    return buffer;
}

static int cmd_build(int argc, char **argv) {
    const char *input = NULL;
    const char *format = "geojson";
    const char *index_path = NULL;
    int use_quadtree = 1;

    UrbisConfig config = urbis_config_default();

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            input = argv[++i];
        } else if (strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
            format = argv[++i];
        } else if (strcmp(argv[i], "--index") == 0 && i + 1 < argc) {
            index_path = argv[++i];
        } else if (strcmp(argv[i], "--block-size") == 0 && i + 1 < argc) {
            parse_size(argv[++i], &config.block_size);
        } else if (strcmp(argv[i], "--page-capacity") == 0 && i + 1 < argc) {
            parse_size(argv[++i], &config.page_capacity);
        } else if (strcmp(argv[i], "--cache-size") == 0 && i + 1 < argc) {
            parse_size(argv[++i], &config.cache_size);
        } else if (strcmp(argv[i], "--no-quadtree") == 0) {
            use_quadtree = 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return URBIS_CLI_ERR;
        }
    }

    if (!input || !index_path) {
        fprintf(stderr, "build requires --input and --index\n");
        return URBIS_CLI_ERR;
    }

    config.enable_quadtree = use_quadtree ? true : false;

    UrbisIndex *idx = NULL;
    UrbisStatus status = urbis_index_create(&config, &idx);
    if (status != URBIS_OK || !idx) {
        fprintf(stderr, "Failed to create index: %s\n", urbis_status_str(status));
        return URBIS_CLI_ERR;
    }

    if (strcmp(format, "geojson") == 0) {
        status = urbis_index_load_geojson(idx, input, NULL);
    } else if (strcmp(format, "wkt") == 0) {
        char *wkt = read_text_file(input);
        if (!wkt) {
            fprintf(stderr, "Failed to read WKT file\n");
            urbis_index_destroy(idx);
            return URBIS_CLI_ERR;
        }
        status = urbis_index_load_wkt(idx, wkt, NULL);
        free(wkt);
    } else {
        fprintf(stderr, "Unsupported format: %s\n", format);
        urbis_index_destroy(idx);
        return URBIS_CLI_ERR;
    }

    if (status != URBIS_OK) {
        fprintf(stderr, "Load failed: %s\n", urbis_status_str(status));
        urbis_index_destroy(idx);
        return URBIS_CLI_ERR;
    }

    status = urbis_index_build(idx);
    if (status != URBIS_OK) {
        fprintf(stderr, "Build failed: %s\n", urbis_status_str(status));
        urbis_index_destroy(idx);
        return URBIS_CLI_ERR;
    }

    status = urbis_index_save(idx, index_path);
    if (status != URBIS_OK) {
        fprintf(stderr, "Save failed: %s\n", urbis_status_str(status));
        urbis_index_destroy(idx);
        return URBIS_CLI_ERR;
    }

    urbis_index_destroy(idx);
    printf("Index saved to %s\n", index_path);
    return URBIS_CLI_OK;
}

static void print_objects_json(const UrbisObjectList *list) {
    printf("{\"count\":%zu,\"objects\":[", list ? list->count : 0UL);
    if (list && list->count > 0) {
        for (size_t i = 0; i < list->count; i++) {
            const UrbisObject *obj = &list->objects[i];
            printf("{\"id\":%llu,\"type\":\"%s\",\"centroid\":[%.9g,%.9g],\"mbr\":[%.9g,%.9g,%.9g,%.9g]}",
                   (unsigned long long)obj->id,
                   geom_type_str(obj->type),
                   obj->centroid.x, obj->centroid.y,
                   obj->mbr.min_x, obj->mbr.min_y, obj->mbr.max_x, obj->mbr.max_y);
            if (i + 1 < list->count) {
                printf(",");
            }
        }
    }
    printf("]}\n");
}

static int cmd_query_range(int argc, char **argv) {
    const char *index_path = NULL;
    const char *mbr_text = NULL;
    int as_json = 0;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--index") == 0 && i + 1 < argc) {
            index_path = argv[++i];
        } else if (strcmp(argv[i], "--mbr") == 0 && i + 1 < argc) {
            mbr_text = argv[++i];
        } else if (strcmp(argv[i], "--json") == 0) {
            as_json = 1;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return URBIS_CLI_ERR;
        }
    }

    if (!index_path || !mbr_text) {
        fprintf(stderr, "query-range requires --index and --mbr\n");
        return URBIS_CLI_ERR;
    }

    UrbisMBR region;
    if (!parse_mbr(mbr_text, &region)) {
        fprintf(stderr, "Invalid mbr format\n");
        return URBIS_CLI_ERR;
    }

    UrbisIndex *idx = NULL;
    if (load_index(index_path, &idx) != URBIS_CLI_OK) return URBIS_CLI_ERR;

    UrbisObjectList *result = NULL;
    UrbisStatus status = urbis_index_query_range(idx, &region, &result);
    if (status != URBIS_OK) {
        fprintf(stderr, "Query failed: %s\n", urbis_status_str(status));
        urbis_index_destroy(idx);
        return URBIS_CLI_ERR;
    }

    if (as_json) {
        print_objects_json(result);
    } else {
        printf("Found %zu objects\n", result ? result->count : 0UL);
    }

    urbis_object_list_free(result);
    urbis_index_destroy(idx);
    return URBIS_CLI_OK;
}

static int cmd_query_knn(int argc, char **argv) {
    const char *index_path = NULL;
    const char *point_text = NULL;
    size_t k = 0;
    int as_json = 0;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--index") == 0 && i + 1 < argc) {
            index_path = argv[++i];
        } else if (strcmp(argv[i], "--point") == 0 && i + 1 < argc) {
            point_text = argv[++i];
        } else if (strcmp(argv[i], "--k") == 0 && i + 1 < argc) {
            parse_size(argv[++i], &k);
        } else if (strcmp(argv[i], "--json") == 0) {
            as_json = 1;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return URBIS_CLI_ERR;
        }
    }

    if (!index_path || !point_text || k == 0) {
        fprintf(stderr, "query-knn requires --index, --point, and --k\n");
        return URBIS_CLI_ERR;
    }

    double x = 0.0, y = 0.0;
    if (!parse_point(point_text, &x, &y)) {
        fprintf(stderr, "Invalid point format\n");
        return URBIS_CLI_ERR;
    }

    UrbisIndex *idx = NULL;
    if (load_index(index_path, &idx) != URBIS_CLI_OK) return URBIS_CLI_ERR;

    UrbisObjectList *result = NULL;
    UrbisStatus status = urbis_index_query_knn(idx, x, y, k, &result);
    if (status != URBIS_OK) {
        fprintf(stderr, "Query failed: %s\n", urbis_status_str(status));
        urbis_index_destroy(idx);
        return URBIS_CLI_ERR;
    }

    if (as_json) {
        print_objects_json(result);
    } else {
        printf("Found %zu objects\n", result ? result->count : 0UL);
    }

    urbis_object_list_free(result);
    urbis_index_destroy(idx);
    return URBIS_CLI_OK;
}

static int cmd_pages(int argc, char **argv) {
    const char *index_path = NULL;
    const char *mbr_text = NULL;
    int as_json = 0;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--index") == 0 && i + 1 < argc) {
            index_path = argv[++i];
        } else if (strcmp(argv[i], "--mbr") == 0 && i + 1 < argc) {
            mbr_text = argv[++i];
        } else if (strcmp(argv[i], "--json") == 0) {
            as_json = 1;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return URBIS_CLI_ERR;
        }
    }

    if (!index_path || !mbr_text) {
        fprintf(stderr, "pages requires --index and --mbr\n");
        return URBIS_CLI_ERR;
    }

    UrbisMBR region;
    if (!parse_mbr(mbr_text, &region)) {
        fprintf(stderr, "Invalid mbr format\n");
        return URBIS_CLI_ERR;
    }

    UrbisIndex *idx = NULL;
    if (load_index(index_path, &idx) != URBIS_CLI_OK) return URBIS_CLI_ERR;

    UrbisPageList *pages = NULL;
    UrbisStatus status = urbis_index_find_adjacent_pages(idx, &region, &pages);
    if (status != URBIS_OK) {
        fprintf(stderr, "Adjacent page lookup failed: %s\n", urbis_status_str(status));
        urbis_index_destroy(idx);
        return URBIS_CLI_ERR;
    }

    if (as_json) {
        printf("{\"count\":%zu,\"estimated_seeks\":%zu,\"pages\":[",
               pages ? pages->count : 0UL,
               pages ? pages->estimated_seeks : 0UL);
        if (pages && pages->count > 0) {
            for (size_t i = 0; i < pages->count; i++) {
                printf("{\"page_id\":%u,\"track_id\":%u}",
                       pages->page_ids[i], pages->track_ids[i]);
                if (i + 1 < pages->count) {
                    printf(",");
                }
            }
        }
        printf("]}\n");
    } else {
        printf("Pages: %zu, Estimated seeks: %zu\n",
               pages ? pages->count : 0UL,
               pages ? pages->estimated_seeks : 0UL);
    }

    urbis_page_list_free(pages);
    urbis_index_destroy(idx);
    return URBIS_CLI_OK;
}

static int cmd_stats(int argc, char **argv) {
    const char *index_path = NULL;
    int as_json = 0;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--index") == 0 && i + 1 < argc) {
            index_path = argv[++i];
        } else if (strcmp(argv[i], "--json") == 0) {
            as_json = 1;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return URBIS_CLI_ERR;
        }
    }

    if (!index_path) {
        fprintf(stderr, "stats requires --index\n");
        return URBIS_CLI_ERR;
    }

    UrbisIndex *idx = NULL;
    if (load_index(index_path, &idx) != URBIS_CLI_OK) return URBIS_CLI_ERR;

    UrbisStats stats;
    UrbisStatus status = urbis_index_get_stats(idx, &stats);
    if (status != URBIS_OK) {
        fprintf(stderr, "Stats failed: %s\n", urbis_status_str(status));
        urbis_index_destroy(idx);
        return URBIS_CLI_ERR;
    }

    if (as_json) {
        printf("{\"objects\":%zu,\"blocks\":%zu,\"pages\":%zu,\"tracks\":%zu,",
               stats.total_objects, stats.total_blocks, stats.total_pages, stats.total_tracks);
        printf("\"avg_objects_per_page\":%.9g,\"page_utilization\":%.9g,",
               stats.avg_objects_per_page, stats.page_utilization);
        printf("\"kdtree_depth\":%zu,\"quadtree_depth\":%zu,",
               stats.kdtree_depth, stats.quadtree_depth);
        printf("\"bounds\":[%.9g,%.9g,%.9g,%.9g]}\n",
               stats.bounds.min_x, stats.bounds.min_y, stats.bounds.max_x, stats.bounds.max_y);
    } else {
        printf("Objects: %zu\n", stats.total_objects);
        printf("Blocks: %zu\n", stats.total_blocks);
        printf("Pages: %zu\n", stats.total_pages);
        printf("Tracks: %zu\n", stats.total_tracks);
        printf("KD-tree depth: %zu\n", stats.kdtree_depth);
        printf("Quadtree depth: %zu\n", stats.quadtree_depth);
        printf("Avg objects/page: %.2f\n", stats.avg_objects_per_page);
        printf("Page utilization: %.1f%%\n", stats.page_utilization * 100.0);
        printf("Bounds: [%.6f, %.6f, %.6f, %.6f]\n",
               stats.bounds.min_x, stats.bounds.min_y, stats.bounds.max_x, stats.bounds.max_y);
    }

    urbis_index_destroy(idx);
    return URBIS_CLI_OK;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(stderr);
        return URBIS_CLI_ERR;
    }

    const char *command = argv[1];

    if (strcmp(command, "-h") == 0 || strcmp(command, "--help") == 0) {
        print_usage(stdout);
        return URBIS_CLI_OK;
    }

    if (strcmp(command, "version") == 0) {
        printf("%s\n", urbis_version());
        return URBIS_CLI_OK;
    }

    if (strcmp(command, "build") == 0) {
        return cmd_build(argc - 2, argv + 2);
    }
    if (strcmp(command, "query-range") == 0) {
        return cmd_query_range(argc - 2, argv + 2);
    }
    if (strcmp(command, "query-knn") == 0) {
        return cmd_query_knn(argc - 2, argv + 2);
    }
    if (strcmp(command, "pages") == 0) {
        return cmd_pages(argc - 2, argv + 2);
    }
    if (strcmp(command, "stats") == 0) {
        return cmd_stats(argc - 2, argv + 2);
    }

    fprintf(stderr, "Unknown command: %s\n", command);
    print_usage(stderr);
    return URBIS_CLI_ERR;
}