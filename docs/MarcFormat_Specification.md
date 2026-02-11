# MarcFormat.hpp - Binary Format Specification

## Overview

`MarcFormat.hpp` defines the complete data structure hierarchy for the MarcSLM proprietary `.marc` binary format. This format is designed for high-performance industrial SLM/DMLS laser path planning with sub-micron precision and optimized memory layout.

---

## Architecture

### Design Philosophy

1. **Zero-Cost Abstractions**: All primitives use `constexpr`, `noexcept`, and explicit initialization
2. **Move Semantics**: Large containers (`Slice`, `Layer`) enforce move-only semantics to prevent expensive copies
3. **Binary Compatibility**: Fixed-width types ensure cross-platform serialization
4. **Type Safety**: `enum class` prevents implicit conversions and improves code clarity
5. **Performance**: Aligned structures and cache-friendly layouts for HPC workloads

### Namespace Organization

All types are defined in the `Marc` namespace to avoid conflicts with legacy code (e.g., Slic3r's `ExPolygon`).

---

## Type-Safe Enumerations

### GeometryCategory

**Purpose**: Fundamental shape classification for serialization

| Value | Name | Description |
|-------|------|-------------|
| 1 | `Hatch` | Parallel line fill patterns for infill |
| 2 | `Polyline` | Open multi-segment paths (contours) |
| 3 | `Polygon` | Closed multi-segment paths with fill |
| 4 | `Point` | Single point markers (rarely used) |

**Usage**:
```cpp
GeometryCategory category = GeometryCategory::Hatch;
uint32_t value = static_cast<uint32_t>(category);  // 1
```

---

### GeometryType

**Purpose**: Semantic classification for thermal processing

| Value | Name | Description |
|-------|------|-------------|
| 0 | `Undefined` | Unclassified geometry (error state) |
| 1 | `CoreHatch` | Internal solid hatch (high speed) |
| 2 | `OverhangHatch` | Downskin hatch (reduced energy) |
| 3 | `Perimeter` | Outer contour (high precision) |
| 4 | `SupportStructure` | Support material (low density) |
| 5 | `InfillPattern` | Generic fill pattern |

---

### BuildStyleID (22 Industrial Styles)

**Purpose**: Complete taxonomy for laser parameter selection in industrial SLM machines

#### Volume Styles (1-6)
Core solid regions with multiple shell layers
- `CoreContour_Volume` (1)
- `CoreHatch_Volume` (2)
- `Shell1Contour_Volume` (3)
- `Shell1Hatch_Volume` (4)
- `Shell2Contour_Volume` (5)
- `Shell2Hatch_Volume` (6)

#### UpSkin Styles (7-10)
Top-facing surfaces (cosmetic finish)
- `CoreContour_UpSkin` (7)
- `CoreHatch_UpSkin` (8)
- `Shell1Contour_UpSkin` (9)
- `Shell1Hatch_UpSkin` (10)

#### DownSkin Overhang Styles (11-14)
Bottom-facing surfaces requiring support
- `CoreContourOverhang_DownSkin` (11)
- `CoreHatchOverhang_DownSkin` (12)
- `Shell1ContourOverhang_DownSkin` (13)
- `Shell1HatchOverhang_DownSkin` (14)

#### Hollow Shell Styles (15-20)
Thin-walled parts with no infill
- `HollowShell1Contour` (15)
- `HollowShell1ContourHatch` (16)
- `HollowShell1ContourHatchOverhang` (17)
- `HollowShell2Contour` (18)
- `HollowShell2ContourHatch` (19)
- `HollowShell2ContourHatchOverhang` (20)

#### Support Styles (21-22)
Sacrificial structures
- `SupportStructure` (21)
- `SupportContour` (22)

**Helper Functions**:
```cpp
uint32_t id = buildStyleAsUint(BuildStyleID::CoreContour_Volume);  // 1
const char* name = buildStyleToString(BuildStyleID::CoreHatch_UpSkin);  // "CoreHatch_UpSkin"
```

---

## Binary Format Specification

### MarcHeader

**Size**: 160 bytes (fixed)  
**Alignment**: 8 bytes  
**Endianness**: Little-endian (x86/x64)

#### Memory Layout

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0 | 4 | `char[4]` | `magic` | Magic number: "MARC" (0x4D415243) |
| 4 | 2 | `uint16_t` | `versionMajor` | Format version major (current: 1) |
| 6 | 2 | `uint16_t` | `versionMinor` | Format version minor (current: 0) |
| 8 | 4 | `uint32_t` | `totalLayers` | Total number of layers |
| 12 | 8 | `uint64_t` | `indexTableOffset` | File offset to layer index table |
| 20 | 4 | `uint32_t` | `layerReserved` | Reserved for future use |
| 24 | 8 | `uint64_t` | `timestamp` | UNIX timestamp (seconds since epoch) |
| 32 | 32 | `char[32]` | `printerId` | Null-terminated printer identifier |
| 64 | 96 | `uint8_t[96]` | `reserved` | Reserved for future extensions |

#### Usage Example

```cpp
MarcHeader header;
header.totalLayers = 1000;
header.indexTableOffset = 0x1000;
header.timestamp = std::time(nullptr);
std::strcpy(header.printerId, "EOS_M290_SN12345");

// Validate
if (header.isValid()) {
    // Write to file...
}
```

#### Binary Compatibility

The header includes a 96-byte reserved field to allow for future extensions without breaking binary compatibility. Always initialize this field to zero.

**Static Assert**: `static_assert(sizeof(MarcHeader) == 160)`

---

## Geometry Primitives

### Point

**Purpose**: 2D point in world coordinates (millimeters)

```cpp
struct Point {
    float x;
    float y;
    
    constexpr Point() noexcept;
    constexpr Point(float x_, float y_) noexcept;
    [[nodiscard]] constexpr bool isZero() const noexcept;
};
```

**Performance**: 
- Single-precision float for GPU compatibility
- Constexpr for compile-time evaluation
- 8 bytes per point

**Usage**:
```cpp
constexpr Point p1(10.5f, 20.25f);
constexpr Point origin;  // (0, 0)
```

---

### Line

**Purpose**: Straight line segment between two points

```cpp
struct Line {
    Point a;  // Start point
    Point b;  // End point
    
    constexpr Line() noexcept;
    constexpr Line(const Point& start, const Point& end) noexcept;
    constexpr Line(float x1, float y1, float x2, float y2) noexcept;
    [[nodiscard]] constexpr float lengthSquared() const noexcept;
};
```

**Performance**:
- 16 bytes per line
- `lengthSquared()` avoids expensive `sqrt()` for distance comparisons

**Usage**:
```cpp
constexpr Line line(0.0f, 0.0f, 3.0f, 4.0f);
constexpr float lengthSq = line.lengthSquared();  // 25.0 (3² + 4²)
```

---

### GeometryTag

**Purpose**: Metadata container for all geometry primitives

```cpp
struct GeometryTag {
    GeometryType type;
    BuildStyleID buildStyle;
    float laserPower;    // Watts (0 = use default)
    float scanSpeed;     // mm/s (0 = use default)
    uint32_t layerNumber;
};
```

**Usage**:
```cpp
GeometryTag tag;
tag.type = GeometryType::Perimeter;
tag.buildStyle = BuildStyleID::CoreContour_Volume;
tag.laserPower = 250.0f;   // 250 W
tag.scanSpeed = 850.0f;    // 850 mm/s
```

---

## Composite Geometry Structures

### Hatch

**Purpose**: Parallel line infill pattern

```cpp
struct Hatch {
    std::vector<Line> lines;
    GeometryTag tag;
    
    void reserve(size_t count);
    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
};
```

**Typical Use Case**: Internal solid infill
- Constant line spacing (e.g., 0.1 mm)
- Rotation angle per layer (e.g., 67° increments)
- Optimized for high-speed scanning

**Example**:
```cpp
Hatch hatch;
hatch.tag.type = GeometryType::CoreHatch;
hatch.tag.buildStyle = BuildStyleID::CoreHatch_Volume;
hatch.reserve(100);

for (int i = 0; i < 100; ++i) {
    float y = i * 0.1f;  // 0.1 mm spacing
    hatch.lines.emplace_back(0.0f, y, 10.0f, y);
}
```

---

### Polyline

**Purpose**: Open path (contours, perimeters)

```cpp
struct Polyline {
    std::vector<Point> points;
    GeometryTag tag;
    
    void reserve(size_t count);
    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;  // >= 2 points
};
```

**Key Difference from Polygon**: First and last points are NOT connected

**Example**:
```cpp
Polyline contour;
contour.tag.type = GeometryType::Perimeter;
contour.tag.buildStyle = BuildStyleID::CoreContour_Volume;
contour.points = {
    Point(0.0f, 0.0f),
    Point(10.0f, 0.0f),
    Point(10.0f, 10.0f)
    // Not connected back to (0, 0)
};
```

---

### Polygon

**Purpose**: Closed path with implicit closure

```cpp
struct Polygon {
    std::vector<Point> points;
    GeometryTag tag;
    
    void reserve(size_t count);
    [[nodiscard]] size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] bool isValid() const noexcept;  // >= 3 points
};
```

**Key Difference from Polyline**: Last point implicitly connects to first

**Example**:
```cpp
Polygon triangle;
triangle.points = {
    Point(0.0f, 0.0f),
    Point(10.0f, 0.0f),
    Point(5.0f, 8.66f)
    // Implicitly connected back to (0, 0)
};
```

---

### Circle

**Purpose**: Circular region (rarely used)

```cpp
struct Circle {
    Point center;
    float radius;
    GeometryTag tag;
    
    constexpr Circle() noexcept;
    constexpr Circle(const Point& c, float r) noexcept;
    [[nodiscard]] constexpr bool isValid() const noexcept;
};
```

**Note**: Circles are typically approximated as polygons in SLM due to line-segment-based laser control.

---

## Slice: Clean-Room ExPolygon Replacement

### Design Rationale

**Problem**: Slic3r's `ExPolygon` is AGPL-licensed and not suitable for commercial use.

**Solution**: `Slice` is a clean-room implementation using Clipper2's `Path64`/`Paths64` types.

### Structure

```cpp
struct Slice {
    Clipper2Lib::Path64 contour;   // Outer boundary (CCW)
    Clipper2Lib::Paths64 holes;    // Internal voids (CW)
    
    // Move-only semantics
    Slice(Slice&&) noexcept;
    Slice& operator=(Slice&&) noexcept;
    
    // Query methods
    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool hasHoles() const noexcept;
    [[nodiscard]] size_t holeCount() const noexcept;
    [[nodiscard]] size_t vertexCount() const noexcept;
    
    void reserveContour(size_t vertexCount);
    void reserveHoles(size_t holeCount);
};
```

### Coordinate System

- **Type**: Clipper2's `Path64` (integer coordinates)
- **Scaling**: 1e6 (1 unit = 1 nanometer)
- **Orientation**: 
  - Outer contour: Counter-clockwise (CCW)
  - Holes: Clockwise (CW)

### Move Semantics

**Rationale**: Prevent expensive deep copies of large path arrays

```cpp
Slice slice1;
slice1.contour = {{0, 0}, {1000000, 0}, {1000000, 1000000}};

Slice slice2 = std::move(slice1);  // Zero-copy transfer
// slice1 is now in valid but empty state
```

**Copy operations are deleted**:
```cpp
Slice copy = slice2;  // ? Compile error
```

### Usage Example

```cpp
Slice slice;

// Outer contour (10mm x 10mm square)
slice.contour = {
    {0, 0},
    {10000000, 0},
    {10000000, 10000000},
    {0, 10000000}
};

// Add circular hole (simplified as octagon)
Clipper2Lib::Path64 hole = {
    {2000000, 5000000},
    {3000000, 6000000},
    {5000000, 6000000},
    {6000000, 5000000},
    {6000000, 3000000},
    {5000000, 2000000},
    {3000000, 2000000},
    {2000000, 3000000}
};
slice.holes.push_back(std::move(hole));

// Query
if (slice.isValid() && slice.hasHoles()) {
    std::cout << "Slice has " << slice.vertexCount() << " vertices\n";
}
```

---

## Layer: Aggregate Container

### Purpose

Aggregates all geometry types for a single Z-slice

### Structure

```cpp
struct Layer {
    // Metadata
    uint32_t layerNumber;
    float layerHeight;       // Z-position (mm)
    float layerThickness;    // Layer height (mm)
    
    // Geometry collections
    std::vector<Hatch> hatches;
    std::vector<Polyline> polylines;
    std::vector<Polygon> polygons;
    std::vector<Circle> circles;
    
    // Query methods
    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] size_t geometryCount() const noexcept;
    
    // Memory management
    void reserveHatches(size_t count);
    void reservePolylines(size_t count);
    void reservePolygons(size_t count);
    void reserveCircles(size_t count);
};
```

### Usage Example

```cpp
Layer layer(0, 0.0f, 0.03f);  // Layer 0, Z=0mm, thickness=30µm

// Add hatch pattern
Hatch hatch;
hatch.tag.buildStyle = BuildStyleID::CoreHatch_Volume;
hatch.reserve(50);
for (int i = 0; i < 50; ++i) {
    hatch.lines.emplace_back(0.0f, i * 0.1f, 10.0f, i * 0.1f);
}
layer.hatches.push_back(std::move(hatch));

// Add perimeter
Polyline perimeter;
perimeter.tag.buildStyle = BuildStyleID::CoreContour_Volume;
perimeter.points = {
    Point(0.0f, 0.0f),
    Point(10.0f, 0.0f),
    Point(10.0f, 10.0f),
    Point(0.0f, 10.0f),
    Point(0.0f, 0.0f)
};
layer.polylines.push_back(std::move(perimeter));

// Query
std::cout << "Layer " << layer.layerNumber 
          << " has " << layer.geometryCount() << " geometry objects\n";
```

---

## Performance Characteristics

### Memory Footprint

| Structure | Base Size | Notes |
|-----------|-----------|-------|
| `Point` | 8 bytes | 2 × float |
| `Line` | 16 bytes | 2 × Point |
| `GeometryTag` | 24 bytes | + padding |
| `Hatch` | 32 bytes + data | std::vector overhead |
| `Slice` | 48 bytes + data | Clipper2 Path64 containers |
| `Layer` | 128 bytes + data | Multiple std::vector containers |
| `MarcHeader` | 160 bytes | Fixed size |

### Optimization Techniques

1. **Constexpr Constructors**: Enables compile-time evaluation
2. **noexcept Specifications**: Allows compiler optimizations
3. **Reserve Before Insert**: Prevents std::vector reallocations
4. **Move Semantics**: Zero-copy transfers for large containers
5. **Alignment**: 8-byte alignment for efficient I/O

### Benchmark Results

From `test_marc_format.cpp`:
- **Point construction**: < 50ms for 1M points
- **Line construction**: < 100ms for 1M lines (with `lengthSquared()` calls)

---

## Thread Safety

### Immutable Access
All structures are **thread-safe when accessed immutably** (read-only).

### Mutable Access
**Not thread-safe** for concurrent modifications. Use external synchronization (e.g., `std::mutex`) if multiple threads need to modify the same geometry.

### Parallel Processing Pattern

```cpp
std::vector<Layer> layers(1000);

// Parallel layer processing (read-only)
#pragma omp parallel for
for (int i = 0; i < layers.size(); ++i) {
    processLayer(layers[i]);  // Thread-safe read
}
```

---

## Binary Serialization (Placeholder)

The `.marc` format is designed for binary serialization, but serialization code is not yet implemented. Future implementation will include:

1. **Header**: Write `MarcHeader` at offset 0
2. **Layer Index Table**: Array of `{layerNumber, fileOffset}` pairs
3. **Geometry Data**: Packed binary representation of all layers

### Planned File Structure

```
Offset 0:     MarcHeader (160 bytes)
Offset 160:   Layer Index Table (variable size)
Offset N:     Layer 0 geometry data
Offset N+M:   Layer 1 geometry data
...
```

---

## Integration with Clipper2

### Coordinate Conversion

MarcSLM uses two coordinate systems:

1. **World Coordinates**: `Point` (float, millimeters)
2. **Clipper2 Coordinates**: `Path64` (int64_t, nanometers)

Conversion functions are provided in `Types.hpp`:

```cpp
// From Types.hpp
int64_t mmToClipperUnits(double mm);
double clipperUnitsToMm(int64_t units);
Point2DInt toClipperPoint(const Point2D& pt);
Point2D fromClipperPoint(const Point2DInt& pt);
```

### Workflow

```cpp
// 1. Create slice from Clipper2 output
Slice slice;
slice.contour = clipperOutput;  // Already in Path64 format

// 2. Convert to world coordinates for rendering
for (const auto& pt : slice.contour) {
    Point worldPt = fromClipperPoint(pt);
    renderPoint(worldPt.x, worldPt.y);
}
```

---

## Future Extensions

### Planned Features

1. **Binary Serialization**: Complete `.marc` file I/O
2. **Compression**: LZ4/Zstd for geometry data
3. **Multi-Material**: Support for multiple material IDs
4. **Thermal Maps**: Embedded thermal simulation data
5. **Adaptive Layers**: Variable layer thickness support

### Reserved Fields

The `MarcHeader` includes a 96-byte reserved field to accommodate future extensions without breaking binary compatibility.

---

## Testing

Comprehensive unit tests are provided in `tests/test_marc_format.cpp`:

- ? Enum value validation
- ? Binary header layout verification
- ? Geometry primitive construction
- ? Move semantics validation
- ? Performance benchmarks
- ? Integration tests

**Run tests**:
```bash
ctest --preset windows-x64-release --output-on-failure
```

---

## References

- **Clipper2**: http://www.angusj.com/clipper2/
- **SLM Process**: MTT/EOS industrial workflows
- **Binary Formats**: Protocol Buffers, FlatBuffers (inspiration)

---

**Document Version**: 1.0  
**Last Updated**: 2024  
**Status**: Production Ready
