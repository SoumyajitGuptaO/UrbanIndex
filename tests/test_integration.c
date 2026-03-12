/**
 * @file test_integration.c
 * @brief Integration tests for the Urbis spatial index
 */

#include "../include/urbis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define EPSILON 1e-6
#define ASSERT_NEAR(a, b) assert(fabs((a) - (b)) < EPSILON)
#define ASSERT_OK(expr) assert((expr) == URBIS_OK)

#define TEST(name) \
    static void test_##name(void); \
    static void run_test_##name(void) { \
        printf("  Testing " #name "... "); \
        test_##name(); \
        printf("PASSED\n"); \
        tests_passed++; \
    } \
    static void test_##name(void)

#define RUN_TEST(name) run_test_##name()

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST(basic_workflow) {
    UrbisIndex *idx = NULL;
    ASSERT_OK(urbis_index_create(NULL, &idx));
    assert(idx != NULL);

    uint64_t id1 = 0, id2 = 0, id3 = 0;
    ASSERT_OK(urbis_index_insert_point(idx, 10, 20, &id1));
    ASSERT_OK(urbis_index_insert_point(idx, 30, 40, &id2));
    ASSERT_OK(urbis_index_insert_point(idx, 50, 60, &id3));

    assert(id1 > 0 && id2 > 0 && id3 > 0);

    size_t count = 0;
    ASSERT_OK(urbis_index_count(idx, &count));
    assert(count == 3);

    ASSERT_OK(urbis_index_build(idx));

    UrbisMBR range = urbis_mbr(0, 0, 35, 45);
    UrbisObjectList *result = NULL;
    ASSERT_OK(urbis_index_query_range(idx, &range, &result));
    assert(result != NULL);
    assert(result->count == 2);

    urbis_object_list_free(result);
    urbis_index_destroy(idx);
}

TEST(linestring_workflow) {
    UrbisIndex *idx = NULL;
    ASSERT_OK(urbis_index_create(NULL, &idx));

    UrbisPoint road[] = {
        urbis_point(0, 0),
        urbis_point(100, 0),
        urbis_point(100, 100),
        urbis_point(0, 100)
    };

    uint64_t id = 0;
    ASSERT_OK(urbis_index_insert_linestring(idx, road, 4, &id));
    assert(id > 0);

    ASSERT_OK(urbis_index_build(idx));

    UrbisMBR range = urbis_mbr(40, -10, 60, 10);
    UrbisObjectList *result = NULL;
    ASSERT_OK(urbis_index_query_range(idx, &range, &result));
    assert(result != NULL);
    assert(result->count == 1);

    urbis_object_list_free(result);
    urbis_index_destroy(idx);
}

TEST(polygon_workflow) {
    UrbisIndex *idx = NULL;
    ASSERT_OK(urbis_index_create(NULL, &idx));

    UrbisPoint building[] = {
        urbis_point(10, 10),
        urbis_point(30, 10),
        urbis_point(30, 30),
        urbis_point(10, 30),
        urbis_point(10, 10)
    };

    uint64_t id = 0;
    ASSERT_OK(urbis_index_insert_polygon(idx, building, 5, &id));
    assert(id > 0);

    UrbisObject obj;
    ASSERT_OK(urbis_index_get(idx, id, &obj));
    assert(obj.type == GEOM_POLYGON);

    ASSERT_NEAR(obj.centroid.x, 20);
    ASSERT_NEAR(obj.centroid.y, 20);

    urbis_object_free(&obj);
    urbis_index_destroy(idx);
}

TEST(geojson_loading) {
    UrbisIndex *idx = NULL;
    ASSERT_OK(urbis_index_create(NULL, &idx));

    const char *geojson = "{"
        "\"type\": \"FeatureCollection\"," 
        "\"features\": ["
        "  {\"type\": \"Feature\", \"geometry\": {\"type\": \"Point\", \"coordinates\": [10, 20]}},"
        "  {\"type\": \"Feature\", \"geometry\": {\"type\": \"Point\", \"coordinates\": [30, 40]}},"
        "  {\"type\": \"Feature\", \"geometry\": {\"type\": \"LineString\", \"coordinates\": [[0,0],[50,50]]}}"
        "]"
    "}";

    size_t loaded = 0;
    ASSERT_OK(urbis_index_load_geojson_string(idx, geojson, &loaded));
    assert(loaded == 3);

    size_t count = 0;
    ASSERT_OK(urbis_index_count(idx, &count));
    assert(count == 3);

    urbis_index_destroy(idx);
}

TEST(adjacent_pages) {
    UrbisConfig config = urbis_config_default();
    config.page_capacity = 4;

    UrbisIndex *idx = NULL;
    ASSERT_OK(urbis_index_create(&config, &idx));

    for (int i = 0; i < 50; i++) {
        uint64_t id = 0;
        ASSERT_OK(urbis_index_insert_point(idx, (i % 10) * 100, (i / 10) * 100, &id));
    }

    ASSERT_OK(urbis_index_build(idx));

    UrbisMBR region = urbis_mbr(150, 150, 350, 350);
    UrbisPageList *pages = NULL;
    ASSERT_OK(urbis_index_find_adjacent_pages(idx, &region, &pages));

    if (pages) {
        printf("(found %zu pages, ~%zu seeks) ", pages->count, pages->estimated_seeks);
        urbis_page_list_free(pages);
    }

    urbis_index_destroy(idx);
}

TEST(knn_query) {
    UrbisIndex *idx = NULL;
    ASSERT_OK(urbis_index_create(NULL, &idx));

    uint64_t id = 0;
    ASSERT_OK(urbis_index_insert_point(idx, 0, 0, &id));
    ASSERT_OK(urbis_index_insert_point(idx, 1, 1, &id));
    ASSERT_OK(urbis_index_insert_point(idx, 2, 2, &id));
    ASSERT_OK(urbis_index_insert_point(idx, 10, 10, &id));
    ASSERT_OK(urbis_index_insert_point(idx, 20, 20, &id));

    ASSERT_OK(urbis_index_build(idx));

    UrbisObjectList *result = NULL;
    ASSERT_OK(urbis_index_query_knn(idx, 0.5, 0.5, 3, &result));
    assert(result != NULL);
    assert(result->count == 3);

    urbis_object_list_free(result);
    urbis_index_destroy(idx);
}

TEST(query_adjacent) {
    UrbisIndex *idx = NULL;
    ASSERT_OK(urbis_index_create(NULL, &idx));

    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            uint64_t id = 0;
            ASSERT_OK(urbis_index_insert_point(idx, i * 10, j * 10, &id));
        }
    }

    ASSERT_OK(urbis_index_build(idx));

    UrbisMBR region = urbis_mbr(25, 25, 45, 45);
    UrbisObjectList *result = NULL;
    ASSERT_OK(urbis_index_query_adjacent(idx, &region, &result));

    assert(result != NULL);
    assert(result->count > 0);

    urbis_object_list_free(result);
    urbis_index_destroy(idx);
}

TEST(remove_object) {
    UrbisIndex *idx = NULL;
    ASSERT_OK(urbis_index_create(NULL, &idx));

    uint64_t id1 = 0, id2 = 0, id3 = 0;
    ASSERT_OK(urbis_index_insert_point(idx, 10, 10, &id1));
    ASSERT_OK(urbis_index_insert_point(idx, 20, 20, &id2));
    ASSERT_OK(urbis_index_insert_point(idx, 30, 30, &id3));

    size_t count = 0;
    ASSERT_OK(urbis_index_count(idx, &count));
    assert(count == 3);

    ASSERT_OK(urbis_index_remove(idx, id2));
    ASSERT_OK(urbis_index_count(idx, &count));
    assert(count == 2);

    UrbisObject obj;
    assert(urbis_index_get(idx, id2, &obj) == URBIS_ERR_NOT_FOUND);

    ASSERT_OK(urbis_index_get(idx, id1, &obj));
    urbis_object_free(&obj);
    ASSERT_OK(urbis_index_get(idx, id3, &obj));
    urbis_object_free(&obj);

    urbis_index_destroy(idx);
}

TEST(bounds) {
    UrbisIndex *idx = NULL;
    ASSERT_OK(urbis_index_create(NULL, &idx));

    uint64_t id = 0;
    ASSERT_OK(urbis_index_insert_point(idx, -100, -50, &id));
    ASSERT_OK(urbis_index_insert_point(idx, 200, 150, &id));

    UrbisMBR bounds;
    ASSERT_OK(urbis_index_bounds(idx, &bounds));

    ASSERT_NEAR(bounds.min_x, -100);
    ASSERT_NEAR(bounds.min_y, -50);
    ASSERT_NEAR(bounds.max_x, 200);
    ASSERT_NEAR(bounds.max_y, 150);

    urbis_index_destroy(idx);
}

TEST(stats) {
    UrbisIndex *idx = NULL;
    ASSERT_OK(urbis_index_create(NULL, &idx));

    uint64_t id = 0;
    for (int i = 0; i < 100; i++) {
        ASSERT_OK(urbis_index_insert_point(idx, i * 10, i * 5, &id));
    }

    ASSERT_OK(urbis_index_build(idx));

    UrbisStats stats;
    ASSERT_OK(urbis_index_get_stats(idx, &stats));

    assert(stats.total_objects == 100);
    assert(stats.total_pages > 0);

    printf("\n");
    ASSERT_OK(urbis_index_print_stats(idx, stdout));

    urbis_index_destroy(idx);
}

TEST(wkt_loading) {
    UrbisIndex *idx = NULL;
    ASSERT_OK(urbis_index_create(NULL, &idx));

    uint64_t id = 0;
    ASSERT_OK(urbis_index_load_wkt(idx, "POINT (10 20)", &id));
    ASSERT_OK(urbis_index_load_wkt(idx, "LINESTRING (0 0, 10 10, 20 0)", &id));
    ASSERT_OK(urbis_index_load_wkt(idx, "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0))", &id));

    size_t count = 0;
    ASSERT_OK(urbis_index_count(idx, &count));
    assert(count == 3);

    urbis_index_destroy(idx);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
    printf("Running integration tests...\n\n");

    printf("Version: %s\n\n", urbis_version());

    RUN_TEST(basic_workflow);
    RUN_TEST(linestring_workflow);
    RUN_TEST(polygon_workflow);
    RUN_TEST(geojson_loading);
    RUN_TEST(adjacent_pages);
    RUN_TEST(knn_query);
    RUN_TEST(query_adjacent);
    RUN_TEST(remove_object);
    RUN_TEST(bounds);
    RUN_TEST(stats);
    RUN_TEST(wkt_loading);

    printf("\n=================================\n");
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}