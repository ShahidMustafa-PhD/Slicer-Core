// ==============================================================================
// MarcSLM - Binary .marc File Reader/Writer
// ==============================================================================
// Ported from Legacy MarcFile.hpp/cpp
// Serialises/deserialises layers of SLM scan geometry to a compact binary format
// ==============================================================================

#pragma once

#include "MarcSLM/Core/MarcFormat.hpp"

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

namespace MarcSLM {

/// @brief Reader and writer for the .marc binary file format.
/// @details The .marc format stores layer geometry (hatches, polylines,
///          polygons, circles) in a compact binary layout with an index
///          table for random-access layer retrieval.
///
///          Binary layout:
///          | Offset | Size  | Description                     |
///          |--------|-------|---------------------------------|
///          | 0      | 160B  | MarcHeader                      |
///          | 160    | var   | Layer data blocks (sequential)  |
///          | var    | var   | Index table (uint64_t offsets)   |
///
///          Ported from Legacy Marc::MarcFile.
class MarcFile {
public:
    Marc::MarcHeader             header;     ///< File header (160 bytes)
    std::vector<Marc::Layer>     layers;     ///< All layer data
    std::vector<uint64_t>        indexTable; ///< Byte offsets per layer

    /// @brief Initialize the file structure for a new export session.
    void initialize();

    /// @brief Write all layers to a .marc binary file.
    /// @param filename Output file path.
    /// @return true on success, false on failure.
    bool writeToFile(const std::string& filename);

    /// @brief Read a .marc binary file.
    /// @param filename Input file path.
    /// @return true on success, false on failure.
    bool readFromFile(const std::string& filename);

    /// @brief Add layers from processed slicing data.
    /// @param layerStack Vector of Marc::Layer from the slicing pipeline.
    void addLayers(const std::vector<Marc::Layer>& layerStack);

private:
    void writePoint(std::ofstream& out, const Marc::Point& p);
    void readPoint(std::ifstream& in, Marc::Point& p);
};

} // namespace MarcSLM
