# Urbis - Disk-Aware Spatial Indexing for City-Scale GIS

Urbis is a C library for spatial indexing of city-scale geographic data. It combines KD-tree partitioning with quadtree-based adjacent page lookups to reduce disk seeks on large datasets.

**Features**
- Disk-aware page allocation and adjacency lookup
- KD-tree partitioning for efficient spatial indexing
- GeoJSON and WKT loading
- Points, LineStrings, and Polygons
- Thread-safe core data structures
- Go gRPC server with CGO bindings

**Build**

```bash
cmake -S . -B build
cmake --build build
```

Optional CMake flags:
- `-DURBIS_BUILD_TESTS=ON`
- `-DURBIS_BUILD_CLI=ON`
- `-DURBIS_BUILD_SHARED=ON`
- `-DURBIS_BUILD_STATIC=ON`

The build outputs libraries to `lib/` and binaries to `bin/`.

**CLI Quickstart**

```bash
# Build index from GeoJSON
./bin/urbis build --input examples/data/city.geojson --format geojson --index city.urbis

# Range query
./bin/urbis query-range --index city.urbis --mbr 0,0,100,100

# KNN query
./bin/urbis query-knn --index city.urbis --point 10,10 --k 5

# Adjacent pages
./bin/urbis pages --index city.urbis --mbr 0,0,100,100

# Stats
./bin/urbis stats --index city.urbis
```

**C API Example**

```c
#include <urbis.h>

int main(void) {
    UrbisIndex *idx = NULL;
    UrbisStatus status = urbis_index_create(NULL, &idx);
    if (status != URBIS_OK) return 1;

    urbis_index_insert_point(idx, 10.5, 20.3, NULL);

    UrbisPoint road[] = {
        urbis_point(0, 0),
        urbis_point(100, 100)
    };
    urbis_index_insert_linestring(idx, road, 2, NULL);

    urbis_index_build(idx);

    UrbisMBR region = urbis_mbr(0, 0, 50, 50);
    UrbisObjectList *result = NULL;
    urbis_index_query_range(idx, &region, &result);

    if (result) {
        printf("Found %zu objects\n", result->count);
        urbis_object_list_free(result);
    }

    urbis_index_destroy(idx);
    return 0;
}
```

**Go gRPC API**

Build the C library first so `lib/` is populated, then build the server:

```bash
cmake -S . -B build
cmake --build build

cd api
go build ./cmd/server
./cmd/server
```

See `api/README.md` for gRPC usage examples.

**Tests**

```bash
cmake -S . -B build -DURBIS_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

**License**

MIT. See `LICENSE`.

**Contributing**

Please read `CONTRIBUTING.md` before opening a PR.