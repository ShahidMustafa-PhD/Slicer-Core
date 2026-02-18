// ==============================================================================
// MarcSLM - SVG Exporter for Marc Geometry
// ==============================================================================
// Ported from Legacy marcToSVG.hpp/cpp
// Writes Marc::Point/Line/Polyline/Polygon to SVG files for visualization
// ==============================================================================

#pragma once

#include "MarcSLM/Core/MarcFormat.hpp"

#include <string>
#include <sstream>
#include <cstdio>
#include <vector>

namespace MarcSLM {

/// @brief SVG file writer for Marc geometry primitives.
/// @details Produces SVG output for visual inspection of slice layers.
///          Supports lines, polylines, polygons, points, and text.
///          Ported from Legacy Marc::marcToSVG.
class SVGExporter {
public:
    bool        arrows = false;   ///< Draw arrow markers on line ends
    std::string fill   = "grey";  ///< Default fill colour
    std::string stroke = "black"; ///< Default stroke colour
    bool        flipY  = false;   ///< Flip Y axis for screen coordinates
    std::string filename;         ///< Output file path
    Marc::Point origin;           ///< Coordinate origin offset

    /// @brief Construct and open an SVG file for writing.
    explicit SVGExporter(const char* afilename);

    /// @brief Construct with std::string filename.
    explicit SVGExporter(const std::string& afilename)
        : SVGExporter(afilename.c_str()) {}

    /// @brief Destructor — closes the SVG file if still open.
    ~SVGExporter();

    SVGExporter(const SVGExporter&) = delete;
    SVGExporter& operator=(const SVGExporter&) = delete;

    // ---- Drawing primitives ------------------------------------------------

    void draw(const Marc::Line& line,
              const std::string& color = "green", float stroke_width = 0.1f);
    void draw(const std::vector<Marc::Line>& lines,
              const std::string& color = "green", float stroke_width = 0.1f);

    void draw(const Marc::Polyline& polyline,
              const std::string& color = "black", float stroke_width = 0.1f);
    void draw(const std::vector<Marc::Polyline>& polylines,
              const std::string& color = "black", float stroke_width = 0.1f);

    void draw(const Marc::Polygon& polygon,
              const std::string& color = "red", float stroke_width = 0.1f);
    void draw(const std::vector<Marc::Polygon>& polygons,
              const std::string& color = "red", float stroke_width = 0.1f);

    void draw(const Marc::Point& point,
              const std::string& fillColor = "black", float radius = 0.0f);
    void draw(const std::vector<Marc::Point>& points,
              const std::string& fillColor = "black", float radius = 0.0f);

    void draw_text(const Marc::Point& pt, const char* text, const char* color);
    void draw_legend(const Marc::Point& pt, const char* text, const char* color);

    void layer_section_start(const char* text);
    void layer_section_end();

    /// @brief Finalize and close the SVG file.
    void Close();

private:
    FILE* f = nullptr;

    bool open(const char* afilename);

    void path(const std::string& d, bool filled, float stroke_width,
              float fill_opacity);

    [[nodiscard]] std::string
    get_path_d(const std::vector<Marc::Point>& pts, bool closed = false) const;
};

} // namespace MarcSLM
