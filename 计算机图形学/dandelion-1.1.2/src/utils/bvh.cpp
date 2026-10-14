#include "bvh.h"

#include <cassert>
#include <iostream>
#include <optional>
#include <algorithm>

#include <Eigen/Geometry>
#include "formatter.hpp"
#include <spdlog/spdlog.h>

#include "math.hpp"
#include "aabb.h"

using Eigen::Vector3f;
using std::optional;
using std::vector;

BVHNode::BVHNode() : left(nullptr), right(nullptr), face_idx(0)
{
}

BVH::BVH(const GL::Mesh& mesh) : root(nullptr), mesh(mesh)
{
}

// 建立bvh，将需要建立BVH的图元索引初始化
void BVH::build()
{
    if (mesh.faces.count() == 0) {
        root = nullptr;
        return;
    }

    primitives.resize(mesh.faces.count());
    for (size_t i = 0; i < mesh.faces.count(); i++) primitives[i] = i;

    root = recursively_build(primitives);
    return;
}

// 删除bvh
void BVH::recursively_delete(BVHNode* node)
{
    if (node == nullptr)
        return;
    recursively_delete(node->left);
    recursively_delete(node->right);
    delete node;
    node = nullptr;
}

// 统计BVH树建立的节点个数
size_t BVH::count_nodes(BVHNode* node)
{
    if (node == nullptr)
        return 0;
    else
        return count_nodes(node->left) + count_nodes(node->right) + 1;
}

// 递归建立BVH
BVHNode* BVH::recursively_build(vector<size_t> faces_idx)
{
    if (faces_idx.empty()) {
        return nullptr;
    }

    BVHNode* node = new BVHNode();

    AABB aabb;
    for (size_t idx: faces_idx) {
        aabb = union_AABB(aabb, get_aabb(mesh, idx));
    }
    node->aabb = aabb;

    // Leaf node, keep the single primitive.
    if (faces_idx.size() == 1) {
        node->face_idx = faces_idx[0];
        return node;
    }

    // Split along the longest axis of the bounding box.
    int axis = node->aabb.max_extent();
    std::sort(
        faces_idx.begin(), faces_idx.end(), [&](size_t lhs, size_t rhs) {
            Vector3f lc = get_aabb(mesh, lhs).centroid();
            Vector3f rc = get_aabb(mesh, rhs).centroid();
            return lc[axis] < rc[axis];
        }
    );

    size_t mid = faces_idx.size() / 2;
    vector<size_t> left_indices(faces_idx.begin(), faces_idx.begin() + mid);
    vector<size_t> right_indices(faces_idx.begin() + mid, faces_idx.end());

    // Degenerate split fallback.
    if (left_indices.empty() || right_indices.empty()) {
        left_indices.assign(faces_idx.begin(), faces_idx.begin() + faces_idx.size() / 2);
        right_indices.assign(faces_idx.begin() + faces_idx.size() / 2, faces_idx.end());
    }

    node->left  = recursively_build(left_indices);
    node->right = recursively_build(right_indices);
    if (node->left && node->right) {
        node->aabb = union_AABB(node->left->aabb, node->right->aabb);
    }
    return node;
}

// 使用BVH求交
optional<Intersection> BVH::intersect(
    const Ray& ray, [[maybe_unused]] const GL::Mesh& mesh, const Eigen::Matrix4f obj_model
)
{
    model = obj_model;
    if (!root) {
        return std::nullopt;
    }

    Eigen::Matrix4f model_inv = model.inverse();
    Eigen::Vector4f o_h(ray.origin.x(), ray.origin.y(), ray.origin.z(), 1.0f);
    Eigen::Vector4f o_m_h = model_inv * o_h;
    Eigen::Vector3f o_m   = o_m_h.head<3>();
    Eigen::Vector3f d_m   = model_inv.block<3, 3>(0, 0) * ray.direction;
    Ray             model_ray{o_m, d_m};

    optional<Intersection> isect_model = ray_node_intersect(root, model_ray);
    if (!isect_model) {
        return std::nullopt;
    }

    Intersection result  = *isect_model;
    Eigen::Vector3f p_m  = o_m + result.t * d_m;
    Eigen::Vector4f p_mh(p_m.x(), p_m.y(), p_m.z(), 1.0f);
    Eigen::Vector3f p_w  = (model * p_mh).head<3>();
    Eigen::Matrix3f n_tr = model_inv.transpose().block<3, 3>(0, 0);
    if (result.normal.squaredNorm() > 1e-8f) {
        result.normal = (n_tr * result.normal).normalized();
    }
    if (result.geom_normal.squaredNorm() > 1e-8f) {
        result.geom_normal = (n_tr * result.geom_normal).normalized();
    }
    result.t = (p_w - ray.origin).norm();
    return result;
}

// 发射的射线与当前节点求交，并递归获取最终的求交结果
optional<Intersection> BVH::ray_node_intersect(BVHNode* node, const Ray& ray) const
{
    if (node == nullptr) {
        return std::nullopt;
    }

    const Vector3f inv_dir(
        1.0f / ray.direction.x(), 1.0f / ray.direction.y(), 1.0f / ray.direction.z()
    );
    const std::array<int, 3> dir_is_neg = {
        inv_dir.x() < 0.0f ? 1 : 0, inv_dir.y() < 0.0f ? 1 : 0, inv_dir.z() < 0.0f ? 1 : 0
    };

    if (!node->aabb.intersect(ray, inv_dir, dir_is_neg)) {
        return std::nullopt;
    }

    // Leaf node.
    if (node->left == nullptr && node->right == nullptr) {
        if (node->face_idx < mesh.faces.count()) {
            return ray_triangle_intersect(ray, mesh, node->face_idx);
        }
        return std::nullopt;
    }

    optional<Intersection> left_hit;
    optional<Intersection> right_hit;
    if (node->left) {
        left_hit = ray_node_intersect(node->left, ray);
    }
    if (node->right) {
        right_hit = ray_node_intersect(node->right, ray);
    }

    if (left_hit && right_hit) {
        return (left_hit->t <= right_hit->t) ? left_hit : right_hit;
    }
    if (left_hit) {
        return left_hit;
    }
    if (right_hit) {
        return right_hit;
    }
    return std::nullopt;
}
