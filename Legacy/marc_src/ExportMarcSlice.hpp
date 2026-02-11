#pragma once
#include "../libslic3r/ExPolygon.hpp"
#include "../libslic3r/Line.hpp"
#include "../libslic3r/Surface.hpp"
#include <cstdint>
#include <ctime>
#include <vector>

namespace Marc {
// -------------------------------------------
// Custom Binary File Format: .marc
// Used in L-PBF (Laser Powder Bed Fusion) 3D Metal Printer
// -------------------------------------------

// -------------------------------
// Type-Safe Enumerations (C++17)
// -------------------------------

/// @brief Geometry category classification for layer elements
enum class GeometryCategory : uint32_t {
    Hatch = 1,      ///< Laser scan path segments
    Polyline = 2,   ///< Open connected point sequences
    Polygon = 3,    ///< Closed boundary contours
    Point = 4       ///< Discrete points (e.g., support markers)
};

/// @brief Geometry type/role classification (aligned with Slic3r ExtrusionRole)
enum class GeometryType : uint32_t {
    Undefined = 0,
    CoreHatch = 1,
    OverhangHatch = 2,
    Perimeter = 3,
    SupportStructure = 4,
    InfillPattern = 5
    // Extensible to 15 types (0-15 range preserved for binary format)
};

// -------------------------------
// File Header Structure (160 bytes)
// -------------------------------
struct MarcHeader {
    char magic[4] = {'M','A','R','C'};         // 4 bytes: File identifier
    uint32_t version = 1;                      // 4 bytes: Version number of format
    uint32_t totalLayers = 0;                  // 4 bytes: Total number of layers
    uint64_t indexTableOffset = 0;             // 8 bytes: File offset where index table starts
    uint64_t timestamp = std::time(nullptr);   // 8 bytes: File creation timestamp
    char printerId[32] = {0};                  // 32 bytes: Optional printer identification
    char reserved[100] = {0};                  // 100 bytes: Reserved for future use
};

// -------------------------------
// Geometry Structures (Modern C++17)
// -------------------------------

struct Point {
    float x = 0.0f;  // X-coordinate (explicit initialization)
    float y = 0.0f;  // Y-coordinate
    
    constexpr Point() noexcept = default;
    constexpr Point(float x_, float y_) noexcept : x(x_), y(y_) {}
};

struct Line {
    Point a;  // Start point
    Point b;  // End point
    
    constexpr Line() noexcept = default;
    constexpr Line(Point start, Point end) noexcept : a(start), b(end) {}
};

/// @brief Metadata tag for geometry primitives
struct GeometryTag {
    GeometryType type = GeometryType::Undefined;       // Semantic type
    GeometryCategory category = GeometryCategory::Point; // Structural category
    uint32_t pointCount = 0;                           // Total points in geometry
    
    constexpr GeometryTag() noexcept = default;
    constexpr GeometryTag(GeometryType t, GeometryCategory c, uint32_t count) noexcept
        : type(t), category(c), pointCount(count) {}
    
    // Helper for binary serialization (maintains backward compatibility)
    [[nodiscard]] constexpr uint32_t typeAsUint() const noexcept {
        return static_cast<uint32_t>(type);
    }
    [[nodiscard]] constexpr uint32_t categoryAsUint() const noexcept {
        return static_cast<uint32_t>(category);
    }
};

struct Circle {
    GeometryTag tag;
    Point center;
    float radius = 0.0f;
    
    Circle() noexcept = default;
    explicit Circle(Point c, float r) noexcept 
        : tag(GeometryType::SupportStructure, GeometryCategory::Point, 1)
        , center(c)
        , radius(r) {}
};

struct Hatch {
    GeometryTag tag;
    std::vector<Line> lines;
    
    Hatch() noexcept = default;
    explicit Hatch(GeometryType type) noexcept 
        : tag(type, GeometryCategory::Hatch, 0) {}
};

struct Polyline {
    GeometryTag tag;
    std::vector<Point> points;
    
    Polyline() noexcept = default;
    explicit Polyline(GeometryType type) noexcept
        : tag(type, GeometryCategory::Polyline, 0) {}
};

struct Polygon {
    GeometryTag tag;
    std::vector<Point> points;
    
    Polygon() noexcept = default;
    explicit Polygon(GeometryType type) noexcept
        : tag(type, GeometryCategory::Polygon, 0) {}
};

// -------------------------------
// Layer Data Block
// -------------------------------
struct Layer {
    uint32_t layerNumber = 0;
    float layerHeight = 0.0f;
    float layerThickness = 0.0f;
    std::vector<Hatch> hatches;
    std::vector<Polyline> polylines;
    std::vector<Polygon> polygons;
    std::vector<Circle> support_circles;
};

// -------------------------------
// Geometry Converter (Slic3r → Marc)
// -------------------------------

/// @brief Converts Slic3r geometry types to Marc binary format structures
/// @note Non-owning observer of data; does not manage Slic3r object lifetimes
/// @note Thread-safe for read operations (all conversions are const)
class ExportMarcSlice {
public:
    // Constructors
    ExportMarcSlice() noexcept : origin_{0.0f, 0.0f} {}
    explicit ExportMarcSlice(const Point& origin) noexcept : origin_(origin) {}
    
    // Rule of Zero: Compiler-generated special members are correct
    ~ExportMarcSlice() = default;
    ExportMarcSlice(const ExportMarcSlice&) = default;
    ExportMarcSlice& operator=(const ExportMarcSlice&) = default;
    ExportMarcSlice(ExportMarcSlice&&) noexcept = default;
    ExportMarcSlice& operator=(ExportMarcSlice&&) noexcept = default;
    
    /// @brief Get the current coordinate system origin (read-only)
    [[nodiscard]] const Point& origin() const noexcept { return origin_; }
    
    // Conversion methods (now all const-correct)
    void convrt(const Slic3r::Line& line, Line& m_line) const;
    void convrt(const Slic3r::Lines& lines, std::vector<Line>& m_lines) const;
    void convrt(const Slic3r::ExPolygon& expolygon, std::vector<Polygon>& m_polygons) const;
    void convrt(const Slic3r::ExPolygons& expolygons, std::vector<Polygon>& m_polygons) const;
    void convrt(const Slic3r::Surface& surface, std::vector<Polygon>& m_polygons) const;
    void convrt(const Slic3r::Surfaces& surfaces, std::vector<Polygon>& m_polygons) const;
    void convrt(const Slic3r::Polygon& polygon, Polygon& m_polygon) const;
    void convrt(const Slic3r::MultiPoint& mp, Polyline& m_polyline) const;
    void convrt(const Slic3r::Polygons& polygons, std::vector<Polygon>& m_polygons) const;
    void convrt(const Slic3r::Polyline& polyline, Polyline& m_polyline) const;
    void convrt(const Slic3r::Polylines& polylines, std::vector<Polyline>& m_polylines) const;
    void convrt(const Slic3r::Point& point, Point& m_point) const;
    void convrt(const Slic3r::Points& points, std::vector<Point>& m_points) const;
    
    void Close();  // Finalization hook (currently unused, reserved for future use)

private:
    const Point origin_;  // Immutable coordinate system origin
};

} // namespace Marc