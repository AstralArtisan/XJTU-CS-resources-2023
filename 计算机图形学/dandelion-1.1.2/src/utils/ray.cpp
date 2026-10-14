#include "ray.h"

#include <cmath>
#include <array>

#include <Eigen/Dense>
#include <spdlog/spdlog.h>

#include "../utils/math.hpp"

using Eigen::Matrix3f;
using Eigen::Matrix4f;
using Eigen::Vector2f;
using Eigen::Vector3f;
using Eigen::Vector4f;
using std::numeric_limits;
using std::optional;
using std::size_t;

constexpr float infinity = 1e5f;
constexpr float eps      = 1e-5f;

Intersection::Intersection() :
    t(numeric_limits<float>::infinity()), face_index(0), barycentric_coord(Vector3f::Zero()),
    normal(Vector3f::Zero()), geom_normal(Vector3f::Zero())
{
}

Ray generate_ray(int width, int height, int x, int y, Camera& camera, float depth)
{
    // Pixel center in image space (origin at top-left, +x right, +y down).
    Vector2f pos(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
    Vector2f center(static_cast<float>(width) / 2.0f, static_cast<float>(height) / 2.0f);

    // Compute physical image plane size at the given depth from fov_y.
    float     fov_y             = radians(camera.fov_y_degrees);
    float     image_plane_h     = 2.0f * depth * std::tan(fov_y * 0.5f);
    float     image_plane_w     = image_plane_h * camera.aspect_ratio;
    float     ratio_x           = image_plane_w / static_cast<float>(width);
    float     ratio_y           = image_plane_h / static_cast<float>(height);
    Vector4f  view_pos_specified(
        (pos.x() - center.x()) * ratio_x, -(pos.y() - center.y()) * ratio_y, -depth, 1.0f
    );
    Matrix4f  inv_view            = camera.view().inverse();
    Vector4f  world_pos_specified = inv_view * view_pos_specified;
    Vector3f  world_pos           = world_pos_specified.head<3>();
    Vector3f  origin              = camera.position;
    return {origin, (world_pos - origin).normalized()};
}

optional<Intersection> ray_triangle_intersect(const Ray& ray, const GL::Mesh& mesh, size_t index)
{
    Intersection result;
    const Vector3f& o = ray.origin;
    const Vector3f& d = ray.direction;
    std::array<size_t, 3> f = mesh.face(index);
    Vector3f              a = mesh.vertex(f[0]);
    Vector3f              b = mesh.vertex(f[1]);
    Vector3f              c = mesh.vertex(f[2]);

    Matrix3f A;
    A.col(0) = d;
    A.col(1) = a - b;
    A.col(2) = a - c;
    float detA = A.determinant();
    if (std::fabs(detA) < eps) {
        return std::nullopt;
    }

    Vector3f rhs = a - o;
    Vector3f x   = A.fullPivLu().solve(rhs);

    float t     = x[0];
    float beta  = x[1];
    float gamma = x[2];
    if (t < eps || beta < -eps || gamma < -eps || beta + gamma > 1.0f + eps) {
        return std::nullopt;
    }
    float alpha = 1.0f - beta - gamma;

    result.t                 = t;
    result.face_index        = index;
    result.barycentric_coord = Vector3f(alpha, beta, gamma);

    Vector3f n_geom = (b - a).cross(c - a);
    constexpr float normal_eps = 1e-12f;
    if (n_geom.squaredNorm() < normal_eps) {
        return std::nullopt;
    }

    // Face geometric normal in model space.
    result.geom_normal = n_geom.normalized();

    // Use smooth shading: interpolate vertex normals via barycentric coordinates.
    Vector3f na = mesh.normal(f[0]);
    Vector3f nb = mesh.normal(f[1]);
    Vector3f nc = mesh.normal(f[2]);
    Vector3f n_smooth = alpha * na + beta * nb + gamma * nc;

    if (n_smooth.squaredNorm() < normal_eps) {
        // Fallback: if vertex normals are not available, use geometric normal.
        result.normal = result.geom_normal;
    } else {
        result.normal = n_smooth.normalized();
    }
    return result;
}

optional<Intersection> naive_intersect(const Ray& ray, const GL::Mesh& mesh, const Matrix4f model)
{
    // Transform ray into model space.
    Matrix4f inv_model = model.inverse();
    Vector4f o4(ray.origin.x(), ray.origin.y(), ray.origin.z(), 1.0f);
    Vector4f d4(ray.direction.x(), ray.direction.y(), ray.direction.z(), 0.0f);
    Vector3f o_model = (inv_model * o4).head<3>();
    Vector3f d_model = (inv_model * d4).head<3>();
    d_model.normalize();
    Ray ray_model(o_model, d_model);

    Intersection best;
    bool         hit_any = false;

    for (size_t i = 0; i < mesh.faces.count(); ++i) {
        auto local_hit = ray_triangle_intersect(ray_model, mesh, i);
        if (!local_hit) {
            continue;
        }
        if (!hit_any || local_hit->t + eps < best.t) {
            best    = *local_hit;
            hit_any = true;
        }
    }

    if (!hit_any) {
        return std::nullopt;
    }

    // Transform intersection point and normals back to world space.
    Vector3f p_model = ray_model.origin + best.t * ray_model.direction;
    Vector4f p4_model(p_model.x(), p_model.y(), p_model.z(), 1.0f);
    Vector3f p_world = (model * p4_model).head<3>();

    Matrix3f normal_mat = inv_model.transpose().topLeftCorner<3, 3>();
    if (best.normal.squaredNorm() > eps) {
        best.normal = (normal_mat * best.normal).normalized();
    }
    if (best.geom_normal.squaredNorm() > eps) {
        best.geom_normal = (normal_mat * best.geom_normal).normalized();
    }

    // Recompute t in world space along the original ray.
    float t_world = (p_world - ray.origin).dot(ray.direction);

    best.t = t_world;
    return best;
}
