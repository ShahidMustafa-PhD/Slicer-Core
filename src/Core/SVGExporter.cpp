// ==============================================================================
// MarcSLM - SVG Exporter Implementation
// ==============================================================================
// Coordinate system: native millimetres throughout.
//   viewBox="0 0 W H"  where W = bedWidth+2*margin, H = bedDepth+2*margin  [mm]
//   width/height set to viewBox * 10 so viewers open at 10x physical scale.
//
// Thermal legend
// --------------
// draw_thermal_legend() renders a colour-coded panel in the right margin of
// every SVG.  The panel lists all 22 ThermalSegmentType values with:
//   - a filled colour swatch rectangle
//   - the segment ID number
//   - the human-readable name
// The panel is drawn in native mm, so it scales correctly at any zoom level.
// ==============================================================================

#include "MarcSLM/Core/SVGExporter.hpp"

#include <cmath>
#include <iostream>

namespace MarcSLM {

// ===========================================================================
// Thermal segment colour table (22 entries, one per ThermalSegmentType)
// ===========================================================================
// Each entry: { ThermalSegmentType, hex colour string, display name }
// Colours chosen for maximum visual discrimination on white background.
// ===========================================================================

namespace {

struct ThermalColourEntry {
    ThermalSegmentType type;
    const char*        hexColor;   ///< #rrggbb
    const char*        label;      ///< Short display name
};

// clang-format off
static constexpr ThermalColourEntry kThermalColors[] = {
    { ThermalSegmentType::CoreContour_Volume,               "#000000", "CoreContour_Volume"              },
    { ThermalSegmentType::CoreContour_Overhang,             "#8B0000", "CoreContour_Overhang"            },
    { ThermalSegmentType::HollowShell1Contour_Volume,       "#0000CC", "Shell1Contour_Volume"            },
    { ThermalSegmentType::HollowShell1Contour_Overhang,     "#000080", "Shell1Contour_Overhang"          },
    { ThermalSegmentType::HollowShell2Contour_Volume,       "#B8860B", "Shell2Contour_Volume"            },
    { ThermalSegmentType::HollowShell2Contour_Overhang,     "#006400", "Shell2Contour_Overhang"          },
    { ThermalSegmentType::CoreOverhangHatch,                "#FF8C00", "CoreOverhangHatch"               },
    { ThermalSegmentType::CoreNormalHatch,                  "#228B22", "CoreNormalHatch"                 },
    { ThermalSegmentType::CoreContourHatch,                 "#800080", "CoreContourHatch"                },
    { ThermalSegmentType::HollowShell1OverhangHatch,        "#FF69B4", "Shell1OverhangHatch"             },
    { ThermalSegmentType::HollowShell1NormalHatch,          "#00CED1", "Shell1NormalHatch"               },
    { ThermalSegmentType::HollowShell1ContourHatch,         "#FF00FF", "Shell1ContourHatch"              },
    { ThermalSegmentType::HollowShell2OverhangHatch,        "#A52A2A", "Shell2OverhangHatch"             },
    { ThermalSegmentType::HollowShell2NormalHatch,          "#808080", "Shell2NormalHatch"               },
    { ThermalSegmentType::HollowShell2ContourHatch,         "#32CD32", "Shell2ContourHatch"              },
    { ThermalSegmentType::SupportContourVolume,             "#1A237E", "SupportContourVolume"            },
    { ThermalSegmentType::SupportHatch,                     "#827717", "SupportHatch"                    },
    { ThermalSegmentType::PointSequence,                    "#006064", "PointSequence"                   },
    { ThermalSegmentType::ExternalSupports,                 "#B71C1C", "ExternalSupports"                },
    { ThermalSegmentType::CoreContourHatchOverhang,         "#E64A19", "CoreContourHatchOverhang"        },
    { ThermalSegmentType::HollowShell1ContourHatchOverhang, "#311B92", "Shell1ContourHatchOverhang"      },
    { ThermalSegmentType::HollowShell2ContourHatchOverhang, "#880E4F", "Shell2ContourHatchOverhang"      },
};
// clang-format on

static constexpr int kThermalColorCount =
    static_cast<int>(sizeof(kThermalColors) / sizeof(kThermalColors[0]));

/// @brief Look up the hex colour for a ThermalSegmentType.
///        Returns "#888888" for any unmapped type.
const char* thermalHexColor(ThermalSegmentType t) {
    for (int i = 0; i < kThermalColorCount; ++i) {
        if (kThermalColors[i].type == t) return kThermalColors[i].hexColor;
    }
    return "#888888";
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SVGExporter::SVGExporter(const char* afilename) {
    filename = afilename;
    open(afilename, 120.0f, 120.0f, 5.0f);
}

SVGExporter::SVGExporter(const char* afilename,
                         float bedWidth, float bedDepth, float margin)
    : bedWidth_(bedWidth), bedDepth_(bedDepth), margin_(margin)
{
    filename = afilename;
    open(afilename, bedWidth, bedDepth, margin);
}

SVGExporter::~SVGExporter() {
    if (f != nullptr) Close();
}

// ---------------------------------------------------------------------------
// open() - write SVG header
// ---------------------------------------------------------------------------

bool SVGExporter::open(const char* afilename,
                       float bedWidth, float bedDepth, float margin)
{
    bedWidth_ = bedWidth;
    bedDepth_ = bedDepth;
    margin_   = margin;

    // Origin offset: geometry at (0,0) mm lands at (margin, margin) in canvas
    origin.x = -margin;
    origin.y = -margin;

    filename = afilename;
    f = fopen(afilename, "w");
    if (f == nullptr) {
        std::cerr << "SVGExporter: cannot open '" << afilename << "'\n";
        return false;
    }

    // Canvas coordinate space [mm]
    // Extra right margin reserved for the thermal legend panel (40 mm wide)
    const float legendW  = 42.0f;  // legend panel width [mm]
    const float canvasW  = bedWidth  + 2.0f * margin + legendW;
    const float canvasH  = bedDepth  + 2.0f * margin;

    // Bed rectangle top-left inside canvas [mm]
    const float bedX = margin;
    const float bedY = margin;

    // 10x display zoom: width/height = viewBox * 10 (in mm)
    const float displayW = canvasW * 10.0f;
    const float displayH = canvasH * 10.0f;

    // Centre of build plate (for circle + crosshair)
    const float circleCX = bedX + bedWidth  * 0.5f;
    const float circleCY = bedY + bedDepth  * 0.5f;
    const float circleR  = std::min(bedWidth, bedDepth) * 0.5f;

    // ------------------------------------------------------------------
    // SVG root element
    //   width / height  - display size (10x physical scale, in mm)
    //   viewBox         - coordinate space in mm (1 SVG unit = 1 mm)
    // ------------------------------------------------------------------
    fprintf(f,
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\"\n"
        "     xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
        "     width=\"%.4fmm\" height=\"%.4fmm\"\n"
        "     viewBox=\"0 0 %.6f %.6f\">\n"
        "  <!--\n"
        "    Build plate : %.2f x %.2f mm\n"
        "    Coordinate  : 1 SVG unit = 1 mm\n"
        "    Display zoom: 10x  (width/height = viewBox x 10)\n"
        "    Hatch stroke: 0.04 mm = ~28 mm on screen at this zoom\n"
        "    Legend panel: right margin (%.0f mm wide)\n"
        "  -->\n",
        displayW, displayH,
        canvasW,  canvasH,
        bedWidth, bedDepth,
        legendW);

    // Arrow marker (2 mm arrowhead, correct at mm scale)
    fprintf(f,
        "  <defs>\n"
        "    <marker id=\"endArrow\" markerWidth=\"2\" markerHeight=\"1.4\"\n"
        "            refX=\"0\" refY=\"0.7\" orient=\"auto\"\n"
        "            markerUnits=\"strokeWidth\">\n"
        "      <polyline fill=\"darkblue\" points=\"0,0 2,0.7 0,1.4 0.2,0.7\"/>\n"
        "    </marker>\n"
        "  </defs>\n");

    // Full-canvas background (light grey)
    fprintf(f,
        "  <!-- Background -->\n"
        "  <rect x=\"0\" y=\"0\" width=\"%.6f\" height=\"%.6f\""
        " fill=\"#f0f0f0\" stroke=\"none\"/>\n",
        canvasW, canvasH);

    // Build plate area (white)
    fprintf(f,
        "  <!-- Build plate boundary (%.0f x %.0f mm) -->\n"
        "  <rect x=\"%.6f\" y=\"%.6f\" width=\"%.6f\" height=\"%.6f\"\n"
        "        fill=\"#ffffff\" stroke=\"#aaaaaa\""
        " stroke-width=\"0.2\" stroke-dasharray=\"1,0.5\"/>\n",
        bedWidth, bedDepth,
        bedX, bedY, bedWidth, bedDepth);

    // Centre circle (inscribed in bed, dashed grey)
    fprintf(f,
        "  <!-- Build plate centre circle (r = %.2f mm) -->\n"
        "  <circle cx=\"%.6f\" cy=\"%.6f\" r=\"%.6f\"\n"
        "          fill=\"none\" stroke=\"#bbbbbb\" stroke-width=\"0.15\""
        " stroke-dasharray=\"0.8,0.4\"/>\n",
        circleR, circleCX, circleCY, circleR);

    // Centre cross-hair (0.1 mm stroke, light grey)
    fprintf(f,
        "  <!-- Centre cross-hair -->\n"
        "  <line x1=\"%.6f\" y1=\"%.6f\" x2=\"%.6f\" y2=\"%.6f\""
        " stroke=\"#cccccc\" stroke-width=\"0.1\"/>\n"
        "  <line x1=\"%.6f\" y1=\"%.6f\" x2=\"%.6f\" y2=\"%.6f\""
        " stroke=\"#cccccc\" stroke-width=\"0.1\"/>\n",
        bedX,      circleCY, bedX + bedWidth, circleCY,
        circleCX,  bedY,     circleCX,        bedY + bedDepth);

    // Centre dot
    fprintf(f,
        "  <circle cx=\"%.6f\" cy=\"%.6f\" r=\"0.3\""
        " fill=\"#cccccc\" stroke=\"none\"/>\n",
        circleCX, circleCY);

    // Corner dimension labels (2 mm font)
    fprintf(f,
        "  <!-- Corner labels -->\n"
        "  <text x=\"%.6f\" y=\"%.6f\""
        " font-family=\"sans-serif\" font-size=\"2\" fill=\"#888888\">0,0</text>\n"
        "  <text x=\"%.6f\" y=\"%.6f\""
        " font-family=\"sans-serif\" font-size=\"2\" fill=\"#888888\""
        " text-anchor=\"end\">%.0f,0</text>\n"
        "  <text x=\"%.6f\" y=\"%.6f\""
        " font-family=\"sans-serif\" font-size=\"2\" fill=\"#888888\">0,%.0f</text>\n",
        bedX + 0.4f,             bedY - 0.4f,
        bedX + bedWidth - 0.4f,  bedY - 0.4f,  bedWidth,
        bedX + 0.4f,             bedY + bedDepth + 2.2f, bedDepth);

    // Open main geometry group
    fprintf(f,
        "  <!-- Layer geometry -->\n"
        "  <g id=\"geometry\">\n");

    return true;
}

bool SVGExporter::open(const char* afilename) {
    return open(afilename, 120.0f, 120.0f, 5.0f);
}

// ---------------------------------------------------------------------------
// Draw primitives - standard geometry
// ---------------------------------------------------------------------------

void SVGExporter::draw(const Marc::Line& line,
                       const std::string& color, float stroke_width) {
    if (!f) return;
    const float sw = (stroke_width <= 0.0f) ? 0.04f : stroke_width;
    fprintf(f,
        "    <line x1=\"%.5f\" y1=\"%.5f\" x2=\"%.5f\" y2=\"%.5f\""
        " stroke=\"%s\" stroke-width=\"%.5f\"",
        line.a.x - origin.x, line.a.y - origin.y,
        line.b.x - origin.x, line.b.y - origin.y,
        color.c_str(), sw);
    if (arrows) fprintf(f, " marker-end=\"url(#endArrow)\"");
    fprintf(f, "/>\n");
}

void SVGExporter::draw(const std::vector<Marc::Line>& lines,
                       const std::string& color, float stroke_width) {
    for (const auto& l : lines) draw(l, color, stroke_width);
}

void SVGExporter::draw(const Marc::Polyline& polyline,
                       const std::string& color, float stroke_width) {
    if (!f || polyline.points.empty()) return;
    stroke = color;
    const float sw = (stroke_width <= 0.0f) ? 0.04f : stroke_width;
    path(get_path_d(polyline.points, false), false, sw, 1.0f);
}

void SVGExporter::draw(const std::vector<Marc::Polyline>& polylines,
                       const std::string& color, float stroke_width) {
    for (const auto& pl : polylines) draw(pl, color, stroke_width);
}

void SVGExporter::draw(const Marc::Polygon& polygon,
                       const std::string& color, float stroke_width) {
    if (!f || polygon.points.empty()) return;
    stroke = color;
    const float sw = (stroke_width <= 0.0f) ? 0.05f : stroke_width;
    path(get_path_d(polygon.points, true), false, sw, 1.0f);
}

void SVGExporter::draw(const std::vector<Marc::Polygon>& polygons,
                       const std::string& color, float stroke_width) {
    for (const auto& pg : polygons) draw(pg, color, stroke_width);
}

void SVGExporter::draw(const Marc::Point& point,
                       const std::string& fillColor, float iradius) {
    if (!f) return;
    const float r = (iradius <= 0.0f) ? 0.3f : iradius;
    fprintf(f,
        "    <circle cx=\"%.5f\" cy=\"%.5f\" r=\"%.5f\""
        " fill=\"%s\" stroke=\"none\"/>\n",
        point.x - origin.x, point.y - origin.y,
        r, fillColor.c_str());
}

void SVGExporter::draw(const std::vector<Marc::Point>& points,
                       const std::string& fillColor, float radius) {
    for (const auto& pt : points) draw(pt, fillColor, radius);
}

// ---------------------------------------------------------------------------
// Draw primitives - thermal segment types
// ---------------------------------------------------------------------------

void SVGExporter::draw(const ScanSegmentHatch& seg, float stroke_width) {
    if (!f) return;
    const char* color = thermalHexColor(seg.type);
    const float sw    = (stroke_width <= 0.0f) ? 0.04f : stroke_width;
    // Open a group tagged with the segment type ID for easy scripting
    fprintf(f,
        "    <g class=\"thermal-hatch\" data-segment=\"%u\""
        " data-name=\"%s\">\n",
        static_cast<unsigned>(seg.type),
        thermalSegmentToString(seg.type));
    for (const auto& line : seg.hatches) {
        fprintf(f,
            "      <line x1=\"%.5f\" y1=\"%.5f\" x2=\"%.5f\" y2=\"%.5f\""
            " stroke=\"%s\" stroke-width=\"%.5f\"/>\n",
            line.a.x - origin.x, line.a.y - origin.y,
            line.b.x - origin.x, line.b.y - origin.y,
            color, sw);
    }
    fprintf(f, "    </g>\n");
}

void SVGExporter::draw(const std::vector<ScanSegmentHatch>& segs,
                       float stroke_width) {
    for (const auto& seg : segs) draw(seg, stroke_width);
}

void SVGExporter::draw(const ScanSegmentPolyline& seg, float stroke_width) {
    if (!f) return;
    const char* color = thermalHexColor(seg.type);
    const float sw    = (stroke_width <= 0.0f) ? 0.05f : stroke_width;
    fprintf(f,
        "    <g class=\"thermal-contour\" data-segment=\"%u\""
        " data-name=\"%s\">\n",
        static_cast<unsigned>(seg.type),
        thermalSegmentToString(seg.type));
    for (const auto& polyline : seg.polylines) {
        if (polyline.points.empty()) continue;
        stroke = color;
        path(get_path_d(polyline.points, false), false, sw, 1.0f);
    }
    fprintf(f, "    </g>\n");
}

void SVGExporter::draw(const std::vector<ScanSegmentPolyline>& segs,
                       float stroke_width) {
    for (const auto& seg : segs) draw(seg, stroke_width);
}

// ---------------------------------------------------------------------------
// Thermal legend
// ---------------------------------------------------------------------------
//
// Layout (all values in mm, drawn outside the bed area):
//
//   Legend panel origin: (bedX + bedWidth + margin + 2, margin)
//   Panel background   : rounded rect, light grey
//   Title row          : "Thermal Segments" in bold-ish 2.5mm font
//   Each of 22 rows    :
//     - colour swatch  : filled rect, 3 mm wide x 2 mm tall
//     - ID number      : right-justified, 1.6 mm font
//     - name text      : 1.6 mm font
//   Row pitch          : 3.0 mm
//
// The legend is placed in the right-hand margin that was already added to
// canvasW in open() (legendW = 42 mm), so it never obscures scan geometry.
// ---------------------------------------------------------------------------

void SVGExporter::draw_thermal_legend() {
    if (!f) return;

    // Legend panel position: just right of the bed + normal margin
    // The extra margin that was added to canvasW in open() starts at:
    //   bedX + bedWidth + margin_  (= margin_ + bedWidth_ + margin_)
    const float panelX    = margin_ + bedWidth_ + margin_ + 1.0f;  // [mm]
    const float panelY    = margin_;                                 // [mm]
    const float panelW    = 40.0f;   // mm - matches legendW reserved in open()
    const float rowH      = 3.0f;    // mm per legend row
    const float swatchW   = 3.5f;    // mm swatch rectangle width
    const float swatchH   = 2.0f;    // mm swatch rectangle height
    const float titleH    = 4.0f;    // mm reserved for the title row
    const float fontSize  = 1.6f;    // mm font size for names
    const float titleSize = 2.2f;    // mm font size for title

    const float panelH = titleH + static_cast<float>(kThermalColorCount) * rowH + 2.0f;

    // ---- Panel background ------------------------------------------------
    fprintf(f,
        "    <!-- ===== Thermal Segment Colour Legend ===== -->\n"
        "    <g id=\"thermal-legend\">\n"
        "    <rect x=\"%.4f\" y=\"%.4f\" width=\"%.4f\" height=\"%.4f\"\n"
        "          rx=\"1\" ry=\"1\"\n"
        "          fill=\"#fafafa\" stroke=\"#cccccc\" stroke-width=\"0.15\"/>\n",
        panelX, panelY, panelW, panelH);

    // ---- Title -----------------------------------------------------------
    fprintf(f,
        "    <text x=\"%.4f\" y=\"%.4f\""
        " font-family=\"sans-serif\" font-size=\"%.4f\""
        " font-weight=\"bold\" fill=\"#222222\""
        " text-anchor=\"middle\">Thermal Segments</text>\n",
        panelX + panelW * 0.5f,
        panelY + titleSize + 0.4f,
        titleSize);

    // ---- Separator line below title -------------------------------------
    fprintf(f,
        "    <line x1=\"%.4f\" y1=\"%.4f\" x2=\"%.4f\" y2=\"%.4f\""
        " stroke=\"#bbbbbb\" stroke-width=\"0.1\"/>\n",
        panelX + 0.5f,         panelY + titleH - 0.3f,
        panelX + panelW - 0.5f, panelY + titleH - 0.3f);

    // ---- Legend rows (22 entries) ----------------------------------------
    const float col1X = panelX + 0.8f;           // swatch left edge
    const float col2X = col1X + swatchW + 0.8f;  // ID number left edge
    const float col3X = col2X + 3.5f;            // name text left edge

    for (int i = 0; i < kThermalColorCount; ++i) {
        const auto& entry = kThermalColors[i];
        const float rowY  = panelY + titleH + static_cast<float>(i) * rowH;
        const float textY = rowY + swatchH - 0.1f;  // text baseline

        // Colour swatch
        fprintf(f,
            "    <rect x=\"%.4f\" y=\"%.4f\" width=\"%.4f\" height=\"%.4f\""
            " fill=\"%s\" stroke=\"#333333\" stroke-width=\"0.08\""
            " rx=\"0.3\" ry=\"0.3\"/>\n",
            col1X, rowY, swatchW, swatchH,
            entry.hexColor);

        // Segment ID (right-justified in its column)
        fprintf(f,
            "    <text x=\"%.4f\" y=\"%.4f\""
            " font-family=\"monospace\" font-size=\"%.4f\""
            " fill=\"#444444\" text-anchor=\"end\">%2u</text>\n",
            col2X + 2.5f, textY,
            fontSize,
            static_cast<unsigned>(entry.type));

        // Segment name
        fprintf(f,
            "    <text x=\"%.4f\" y=\"%.4f\""
            " font-family=\"sans-serif\" font-size=\"%.4f\""
            " fill=\"#111111\">%s</text>\n",
            col3X, textY,
            fontSize,
            entry.label);
    }

    // ---- Close legend group ---------------------------------------------
    fprintf(f, "    </g>\n");
}

// ---------------------------------------------------------------------------
// Annotation helpers
// ---------------------------------------------------------------------------

void SVGExporter::draw_text(const Marc::Point& pt, const char* text,
                            const char* color) {
    if (!f) return;
    fprintf(f,
        "    <text x=\"%.5f\" y=\"%.5f\""
        " font-family=\"sans-serif\" font-size=\"2\" fill=\"%s\">%s</text>\n",
        pt.x - origin.x, pt.y - origin.y, color, text);
}

void SVGExporter::draw_legend(const Marc::Point& pt, const char* text,
                              const char* color) {
    if (!f) return;
    const float cx = pt.x - origin.x;
    const float cy = pt.y - origin.y;
    fprintf(f,
        "    <circle cx=\"%.5f\" cy=\"%.5f\" r=\"1.0\" fill=\"%s\"/>\n"
        "    <text x=\"%.5f\" y=\"%.5f\""
        " font-family=\"sans-serif\" font-size=\"1.5\" fill=\"black\">%s</text>\n",
        cx, cy, color,
        cx + 1.5f, cy + 0.5f, text);
}

void SVGExporter::layer_section_start(const char* text) {
    if (!f) return;
    fprintf(f, "    <g id=\"%s\">\n", text);
}

void SVGExporter::layer_section_end() {
    if (!f) return;
    fprintf(f, "    </g>\n");
}

void SVGExporter::Close() {
    if (!f) return;
    fprintf(f, "  </g>\n</svg>\n");
    fclose(f);
    f = nullptr;
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void SVGExporter::path(const std::string& d, bool filled, float stroke_width,
                       float fill_opacity) {
    if (!f) return;
    fprintf(f,
        "    <path d=\"%s\" fill=\"%s\" stroke=\"%s\""
        " stroke-width=\"%.5f\" fill-rule=\"evenodd\""
        " fill-opacity=\"%.3f\"%s/>\n",
        d.c_str(),
        filled ? fill.c_str() : "none",
        stroke.c_str(),
        stroke_width,
        fill_opacity,
        (arrows && !filled) ? " marker-end=\"url(#endArrow)\"" : "");
}

std::string SVGExporter::get_path_d(const std::vector<Marc::Point>& pts,
                                    bool closed) const {
    std::ostringstream d;
    bool first = true;
    for (const auto& p : pts) {
        const float x = p.x - origin.x;
        const float y = p.y - origin.y;
        if (first) {
            d << "M " << x << " " << y;
            first = false;
        } else {
            d << " L " << x << " " << y;
        }
    }
    if (closed && !pts.empty()) d << " Z";
    return d.str();
}

} // namespace MarcSLM
