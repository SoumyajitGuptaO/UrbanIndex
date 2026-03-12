/**
 * @file real_map_demo.c
 * @brief Example demonstrating Urbis spatial index with real GeoJSON data
 */

#include "../include/urbis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

static double get_time_ms(void) {
    return (double)clock() / CLOCKS_PER_SEC * 1000.0;
}

static void format_number(size_t n, char *buf, size_t buf_size) {
    if (n < 1000) {
        snprintf(buf, buf_size, "%zu", n);
    } else if (n < 1000000) {
        snprintf(buf, buf_size, "%.1fk", n / 1000.0);
    } else {
        snprintf(buf, buf_size, "%.1fM", n / 1000000.0);
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char *argv[]) {
    const char *geojson_path = "examples/data/city.geojson";

    if (argc > 1) {
        geojson_path = argv[1];
    }

    printf("========================================\n");
    printf("Urbis Real Map Demo\n");
    printf("Version: %s\n", urbis_version());
    printf("========================================\n\n");

    printf("Loading GeoJSON: %s\n\n", geojson_path);

    UrbisConfig config = urbis_config_default();
    config.block_size = 1024;
    config.page_capacity = 64;
    config.enable_quadtree = true;

    UrbisIndex *idx = NULL;
    UrbisStatus status = urbis_index_create(&config, &idx);
    if (status != URBIS_OK || !idx) {
        fprintf(stderr, "Failed to create index\n");
        return 1;
    }

    double load_start = get_time_ms();
    UrbisStatus err = urbis_index_load_geojson(idx, geojson_path, NULL);
    double load_end = get_time_ms();
    double load_time = load_end - load_start;

    if (err != URBIS_OK) {
        fprintf(stderr, "Failed to load GeoJSON: %s\n", geojson_path);
        urbis_index_destroy(idx);
        return 1;
    }

    size_t total = 0;
    urbis_index_count(idx, &total);

    char num_buf[32];
    format_number(total, num_buf, sizeof(num_buf));
    printf("Loaded %s features in %.2f ms\n", num_buf, load_time);
    printf("Load rate: %.0f features/sec\n\n", total / (load_time / 1000));

    printf("Building spatial index...\n");
    double build_start = get_time_ms();
    err = urbis_index_build(idx);
    double build_end = get_time_ms();

    if (err != URBIS_OK) {
        fprintf(stderr, "Failed to build index\n");
        urbis_index_destroy(idx);
        return 1;
    }

    printf("Index built in %.2f ms\n\n", build_end - build_start);

    UrbisStats stats;
    urbis_index_get_stats(idx, &stats);

    printf("=== Index Statistics ===\n");
    printf("Objects: %zu\n", stats.total_objects);
    printf("Blocks: %zu\n", stats.total_blocks);
    printf("Pages: %zu\n", stats.total_pages);
    printf("Tracks: %zu\n", stats.total_tracks);
    printf("\n");

    /* Range query demo */
    if (!mbr_is_empty(&stats.bounds)) {
        double width = stats.bounds.max_x - stats.bounds.min_x;
        double height = stats.bounds.max_y - stats.bounds.min_y;

        UrbisMBR region = urbis_mbr(
            stats.bounds.min_x + width * 0.25,
            stats.bounds.min_y + height * 0.25,
            stats.bounds.min_x + width * 0.75,
            stats.bounds.min_y + height * 0.75
        );

        printf("=== Range Query Demo ===\n");
        printf("Region: (%.4f, %.4f) to (%.4f, %.4f)\n",
               region.min_x, region.min_y, region.max_x, region.max_y);

        double query_start = get_time_ms();
        UrbisObjectList *result = NULL;
        urbis_index_query_range(idx, &region, &result);
        double query_end = get_time_ms();

        if (result) {
            printf("Found %zu objects in %.2f ms\n\n",
                   result->count, query_end - query_start);
            urbis_object_list_free(result);
        }
    }

    /* Adjacent pages demo */
    if (!mbr_is_empty(&stats.bounds)) {
        double width = stats.bounds.max_x - stats.bounds.min_x;
        double height = stats.bounds.max_y - stats.bounds.min_y;

        UrbisMBR region = urbis_mbr(
            stats.bounds.min_x + width * 0.4,
            stats.bounds.min_y + height * 0.4,
            stats.bounds.min_x + width * 0.6,
            stats.bounds.min_y + height * 0.6
        );

        printf("=== Adjacent Pages Demo ===\n");
        printf("Region: (%.4f, %.4f) to (%.4f, %.4f)\n",
               region.min_x, region.min_y, region.max_x, region.max_y);

        UrbisPageList *pages = NULL;
        urbis_index_find_adjacent_pages(idx, &region, &pages);

        if (pages) {
            printf("Pages accessed: %zu\n", pages->count);
            printf("Estimated seeks: %zu\n\n", pages->estimated_seeks);
            urbis_page_list_free(pages);
        }
    }

    /* KNN demo */
    if (!mbr_is_empty(&stats.bounds)) {
        double center_lon = (stats.bounds.min_x + stats.bounds.max_x) / 2.0;
        double center_lat = (stats.bounds.min_y + stats.bounds.max_y) / 2.0;

        printf("=== KNN Query Demo ===\n");
        printf("Center point: (%.4f, %.4f)\n", center_lon, center_lat);

        UrbisObjectList *knn_result = NULL;
        urbis_index_query_knn(idx, center_lon, center_lat, 10, &knn_result);

        if (knn_result) {
            printf("Found %zu nearest objects\n", knn_result->count);
            for (size_t i = 0; i < knn_result->count && i < 5; i++) {
                UrbisObject *obj = &knn_result->objects[i];
                UrbisPoint query_pt = urbis_point(center_lon, center_lat);
                double dist = point_distance(&query_pt, &obj->centroid);
                printf("  %zu. %s at (%.4f, %.4f), distance: %.4f\n",
                       i + 1,
                       obj->type == GEOM_POINT ? "Point" :
                       obj->type == GEOM_LINESTRING ? "Line" : "Polygon",
                       obj->centroid.x, obj->centroid.y, dist);
            }
            printf("\n");
            urbis_object_list_free(knn_result);
        }
    }

    urbis_index_destroy(idx);
    return 0;
}