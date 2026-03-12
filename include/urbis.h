/**
 * @file urbis.h
 * @brief Public API for the Urbis GIS spatial indexing library
 */

#ifndef URBIS_H
#define URBIS_H

#include "geometry.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Version Information
 * ============================================================================ */

#define URBIS_VERSION_MAJOR 1
#define URBIS_VERSION_MINOR 0
#define URBIS_VERSION_PATCH 0
#define URBIS_VERSION_STRING "1.0.0"

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Opaque index handle
 */
typedef struct SpatialIndex UrbisIndex;

/**
 * @brief Public geometry aliases
 */
typedef Point UrbisPoint;
typedef MBR UrbisMBR;
typedef SpatialObject UrbisObject;

/**
 * @brief Index configuration
 */
typedef struct {
    size_t block_size;            /**< Max objects per block (default: 1024) */
    size_t page_capacity;         /**< Max objects per page (default: 64) */
    size_t cache_size;            /**< Page cache size (default: 128) */
    bool enable_quadtree;         /**< Enable quadtree for adjacency (default: true) */
    bool persist;                 /**< Enable persistence (default: false) */
    const char *data_path;        /**< Path for data file (if persist=true) */
} UrbisConfig;

/**
 * @brief List of objects returned from queries
 */
typedef struct {
    UrbisObject *objects;
    size_t count;
} UrbisObjectList;

/**
 * @brief List of pages returned from queries
 */
typedef struct {
    uint32_t *page_ids;
    uint32_t *track_ids;
    size_t count;
    size_t estimated_seeks;
} UrbisPageList;

/**
 * @brief Index statistics
 */
typedef struct {
    size_t total_objects;
    size_t total_blocks;
    size_t total_pages;
    size_t total_tracks;
    double avg_objects_per_page;
    double page_utilization;
    size_t kdtree_depth;
    size_t quadtree_depth;
    UrbisMBR bounds;
} UrbisStats;

/**
 * @brief Status codes
 */
typedef enum {
    URBIS_OK = 0,
    URBIS_ERR_NULL = -1,
    URBIS_ERR_ALLOC = -2,
    URBIS_ERR_IO = -3,
    URBIS_ERR_PARSE = -4,
    URBIS_ERR_NOT_FOUND = -5,
    URBIS_ERR_FULL = -6,
    URBIS_ERR_INVALID = -7,
    URBIS_ERR_NOT_BUILT = -8
} UrbisStatus;

/* ============================================================================
 * Configuration and Lifecycle
 * ============================================================================ */

/**
 * @brief Get default configuration
 */
UrbisConfig urbis_config_default(void);

/**
 * @brief Create a new spatial index
 */
UrbisStatus urbis_index_create(const UrbisConfig *config, UrbisIndex **out_index);

/**
 * @brief Destroy an index
 */
void urbis_index_destroy(UrbisIndex *idx);

/**
 * @brief Get library version string
 */
const char* urbis_version(void);

/**
 * @brief Get string for a status code
 */
const char* urbis_status_str(UrbisStatus status);

/* ============================================================================
 * Data Loading
 * ============================================================================ */

/**
 * @brief Load data from a GeoJSON file
 */
UrbisStatus urbis_index_load_geojson(UrbisIndex *idx, const char *path, size_t *out_loaded);

/**
 * @brief Load data from a GeoJSON string
 */
UrbisStatus urbis_index_load_geojson_string(UrbisIndex *idx, const char *json, size_t *out_loaded);

/**
 * @brief Load data from a WKT string
 */
UrbisStatus urbis_index_load_wkt(UrbisIndex *idx, const char *wkt, uint64_t *out_id);

/* ============================================================================
 * Object Operations
 * ============================================================================ */

/**
 * @brief Insert a spatial object (copied into the index)
 */
UrbisStatus urbis_index_insert_object(UrbisIndex *idx, const UrbisObject *obj, uint64_t *out_id);

/**
 * @brief Insert a point
 */
UrbisStatus urbis_index_insert_point(UrbisIndex *idx, double x, double y, uint64_t *out_id);

/**
 * @brief Insert a linestring
 */
UrbisStatus urbis_index_insert_linestring(UrbisIndex *idx, const UrbisPoint *points, size_t count, uint64_t *out_id);

/**
 * @brief Insert a polygon (exterior ring)
 */
UrbisStatus urbis_index_insert_polygon(UrbisIndex *idx, const UrbisPoint *exterior, size_t count, uint64_t *out_id);

/**
 * @brief Remove an object by ID
 */
UrbisStatus urbis_index_remove(UrbisIndex *idx, uint64_t object_id);

/**
 * @brief Get an object by ID (deep copy into out_object)
 */
UrbisStatus urbis_index_get(UrbisIndex *idx, uint64_t object_id, UrbisObject *out_object);

/* ============================================================================
 * Index Building
 * ============================================================================ */

/**
 * @brief Build the spatial index
 */
UrbisStatus urbis_index_build(UrbisIndex *idx);

/**
 * @brief Optimize index for better query performance
 */
UrbisStatus urbis_index_optimize(UrbisIndex *idx);

/* ============================================================================
 * Spatial Queries
 * ============================================================================ */

/**
 * @brief Query objects in a bounding box
 */
UrbisStatus urbis_index_query_range(UrbisIndex *idx, const UrbisMBR *range, UrbisObjectList **out_list);

/**
 * @brief Query objects at a point
 */
UrbisStatus urbis_index_query_point(UrbisIndex *idx, double x, double y, UrbisObjectList **out_list);

/**
 * @brief Query k nearest neighbors
 */
UrbisStatus urbis_index_query_knn(UrbisIndex *idx, double x, double y, size_t k, UrbisObjectList **out_list);

/**
 * @brief Find adjacent pages to a region (disk-aware)
 */
UrbisStatus urbis_index_find_adjacent_pages(UrbisIndex *idx, const UrbisMBR *region, UrbisPageList **out_pages);

/**
 * @brief Query objects in adjacent pages
 */
UrbisStatus urbis_index_query_adjacent(UrbisIndex *idx, const UrbisMBR *region, UrbisObjectList **out_list);

/* ============================================================================
 * Persistence
 * ============================================================================ */

/**
 * @brief Save index to a file
 */
UrbisStatus urbis_index_save(UrbisIndex *idx, const char *path);

/**
 * @brief Load index from a file (creates a new index)
 */
UrbisStatus urbis_index_load(const char *path, UrbisIndex **out_index);

/**
 * @brief Sync changes to disk (if persistence enabled)
 */
UrbisStatus urbis_index_sync(UrbisIndex *idx);

/* ============================================================================
 * Statistics and Utilities
 * ============================================================================ */

/**
 * @brief Get index statistics
 */
UrbisStatus urbis_index_get_stats(const UrbisIndex *idx, UrbisStats *stats);

/**
 * @brief Get number of objects in index
 */
UrbisStatus urbis_index_count(const UrbisIndex *idx, size_t *out_count);

/**
 * @brief Get spatial bounds of all data
 */
UrbisStatus urbis_index_bounds(const UrbisIndex *idx, UrbisMBR *out_bounds);

/**
 * @brief Print statistics to a file
 */
UrbisStatus urbis_index_print_stats(const UrbisIndex *idx, FILE *out);

/**
 * @brief Estimate disk seeks for a sequence of queries
 */
UrbisStatus urbis_index_estimate_seeks(const UrbisIndex *idx,
                                      const UrbisMBR *regions, size_t count,
                                      size_t *out_seeks);

/* ============================================================================
 * Result List Operations
 * ============================================================================ */

/**
 * @brief Free an object (including any geometry memory)
 */
void urbis_object_free(UrbisObject *obj);

/**
 * @brief Free an object list
 */
void urbis_object_list_free(UrbisObjectList *list);

/**
 * @brief Free a page list
 */
void urbis_page_list_free(UrbisPageList *list);

/* ============================================================================
 * Convenience Functions
 * ============================================================================ */

/**
 * @brief Create an MBR (bounding box)
 */
static inline UrbisMBR urbis_mbr(double min_x, double min_y, double max_x, double max_y) {
    return mbr_create(min_x, min_y, max_x, max_y);
}

/**
 * @brief Create a point
 */
static inline UrbisPoint urbis_point(double x, double y) {
    return point_create(x, y);
}

#ifdef __cplusplus
}
#endif

#endif /* URBIS_H */