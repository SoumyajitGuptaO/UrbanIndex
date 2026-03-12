/**
 * @file urbis.c
 * @brief Public API implementation
 */

#include "urbis.h"
#include "spatial_index.h"
#include "parser.h"
#include "disk_manager.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static UrbisStatus urbis_from_si(int err) {
    switch (err) {
        case SI_OK: return URBIS_OK;
        case SI_ERR_NULL_PTR: return URBIS_ERR_NULL;
        case SI_ERR_ALLOC: return URBIS_ERR_ALLOC;
        case SI_ERR_NOT_BUILT: return URBIS_ERR_NOT_BUILT;
        case SI_ERR_NOT_FOUND: return URBIS_ERR_NOT_FOUND;
        case SI_ERR_FULL: return URBIS_ERR_FULL;
        case SI_ERR_IO: return URBIS_ERR_IO;
        case SI_ERR_INVALID: return URBIS_ERR_INVALID;
        default: return URBIS_ERR_INVALID;
    }
}

static UrbisStatus urbis_from_geom(int err) {
    switch (err) {
        case GEOM_OK: return URBIS_OK;
        case GEOM_ERR_NULL_PTR: return URBIS_ERR_NULL;
        case GEOM_ERR_ALLOC: return URBIS_ERR_ALLOC;
        case GEOM_ERR_INVALID_GEOM: return URBIS_ERR_INVALID;
        case GEOM_ERR_EMPTY_GEOM: return URBIS_ERR_INVALID;
        default: return URBIS_ERR_INVALID;
    }
}

static UrbisStatus urbis_copy_query_result(const SpatialQueryResult *result,
                                           UrbisObjectList **out_list) {
    if (!out_list) return URBIS_ERR_NULL;

    UrbisObjectList *list = (UrbisObjectList *)calloc(1, sizeof(UrbisObjectList));
    if (!list) return URBIS_ERR_ALLOC;

    if (!result || result->count == 0) {
        *out_list = list;
        return URBIS_OK;
    }

    list->objects = (UrbisObject *)calloc(result->count, sizeof(UrbisObject));
    if (!list->objects) {
        free(list);
        return URBIS_ERR_ALLOC;
    }

    for (size_t i = 0; i < result->count; i++) {
        int err = spatial_object_copy(&list->objects[i], result->objects[i]);
        if (err != GEOM_OK) {
            for (size_t j = 0; j < i; j++) {
                spatial_object_free(&list->objects[j]);
            }
            free(list->objects);
            free(list);
            return urbis_from_geom(err);
        }
    }

    list->count = result->count;
    *out_list = list;
    return URBIS_OK;
}

/* ============================================================================
 * Configuration and Lifecycle
 * ============================================================================ */

UrbisConfig urbis_config_default(void) {
    UrbisConfig config = {
        .block_size = SI_DEFAULT_BLOCK_SIZE,
        .page_capacity = SI_DEFAULT_PAGE_CAPACITY,
        .cache_size = DM_DEFAULT_CACHE_SIZE,
        .enable_quadtree = true,
        .persist = false,
        .data_path = NULL
    };
    return config;
}

UrbisStatus urbis_index_create(const UrbisConfig *config, UrbisIndex **out_index) {
    if (!out_index) return URBIS_ERR_NULL;

    SpatialIndexConfig si_config = spatial_index_default_config();
    char *data_path = NULL;

    if (config) {
        si_config.block_size = config->block_size;
        si_config.page_capacity = config->page_capacity;
        si_config.cache_size = config->cache_size;
        si_config.build_quadtree = config->enable_quadtree;
        si_config.persist = config->persist;
        if (config->data_path) {
            data_path = strdup(config->data_path);
            if (!data_path) return URBIS_ERR_ALLOC;
            si_config.data_path = data_path;
        }
    }

    UrbisIndex *idx = spatial_index_create(&si_config);
    if (!idx) {
        free(data_path);
        return URBIS_ERR_ALLOC;
    }

    *out_index = idx;
    return URBIS_OK;
}

void urbis_index_destroy(UrbisIndex *idx) {
    spatial_index_destroy(idx);
}

const char* urbis_version(void) {
    return URBIS_VERSION_STRING;
}

const char* urbis_status_str(UrbisStatus status) {
    switch (status) {
        case URBIS_OK: return "OK";
        case URBIS_ERR_NULL: return "Null pointer";
        case URBIS_ERR_ALLOC: return "Allocation failed";
        case URBIS_ERR_IO: return "I/O error";
        case URBIS_ERR_PARSE: return "Parse error";
        case URBIS_ERR_NOT_FOUND: return "Not found";
        case URBIS_ERR_FULL: return "Index full";
        case URBIS_ERR_INVALID: return "Invalid argument";
        case URBIS_ERR_NOT_BUILT: return "Index not built";
        default: return "Unknown error";
    }
}

/* ============================================================================
 * Data Loading
 * ============================================================================ */

UrbisStatus urbis_index_load_geojson(UrbisIndex *idx, const char *path, size_t *out_loaded) {
    if (!idx || !path) return URBIS_ERR_NULL;

    FeatureCollection fc;
    int err = geojson_parse_file(path, &fc);
    if (err != PARSE_OK) return URBIS_ERR_PARSE;

    for (size_t i = 0; i < fc.count; i++) {
        err = spatial_index_insert(idx, &fc.features[i].object);
        if (err != SI_OK) {
            feature_collection_free(&fc);
            return urbis_from_si(err);
        }
    }

    if (out_loaded) {
        *out_loaded = fc.count;
    }

    feature_collection_free(&fc);
    return URBIS_OK;
}

UrbisStatus urbis_index_load_geojson_string(UrbisIndex *idx, const char *json, size_t *out_loaded) {
    if (!idx || !json) return URBIS_ERR_NULL;

    FeatureCollection fc;
    int err = geojson_parse_string(json, &fc);
    if (err != PARSE_OK) return URBIS_ERR_PARSE;

    for (size_t i = 0; i < fc.count; i++) {
        err = spatial_index_insert(idx, &fc.features[i].object);
        if (err != SI_OK) {
            feature_collection_free(&fc);
            return urbis_from_si(err);
        }
    }

    if (out_loaded) {
        *out_loaded = fc.count;
    }

    feature_collection_free(&fc);
    return URBIS_OK;
}

UrbisStatus urbis_index_load_wkt(UrbisIndex *idx, const char *wkt, uint64_t *out_id) {
    if (!idx || !wkt) return URBIS_ERR_NULL;

    SpatialObject obj;
    int err = wkt_parse(wkt, &obj);
    if (err != PARSE_OK) return URBIS_ERR_PARSE;

    err = spatial_index_insert(idx, &obj);
    uint64_t id = obj.id;
    spatial_object_free(&obj);

    if (err != SI_OK) return urbis_from_si(err);

    if (out_id) {
        *out_id = id;
    }

    return URBIS_OK;
}

/* ============================================================================
 * Object Operations
 * ============================================================================ */

UrbisStatus urbis_index_insert_object(UrbisIndex *idx, const UrbisObject *obj, uint64_t *out_id) {
    if (!idx || !obj) return URBIS_ERR_NULL;

    SpatialObject copy;
    int err = spatial_object_copy(&copy, obj);
    if (err != GEOM_OK) return urbis_from_geom(err);

    err = spatial_index_insert(idx, &copy);
    uint64_t id = copy.id;
    spatial_object_free(&copy);

    if (err != SI_OK) return urbis_from_si(err);

    if (out_id) {
        *out_id = id;
    }

    return URBIS_OK;
}

UrbisStatus urbis_index_insert_point(UrbisIndex *idx, double x, double y, uint64_t *out_id) {
    if (!idx) return URBIS_ERR_NULL;

    SpatialObject obj;
    int err = spatial_object_init_point(&obj, 0, point_create(x, y));
    if (err != GEOM_OK) return urbis_from_geom(err);

    err = spatial_index_insert(idx, &obj);
    uint64_t id = obj.id;
    spatial_object_free(&obj);

    if (err != SI_OK) return urbis_from_si(err);

    if (out_id) {
        *out_id = id;
    }

    return URBIS_OK;
}

UrbisStatus urbis_index_insert_linestring(UrbisIndex *idx, const UrbisPoint *points, size_t count, uint64_t *out_id) {
    if (!idx || !points || count == 0) return URBIS_ERR_NULL;

    SpatialObject obj;
    int err = spatial_object_init_linestring(&obj, 0, count);
    if (err != GEOM_OK) return urbis_from_geom(err);

    for (size_t i = 0; i < count; i++) {
        linestring_add_point(&obj.geom.line, points[i]);
    }
    spatial_object_update_derived(&obj);

    err = spatial_index_insert(idx, &obj);
    uint64_t id = obj.id;
    spatial_object_free(&obj);

    if (err != SI_OK) return urbis_from_si(err);

    if (out_id) {
        *out_id = id;
    }

    return URBIS_OK;
}

UrbisStatus urbis_index_insert_polygon(UrbisIndex *idx, const UrbisPoint *exterior, size_t count, uint64_t *out_id) {
    if (!idx || !exterior || count < 3) return URBIS_ERR_NULL;

    SpatialObject obj;
    int err = spatial_object_init_polygon(&obj, 0, count);
    if (err != GEOM_OK) return urbis_from_geom(err);

    for (size_t i = 0; i < count; i++) {
        polygon_add_exterior_point(&obj.geom.polygon, exterior[i]);
    }
    spatial_object_update_derived(&obj);

    err = spatial_index_insert(idx, &obj);
    uint64_t id = obj.id;
    spatial_object_free(&obj);

    if (err != SI_OK) return urbis_from_si(err);

    if (out_id) {
        *out_id = id;
    }

    return URBIS_OK;
}

UrbisStatus urbis_index_remove(UrbisIndex *idx, uint64_t object_id) {
    if (!idx) return URBIS_ERR_NULL;

    int err = spatial_index_remove(idx, object_id);
    return urbis_from_si(err);
}

UrbisStatus urbis_index_get(UrbisIndex *idx, uint64_t object_id, UrbisObject *out_object) {
    if (!idx || !out_object) return URBIS_ERR_NULL;

    SpatialObject *obj = spatial_index_get(idx, object_id);
    if (!obj) return URBIS_ERR_NOT_FOUND;

    int err = spatial_object_copy(out_object, obj);
    return urbis_from_geom(err);
}

/* ============================================================================
 * Index Building
 * ============================================================================ */

UrbisStatus urbis_index_build(UrbisIndex *idx) {
    if (!idx) return URBIS_ERR_NULL;
    return urbis_from_si(spatial_index_build(idx));
}

UrbisStatus urbis_index_optimize(UrbisIndex *idx) {
    if (!idx) return URBIS_ERR_NULL;
    return urbis_from_si(spatial_index_optimize(idx));
}

/* ============================================================================
 * Spatial Queries
 * ============================================================================ */

UrbisStatus urbis_index_query_range(UrbisIndex *idx, const UrbisMBR *range, UrbisObjectList **out_list) {
    if (!idx || !range || !out_list) return URBIS_ERR_NULL;

    SpatialQueryResult result;
    if (spatial_result_init(&result, 64) != SI_OK) return URBIS_ERR_ALLOC;

    int err = spatial_index_query_range(idx, range, &result);
    if (err != SI_OK) {
        spatial_result_free(&result);
        return urbis_from_si(err);
    }

    UrbisStatus status = urbis_copy_query_result(&result, out_list);
    spatial_result_free(&result);
    return status;
}

UrbisStatus urbis_index_query_point(UrbisIndex *idx, double x, double y, UrbisObjectList **out_list) {
    if (!idx || !out_list) return URBIS_ERR_NULL;

    SpatialQueryResult result;
    if (spatial_result_init(&result, 16) != SI_OK) return URBIS_ERR_ALLOC;

    Point p = point_create(x, y);
    int err = spatial_index_query_point(idx, p, &result);
    if (err != SI_OK) {
        spatial_result_free(&result);
        return urbis_from_si(err);
    }

    UrbisStatus status = urbis_copy_query_result(&result, out_list);
    spatial_result_free(&result);
    return status;
}

UrbisStatus urbis_index_query_knn(UrbisIndex *idx, double x, double y, size_t k, UrbisObjectList **out_list) {
    if (!idx || !out_list) return URBIS_ERR_NULL;
    if (k == 0) return URBIS_ERR_INVALID;

    SpatialQueryResult result;
    if (spatial_result_init(&result, k) != SI_OK) return URBIS_ERR_ALLOC;

    Point p = point_create(x, y);
    int err = spatial_index_query_knn(idx, p, k, &result);
    if (err != SI_OK) {
        spatial_result_free(&result);
        return urbis_from_si(err);
    }

    UrbisStatus status = urbis_copy_query_result(&result, out_list);
    spatial_result_free(&result);
    return status;
}

UrbisStatus urbis_index_find_adjacent_pages(UrbisIndex *idx, const UrbisMBR *region, UrbisPageList **out_pages) {
    if (!idx || !region || !out_pages) return URBIS_ERR_NULL;

    AdjacentPagesResult result;
    if (adjacent_result_init(&result, 64) != SI_OK) return URBIS_ERR_ALLOC;

    int err = spatial_index_find_adjacent_pages(idx, region, &result);
    if (err != SI_OK) {
        adjacent_result_free(&result);
        return urbis_from_si(err);
    }

    UrbisPageList *list = (UrbisPageList *)calloc(1, sizeof(UrbisPageList));
    if (!list) {
        adjacent_result_free(&result);
        return URBIS_ERR_ALLOC;
    }

    list->count = result.count;
    if (result.count > 0) {
        list->page_ids = (uint32_t *)malloc(result.count * sizeof(uint32_t));
        list->track_ids = (uint32_t *)malloc(result.count * sizeof(uint32_t));

        if (!list->page_ids || !list->track_ids) {
            free(list->page_ids);
            free(list->track_ids);
            free(list);
            adjacent_result_free(&result);
            return URBIS_ERR_ALLOC;
        }

        for (size_t i = 0; i < result.count; i++) {
            list->page_ids[i] = result.pages[i]->header.page_id;
            list->track_ids[i] = result.track_ids[i];
        }
    }

    /* Estimate seeks (track transitions) */
    list->estimated_seeks = 0;
    uint32_t last_track = 0;
    for (size_t i = 0; i < list->count; i++) {
        if (list->track_ids[i] != last_track && last_track != 0) {
            list->estimated_seeks++;
        }
        last_track = list->track_ids[i];
    }

    adjacent_result_free(&result);

    *out_pages = list;
    return URBIS_OK;
}

UrbisStatus urbis_index_query_adjacent(UrbisIndex *idx, const UrbisMBR *region, UrbisObjectList **out_list) {
    if (!idx || !region || !out_list) return URBIS_ERR_NULL;

    AdjacentPagesResult pages;
    if (adjacent_result_init(&pages, 64) != SI_OK) return URBIS_ERR_ALLOC;

    int err = spatial_index_find_adjacent_pages(idx, region, &pages);
    if (err != SI_OK) {
        adjacent_result_free(&pages);
        return urbis_from_si(err);
    }

    UrbisObjectList *list = (UrbisObjectList *)calloc(1, sizeof(UrbisObjectList));
    if (!list) {
        adjacent_result_free(&pages);
        return URBIS_ERR_ALLOC;
    }

    size_t total = 0;
    for (size_t i = 0; i < pages.count; i++) {
        Page *page = pages.pages[i];
        if (page) {
            total += page->header.object_count;
        }
    }

    if (total == 0) {
        adjacent_result_free(&pages);
        *out_list = list;
        return URBIS_OK;
    }

    list->objects = (UrbisObject *)calloc(total, sizeof(UrbisObject));
    if (!list->objects) {
        free(list);
        adjacent_result_free(&pages);
        return URBIS_ERR_ALLOC;
    }

    size_t count = 0;
    for (size_t i = 0; i < pages.count; i++) {
        Page *page = pages.pages[i];
        if (!page) continue;
        for (size_t j = 0; j < page->header.object_count; j++) {
            SpatialObject *obj = &page->objects[j];
            if (mbr_intersects(&obj->mbr, region)) {
                int copy_err = spatial_object_copy(&list->objects[count], obj);
                if (copy_err != GEOM_OK) {
                    for (size_t k = 0; k < count; k++) {
                        spatial_object_free(&list->objects[k]);
                    }
                    free(list->objects);
                    free(list);
                    adjacent_result_free(&pages);
                    return urbis_from_geom(copy_err);
                }
                count++;
            }
        }
    }

    list->count = count;
    adjacent_result_free(&pages);

    *out_list = list;
    return URBIS_OK;
}

/* ============================================================================
 * Persistence
 * ============================================================================ */

UrbisStatus urbis_index_save(UrbisIndex *idx, const char *path) {
    if (!idx || !path) return URBIS_ERR_NULL;
    return urbis_from_si(spatial_index_save(idx, path));
}

UrbisStatus urbis_index_load(const char *path, UrbisIndex **out_index) {
    if (!path || !out_index) return URBIS_ERR_NULL;

    UrbisIndex *idx = NULL;
    UrbisStatus status = urbis_index_create(NULL, &idx);
    if (status != URBIS_OK) return status;

    int err = spatial_index_load(idx, path);
    if (err != SI_OK) {
        urbis_index_destroy(idx);
        return urbis_from_si(err);
    }

    *out_index = idx;
    return URBIS_OK;
}

UrbisStatus urbis_index_sync(UrbisIndex *idx) {
    if (!idx) return URBIS_ERR_NULL;

    int err = disk_manager_sync(&idx->disk);
    return (err == DM_OK) ? URBIS_OK : URBIS_ERR_IO;
}

/* ============================================================================
 * Statistics and Utilities
 * ============================================================================ */

UrbisStatus urbis_index_get_stats(const UrbisIndex *idx, UrbisStats *stats) {
    if (!idx || !stats) return URBIS_ERR_NULL;

    SpatialIndexStats si_stats;
    spatial_index_stats(idx, &si_stats);

    stats->total_objects = si_stats.total_objects;
    stats->total_blocks = si_stats.total_blocks;
    stats->total_pages = si_stats.total_pages;
    stats->total_tracks = si_stats.total_tracks;
    stats->avg_objects_per_page = si_stats.avg_objects_per_page;
    stats->page_utilization = si_stats.page_utilization;
    stats->kdtree_depth = si_stats.kdtree_depth;
    stats->quadtree_depth = si_stats.quadtree_depth;
    stats->bounds = si_stats.bounds;

    return URBIS_OK;
}

UrbisStatus urbis_index_count(const UrbisIndex *idx, size_t *out_count) {
    if (!idx || !out_count) return URBIS_ERR_NULL;

    UrbisStats stats;
    UrbisStatus status = urbis_index_get_stats(idx, &stats);
    if (status != URBIS_OK) return status;

    *out_count = stats.total_objects;
    return URBIS_OK;
}

UrbisStatus urbis_index_bounds(const UrbisIndex *idx, UrbisMBR *out_bounds) {
    if (!idx || !out_bounds) return URBIS_ERR_NULL;

    *out_bounds = idx->bounds;
    return URBIS_OK;
}

UrbisStatus urbis_index_print_stats(const UrbisIndex *idx, FILE *out) {
    if (!idx || !out) return URBIS_ERR_NULL;

    fprintf(out, "=== Urbis Spatial Index ===\n");
    fprintf(out, "Version: %s\n\n", urbis_version());

    spatial_index_print_stats(idx, out);
    return URBIS_OK;
}

UrbisStatus urbis_index_estimate_seeks(const UrbisIndex *idx,
                                      const UrbisMBR *regions, size_t count,
                                      size_t *out_seeks) {
    if (!idx || !regions || count == 0 || !out_seeks) return URBIS_ERR_NULL;

    size_t total_seeks = 0;

    for (size_t i = 0; i < count; i++) {
        UrbisPageList *pages = NULL;
        UrbisStatus status = urbis_index_find_adjacent_pages((UrbisIndex *)idx, &regions[i], &pages);
        if (status == URBIS_OK && pages) {
            total_seeks += pages->estimated_seeks;
            urbis_page_list_free(pages);
        }
    }

    *out_seeks = total_seeks;
    return URBIS_OK;
}

/* ============================================================================
 * Result List Operations
 * ============================================================================ */

void urbis_object_free(UrbisObject *obj) {
    spatial_object_free(obj);
}

void urbis_object_list_free(UrbisObjectList *list) {
    if (!list) return;

    for (size_t i = 0; i < list->count; i++) {
        spatial_object_free(&list->objects[i]);
    }
    free(list->objects);
    free(list);
}

void urbis_page_list_free(UrbisPageList *list) {
    if (!list) return;
    free(list->page_ids);
    free(list->track_ids);
    free(list);
}