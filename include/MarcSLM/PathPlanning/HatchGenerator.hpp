// ==============================================================================
// MarcSLM - Hatch Generator for SLM Infill
// ==============================================================================
// Ported from Legacy slmHatches.hpp/cpp + FillRectilinear + FillSLMisland
// Generates parallel hatch scan lines within slice contours using Clipper2
// ==============================================================================

#pragma once

#include "MarcSLM/Core/MarcFormat.hpp"
#include "MarcSLM/Core/SlmConfig.hpp"
#include "MarcSLM/Core/Types.hpp"

#include <clipper2/clipper.h>

#include <vector>

namespace MarcSLM {
namespace PathPlanning {

/// @brief Generates hatch (infill) scan lines for SLM layer regions.
/// @details Produces parallel scan vectors at configurable spacing and
///          angle within the boundaries defined by slice contours.
///          Uses Clipper2 for robust line-polygon intersection.
///
///          Hatch generation algorithm:
///          1. Compute bounding box of the input polygon
///          2. Generate a family of parallel lines at hatch_spacing
///          3. Rotate lines by hatch_angle
///          4. Clip lines to the polygon boundary using Clipper2
///          5. Optionally extend line endpoints for overlap
///          6. Optionally connect adjacent lines into polylines
///          7. Return clipped line segments as Marc::Line vectors
///
///          Ported from Legacy Marc::slmHatches + Slic3r FillRectilinear
///          + FillSLMisland logic.
class HatchGenerator {
public:
    /// @brief Construct with SLM configuration.
    explicit HatchGenerator(const SlmConfig& config);

    /// @brief Generate hatch lines for a closed polygon contour.
    /// @param contour Outer boundary (Clipper2 Path64, CCW).
    /// @param holes   Inner holes (Clipper2 Paths64, CW each).
    /// @param angle   Hatch angle override [degrees]. Use -1 for config default.
    /// @return Vector of Marc::Line hatch segments.
    [[nodiscard]] std::vector<Marc::Line>
    generateHatches(const Clipper2Lib::Path64& contour,
                    const Clipper2Lib::Paths64& holes = {},
                    double angle = -1.0) const;

    /// @brief Generate hatch lines for a Marc::Polygon.
    /// @param polygon Input polygon in mm coordinates.
    /// @param angle   Hatch angle override [degrees]. Use -1 for config default.
    /// @return Vector of Marc::Line hatch segments.
    [[nodiscard]] std::vector<Marc::Line>
    generateHatches(const Marc::Polygon& polygon,
                    double angle = -1.0) const;

    /// @brief Generate hatch lines for a MarcSLM::Core::Slice (contour + holes).
    /// @param slice Input slice with Clipper2 contour and holes.
    /// @param angle Hatch angle override [degrees]. Use -1 for config default.
    /// @return Vector of Marc::Line hatch segments.
    [[nodiscard]] std::vector<Marc::Line>
    generateHatches(const Clipper2Lib::Path64& contour,
                    const Clipper2Lib::Paths64& holes,
                    double angle,
                    bool useIslands) const;

    /// @brief Generate island/checkerboard hatches for a contour.
    /// @details Splits the contour into island_width x island_height cells
    ///          and hatches each cell alternately at 0° and 90°.
    ///          Each cell is clipped first to its island rectangle, then
    ///          to the overall contour boundary (matching Legacy FillSLMisland).
    /// @param contour Outer boundary (Clipper2 Path64, CCW).
    /// @param holes   Inner holes (Clipper2 Paths64, CW each).
    /// @return Vector of Marc::Line hatch segments.
    [[nodiscard]] std::vector<Marc::Line>
    generateIslandHatches(const Clipper2Lib::Path64& contour,
                          const Clipper2Lib::Paths64& holes = {}) const;

    /// @brief Connect adjacent parallel hatch lines into continuous polylines.
    /// @details Reduces laser on/off transitions by linking adjacent lines
    ///          end-to-end where they share the same scan direction boundary.
    ///          Ported from Legacy FillRectilinear connection logic.
    /// @param lines Input disconnected hatch lines.
    /// @return Vector of Marc::Polyline connected scan paths.
    [[nodiscard]] std::vector<Marc::Polyline>
    connectHatchLines(const std::vector<Marc::Line>& lines) const;

    /// @brief Sort hatch lines with alternating scan direction.
    /// @details Alternates left-to-right / right-to-left for even thermal
    ///          distribution across the powder bed.
    /// @param lines Input hatch lines (modified in place).
    void sortAlternating(std::vector<Marc::Line>& lines) const;

    /// @brief Compute the hatch angle for a given layer index.
    /// @details Standard SLM practice: rotate by 67° per layer.
    /// @param layerIndex Zero-based layer index.
    /// @return Effective hatch angle in degrees.
    [[nodiscard]] double layerAngle(size_t layerIndex) const;

    /// @brief Set the hatch spacing.
    void setHatchSpacing(double spacing) { hatchSpacing_ = spacing; }

    /// @brief Set the hatch angle.
    void setHatchAngle(double angleDeg) { hatchAngle_ = angleDeg; }

    /// @brief Set endpoint overlap extension [mm].
    /// @details Lines are extended at each end by this amount to avoid
    ///          gaps between hatches and perimeters. Matches Legacy
    ///          Fill::endpoints_overlap logic.
    void setEndpointOverlap(double overlapMm) { endpointOverlap_ = overlapMm; }

    /// @brief Set the hatch rotation increment per layer [degrees].
    void setLayerRotation(double rotDeg) { layerRotation_ = rotDeg; }

private:
    double hatchSpacing_;     ///< Hatch line spacing [mm]
    double hatchAngle_;       ///< Base hatch angle [degrees]
    double islandWidth_;      ///< Island width [mm]
    double islandHeight_;     ///< Island height [mm]
    double endpointOverlap_;  ///< Endpoint extension [mm] (default 0.0)
    double layerRotation_;    ///< Per-layer angle increment [degrees] (default 67°)

    /// @brief Clip a set of infinite lines against a polygon boundary.
    [[nodiscard]] std::vector<Marc::Line>
    clipLinesToPolygon(const std::vector<Marc::Line>& lines,
                      const Clipper2Lib::Path64& contour,
                      const Clipper2Lib::Paths64& holes) const;

    /// @brief Generate a family of parallel lines covering a bounding box.
    [[nodiscard]] std::vector<Marc::Line>
    generateParallelLines(double xMin, double yMin, double xMax, double yMax,
                         double spacing, double angleDeg) const;

    /// @brief Build a Clipper2 rectangular path for island cell clipping.
    [[nodiscard]] Clipper2Lib::Path64
    makeRectanglePath(double xMin, double yMin, double xMax, double yMax) const;

    /// @brief Apply endpoint overlap extension to clipped lines.
    /// @details Extends each line segment along its direction by
    ///          endpointOverlap_ at both ends.
    void applyEndpointOverlap(std::vector<Marc::Line>& lines) const;
};

} // namespace PathPlanning
} // namespace MarcSLM
