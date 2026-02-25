// ==============================================================================
// MarcSLM - Binary Format Specification and Geometry Primitives
// ==============================================================================
// Copyright (c) 2024 MarcSLM Project
// Licensed under a commercial-friendly license (non-AGPL)
// ==============================================================================
// This header defines the data-core for industrial SLM laser path planning.
// Performance-critical structures use constexpr, noexcept, and explicit
// initialization for zero-overhead abstractions.
// ==============================================================================

#pragma once

#include <clipper2/clipper.h>

#include <cstdint>
#include <ctime>
#include <vector>

namespace Marc {

// ==============================================================================
// Type-Safe Enumeration: Geometry Category
// ==============================================================================

/// @brief Fundamental classification of geometric primitives
/// @details Defines the basic shape category for serialization and dispatch
///          inside the .marc binary format.
/// @note Underlying type is uint32_t to match the 4-byte field width used
///       in the binary layer-index table.
enum class GeometryCategory : uint32_t {
    Hatch    = 1,  ///< Parallel line fill patterns for infill regions.
    Polyline = 2,  ///< Open multi-segment paths (e.g., perimeters, contours).
    Polygon  = 3,  ///< Closed multi-segment paths with implicit fill.
    Point    = 4   ///< Single point markers (fiducials, alignment marks).
};

/// @brief Convert GeometryCategory to its underlying uint32_t value.
/// @param cat The category to convert.
/// @return The raw unsigned integer representation.
[[nodiscard]] constexpr uint32_t typeAsUint(GeometryCategory cat) noexcept {
    return static_cast<uint32_t>(cat);
}

// ==============================================================================
// Type-Safe Enumeration: Geometry Type
// ==============================================================================

/// @brief Semantic classification of geometry for thermal processing.
/// @details Maps geometric primitives to their functional role in the build
///          process. The machine controller selects laser parameters (power,
///          speed, focus offset) based on this classification.
enum class GeometryType : uint32_t {
    Undefined        = 0,  ///< Unclassified geometry (error / sentinel state).
    CoreHatch        = 1,  ///< Internal solid hatch (high speed, moderate power).
    OverhangHatch    = 2,  ///< Down-skin hatch (reduced energy to avoid warping).
    Perimeter        = 3,  ///< Outer contour (high precision, lower speed).
    SupportStructure = 4,  ///< Support material (low density, easy removal).
    InfillPattern    = 5   ///< Generic fill pattern (user-defined parameters).
};

/// @brief Convert GeometryType to its underlying uint32_t value.
/// @param gt The geometry type to convert.
/// @return The raw unsigned integer representation.
[[nodiscard]] constexpr uint32_t typeAsUint(GeometryType gt) noexcept {
    return static_cast<uint32_t>(gt);
}

// ==============================================================================
// Type-Safe Enumeration: BuildStyle Identifier (22-Style Industrial Taxonomy)
// ==============================================================================

/// @brief Complete BuildStyle taxonomy for industrial SLM machines.
/// @details Each BuildStyle maps 1:1 to a laser parameter set (power, speed,
///          spot size, focus offset, skywriting, etc.).  These IDs are stored
///          in every GeometryTag and written verbatim into the .marc format so
///          that the machine controller can select parameters without
///          round-tripping through a parameter file.
///
/// Naming Convention (read right-to-left):
///   - Region:   Volume | UpSkin | DownSkin | HollowShell
///   - Strategy:  Contour (perimeter) | Hatch (infill)
///   - Qualifier: Overhang (angle > critical threshold)
///
/// @par Example
/// @code
///   GeometryTag tag(GeometryType::CoreHatch,
///                   BuildStyleID::CoreHatch_Volume);
/// @endcode
///
/// @note The 22-value set is derived from industrial MTT / Renishaw / EOS
///       workflow conventions.
enum class BuildStyleID : uint32_t {
    // ---- Volume (solid interior) -------------------------------------------
    CoreContour_Volume                = 1,   ///< Volume core perimeter path.
    CoreHatch_Volume                  = 2,   ///< Volume core infill hatching.
    Shell1Contour_Volume              = 3,   ///< Volume primary-shell contour.
    Shell1Hatch_Volume                = 4,   ///< Volume primary-shell infill.
    Shell2Contour_Volume              = 5,   ///< Volume secondary-shell contour.
    Shell2Hatch_Volume                = 6,   ///< Volume secondary-shell infill.

    // ---- UpSkin (top-facing surfaces) --------------------------------------
    CoreContour_UpSkin                = 7,   ///< UpSkin core perimeter path.
    CoreHatch_UpSkin                  = 8,   ///< UpSkin core infill hatching.
    Shell1Contour_UpSkin              = 9,   ///< UpSkin primary-shell contour.
    Shell1Hatch_UpSkin                = 10,  ///< UpSkin primary-shell infill.

    // ---- DownSkin / Overhang -----------------------------------------------
    CoreContourOverhang_DownSkin      = 11,  ///< DownSkin overhang perimeter.
    CoreHatchOverhang_DownSkin        = 12,  ///< DownSkin overhang infill.
    Shell1ContourOverhang_DownSkin    = 13,  ///< DownSkin primary-shell contour.
    Shell1HatchOverhang_DownSkin      = 14,  ///< DownSkin primary-shell infill.

    // ---- Hollow Shell 1 (thin-walled parts, primary wall) ------------------
    HollowShell1Contour               = 15,  ///< Hollow-shell-1 contour.
    HollowShell1ContourHatch          = 16,  ///< Hollow-shell-1 contour fill.
    HollowShell1ContourHatchOverhang  = 17,  ///< Hollow-shell-1 overhang fill.

    // ---- Hollow Shell 2 (thin-walled parts, secondary wall) ----------------
    HollowShell2Contour               = 18,  ///< Hollow-shell-2 contour.
    HollowShell2ContourHatch          = 19,  ///< Hollow-shell-2 contour fill.
    HollowShell2ContourHatchOverhang  = 20,  ///< Hollow-shell-2 overhang fill.

    // ---- Support -----------------------------------------------------------
    SupportStructure                  = 21,  ///< Support-material hatch.
    SupportContour                    = 22   ///< Support-material perimeter.
};

/// @brief Convert BuildStyleID to its underlying uint32_t value.
/// @param id The BuildStyle to convert.
/// @return The raw unsigned integer representation.
[[nodiscard]] constexpr uint32_t typeAsUint(BuildStyleID id) noexcept {
    return static_cast<uint32_t>(id);
}

/// @brief Convert a BuildStyleID to a human-readable C-string.
/// @param id The BuildStyle to stringify.
/// @return Pointer to a static string literal; never null.
[[nodiscard]] inline const char* buildStyleToString(BuildStyleID id) noexcept {
    switch (id) {
        case BuildStyleID::CoreContour_Volume:                return "CoreContour_Volume";
        case BuildStyleID::CoreHatch_Volume:                  return "CoreHatch_Volume";
        case BuildStyleID::Shell1Contour_Volume:              return "Shell1Contour_Volume";
        case BuildStyleID::Shell1Hatch_Volume:                return "Shell1Hatch_Volume";
        case BuildStyleID::Shell2Contour_Volume:              return "Shell2Contour_Volume";
        case BuildStyleID::Shell2Hatch_Volume:                return "Shell2Hatch_Volume";
        case BuildStyleID::CoreContour_UpSkin:                return "CoreContour_UpSkin";
        case BuildStyleID::CoreHatch_UpSkin:                  return "CoreHatch_UpSkin";
        case BuildStyleID::Shell1Contour_UpSkin:              return "Shell1Contour_UpSkin";
        case BuildStyleID::Shell1Hatch_UpSkin:                return "Shell1Hatch_UpSkin";
        case BuildStyleID::CoreContourOverhang_DownSkin:      return "CoreContourOverhang_DownSkin";
        case BuildStyleID::CoreHatchOverhang_DownSkin:        return "CoreHatchOverhang_DownSkin";
        case BuildStyleID::Shell1ContourOverhang_DownSkin:    return "Shell1ContourOverhang_DownSkin";
        case BuildStyleID::Shell1HatchOverhang_DownSkin:      return "Shell1HatchOverhang_DownSkin";
        case BuildStyleID::HollowShell1Contour:               return "HollowShell1Contour";
        case BuildStyleID::HollowShell1ContourHatch:          return "HollowShell1ContourHatch";
        case BuildStyleID::HollowShell1ContourHatchOverhang:  return "HollowShell1ContourHatchOverhang";
        case BuildStyleID::HollowShell2Contour:               return "HollowShell2Contour";
        case BuildStyleID::HollowShell2ContourHatch:          return "HollowShell2ContourHatch";
        case BuildStyleID::HollowShell2ContourHatchOverhang:  return "HollowShell2ContourHatchOverhang";
        case BuildStyleID::SupportStructure:                  return "SupportStructure";
        case BuildStyleID::SupportContour:                    return "SupportContour";
        default:                                              return "Unknown";
    }
}

// ==============================================================================
// Binary Format: .marc File Header
// ==============================================================================

/// @brief Wire-format header for the .marc binary file.
/// @details Appears at byte offset 0 of every .marc file.  The struct is
///          packed to guarantee a deterministic 160-byte layout that can be
///          written/read with a single fread/fwrite on any little-endian
///          platform.
///
/// Field Map (byte offsets):
/// | Offset | Size | Field            | Description                         |
/// |--------|------|------------------|-------------------------------------|
/// |   0    |   4  | magic[4]         | "MARC" (0x4D 0x41 0x52 0x43)       |
/// |   4    |   4  | version          | Format version (packed uint32_t)    |
/// |   8    |   4  | totalLayers      | Number of layers in the build       |
/// |  12    |   8  | indexTableOffset  | Byte offset to layer index table    |
/// |  20    |   8  | timestamp         | UNIX time_t (seconds since epoch)   |
/// |  28    |  32  | printerId[32]     | Null-terminated printer identifier  |
/// |  60    | 100  | reserved[100]     | Must be zero; future extensions     |
/// |  160   |      |                  | --- end of header ---               |
///
/// @warning Do NOT add virtual methods or non-trivial members.  This struct
///          is memcpy'd / reinterpret_cast'd during I/O.
///
/// @par Version encoding
///   Bits [31:16] = major, bits [15:0] = minor.
///   Current version: 1.0 ? 0x0001'0000.
#pragma pack(push, 1)
struct MarcHeader {
    /// @brief Magic bytes: 'M','A','R','C' (4 bytes).
    char magic[4];

    /// @brief Format version.  Major in upper 16 bits, minor in lower 16.
    uint32_t version;

    /// @brief Total number of Z-layers stored in the file.
    uint32_t totalLayers;

    /// @brief Absolute byte offset to the layer-index table.
    uint64_t indexTableOffset;

    /// @brief UNIX timestamp of file creation (seconds since epoch).
    uint64_t timestamp;

    /// @brief Null-terminated printer / machine identifier string.
    char printerId[32];

    /// @brief Reserved for future use.  Must be zero-initialized.
    uint8_t reserved[100];

    // =========================================================================
    // Constructors
    // =========================================================================

    /// @brief Default-constructs a valid v1.0 header with all fields zeroed
    ///        except magic and version.
    MarcHeader() noexcept
        : magic{'M', 'A', 'R', 'C'}
        , version(0x00010000u)          // v1.0
        , totalLayers(0)
        , indexTableOffset(0)
        , timestamp(0)
        , printerId{}
        , reserved{} {
    }

    // =========================================================================
    // Query helpers
    // =========================================================================

    /// @brief Validate the magic bytes.
    /// @return true if magic == "MARC".
    [[nodiscard]] bool isValid() const noexcept {
        return magic[0] == 'M' && magic[1] == 'A' &&
               magic[2] == 'R' && magic[3] == 'C';
    }

    /// @brief Extract the major version number.
    [[nodiscard]] constexpr uint16_t versionMajor() const noexcept {
        return static_cast<uint16_t>(version >> 16);
    }

    /// @brief Extract the minor version number.
    [[nodiscard]] constexpr uint16_t versionMinor() const noexcept {
        return static_cast<uint16_t>(version & 0xFFFFu);
    }
};
#pragma pack(pop)

// Compile-time proof that the header is exactly 160 bytes.
static_assert(sizeof(MarcHeader) == 160,
    "MarcHeader must be exactly 160 bytes for binary compatibility.");

// ==============================================================================
// Geometry Primitives: High-Performance Tier
// ==============================================================================

/// @brief 2D point primitive in single-precision float.
/// @details Coordinates are in millimetres (mm) in build-plate space.
///          sizeof(Point) == 8.
struct Point {
    float x;  ///< X-coordinate in mm.
    float y;  ///< Y-coordinate in mm.

    /// @brief Zero-initialising default constructor.
    constexpr Point() noexcept : x(0.0f), y(0.0f) {}

    /// @brief Construct from explicit coordinates.
    constexpr Point(float x_, float y_) noexcept : x(x_), y(y_) {}

    /// @brief Test whether the point sits at the origin.
    [[nodiscard]] constexpr bool isZero() const noexcept {
        return x == 0.0f && y == 0.0f;
    }
};

/// @brief Straight line segment between two endpoints.
/// @details Represents an atomic laser-on vector.  sizeof(Line) == 16.
struct Line {
    Point a;  ///< Start (laser-on) point.
    Point b;  ///< End   (laser-off) point.

    /// @brief Zero-initialising default constructor.
    constexpr Line() noexcept : a(), b() {}

    /// @brief Construct from two Point values.
    constexpr Line(const Point& start, const Point& end) noexcept
        : a(start), b(end) {}

    /// @brief Construct from four scalar coordinates.
    constexpr Line(float x1, float y1, float x2, float y2) noexcept
        : a(x1, y1), b(x2, y2) {}

    /// @brief Squared Euclidean length (avoids sqrt).
    [[nodiscard]] constexpr float lengthSquared() const noexcept {
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        return dx * dx + dy * dy;
    }
};

// ==============================================================================
// Geometry Metadata Tag
// ==============================================================================

/// @brief Metadata container attached to every geometry primitive.
/// @details Carries the semantic classification and (optional) per-vector
///          laser overrides so the machine controller can select parameters
///          without consulting an external table.
///
/// @par Example
/// @code
///   GeometryTag tag(GeometryType::Perimeter,
///                   BuildStyleID::CoreContour_Volume);
///   tag.laserPower = 200.0f;   // watts
///   tag.scanSpeed  = 800.0f;   // mm/s
/// @endcode
struct GeometryTag {
    GeometryType type;           ///< Functional role (hatch, perimeter, …).
    BuildStyleID buildStyle;     ///< Thermal-processing strategy ID.
    float        laserPower;     ///< Laser power override [W]  (0 = default).
    float        scanSpeed;      ///< Scan speed override [mm/s] (0 = default).
    uint32_t     layerNumber;    ///< Originating layer index (0-based).

    /// @brief Default constructor — Undefined type, zeroed overrides.
    constexpr GeometryTag() noexcept
        : type(GeometryType::Undefined)
        , buildStyle(BuildStyleID::CoreContour_Volume)
        , laserPower(0.0f)
        , scanSpeed(0.0f)
        , layerNumber(0) {
    }

    /// @brief Construct with explicit type and BuildStyle.
    constexpr GeometryTag(GeometryType t, BuildStyleID bs) noexcept
        : type(t)
        , buildStyle(bs)
        , laserPower(0.0f)
        , scanSpeed(0.0f)
        , layerNumber(0) {
    }
};

// ==============================================================================
// Hatch Geometry (parallel-line infill)
// ==============================================================================

/// @brief A set of parallel scan-vector lines that fill a region.
/// @details Each line in the vector is an independent laser-on segment.
///          Hatch patterns are generated with constant spacing and an angle
///          that typically rotates 67° per layer.
struct Hatch {
    std::vector<Line> lines;  ///< Ordered collection of hatch vectors.
    GeometryTag       tag;    ///< Metadata for the entire hatch group.

    /// @brief Default constructor.
    Hatch() noexcept = default;

    /// @brief Pre-allocate storage for @p count lines.
    void reserve(size_t count) { lines.reserve(count); }

    /// @brief Number of lines in this hatch group.
    [[nodiscard]] size_t size()  const noexcept { return lines.size();  }

    /// @brief True when no lines are stored.
    [[nodiscard]] bool   empty() const noexcept { return lines.empty(); }
};

// ==============================================================================
// Polyline Geometry (open path)
// ==============================================================================

/// @brief An open, ordered sequence of connected points.
/// @details The first and last points are NOT implicitly connected.
///          Used for contour perimeters, travel moves, etc.
struct Polyline {
    std::vector<Point> points;  ///< Ordered vertices of the path.
    GeometryTag        tag;     ///< Metadata for the entire polyline.

    /// @brief Default constructor.
    Polyline() noexcept = default;

    /// @brief Pre-allocate storage for @p count points.
    void reserve(size_t count) { points.reserve(count); }

    /// @brief Number of vertices.
    [[nodiscard]] size_t size()  const noexcept { return points.size();  }

    /// @brief True when no vertices are stored.
    [[nodiscard]] bool   empty() const noexcept { return points.empty(); }

    /// @brief A polyline needs ? 2 points to define at least one segment.
    [[nodiscard]] bool   isValid() const noexcept { return points.size() >= 2; }
};

// ==============================================================================
// Polygon Geometry (closed path)
// ==============================================================================

/// @brief A closed, ordered sequence of points (last ? first is implicit).
/// @details Identical storage to Polyline, but semantically closed.
///          Requires ? 3 points to form a valid area.
struct Polygon {
    std::vector<Point> points;  ///< Ordered vertices (implicitly closed).
    GeometryTag        tag;     ///< Metadata for the entire polygon.

    /// @brief Default constructor.
    Polygon() noexcept = default;

    /// @brief Pre-allocate storage for @p count points.
    void reserve(size_t count) { points.reserve(count); }

    /// @brief Number of vertices.
    [[nodiscard]] size_t size()  const noexcept { return points.size();  }

    /// @brief True when no vertices are stored.
    [[nodiscard]] bool   empty() const noexcept { return points.empty(); }

    /// @brief A polygon needs ? 3 points to enclose an area.
    [[nodiscard]] bool   isValid() const noexcept { return points.size() >= 3; }
};

// ==============================================================================
// Circle Geometry
// ==============================================================================

/// @brief Circle or arc primitive (centre + radius).
/// @details Rarely used in powder-bed fusion because the laser path is
///          always linearised, but retained for completeness and for
///          future curved-hatching extensions.
struct Circle {
    Point center;       ///< Centre point in mm.
    float radius;       ///< Radius in mm.
    GeometryTag tag;    ///< Metadata.

    /// @brief Default constructor — zero centre, zero radius.
    constexpr Circle() noexcept
        : center(), radius(0.0f), tag() {}

    /// @brief Construct from centre and radius.
    constexpr Circle(const Point& c, float r) noexcept
        : center(c), radius(r), tag() {}

    /// @brief A circle is valid when its radius is strictly positive.
    [[nodiscard]] constexpr bool isValid() const noexcept {
        return radius > 0.0f;
    }
};

// ==============================================================================
// ExPolygon: Contour with Holes (for hole-aware hatching)
// ==============================================================================

/// @brief A closed contour with zero or more interior holes.
/// @details Preserves the contour-hole association from the slicer so that
///          hatch generators can subtract holes from scan vectors.
///          This mirrors the Legacy Slic3r ExPolygon concept and PySLM's
///          slice representation.
///
///          Convention:
///          - contour:  outer boundary (CCW winding)
///          - holes:    interior voids (CW winding each)
///          - Hatches fill the region: contour MINUS union(holes)
struct ExPolygon {
    Polygon              contour;   ///< Outer boundary polygon.
    std::vector<Polygon> holes;     ///< Interior holes (voids, islands).
    GeometryTag          tag;       ///< Metadata for the contour.

    /// @brief Default constructor.
    ExPolygon() noexcept = default;

    /// @brief True when the contour is valid (? 3 points).
    [[nodiscard]] bool isValid() const noexcept { return contour.isValid(); }

    /// @brief True when at least one hole exists.
    [[nodiscard]] bool hasHoles() const noexcept { return !holes.empty(); }

    /// @brief Number of holes.
    [[nodiscard]] size_t holeCount() const noexcept { return holes.size(); }
};

// ==============================================================================
// Slice — Clean-Room ExPolygon Replacement
// ==============================================================================

/// @brief 2D cross-section with an outer boundary and zero or more holes.
/// @details This is the MarcSLM project's clean-room replacement for
///          Slic3r / PrusaSlicer's AGPL-licensed ExPolygon.  It stores
///          Clipper2 integer paths directly so that boolean operations,
///          offsets, and Minkowski sums can be performed without conversion.
///
/// Coordinate conventions:
///   - Clipper2 Path64: signed 64-bit integer coordinates.
///   - Scale factor 10^6 (1 unit ? 1 nm when input is in mm).
///   - Outer contour: counter-clockwise (CCW).
///   - Holes: clockwise (CW).
///
/// @par Move-only semantics
/// Copy construction and copy assignment are deleted.  Large path arrays
/// are transferred through the pipeline with std::move exclusively.
struct Slice {
    Clipper2Lib::Path64  contour;  ///< Outer boundary (CCW).
    Clipper2Lib::Paths64 holes;    ///< Internal voids  (CW each).

    // =========================================================================
    // Constructors
    // =========================================================================

    /// @brief Default constructor — empty contour, no holes.
    Slice() noexcept = default;

    /// @brief Move constructor — transfers ownership of path data.
    Slice(Slice&& other) noexcept
        : contour(std::move(other.contour))
        , holes(std::move(other.holes)) {
    }

    /// @brief Move-assignment operator.
    Slice& operator=(Slice&& other) noexcept {
        if (this != &other) {
            contour = std::move(other.contour);
            holes   = std::move(other.holes);
        }
        return *this;
    }

    /// @brief Deleted copy constructor — enforces move-only semantics.
    Slice(const Slice&) = delete;
    /// @brief Deleted copy-assignment — enforces move-only semantics.
    Slice& operator=(const Slice&) = delete;

    // =========================================================================
    // Query helpers
    // =========================================================================

    /// @brief True when the outer contour has ? 3 vertices.
    [[nodiscard]] bool isValid()   const noexcept { return contour.size() >= 3; }

    /// @brief True when at least one hole path exists.
    [[nodiscard]] bool hasHoles()  const noexcept { return !holes.empty(); }

    /// @brief Number of hole paths.
    [[nodiscard]] size_t holeCount()   const noexcept { return holes.size(); }

    /// @brief Total vertex count across contour and all holes.
    [[nodiscard]] size_t vertexCount() const noexcept {
        size_t n = contour.size();
        for (const auto& h : holes) n += h.size();
        return n;
    }

    /// @brief Reserve memory for the outer contour.
    void reserveContour(size_t n) { contour.reserve(n); }

    /// @brief Reserve memory for the hole vector (number of holes, not vertices).
    void reserveHoles(size_t n)   { holes.reserve(n); }
};

// ==============================================================================
// Layer — Aggregate container for one Z-height
// ==============================================================================

/// @brief All geometry for a single printed layer.
/// @details Aggregates Hatch, Polyline, Polygon, Circle, and ExPolygon
///          collections together with the layer's physical metadata.
struct Layer {
    // ---- metadata ----------------------------------------------------------
    uint32_t layerNumber;     ///< 0-based index from the build plate upward.
    float    layerHeight;     ///< Absolute Z-position of this layer [mm].
    float    layerThickness;  ///< Incremental layer thickness [mm] (20–100 µm typical).

    // ---- geometry collections ----------------------------------------------
    std::vector<Hatch>     hatches;     ///< Infill hatch groups.
    std::vector<Polyline>  polylines;   ///< Open contour / perimeter paths.
    std::vector<Polygon>   polygons;    ///< Closed filled regions.
    std::vector<Circle>    circles;     ///< Arcs / circles (rarely used).
    std::vector<ExPolygon> exPolygons;  ///< Contours with holes (for hole-aware hatching).

    // ---- constructors ------------------------------------------------------

    Layer() noexcept
        : layerNumber(0), layerHeight(0.0f), layerThickness(0.0f) {}

    explicit Layer(uint32_t number, float height, float thickness) noexcept
        : layerNumber(number), layerHeight(height), layerThickness(thickness) {}

    // ---- capacity helpers --------------------------------------------------

    void reserveHatches(size_t n)    { hatches.reserve(n);    }
    void reservePolylines(size_t n)  { polylines.reserve(n);  }
    void reservePolygons(size_t n)   { polygons.reserve(n);   }
    void reserveCircles(size_t n)    { circles.reserve(n);    }
    void reserveExPolygons(size_t n) { exPolygons.reserve(n); }

    // ---- query helpers -----------------------------------------------------

    [[nodiscard]] bool isEmpty() const noexcept {
        return hatches.empty() && polylines.empty() &&
               polygons.empty() && circles.empty() &&
               exPolygons.empty();
    }

    [[nodiscard]] size_t geometryCount() const noexcept {
        return hatches.size() + polylines.size() +
               polygons.size() + circles.size() +
               exPolygons.size();
    }
};

} // namespace Marc
