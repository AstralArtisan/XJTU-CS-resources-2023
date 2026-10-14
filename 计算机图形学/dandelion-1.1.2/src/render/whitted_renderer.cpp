#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <vector>
#include <optional>
#include <iostream>
#include <chrono>
#include <tuple>
#include <limits>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include "render_engine.h"
#include "../scene/light.h"
#include "../utils/math.hpp"
#include "../utils/ray.h"
#include "../utils/logger.h"

using std::chrono::steady_clock;
using duration   = std::chrono::duration<float>;
using time_point = std::chrono::time_point<steady_clock, duration>;
using Eigen::Vector3f;

// 最大的反射次数
constexpr int   MAX_DEPTH      = 5;
constexpr float INFINITY_FLOAT = std::numeric_limits<float>::max();
// 考虑物体与光线相交点的偏移值
constexpr float EPSILON = 1e-4f;
constexpr float DEFAULT_IOR = 1.5f;

// 当前物体的材质类型，根据不同材质类型光线会有不同的反射情况
enum class MaterialType
{
    DIFFUSE_AND_GLOSSY,
    REFLECTION
};

// 显示渲染的进度条
void update_progress(float progress)
{
    int barwidth = 70;
    std::cout << "[";
    int pos = static_cast<int>(barwidth * progress);
    for (int i = 0; i < barwidth; i++) {
        if (i < pos)
            std::cout << "=";
        else if (i == pos)
            std::cout << ">";
        else
            std::cout << " ";
    }
    std::cout << "]" << int(progress * 100.0) << " %\r";
    std::cout.flush();
}

WhittedRenderer::WhittedRenderer(RenderEngine& engine) :
    width(engine.width), height(engine.height), n_threads(engine.n_threads), use_bvh(false),
    rendering_res(engine.rendering_res)
{
    logger = get_logger("Whitted Renderer");
}

// whitted-style渲染的实现
void WhittedRenderer::render(Scene& scene)
{
    time_point begin_time = steady_clock::now();
    width                 = std::floor(width);
    height                = std::floor(height);

    // initialize frame buffer
    std::vector<Vector3f> framebuffer(static_cast<size_t>(width * height));
    for (auto& v: framebuffer) {
        v = Vector3f(0.0f, 0.0f, 0.0f);
    }

    int idx = 0;
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            // generate ray
            Ray ray = generate_ray(
                static_cast<int>(width), static_cast<int>(height), i, j, scene.camera, 1.0f
            );
            // cast ray
            framebuffer[idx++] = cast_ray(ray, scene, 0);
        }
        update_progress(static_cast<float>(j + 1) / static_cast<float>(height));
    }
    static unsigned char color_res[3];
    rendering_res.clear();
    for (long unsigned int i = 0; i < framebuffer.size(); i++) {
        color_res[0] = static_cast<unsigned char>(255 * clamp(0.f, 1.f, framebuffer[i][0]));
        color_res[1] = static_cast<unsigned char>(255 * clamp(0.f, 1.f, framebuffer[i][1]));
        color_res[2] = static_cast<unsigned char>(255 * clamp(0.f, 1.f, framebuffer[i][2]));
        rendering_res.push_back(color_res[0]);
        rendering_res.push_back(color_res[1]);
        rendering_res.push_back(color_res[2]);
    }
    time_point end_time           = steady_clock::now();
    duration   rendering_duration = end_time - begin_time;
    logger->info("rendering takes {:.6f} seconds", rendering_duration.count());
}

// 菲涅尔定理计算反射光线
float WhittedRenderer::fresnel(const Vector3f& I, const Vector3f& N, const float& ior)
{
    float cos_theta = std::clamp(-I.normalized().dot(N.normalized()), -1.0f, 1.0f);
    float r0        = (1.0f - ior) / (1.0f + ior);
    r0 *= r0;
    float result = r0 + (1.0f - r0) * std::pow(1.0f - cos_theta, 5.0f);
    return std::clamp(result, 0.0f, 1.0f);
}

// 如果相交返回Intersection结构体，如果不相交则返回false
std::optional<std::tuple<Intersection, GL::Material>>
WhittedRenderer::trace(const Ray& ray, const Scene& scene)
{
    std::optional<Intersection> closest;
    GL::Material                material;
    for (const auto& group: scene.groups) {
        for (const auto& object: group->objects) {
            Eigen::Matrix4f             model = object->model();
            std::optional<Intersection> hit;
            if (use_bvh && object->bvh) {
                hit = object->bvh->intersect(ray, object->mesh, model);
            } else {
                hit = naive_intersect(ray, object->mesh, model);
            }

            if (!hit) {
                continue;
            }
            if (!closest || hit->t + EPSILON < closest->t) {
                closest  = hit;
                material = object->mesh.material;
            }
        }
    }

    if (!closest.has_value()) {
        return std::nullopt;
    }
    return std::make_tuple(closest.value(), material);
}

// Whitted-style的光线传播算法实现
Vector3f WhittedRenderer::cast_ray(const Ray& ray, const Scene& scene, int depth)
{
    if (depth > MAX_DEPTH) {
        return RenderEngine::background_color;
    }

    auto result = trace(ray, scene);
    if (!result.has_value()) {
        return RenderEngine::background_color;
    }

    auto [isect, material] = result.value();
    Vector3f hit_point = ray.origin + isect.t * ray.direction;

    // Shading normal (usually interpolated vertex normal).
    Vector3f shading_normal = isect.normal;
    // Geometric face normal for perfect mirrors.
    Vector3f geom_normal = isect.geom_normal;

    if (shading_normal.squaredNorm() <= EPSILON) {
        shading_normal = geom_normal;
    }
    if (shading_normal.squaredNorm() <= EPSILON) {
        return RenderEngine::background_color;
    }
    shading_normal.normalize();

    // Alias for existing code that still uses `normal`.
    Vector3f normal = shading_normal;

    Vector3f view_dir = -ray.direction.normalized();

    MaterialType material_type =
        material.shininess > mirror_threshold ? MaterialType::REFLECTION
                                              : MaterialType::DIFFUSE_AND_GLOSSY;

    // Handle perfect mirrors using geometric face normal (no interpolation).
    if (material_type == MaterialType::REFLECTION) {
        Vector3f reflect_dir   = reflect(ray.direction.normalized(), geom_normal).normalized();
        Vector3f reflect_start = hit_point
                               + geom_normal
                                     * ((reflect_dir.dot(geom_normal) > 0.0f) ? EPSILON
                                                                              : -EPSILON);
        Ray reflect_ray(reflect_start, reflect_dir);
        float kr = fresnel(ray.direction, geom_normal, DEFAULT_IOR);
        Vector3f base_color = 0.1f * material.ambient + 0.1f * material.diffuse;
        Vector3f refl_color = cast_ray(reflect_ray, scene, depth + 1);
        return (1.0f - kr) * base_color + kr * refl_color;
    }

    if (material_type == MaterialType::REFLECTION && false) {
        Vector3f reflect_dir   = reflect(ray.direction.normalized(), normal).normalized();
        Vector3f reflect_start = hit_point
                               + normal * ((reflect_dir.dot(normal) > 0.0f) ? EPSILON : -EPSILON);
        Ray reflect_ray(reflect_start, reflect_dir);
        float kr = fresnel(ray.direction, normal, DEFAULT_IOR);
        // 混入一点自身底色，避免纯镜面时无可反射物体导致“消失”
        Vector3f base_color = material.ambient + material.diffuse;
        Vector3f refl_color = cast_ray(reflect_ray, scene, depth + 1);
        return (1.0f - kr) * base_color + kr * refl_color;
    }

    
    // initialize hit color
    Vector3f hitcolor = material.ambient;
    // local illumination with shadow testing
    for (const auto& light: scene.lights) {
        Vector3f light_dir  = light.position - hit_point;
        float    light_dist = light_dir.norm();
        if (light_dist <= EPSILON) {
            continue;
        }
        light_dir.normalize();

        Vector3f shadow_origin =
            hit_point + normal * ((light_dir.dot(normal) > 0.0f) ? EPSILON : -EPSILON);
        Ray shadow_ray(shadow_origin, light_dir);
        auto shadow_hit = trace(shadow_ray, scene);
        if (shadow_hit.has_value()) {
            const Intersection& shadow_isect = std::get<0>(shadow_hit.value());
            if (shadow_isect.t > EPSILON && shadow_isect.t + EPSILON < light_dist) {
                continue;
            }
        }

        float attenuation = light.intensity / (light_dist * light_dist);
        float n_dot_l     = std::max(0.0f, normal.dot(light_dir));
        hitcolor += material.diffuse * (n_dot_l * attenuation);

        Vector3f half_dir = (light_dir + view_dir).normalized();
        float    n_dot_h  = std::max(0.0f, normal.dot(half_dir));
        if (n_dot_l > 0.0f) {
            hitcolor += material.specular
                      * (std::pow(n_dot_h, material.shininess) * attenuation);
        }
    }

    // recursive reflection for glossy materials
    if (material.specular.norm() > EPSILON) {
        Vector3f reflect_dir   = reflect(ray.direction.normalized(), normal).normalized();
        Vector3f reflect_start = hit_point
                               + normal * ((reflect_dir.dot(normal) > 0.0f) ? EPSILON : -EPSILON);
        Ray reflect_ray(reflect_start, reflect_dir);
        float kr       = fresnel(ray.direction, normal, DEFAULT_IOR);
        Vector3f reflc = cast_ray(reflect_ray, scene, depth + 1);
        hitcolor += kr * reflc.cwiseProduct(material.specular);
    }

    return hitcolor;
}
