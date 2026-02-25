// ==============================================================================
// MarcSLM - Layer Polygon Extractor
// ==============================================================================
// Converts Marc::Layer geometry to Clipper2 Paths64 and back.
// Thread-safe: all methods are const or static with no shared mutable state.
// ==============================================================================

#pragma once

#include "MarcSLM/Core/MarcFormat.hpp"
#include "MarcSLM/Thermal/ClipperBoolean.hpp"
#include "MarcSLM/Thermal/ThermalSegmentTypes.hpp"

#include <cmath>
#include <vector>

namespace MarcSLM {
namespace Thermal {

/// @brief Converts between Marc geometry and Clipper2 Paths64.
/// @details All coordinate conversion uses a configurable scale factor.
///          The class is stateless and fully thread-safe.
class LayerPolygonExtractor {
public:
    /// @param clipperScale  mm -> Clipper2 int64 scale factor (e.g. 1e4).
    explicit LayerPolygonExtractor(double clipperScale) noexcept
        : scale_(clipperScale) {}

    // ---- Marc -> Clipper2 -------------------------------------------------

    /// @brief Extract the solid-area polygon set from a Marc::Layer.
    /// @details Converts all ExPolygons (contour + holes), polygons, and closed
    ///          polylines to Clipper2 Paths64, then performs a Union via PolyTree
    ///          to produce clean topology.  The result includes both outer
    ///          contour paths (CCW) and hole paths (CW) as a flat Paths64, so
    ///          that downstream clipping with FillRule::NonZero correctly
    ///          excludes holes from scan vectors.
    [[nodiscard]] Clipper2Lib::Paths64
    extract(const Marc::Layer& layer) const
    {
        Clipper2Lib::Paths64 rawPaths;
        rawPaths.reserve(layer.exPolygons.size() * 2 +
                         layer.polygons.size() +
                         layer.polylines.size());

        // ---- Preferred: ExPolygons with explicit hole topology ----
        if (!layer.exPolygons.empty()) {
            for (const auto& exPoly : layer.exPolygons) {
                if (exPoly.contour.points.size() < 3) continue;

                // Outer contour
                Clipper2Lib::Path64 contourPath;
                contourPath.reserve(exPoly.contour.points.size());
                for (const auto& pt : exPoly.contour.points) {
                    contourPath.emplace_back(toClip(pt.x), toClip(pt.y));
                }
                rawPaths.push_back(std::move(contourPath));

                // Holes (keep as separate paths — CW winding)
                for (const auto& hole : exPoly.holes) {
                    if (hole.points.size() < 3) continue;
                    Clipper2Lib::Path64 holePath;
                    holePath.reserve(hole.points.size());
                    for (const auto& pt : hole.points) {
                        holePath.emplace_back(toClip(pt.x), toClip(pt.y));
                    }
                    rawPaths.push_back(std::move(holePath));
                }
            }
        }

        // ---- Fallback: standalone polygons (no hole info) ----
        for (const auto& polygon : layer.polygons) {
            if (polygon.points.size() < 3) continue;
            Clipper2Lib::Path64 path;
            path.reserve(polygon.points.size());
            for (const auto& pt : polygon.points) {
                path.emplace_back(toClip(pt.x), toClip(pt.y));
            }
            rawPaths.push_back(std::move(path));
        }

        // ---- Fallback: closed polylines (only when no ExPolygons present) ----
        if (layer.exPolygons.empty()) {
            for (const auto& polyline : layer.polylines) {
                if (polyline.points.size() < 3) continue;
                Clipper2Lib::Path64 path;
                path.reserve(polyline.points.size());
                for (const auto& pt : polyline.points) {
                    path.emplace_back(toClip(pt.x), toClip(pt.y));
                }
                rawPaths.push_back(std::move(path));
            }
        }

        if (rawPaths.empty()) return {};

        // ---- Union via PolyTree to preserve hole topology ----
        //
        // The flat Clipper2::Union() output discards holes — only outer
        // contours appear in Paths64.  We must use the PolyTree overload
        // to retrieve holes (level-2 nodes), then flatten back into a
        // Paths64 that includes them.  Downstream NonZero clipping then
        // correctly treats CW hole paths as void regions.
        Clipper2Lib::Clipper64 clipper;
        clipper.AddSubject(rawPaths);
        Clipper2Lib::PolyTree64 polyTree;
        Clipper2Lib::Paths64 openSolution;
        clipper.Execute(Clipper2Lib::ClipType::Union,
                        Clipper2Lib::FillRule::NonZero,
                        polyTree, openSolution);

        // Flatten PolyTree into a Paths64 that contains both contour and
        // hole paths so that NonZero clipping downstream works correctly.
        Clipper2Lib::Paths64 result;
        std::function<void(const Clipper2Lib::PolyPath64&)> flattenNode;
        flattenNode = [&result, &flattenNode](const Clipper2Lib::PolyPath64& node) {
            if (!node.Polygon().empty()) {
                result.push_back(node.Polygon());
            }
            for (const auto& child : node) {
                flattenNode(*child);
            }
        };
        flattenNode(polyTree);

        return result;
    }

    // ---- Clipper2 -> Marc (polyline contours) -----------------------------

    /// @brief Convert Clipper2 Paths64 to a ScanSegmentPolyline.
    /// @details Each path becomes a closed Marc::Polyline (first point repeated).
    void toPolylines(const Clipper2Lib::Paths64& paths,
                     ThermalSegmentType type,
                     uint32_t layerNumber,
                     std::vector<ScanSegmentPolyline>& out) const
    {
        if (paths.empty()) return;

        ScanSegmentPolyline seg;
        seg.type = type;

        for (const auto& path : paths) {
            if (path.size() < 2) continue;
            Marc::Polyline pl;
            pl.points.reserve(path.size() + 1);
            for (const auto& pt : path) {
                pl.points.emplace_back(fromClip(pt.x), fromClip(pt.y));
            }
            // Close the polyline (Clipper2 paths are implicitly closed)
            pl.points.emplace_back(fromClip(path.front().x),
                                   fromClip(path.front().y));
            pl.tag.type = Marc::GeometryType::Perimeter;
            pl.tag.layerNumber = layerNumber;
            seg.polylines.push_back(std::move(pl));
        }

        if (!seg.polylines.empty()) {
            out.push_back(std::move(seg));
        }
    }

    // ---- Clipper2 -> Marc (hatch boundary lines) --------------------------

    /// @brief Convert Clipper2 Paths64 to a ScanSegmentHatch (boundary edges).
    void toHatches(const Clipper2Lib::Paths64& paths,
                   ThermalSegmentType type,
                   uint32_t layerNumber,
                   std::vector<ScanSegmentHatch>& out) const
    {
        if (paths.empty()) return;

        ScanSegmentHatch seg;
        seg.type = type;

        for (const auto& path : paths) {
            if (path.size() < 2) continue;
            for (size_t i = 0; i < path.size(); ++i) {
                const size_t next = (i + 1) % path.size();
                seg.hatches.emplace_back(
                    fromClip(path[i].x),    fromClip(path[i].y),
                    fromClip(path[next].x), fromClip(path[next].y));
            }
        }

        if (!seg.hatches.empty()) {
            out.push_back(std::move(seg));
        }
    }

    // ---- Coordinate helpers -----------------------------------------------

    [[nodiscard]] int64_t toClip(double mm) const noexcept {
        return static_cast<int64_t>(std::round(mm * scale_));
    }

    [[nodiscard]] float fromClip(int64_t v) const noexcept {
        return static_cast<float>(static_cast<double>(v) / scale_);
    }

    [[nodiscard]] double scaledMm(double mm) const noexcept {
        return mm * scale_;
    }

private:
    double scale_;
};

} // namespace Thermal
} // namespace MarcSLM
