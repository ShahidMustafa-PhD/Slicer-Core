// ==============================================================================
// MarcSLM - SVG Exporter for Marc Geometry
// ==============================================================================
// Produces SVG files in native millimetre units.
//
// Coordinate conventions
// ----------------------
//  * All geometry is in millimetres (mm).
//  * 1 SVG unit == 1 mm  (no pixel scaling).
 //  * width/height attrs use "mm" units; viewBox matches exactly.
//  * At 1:1 the canvas shows the full build plate.
//  * At 10x zoom individual hatch scan vectors (0.04 mm wide) are visible.
//  * A white rectangle with a dashed grey border marks the bed boundary.
//
// Thermal Segment Visualisation
// ------------------------------
//  * draw(ScanSegmentHatch)   — renders hatch lines in the segment colour.
//  * draw(ScanSegmentPolyline)— renders contour polylines in segment colour.
//  * draw_thermal_legend()    — writes a colour-coded legend panel for all
//                               22 ThermalSegmentType values into the SVG.
// ==============================================================================

#pragma once

#include "MarcSLM/Core/MarcFormat.hpp"
#include "MarcSLM/Thermal/ThermalSegmentTypes.hpp"

#include <string>
#include <sstream>
#include <cstdio>
#include <vector>

namespace MarcSLM {

/// @brief SVG file writer for Marc geometry primitives.
/// @details All coordinates and stroke widths are in millimetres.
///          1 SVG unit = 1 mm. Zoom to ~700x in any viewer to see
///          individual scan vectors.
class SVGExporter {
public:
    // ---- State -------------------------------------------------------------
    bool        arrows   = false;   ///< Draw arrow markers on line ends
    std::string fill     = "grey";  ///< Default fill colour
    std::string stroke   = "black"; ///< Default stroke colour
    bool        flipY    = false;   ///< (reserved - not used)
    std::string filename;           ///< Output file path
    Marc::Point origin;             ///< Coordinate origin offset [mm]

    // ---- Construction ------------------------------------------------------

    /// @brief Open an SVG with a default 120x120 mm bed, 5 mm margin.
    explicit SVGExporter(const char* afilename);

    /// @brief Open an SVG sized to the given build-plate dimensions [mm].
    SVGExporter(const char* afilename,
                float bedWidth, float bedDepth,
                float margin = 5.0f);

    explicit SVGExporter(const std::string& afilename)
        : SVGExporter(afilename.c_str()) {}

    SVGExporter(const std::string& afilename,
                float bedWidth, float bedDepth,
                float margin = 5.0f)
        : SVGExporter(afilename.c_str(), bedWidth, bedDepth, margin) {}

    ~SVGExporter();

    SVGExporter(const SVGExporter&)            = delete;
    SVGExporter& operator=(const SVGExporter&) = delete;

    // ---- Standard geometry draw primitives (units: mm) --------------------

    void draw(const Marc::Line& line,
              const std::string& color = "green", float stroke_width = 0.0f);
    void draw(const std::vector<Marc::Line>& lines,
              const std::string& color = "green", float stroke_width = 0.0f);

    void draw(const Marc::Polyline& polyline,
              const std::string& color = "black", float stroke_width = 0.0f);
    void draw(const std::vector<Marc::Polyline>& polylines,
              const std::string& color = "black", float stroke_width = 0.0f);

    void draw(const Marc::Polygon& polygon,
              const std::string& color = "red", float stroke_width = 0.0f);
    void draw(const std::vector<Marc::Polygon>& polygons,
              const std::string& color = "red", float stroke_width = 0.0f);

    void draw(const Marc::Point& point,
              const std::string& fillColor = "black", float radius = 0.0f);
    void draw(const std::vector<Marc::Point>& points,
              const std::string& fillColor = "black", float radius = 0.0f);

    // ---- Thermal segment draw primitives ----------------------------------

    /// @brief Draw a classified hatch segment using its thermal segment colour.
    /// @param seg    The classified hatch group.
    /// @param stroke_width Stroke width in mm (0 = default 0.04 mm).
    void draw(const ScanSegmentHatch& seg, float stroke_width = 0.0f);

    /// @brief Draw a vector of classified hatch segments.
    void draw(const std::vector<ScanSegmentHatch>& segs, float stroke_width = 0.0f);

    /// @brief Draw a classified polyline segment using its thermal segment colour.
    void draw(const ScanSegmentPolyline& seg, float stroke_width = 0.0f);

    /// @brief Draw a vector of classified polyline segments.
    void draw(const std::vector<ScanSegmentPolyline>& segs, float stroke_width = 0.0f);

    // ---- Legend -----------------------------------------------------------

    /// @brief Write a full 22-entry thermal-segment colour legend into the SVG.
    /// @details The legend is drawn in the right margin (outside the bed) so
    ///          it never obscures the scan geometry.  Each row shows a colour
    ///          swatch, the segment ID number, and the human-readable name.
    ///          The legend is sized in mm so it scales correctly at any zoom.
    void draw_thermal_legend();

    // ---- Text / annotation ------------------------------------------------

    void draw_text(const Marc::Point& pt, const char* text, const char* color);
    void draw_legend(const Marc::Point& pt, const char* text, const char* color);

    void layer_section_start(const char* text);
    void layer_section_end();

    /// @brief Finalize and close the SVG file.
    void Close();

private:
    FILE*  f         = nullptr;
    float  bedWidth_ = 120.0f;
    float  bedDepth_ = 120.0f;
    float  margin_   = 5.0f;

    bool open(const char* afilename);
    bool open(const char* afilename,
              float bedWidth, float bedDepth, float margin);

    void path(const std::string& d, bool filled, float stroke_width,
              float fill_opacity);

    [[nodiscard]] std::string
    get_path_d(const std::vector<Marc::Point>& pts, bool closed = false) const;
};

} // namespace MarcSLM
