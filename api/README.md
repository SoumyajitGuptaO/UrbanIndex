# Urbis gRPC API

Go gRPC backend for the Urbis disk-aware spatial indexing library.

**Prerequisites**
- Go 1.21+
- protobuf compiler (`protoc`)
- protoc Go plugins

**Build the C library**

```bash
cmake -S .. -B ../build
cmake --build ../build
```

This produces `lib/` in the repo root, which the CGO bindings link against.

**Generate protobuf code**

```bash
protoc --go_out=pkg/pb --go-grpc_out=pkg/pb proto/urbis.proto
```

**Build and run the server**

```bash
go build ./cmd/server
./cmd/server
```

**Usage Examples**

### Using grpcurl

```bash
grpcurl -plaintext localhost:50051 list

grpcurl -plaintext \
  -d '{"index_id": "city"}' \
  localhost:50051 urbis.UrbisService/CreateIndex

grpcurl -plaintext \
  -d '{"index_id": "city", "path": "/path/to/data.geojson"}' \
  localhost:50051 urbis.UrbisService/LoadGeoJSON

grpcurl -plaintext \
  -d '{"index_id": "city"}' \
  localhost:50051 urbis.UrbisService/Build

grpcurl -plaintext \
  -d '{"index_id": "city", "range": {"min_x": 0, "min_y": 0, "max_x": 100, "max_y": 100}}' \
  localhost:50051 urbis.UrbisService/QueryRange
```

### Using Go Client

```go
package main

import (
    "context"
    "log"

    "github.com/urbis/api/pkg/pb"
    "google.golang.org/grpc"
    "google.golang.org/grpc/credentials/insecure"
)

func main() {
    conn, err := grpc.Dial("localhost:50051",
        grpc.WithTransportCredentials(insecure.NewCredentials()))
    if err != nil {
        log.Fatal(err)
    }
    defer conn.Close()

    client := pb.NewUrbisServiceClient(conn)
    ctx := context.Background()

    _, err = client.CreateIndex(ctx, &pb.CreateIndexRequest{IndexId: "myindex"})
    if err != nil {
        log.Fatal(err)
    }

    _, err = client.LoadGeoJSON(ctx, &pb.LoadGeoJSONRequest{
        IndexId: "myindex",
        Path:    "/path/to/data.geojson",
    })
    if err != nil {
        log.Fatal(err)
    }

    _, err = client.Build(ctx, &pb.BuildRequest{IndexId: "myindex"})
    if err != nil {
        log.Fatal(err)
    }

    resp, err := client.QueryRange(ctx, &pb.RangeQueryRequest{
        IndexId: "myindex",
        Range: &pb.MBR{MinX: 0, MinY: 0, MaxX: 100, MaxY: 100},
    })
    if err != nil {
        log.Fatal(err)
    }

    log.Printf("Found %d objects", resp.Count)
}
```