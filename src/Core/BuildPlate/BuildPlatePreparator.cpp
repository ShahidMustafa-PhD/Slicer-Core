// ==============================================================================
// MarcSLM - Build Plate Preparator — Implementation
// ==============================================================================
// Ported conceptually from Legacy Slic3r:
//   Model::arrange_objects()  ? arrangeModels()
//   Model::align_to_ground()  ? alignAllToGround()
//   Model::bounding_box()     ? combinedBoundingBox()
// ==============================================================================

#include "MarcSLM/Core/BuildPlate/BuildPlatePreparator.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace MarcSLM {
namespace BP {

// ==============================================================================
// Configuration
// ==============================================================================

void BuildPlatePreparator::setBedSize(float width, float depth) noexcept
{
    bedWidth_ = width;
    bedDepth_ = depth;
}

void BuildPlatePreparator::setMinSpacing(double spacing) noexcept
{
    minSpacing_ = spacing;
}

void BuildPlatePreparator::setProgressCallback(ProgressCallback cb)
{
    progressCb_ = std::move(cb);
}

// ==============================================================================
// Full Pipeline
// ==============================================================================

void BuildPlatePreparator::prepare(
    std::vector<ModelPlacement>&                           placements,
    std::vector<std::unique_ptr<Geometry::MeshProcessor>>& processors)
{
    if (placements.size() != processors.size()) {
        throw std::runtime_error(
            "BuildPlatePreparator: placement / processor count mismatch ("
            + std::to_string(placements.size()) + " vs "
            + std::to_string(processors.size()) + ").");
    }
    if (placements.empty()) {
        throw std::runtime_error(
            "BuildPlatePreparator: no models to prepare.");
    }

    reportProgress("Applying model placements …", 5);

    // Step 1: Apply Euler orientation + position to every mesh
    applyAllPlacements(placements, processors);

    // Step 2: Align all to Z=0 (redundant if applyPlacement already grounds,
    //         but we do an explicit pass for safety)
    reportProgress("Aligning models to ground …", 15);
    alignAllToGround(processors);

    // Step 3: Check for XY overlap
    reportProgress("Checking for collisions …", 20);
    try {
        validateNoOverlap(processors, static_cast<float>(minSpacing_));
    } catch (const std::runtime_error&) {
        // Overlap detected — attempt auto-arrangement
        reportProgress("Overlap detected — auto-arranging …", 25);
        if (!arrangeModels(placements, processors, minSpacing_)) {
            throw std::runtime_error(
                "BuildPlatePreparator: cannot arrange models to avoid collisions.");
        }
    }

    // Step 4: Check bed bounds
    reportProgress("Validating bed bounds …", 35);
    validateFitsInBed(processors);

    // Done
    const auto bb = combinedBoundingBox(processors);
    reportProgress("Build plate preparation complete.", 40);
    std::cout << "BuildPlatePreparator: " << placements.size()
              << " models prepared.\n"
              << "  Combined BB: ["
              << bb.min.x << ", " << bb.min.y << ", " << bb.min.z
              << "] ? ["
              << bb.max.x << ", " << bb.max.y << ", " << bb.max.z
              << "]\n";
}

// ==============================================================================
// Step 1: Apply Placements
// ==============================================================================

void BuildPlatePreparator::applyAllPlacements(
    const std::vector<ModelPlacement>&                     placements,
    std::vector<std::unique_ptr<Geometry::MeshProcessor>>& processors) const
{
    for (std::size_t i = 0; i < placements.size(); ++i) {
        const auto& pl = placements[i];
        auto& proc     = processors[i];

        if (!proc || !proc->hasValidMesh()) {
            throw std::runtime_error(
                "BuildPlatePreparator: mesh processor " + std::to_string(i)
                + " has no valid mesh (model: " + pl.modelPath + ").");
        }

        if (pl.hasTransform()) {
            std::cout << "  Applying placement to model " << i
                      << " (" << pl.modelPath << "): "
                      << "pos=(" << pl.x << ", " << pl.y << ", " << pl.z << ") "
                      << "rot=(" << pl.roll << ", " << pl.pitch << ", " << pl.yaw
                      << ")\n";
            pl.applyTo(*proc);
        }
    }
}

// ==============================================================================
// Step 2: Align to Ground
// ==============================================================================

void BuildPlatePreparator::alignAllToGround(
    std::vector<std::unique_ptr<Geometry::MeshProcessor>>& processors) const
{
    for (std::size_t i = 0; i < processors.size(); ++i) {
        auto& proc = processors[i];
        if (!proc || !proc->hasValidMesh()) continue;

        const auto bb = proc->getBoundingBox();
        if (std::abs(bb.min.z) > 1e-6f) {
            proc->mesh_->translate(0.0f, 0.0f, -bb.min.z);
            proc->bbox_.reset();
        }
    }
}

// ==============================================================================
// Step 3: Overlap Validation
// ==============================================================================

bool BuildPlatePreparator::bboxOverlapXY(
    const Geometry::BBox3f& a,
    const Geometry::BBox3f& b,
    float gap) noexcept
{
    if (a.max.x + gap <= b.min.x || b.max.x + gap <= a.min.x) return false;
    if (a.max.y + gap <= b.min.y || b.max.y + gap <= a.min.y) return false;
    return true;
}

void BuildPlatePreparator::validateNoOverlap(
    const std::vector<std::unique_ptr<Geometry::MeshProcessor>>& processors,
    float gap) const
{
    for (std::size_t i = 0; i < processors.size(); ++i) {
        if (!processors[i] || !processors[i]->hasValidMesh()) continue;
        const auto bbI = processors[i]->getBoundingBox();

        for (std::size_t j = i + 1; j < processors.size(); ++j) {
            if (!processors[j] || !processors[j]->hasValidMesh()) continue;
            const auto bbJ = processors[j]->getBoundingBox();

            if (bboxOverlapXY(bbI, bbJ, gap)) {
                throw std::runtime_error(
                    "BuildPlatePreparator: models " + std::to_string(i)
                    + " and " + std::to_string(j)
                    + " have overlapping bounding boxes on XY.");
            }
        }
    }
}

// ==============================================================================
// Step 4: Bed Bounds Validation
// ==============================================================================

void BuildPlatePreparator::validateFitsInBed(
    const std::vector<std::unique_ptr<Geometry::MeshProcessor>>& processors) const
{
    for (std::size_t i = 0; i < processors.size(); ++i) {
        if (!processors[i] || !processors[i]->hasValidMesh()) continue;
        const auto bb = processors[i]->getBoundingBox();

        if (bb.min.x < 0.0f || bb.max.x > bedWidth_ ||
            bb.min.y < 0.0f || bb.max.y > bedDepth_)
        {
            std::cerr << "BuildPlatePreparator: Warning — model " << i
                      << " extends outside the bed ("
                      << bedWidth_ << " x " << bedDepth_ << " mm)."
                      << " BBox: [" << bb.min.x << "," << bb.min.y << "] ? ["
                      << bb.max.x << "," << bb.max.y << "]\n";
            // In SLM practice this is a warning, not a hard error,
            // because the slicer clips to the bed region.
        }
    }
}

// ==============================================================================
// Auto-Arrange  (Legacy Geometry::arrange() cell-grid algorithm)
// ==============================================================================
//
// Mirrors BuildPlate::arrangeModels: uniform cell size = largest model + gap,
// cells sorted by squared-distance from bed centre (nearest first), each model
// centred within its assigned cell.  Minimum gap is 5 mm.
// ==============================================================================

bool BuildPlatePreparator::arrangeModels(
    std::vector<ModelPlacement>&                           placements,
    std::vector<std::unique_ptr<Geometry::MeshProcessor>>& processors,
    double spacing) const
{
    if (processors.empty()) return true;

    // Minimum 5 mm gap between models
    const double gap  = std::max(5.0, (spacing > 0.0) ? spacing : minSpacing_);
    const double bedW = static_cast<double>(bedWidth_);
    const double bedD = static_cast<double>(bedDepth_);
    const std::size_t total = processors.size();

    // ------------------------------------------------------------------
    // 1. Collect model sizes; find uniform cell dimensions
    // ------------------------------------------------------------------
    struct ModelInfo { std::size_t idx; double w; double d; };
    std::vector<ModelInfo> infos;
    infos.reserve(total);

    double maxW = 0.0, maxD = 0.0;
    for (std::size_t i = 0; i < total; ++i) {
        if (!processors[i] || !processors[i]->hasValidMesh()) {
            infos.push_back({i, 0.0, 0.0});
            continue;
        }
        const auto bb = processors[i]->getBoundingBox();
        const double w = static_cast<double>(bb.sizeX());
        const double d = static_cast<double>(bb.sizeY());
        infos.push_back({i, w, d});
        if (w > maxW) maxW = w;
        if (d > maxD) maxD = d;
    }

    if (maxW <= 0.0 || maxD <= 0.0) return true;

    const double cellW = maxW + gap;
    const double cellD = maxD + gap;

    // ------------------------------------------------------------------
    // 2. How many cells fit in the bed?
    // ------------------------------------------------------------------
    const std::size_t cellCols = static_cast<std::size_t>(
        std::floor((bedW + gap) / cellW));
    const std::size_t cellRows = static_cast<std::size_t>(
        std::floor((bedD + gap) / cellD));

    if (cellCols == 0 || cellRows == 0 || total > cellCols * cellRows) {
        std::cerr << "BuildPlatePreparator: " << total
                  << " models cannot fit on " << bedW << "x" << bedD
                  << " mm bed with " << gap << " mm gap.\n";
        return false;
    }

    // ------------------------------------------------------------------
    // 3. Build cell grid, sort by distance from bed centre
    // ------------------------------------------------------------------
    struct Cell {
        std::size_t col, row;
        double cx, cy;
        double dist;
    };

    const double totalCellsW = cellCols * cellW;
    const double totalCellsD = cellRows * cellD;
    const double gridOffX    = (bedW - totalCellsW) / 2.0;
    const double gridOffY    = (bedD - totalCellsD) / 2.0;
    const double bedCX       = bedW / 2.0;
    const double bedCY       = bedD / 2.0;

    std::vector<Cell> cells;
    cells.reserve(cellCols * cellRows);

    for (std::size_t ci = 0; ci < cellCols; ++ci) {
        for (std::size_t ri = 0; ri < cellRows; ++ri) {
            Cell c;
            c.col = ci; c.row = ri;
            c.cx  = gridOffX + ci * cellW + cellW / 2.0;
            c.cy  = gridOffY + ri * cellD + cellD / 2.0;
            const double dx = c.cx - bedCX;
            const double dy = c.cy - bedCY;
            c.dist = dx*dx + dy*dy
                   - std::abs((double)cellCols / 2.0 - (ci + 0.5));
            cells.push_back(c);
        }
    }
    std::sort(cells.begin(), cells.end(),
              [](const Cell& a, const Cell& b){ return a.dist < b.dist; });

    // ------------------------------------------------------------------
    // 4. Assign cells and translate meshes
    // ------------------------------------------------------------------
    for (std::size_t mi = 0; mi < total; ++mi) {
        const std::size_t i    = infos[mi].idx;
        const Cell&       cell = cells[mi];
        auto&             proc = processors[i];

        if (!proc || !proc->hasValidMesh()) continue;

        const auto   bb    = proc->getBoundingBox();
        const double mW    = static_cast<double>(bb.sizeX());
        const double mD    = static_cast<double>(bb.sizeY());
        const double destX = cell.cx - mW / 2.0;
        const double destY = cell.cy - mD / 2.0;

        const double dx = destX - static_cast<double>(bb.min.x);
        const double dy = destY - static_cast<double>(bb.min.y);

        proc->mesh_->translate(static_cast<float>(dx),
                               static_cast<float>(dy), 0.0f);
        proc->bbox_.reset();

        placements[i].x += dx;
        placements[i].y += dy;

        std::cout << "  Model " << i << ": cell ["
                  << cell.col << "," << cell.row << "] -> ("
                  << destX << ", " << destY << ") mm"
                  << "  [" << mW << "x" << mD << " mm]\n";
    }

    std::cout << "BuildPlatePreparator: " << total
              << " models in " << cellCols << "x" << cellRows
              << " grid, gap=" << gap << " mm, bed="
              << bedW << "x" << bedD << " mm.\n";
    return true;
}

// ==============================================================================
// Utilities
// ==============================================================================

Geometry::BBox3f BuildPlatePreparator::combinedBoundingBox(
    const std::vector<std::unique_ptr<Geometry::MeshProcessor>>& processors) const
{
    Geometry::BBox3f combined;
    for (const auto& proc : processors) {
        if (!proc || !proc->hasValidMesh()) continue;
        const auto bb = proc->getBoundingBox();
        combined.merge(bb.min);
        combined.merge(bb.max);
    }
    return combined;
}

void BuildPlatePreparator::reportProgress(const char* msg, int pct) const
{
    if (progressCb_) progressCb_(msg, pct);
    std::cout << "  [" << pct << "%] " << msg << '\n';
}

} // namespace BP
} // namespace MarcSLM
