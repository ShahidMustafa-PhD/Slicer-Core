#pragma once

#include <vector>
//#include <nlopt.hpp>
#include "TriangleMesh.hpp"
#include "Point.hpp"
#include <cmath>

namespace Slic3r {
class MeshOrientationOptimizer {
public://Slic3r
    MeshOrientationOptimizer(const Slic3r::TriangleMesh& mesh);

    void optimize(); // Run optimization
    void get_best_angles(float& roll, float& pitch) const;

private:
    Slic3r::TriangleMesh m_mesh;
    float m_best_roll, m_best_pitch, m_best_yaw;

    // ✅ Updated NLopt-compatible signature
    static double evaluate_cost(unsigned n, const double* x, double* grad, void* data);

    double compute_support_cost(float roll, float pitch);

    void rotate_point(double& x, double& y, double& z, const double R[3][3]) const;

    void build_rotation_matrix(double roll, double pitch,
                               double R[3][3]) const;

    double triangle_area(const Pointf3& A, const Pointf3& B, const Pointf3& C) const;
};


} // namespace Slic3r