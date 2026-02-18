// ==============================================================================
// MarcSLM - MarcFile Binary I/O Implementation
// ==============================================================================
// Ported from Legacy MarcFile.cpp
// ==============================================================================

#include "MarcSLM/Core/MarcFile.hpp"

#include <iostream>
#include <cstring>
#include <filesystem>

namespace MarcSLM {

void MarcFile::initialize() {
    layers.clear();
    indexTable.clear();
    header = Marc::MarcHeader();
}

void MarcFile::addLayers(const std::vector<Marc::Layer>& layerStack) {
    layers.insert(layers.end(), layerStack.begin(), layerStack.end());
}

void MarcFile::writePoint(std::ofstream& out, const Marc::Point& p) {
    out.write(reinterpret_cast<const char*>(&p), sizeof(p));
}

void MarcFile::readPoint(std::ifstream& in, Marc::Point& p) {
    in.read(reinterpret_cast<char*>(&p), sizeof(p));
}

bool MarcFile::writeToFile(const std::string& filename) {
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "MarcFile: Cannot open " << filename << " for writing" << std::endl;
        return false;
    }

    header.totalLayers = static_cast<uint32_t>(layers.size());
    header.timestamp = static_cast<uint64_t>(std::time(nullptr));

    // Write header placeholder (will be rewritten with correct offset)
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    indexTable.clear();

    // Write each layer
    for (const auto& layer : layers) {
        indexTable.push_back(static_cast<uint64_t>(out.tellp()));

        out.write(reinterpret_cast<const char*>(&layer.layerNumber), sizeof(layer.layerNumber));
        out.write(reinterpret_cast<const char*>(&layer.layerHeight), sizeof(layer.layerHeight));
        out.write(reinterpret_cast<const char*>(&layer.layerThickness), sizeof(layer.layerThickness));

        // Hatches
        uint32_t hatchCount = static_cast<uint32_t>(layer.hatches.size());
        out.write(reinterpret_cast<const char*>(&hatchCount), sizeof(hatchCount));
        for (const auto& hatch : layer.hatches) {
            out.write(reinterpret_cast<const char*>(&hatch.tag), sizeof(hatch.tag));
            uint32_t lineCount = static_cast<uint32_t>(hatch.lines.size());
            out.write(reinterpret_cast<const char*>(&lineCount), sizeof(lineCount));
            for (const auto& line : hatch.lines) {
                writePoint(out, line.a);
                writePoint(out, line.b);
            }
        }

        // Polylines
        uint32_t polylineCount = static_cast<uint32_t>(layer.polylines.size());
        out.write(reinterpret_cast<const char*>(&polylineCount), sizeof(polylineCount));
        for (const auto& polyline : layer.polylines) {
            out.write(reinterpret_cast<const char*>(&polyline.tag), sizeof(polyline.tag));
            uint32_t ptCount = static_cast<uint32_t>(polyline.points.size());
            out.write(reinterpret_cast<const char*>(&ptCount), sizeof(ptCount));
            for (const auto& pt : polyline.points) writePoint(out, pt);
        }

        // Polygons
        uint32_t polygonCount = static_cast<uint32_t>(layer.polygons.size());
        out.write(reinterpret_cast<const char*>(&polygonCount), sizeof(polygonCount));
        for (const auto& polygon : layer.polygons) {
            out.write(reinterpret_cast<const char*>(&polygon.tag), sizeof(polygon.tag));
            uint32_t ptCount = static_cast<uint32_t>(polygon.points.size());
            out.write(reinterpret_cast<const char*>(&ptCount), sizeof(ptCount));
            for (const auto& pt : polygon.points) writePoint(out, pt);
        }

        // Circles
        uint32_t circleCount = static_cast<uint32_t>(layer.circles.size());
        out.write(reinterpret_cast<const char*>(&circleCount), sizeof(circleCount));
        for (const auto& circle : layer.circles) {
            out.write(reinterpret_cast<const char*>(&circle.tag), sizeof(circle.tag));
            writePoint(out, circle.center);
            out.write(reinterpret_cast<const char*>(&circle.radius), sizeof(circle.radius));
        }
    }

    // Write index table
    header.indexTableOffset = static_cast<uint64_t>(out.tellp());
    for (auto offset : indexTable) {
        out.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
    }

    // Rewrite header with correct index offset
    out.seekp(0);
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    out.close();
    std::cout << "MarcFile: Written " << layers.size() << " layers to "
              << filename << std::endl;
    return true;
}

bool MarcFile::readFromFile(const std::string& filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "MarcFile: Cannot open " << filename << " for reading" << std::endl;
        return false;
    }

    // Read header
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!header.isValid()) {
        std::cerr << "MarcFile: Invalid file format (wrong magic)" << std::endl;
        return false;
    }

    // Read index table
    in.seekg(static_cast<std::streamoff>(header.indexTableOffset));
    indexTable.resize(header.totalLayers);
    for (uint32_t i = 0; i < header.totalLayers; ++i) {
        in.read(reinterpret_cast<char*>(&indexTable[i]), sizeof(uint64_t));
    }

    // Read each layer
    layers.resize(header.totalLayers);
    for (uint32_t i = 0; i < header.totalLayers; ++i) {
        in.seekg(static_cast<std::streamoff>(indexTable[i]));
        auto& layer = layers[i];

        in.read(reinterpret_cast<char*>(&layer.layerNumber), sizeof(layer.layerNumber));
        in.read(reinterpret_cast<char*>(&layer.layerHeight), sizeof(layer.layerHeight));
        in.read(reinterpret_cast<char*>(&layer.layerThickness), sizeof(layer.layerThickness));

        // Hatches
        uint32_t hatchCount = 0;
        in.read(reinterpret_cast<char*>(&hatchCount), sizeof(hatchCount));
        layer.hatches.resize(hatchCount);
        for (auto& hatch : layer.hatches) {
            in.read(reinterpret_cast<char*>(&hatch.tag), sizeof(hatch.tag));
            uint32_t lineCount = 0;
            in.read(reinterpret_cast<char*>(&lineCount), sizeof(lineCount));
            hatch.lines.resize(lineCount);
            for (auto& line : hatch.lines) {
                readPoint(in, line.a);
                readPoint(in, line.b);
            }
        }

        // Polylines
        uint32_t polylineCount = 0;
        in.read(reinterpret_cast<char*>(&polylineCount), sizeof(polylineCount));
        layer.polylines.resize(polylineCount);
        for (auto& polyline : layer.polylines) {
            in.read(reinterpret_cast<char*>(&polyline.tag), sizeof(polyline.tag));
            uint32_t ptCount = 0;
            in.read(reinterpret_cast<char*>(&ptCount), sizeof(ptCount));
            polyline.points.resize(ptCount);
            for (auto& pt : polyline.points) readPoint(in, pt);
        }

        // Polygons
        uint32_t polygonCount = 0;
        in.read(reinterpret_cast<char*>(&polygonCount), sizeof(polygonCount));
        layer.polygons.resize(polygonCount);
        for (auto& polygon : layer.polygons) {
            in.read(reinterpret_cast<char*>(&polygon.tag), sizeof(polygon.tag));
            uint32_t ptCount = 0;
            in.read(reinterpret_cast<char*>(&ptCount), sizeof(ptCount));
            polygon.points.resize(ptCount);
            for (auto& pt : polygon.points) readPoint(in, pt);
        }

        // Circles
        uint32_t circleCount = 0;
        in.read(reinterpret_cast<char*>(&circleCount), sizeof(circleCount));
        layer.circles.resize(circleCount);
        for (auto& circle : layer.circles) {
            in.read(reinterpret_cast<char*>(&circle.tag), sizeof(circle.tag));
            readPoint(in, circle.center);
            in.read(reinterpret_cast<char*>(&circle.radius), sizeof(circle.radius));
        }
    }

    in.close();
    std::cout << "MarcFile: Read " << layers.size() << " layers from "
              << filename << std::endl;
    return true;
}

} // namespace MarcSLM
