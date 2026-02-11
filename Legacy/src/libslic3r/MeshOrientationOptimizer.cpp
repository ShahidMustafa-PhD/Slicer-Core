#include "MeshOrientationOptimizer.hpp"
#include <iostream>
#include <limits>

MeshOrientationOptimizer::MeshOrientationOptimizer(const TriangleMesh& mesh)
    : m_mesh(mesh), m_best_roll(0), m_best_pitch(0), m_best_yaw(0)
{}

/*void MeshOrientationOptimizer::optimize()
{
    nlopt::opt opt(nlopt::LN_SBPLX, 2); // roll, pitch

    std::vector<double> lb = { -M_PI, -M_PI };
    std::vector<double> ub = {  M_PI,  M_PI };
    opt.set_lower_bounds(lb);
    opt.set_upper_bounds(ub);

    opt.set_min_objective(MeshOrientationOptimizer::evaluate_cost, this);
    opt.set_xtol_rel(1e-4);
    opt.set_maxtime(5.0);  // Stop after 5 seconds max

    std::vector<double> x = {0, 0}; // Initial guess
    double minf;

    try {
        nlopt::result result = opt.optimize(x, minf);

        if (result < 0) {
            std::cerr << "NLopt failed with result code: " << result << std::endl;
            return;
        }

        m_best_roll = x[0];
        m_best_pitch = x[1];
        m_best_yaw = 0.0f;
    } catch (const std::exception& e) {
        std::cerr << "NLopt exception: " << e.what() << std::endl;
    }
}*/
void MeshOrientationOptimizer::optimize()
{
    const int steps = 5; // number of angle steps (resolution)
    const double angle_min = -M_PI / 2; // minimum angle to try
    const double angle_max =  M_PI / 2; // maximum angle to try

    double best_cost = std::numeric_limits<double>::max();
    double best_roll = 0.0;
    double best_pitch = 0.0;
 #pragma omp parallel for
    for (double roll = angle_min; roll <= angle_max; roll+steps) {
        for (double pitch = angle_min; pitch <= angle_max; pitch+steps)
         {

            double cost = compute_support_cost(roll, pitch);

            if (cost < best_cost) {
                best_cost = cost;
                best_roll = roll;
                best_pitch = pitch;
            }
        }
    }

    m_best_roll = best_roll;
    m_best_pitch = best_pitch;
    m_best_yaw = 0.0f;

    std::cout << "Best orientation (brute-force): roll=" << m_best_roll
              << ", pitch=" << m_best_pitch
              << ", support area=" << best_cost << std::endl;
}

void MeshOrientationOptimizer::get_best_angles(float& roll, float& pitch) const
{
    roll = m_best_roll;
    pitch = m_best_pitch;
   // yaw = m_best_yaw;
}

// ✅ Correct NLopt-compatible objective function
double MeshOrientationOptimizer::evaluate_cost(unsigned n,
                                               const double* x,
                                               double* grad,
                                               void* data)
{
    auto* self = static_cast<MeshOrientationOptimizer*>(data);

    if (grad) {
        for (unsigned i = 0; i < n; ++i)
            grad[i] = 0.0; // Not computing gradient
    }

    return self->compute_support_cost(x[0], x[1]);
}

double MeshOrientationOptimizer::compute_support_cost(float roll, float pitch)
{
    double R[3][3];
    build_rotation_matrix(roll, pitch, R);

    Pointf3 support_dir(0, 0, -1);
    double total_area = 0.0;

    std::vector<Point3> facets = this->m_mesh.facets();
    std::vector<Pointf3> vertices = this->m_mesh.vertices();
    std::vector<Pointf3> normals = this->m_mesh.normals();

    for (size_t i = 0; i < facets.size(); i++) {
        Pointf3 v0 = vertices[facets[i].x];
        Pointf3 v1 = vertices[facets[i].y];
        Pointf3 v2 = vertices[facets[i].z];
        Pointf3 n = normals[i];

        // Rotate normal
        double nx = n.x, ny = n.y, nz = n.z;
        rotate_point(nx, ny, nz, R);

        //float dot = nx * support_dir.x + ny * support_dir.y + nz * support_dir.z;
        float dot = nz * support_dir.z;
       if (dot > 0.7071) { // cos(45°) with negative axis
            rotate_point(v0.x, v0.y, v0.z, R);
            rotate_point(v1.x, v1.y, v1.z, R);
            rotate_point(v2.x, v2.y, v2.z, R);

            total_area += triangle_area(v0, v1, v2);
        }
    }

    return total_area;
}

void MeshOrientationOptimizer::rotate_point(double& x, double& y,double& z,
                                            const double R[3][3]) const
{ //double z=0;
    double nx = R[0][0] * x + R[0][1] * y; //+ R[0][2] * z;
    double ny = R[1][0] * x + R[1][1] * y; //+ R[1][2] * z;
    double nz = R[2][0] * x + R[2][1] * y; //+ R[2][2] * z;
    x = nx;
    y = ny;
    z = nz;
}

void MeshOrientationOptimizer::build_rotation_matrix(double roll, double pitch, 
                                                     double R[3][3]) const
{//double yaw=0;
    float sr = sin(roll), cr = cos(roll);
    float sp = sin(pitch), cp = cos(pitch);
    float sy = 0;//sin(yaw), 
    float cy = 1;//cos(yaw);
    R[0][0] = cy * cp;
    R[0][1] = cy * sp * sr - sy * cr;
    R[0][2] = cy * sp * cr + sy * sr;

    R[1][0] = sy * cp;
    R[1][1] = sy * sp * sr + cy * cr;
    R[1][2] = sy * sp * cr - cy * sr;

    R[2][0] = -sp;
    R[2][1] = cp * sr;
    R[2][2] = cp * cr;
}

double MeshOrientationOptimizer::triangle_area(const Pointf3& A, const Pointf3& B, const Pointf3& C) const
{
    float ux = B.x - A.x, uy = B.y - A.y, uz = B.z - A.z;
    float vx = C.x - A.x, vy = C.y - A.y, vz = C.z - A.z;

    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;

    return 0.5 * std::sqrt(nx * nx + ny * ny + nz * nz);
}
