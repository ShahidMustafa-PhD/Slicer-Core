// ==============================================================================
// MarcSLM - Model Placement — Implementation
// ==============================================================================

#include "MarcSLM/Core/BuildPlate/ModelPlacement.hpp"
#include "MarcSLM/Geometry/MeshProcessor.hpp"

#include <cmath>
#include <iostream>

namespace MarcSLM {
namespace BP {

void ModelPlacement::applyTo(Geometry::MeshProcessor& processor) const
{
    processor.applyPlacement(x, y, z, roll, pitch, yaw);
}

Geometry::BBox3f ModelPlacement::estimateTransformedBBox(
    const Geometry::BBox3f& src) const noexcept
{
    // For a proper estimate we rotate the 8 corners of the source AABB
    // using the Euler angles, re-ground to Z=0, then translate.

    const float cx = 0.5f * (src.min.x + src.max.x);
    const float cy = 0.5f * (src.min.y + src.max.y);
    const float cz = 0.5f * (src.min.z + src.max.z);

    // Pre-compute trig
    const float cR = static_cast<float>(std::cos(roll));
    const float sR = static_cast<float>(std::sin(roll));
    const float cP = static_cast<float>(std::cos(pitch));
    const float sP = static_cast<float>(std::sin(pitch));
    const float cY = static_cast<float>(std::cos(yaw));
    const float sY = static_cast<float>(std::sin(yaw));

    // ZYX rotation matrix  (yaw ? pitch ? roll)
    auto rotate = [&](float px, float py, float pz,
                      float& ox, float& oy, float& oz)
    {
        // Yaw (Z)
        float t0 = cY * px - sY * py;
        float t1 = sY * px + cY * py;
        float t2 = pz;
        // Pitch (Y)
        float u0 = cP * t0 + sP * t2;
        float u1 = t1;
        float u2 = -sP * t0 + cP * t2;
        // Roll (X)
        ox = u0;
        oy = cR * u1 - sR * u2;
        oz = sR * u1 + cR * u2;
    };

    const float hx = src.sizeX() * 0.5f;
    const float hy = src.sizeY() * 0.5f;
    const float hz = src.sizeZ() * 0.5f;

    // 8 corners relative to centre
    const float corners[8][3] = {
        {-hx, -hy, -hz}, { hx, -hy, -hz},
        {-hx,  hy, -hz}, { hx,  hy, -hz},
        {-hx, -hy,  hz}, { hx, -hy,  hz},
        {-hx,  hy,  hz}, { hx,  hy,  hz},
    };

    Geometry::BBox3f result;
    for (int i = 0; i < 8; ++i) {
        float rx, ry, rz;
        rotate(corners[i][0], corners[i][1], corners[i][2], rx, ry, rz);
        result.merge(Geometry::Vertex3f(rx, ry, rz));
    }

    // Re-ground: shift so min.z == 0
    const float zShift = -result.min.z;
    result.min.z = 0.0f;
    result.max.z += zShift;

    // Apply translation
    result.min.x += static_cast<float>(x);
    result.min.y += static_cast<float>(y);
    result.min.z += static_cast<float>(z);
    result.max.x += static_cast<float>(x);
    result.max.y += static_cast<float>(y);
    result.max.z += static_cast<float>(z);

    return result;
}

} // namespace BP
} // namespace MarcSLM
