// ==============================================================================
// MarcSLM - Core Primitive Types
// ==============================================================================
// Copyright (c) 2024 MarcSLM Project
// Licensed under a commercial-friendly license (non-AGPL)
// ==============================================================================

#pragma once

#include <clipper2/clipper.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>
#include <memory>

namespace MarcSLM {
namespace Core {

// ==============================================================================
// Precision and Unit Definitions
// ==============================================================================

/// @brief Internal coordinate precision multiplier for Clipper2
/// @details Clipper2 uses 64-bit integer coordinates. We scale floating-point
///          coordinates by this factor to maintain sub-micron precision.
///          1e6 provides nanometer-level precision (1 unit = 1 nm).
constexpr double CLIPPER_SCALE = 1e6;

/// @brief Convert millimeters to Clipper2 internal integer coordinates
/// @param mm Coordinate value in millimeters
/// @return Scaled integer coordinate suitable for Clipper2
inline int64_t mmToClipperUnits(double mm) noexcept {
    return static_cast<int64_t>(mm * CLIPPER_SCALE);
}

/// @brief Convert Clipper2 internal integer coordinates to millimeters
/// @param units Clipper2 integer coordinate
/// @return Coordinate value in millimeters
inline double clipperUnitsToMm(int64_t units) noexcept {
    return static_cast<double>(units) / CLIPPER_SCALE;
}

// ==============================================================================
// 2D Point Primitives
// ==============================================================================

/// @brief 2D point in floating-point coordinates (millimeters)
using Point2D = glm::dvec2;

/// @brief 2D point in integer Clipper2 coordinates
using Point2DInt = Clipper2Lib::Point64;

/// @brief Convert floating-point 2D point to Clipper2 integer coordinates
/// @param pt Point in millimeters
/// @return Point in Clipper2 integer units
inline Point2DInt toClipperPoint(const Point2D& pt) noexcept {
    return Point2DInt(mmToClipperUnits(pt.x), mmToClipperUnits(pt.y));
}

/// @brief Convert Clipper2 integer coordinates to floating-point 2D point
/// @param pt Point in Clipper2 integer units
/// @return Point in millimeters
inline Point2D fromClipperPoint(const Point2DInt& pt) noexcept {
    return Point2D(clipperUnitsToMm(pt.x), clipperUnitsToMm(pt.y));
}

// ==============================================================================
// 3D Point Primitives
// ==============================================================================

/// @brief 3D point in floating-point coordinates (millimeters)
using Point3D = glm::dvec3;

/// @brief 3D vector for normals and directions
using Vector3D = glm::dvec3;

// ==============================================================================
// Slice Geometry Data Structure
// ==============================================================================

/// @brief Represents a single 2D slice layer with outer contour and internal holes
/// @details This structure is the fundamental unit of the slicing pipeline.
///          It uses Clipper2's high-performance integer coordinate system for
///          robust boolean operations and offset calculations.
///          
///          Design considerations:
///          - Zero-copy move semantics for pipeline efficiency
///          - Direct Clipper2 integration for topology operations
///          - Separates outer boundary from internal voids (holes)
///          - Thread-safe when accessed immutably
struct Slice {
    // ==========================================================================
    // Data Members
    // ==========================================================================
    
    /// @brief Z-height of this slice in millimeters (layer position)
    double zHeight;
    
    /// @brief Outer boundary contour (single closed path)
    /// @details Oriented counter-clockwise (CCW) by convention.
    ///          This represents the outermost perimeter of the slice.
    Clipper2Lib::Path64 outerContour;
    
    /// @brief Internal holes (voids within the slice)
    /// @details Each path is oriented clockwise (CW) by convention.
    ///          Empty if the slice is solid (no internal voids).
    Clipper2Lib::Paths64 holes;
    
    /// @brief Layer index (0-based, incrementing from build plate)
    uint32_t layerIndex;
    
    /// @brief Optional metadata identifier (e.g., part ID, region ID)
    uint32_t partID;
    
    // ==========================================================================
    // Constructors
    // ==========================================================================
    
    /// @brief Default constructor - creates an empty slice at z=0
    Slice() noexcept
        : zHeight(0.0)
        , layerIndex(0)
        , partID(0) {
    }
    
    /// @brief Construct a slice with specified z-height and layer index
    /// @param z Z-height in millimeters
    /// @param layer Layer index (0-based)
    explicit Slice(double z, uint32_t layer = 0) noexcept
        : zHeight(z)
        , layerIndex(layer)
        , partID(0) {
    }
    
    /// @brief Move constructor for zero-copy pipeline processing
    Slice(Slice&& other) noexcept
        : zHeight(other.zHeight)
        , outerContour(std::move(other.outerContour))
        , holes(std::move(other.holes))
        , layerIndex(other.layerIndex)
        , partID(other.partID) {
    }
    
    /// @brief Move assignment operator
    Slice& operator=(Slice&& other) noexcept {
        if (this != &other) {
            zHeight = other.zHeight;
            outerContour = std::move(other.outerContour);
            holes = std::move(other.holes);
            layerIndex = other.layerIndex;
            partID = other.partID;
        }
        return *this;
    }
    
    // Delete copy operations to enforce move semantics
    Slice(const Slice&) = delete;
    Slice& operator=(const Slice&) = delete;
    
    // ==========================================================================
    // Query Methods
    // ==========================================================================
    
    /// @brief Check if the slice has valid geometry
    /// @return true if the outer contour has at least 3 points
    [[nodiscard]] bool isValid() const noexcept {
        return outerContour.size() >= 3;
    }
    
    /// @brief Check if the slice contains internal holes
    /// @return true if there are any holes
    [[nodiscard]] bool hasHoles() const noexcept {
        return !holes.empty();
    }
    
    /// @brief Get the number of internal holes
    /// @return Number of hole paths
    [[nodiscard]] size_t holeCount() const noexcept {
        return holes.size();
    }
    
    /// @brief Compute the total number of vertices in this slice
    /// @return Sum of vertices in outer contour and all holes
    [[nodiscard]] size_t vertexCount() const noexcept {
        size_t count = outerContour.size();
        for (const auto& hole : holes) {
            count += hole.size();
        }
        return count;
    }
    
    /// @brief Reserve memory for the outer contour
    /// @param count Expected number of vertices
    void reserveOuter(size_t count) {
        outerContour.reserve(count);
    }
    
    /// @brief Reserve memory for holes
    /// @param count Expected number of hole paths
    void reserveHoles(size_t count) {
        holes.reserve(count);
    }
};

// ==============================================================================
// Slice Collection
// ==============================================================================

/// @brief Collection of slices representing a complete sliced model
using SliceStack = std::vector<Slice>;

/// @brief Smart pointer to a slice for optional ownership transfer
using SlicePtr = std::unique_ptr<Slice>;

} // namespace Core
} // namespace MarcSLM
