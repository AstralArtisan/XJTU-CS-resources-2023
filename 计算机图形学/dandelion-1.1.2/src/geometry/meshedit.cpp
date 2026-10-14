#include "halfedge.h"

#include <set>
#include <map>
#include <vector>
#include <string>
#include <cmath>

#include <Eigen/Core>
#include <Eigen/Dense>
#include <spdlog/spdlog.h>

using Eigen::Matrix3f;
using Eigen::Matrix4f;
using Eigen::Vector3f;
using Eigen::Vector4f;
using std::optional;
using std::set;
using std::size_t;
using std::string;
using std::unordered_map;
using std::vector;

HalfedgeMesh::EdgeRecord::EdgeRecord(unordered_map<Vertex*, Matrix4f>& vertex_quadrics, Edge* e) :
    edge(e)
{
    Halfedge* h     = e->halfedge;
    Halfedge* h_inv = h->inv;
    Vertex*   v1    = h->from;
    Vertex*   v2    = h_inv->from;

    Matrix4f Q   = Matrix4f::Zero();
    auto     it1 = vertex_quadrics.find(v1);
    if (it1 != vertex_quadrics.end()) {
        Q += it1->second;
    }
    auto it2 = vertex_quadrics.find(v2);
    if (it2 != vertex_quadrics.end()) {
        Q += it2->second;
    }

    Matrix3f A;
    Vector3f b;
    A << Q(0, 0), Q(0, 1), Q(0, 2), Q(1, 0), Q(1, 1), Q(1, 2), Q(2, 0), Q(2, 1), Q(2, 2);
    b << Q(0, 3), Q(1, 3), Q(2, 3);

    auto quadric_error = [&](const Vector3f& p) {
        Vector4f u(p.x(), p.y(), p.z(), 1.0f);
        return (u.transpose() * Q * u)(0, 0);
    };

    Vector3f p_opt;
    float    detA = A.determinant();
    if (detA != 0.0f) {
        p_opt = -A.inverse() * b;
    } else {
        Vector3f p1   = v1->pos;
        Vector3f p2   = v2->pos;
        Vector3f pmid = 0.5f * (p1 + p2);

        float c1 = quadric_error(p1);
        float c2 = quadric_error(p2);
        float c3 = quadric_error(pmid);

        p_opt      = p1;
        float best = c1;
        if (c2 < best) {
            p_opt = p2;
            best  = c2;
        }
        if (c3 < best) {
            p_opt = pmid;
            best  = c3;
        }

        optimal_pos = p_opt;
        cost        = best;
        return;
    }

    optimal_pos = p_opt;

    Vector4f u(p_opt.x(), p_opt.y(), p_opt.z(), 1.0f);
    cost = (u.transpose() * Q * u)(0, 0);
}

bool operator<(const HalfedgeMesh::EdgeRecord& a, const HalfedgeMesh::EdgeRecord& b)
{
    if (a.cost == b.cost) {
        // Sort by edge id if cost are the same
        return a.edge->id < b.edge->id;
    }
    return a.cost < b.cost;
}

optional<Edge*> HalfedgeMesh::flip_edge(Edge* e)
{
    if (e->on_boundary()) return std::nullopt;
    /* The vertices:
        v3    v2      v3    v2
            /h    ->     \h
        v1    v4      v1    v4
    */
    // Useful halfedges
    Halfedge* h     = e->halfedge;
    Halfedge* h_inv = h->inv;
    Halfedge* h_2_3 = h->next;
    Halfedge* h_3_1 = h_2_3->next;
    Halfedge* h_1_4 = h_inv->next;
    Halfedge* h_4_2 = h_1_4->next;
    // v1 andv2 are vertices along the edge
    Vertex* v1 = h->from;
    Vertex* v2 = h_inv->from;
    // v3 andv4 are vertices opposite the edge
    Vertex* v3 = h_3_1->from;
    Vertex* v4 = h_4_2->from;
    // faces
    Face* f1 = h->face;
    Face* f2 = h_inv->face;

    // h->set_neighbors(next, prev, inv, from, edge, face);
    h->set_neighbors(h_3_1, h_1_4, h_inv, v4, e, f1);
    h_inv->set_neighbors(h_4_2, h_2_3, h, v3, e, f2);
    h_2_3->set_neighbors(h_inv, h_4_2, h_2_3->inv, v2, h_2_3->edge, f2);
    h_3_1->set_neighbors(h_1_4, h, h_3_1->inv, v3, h_3_1->edge, f1);
    h_1_4->set_neighbors(h, h_3_1, h_1_4->inv, v1, h_1_4->edge, f1);
    h_4_2->set_neighbors(h_2_3, h_inv, h_4_2->inv, v4, h_4_2->edge, f2);
    // set the pointers again
    f1->halfedge = h; 
    f2->halfedge = h_inv;
    v1->halfedge = h_1_4;
    v2->halfedge = h_2_3;
    v3->halfedge = h_3_1;
    v4->halfedge = h; 
    e->halfedge = h;
    return e;
}

optional<Vertex*> HalfedgeMesh::split_edge(Edge* e)
{
    if (!e->on_boundary()) {
        Halfedge* h     = e->halfedge;
        Halfedge* h_inv = h->inv;
        Halfedge* h_2_3 = h->next;
        Halfedge* h_3_1 = h_2_3->next;
        Halfedge* h_1_4 = h_inv->next;
        Halfedge* h_4_2 = h_1_4->next;
        // v1 andv2 are vertices along the edge
        Vertex* v1 = h->from;
        Vertex* v2 = h_inv->from;
        // v3 andv4 are vertices opposite the edge
        Vertex* v3 = h_3_1->from;
        Vertex* v4 = h_4_2->from;
        // faces
        Face* f1 = h->face;
        Face* f2 = h_inv->face;
        // Create new vertex at the center of the edge
        Vertex* v5 = new_vertex();
        v5->pos    = e->center();
        // Create new halfedges
        Halfedge* h_5_1 = new_halfedge();
        Halfedge* h_5_2 = new_halfedge();
        Halfedge* h_5_3 = new_halfedge();
        Halfedge* h_5_4 = new_halfedge();
        Halfedge* h_3_5 = new_halfedge();
        Halfedge* h_4_5 = new_halfedge();
        // Create new edges
        Edge* e2 = new_edge();
        Edge* e3 = new_edge();
        Edge* e4 = new_edge();
        // Create new faces
        Face* f3 = new_face();
        Face* f4 = new_face();
        // Set halfedges
        v5->halfedge = h_5_1;
        e2->halfedge = h_inv;
        e3->halfedge = h_3_5;
        e4->halfedge = h_4_5;
        f1->halfedge = h_3_1;
        f2->halfedge = h_4_2;
        f3->halfedge = h_1_4;
        f4->halfedge = h_2_3;
        // Set neighbors
        // h->set_neighbors(next, prev, inv, from, edge, face);
        h->set_neighbors(h_5_3, h_3_1, h_5_1, v1, e, f1);
        h_5_3->set_neighbors(h_3_1, h, h_3_5, v5, e3, f1);
        h_3_1->set_neighbors(h, h_5_3, h_3_1->inv, v3, h_3_1->edge, f1);

        h_inv->set_neighbors(h_5_4, h_4_2, h_5_2, v2, e2, f2);
        h_5_4->set_neighbors(h_4_2, h_inv, h_4_5, v5, e4, f2);
        h_4_2->set_neighbors(h_inv, h_5_4, h_4_2->inv, v4, h_4_2->edge, f2);

        h_5_1->set_neighbors(h_1_4, h_4_5, h, v5, e, f3);
        h_1_4->set_neighbors(h_4_5, h_5_1, h_1_4->inv, v1, h_1_4->edge, f3);
        h_4_5->set_neighbors(h_5_1, h_1_4, h_5_4, v4, e4, f3);

        h_5_2->set_neighbors(h_2_3, h_3_5, h_inv, v5, e2, f4);
        h_2_3->set_neighbors(h_3_5, h_5_2, h_2_3->inv, v2, h_2_3->edge, f4);
        h_3_5->set_neighbors(h_5_2, h_2_3, h_5_3, v3, e3, f4);
        e3->is_new = true;
        e4->is_new = true;
        return v5;
    } else {
        Halfedge* h = e->halfedge;
        Halfedge* h_inv = h->inv;
        Vertex*   v1         = h->from;   
        Vertex*   v2         = h_inv->from; 
        Halfedge* h_2_3      = h->next; 
        Halfedge* h_3_1      = h_2_3->next;
        Vertex*   v3         = h_3_1->from; 
        Face*     f1         = h->face; 
        Face*     f_boundary = h_inv->face; 

        Vertex* v5   = new_vertex();
        v5->pos      = e->center();
        v5->halfedge = nullptr;

        Halfedge* h_5_1 = new_halfedge();
        Halfedge* h_5_2 = new_halfedge();
        Halfedge* h_5_3 = new_halfedge();
        Halfedge* h_3_5 = new_halfedge();
        Halfedge* hb1   = h_inv->prev;
        Halfedge* hb2   = h_inv->next;
        Edge* e2   = new_edge(); 
        Edge* e3   = new_edge(); 
        e3->is_new = true;

        Face* f2     = new_face(); 

        e->halfedge  = h;
        e2->halfedge = h_inv;
        e3->halfedge = h_3_5;
        v5->halfedge = h_5_2;
        f1->halfedge         = h;
        f2->halfedge         = h_5_2;
        f_boundary->halfedge = h_inv; 
        hb2->prev            = h_5_1;

        // h->set_neighbors(next, prev, inv, from, edge, face)
        h->set_neighbors(h_5_3, h_3_1, h_5_1, v1, e, f1);
        h_5_3->set_neighbors(h_3_1, h, h_3_5, v5, e3, f1);
        h_3_1->set_neighbors(h, h_5_3, h_3_1->inv, v3, h_3_1->edge, f1);

        h_inv->set_neighbors(h_5_1, hb1, h_5_2, v2, e2, f_boundary);
        h_5_1->set_neighbors(hb2, h_inv, h, v5, e, f_boundary);

        h_5_2->set_neighbors(h_2_3, h_3_5, h_inv, v5, e2, f2);
        h_2_3->set_neighbors(h_3_5, h_5_2, h_2_3->inv, v2, h_2_3->edge, f2);
        h_3_5->set_neighbors(h_5_2, h_2_3, h_5_3, v3, e3, f2);
        

        e3->is_new   = true;
        validate();
        return v5;
    }
    
}

optional<Vertex*> HalfedgeMesh::collapse_edge(Edge* e)
{
    if (e->on_boundary()) {
        Halfedge* h_1_2 = e->halfedge;
        Halfedge* h_2_3 = h_1_2->next;
        Halfedge* h_3_1 = h_2_3->next;
        Halfedge* h_2_1 = h_1_2->inv;
        Halfedge* h_3_2 = h_2_3->inv;
        Halfedge* h_1_3 = h_3_1->inv;

        Edge* e1 = h_3_2->edge;
        Edge* e2 = h_3_1->edge;

        Vertex* v1 = h_1_2->from;
        Vertex* v2 = h_2_3->from;
        Vertex* v3 = h_3_1->from;

        Face* f1 = h_1_2->face;

        Edge* E1 = new_edge();

        v1->halfedge = h_1_3;
        E1->halfedge = h_1_3;
        // h->set_neighbors(next, prev, inv, from, edge, face);
        h_1_3->set_neighbors(h_1_3->next, h_1_3->prev, h_3_2, v1, E1, h_1_3->face);
        h_3_2->set_neighbors(h_3_2->next, h_3_2->prev, h_1_3, v3, E1, h_3_2->face);

        h_3_2->next->from = v1;
        h_2_1->next->from = v1;

        Halfedge* H = h_2_1;
        while (H->next->inv->next != h_1_3) {
            H->next->from            = v1;
            H->next->inv->next->from = v1;
            H                        = H->next->inv;
        }
        H = h_1_2;
        while (H->next->inv->next != h_2_3) {
            H->next->from            = v1;
            H->next->inv->next->from = v1;
            H                        = H->next->inv;
        }
        v1->pos = e->center();

        erase(h_3_1);
        erase(h_2_3);
        erase(h_1_2);
        erase(h_2_1);
        erase(v2);
        erase(e);
        erase(e1);
        erase(e2);
        erase(f1);
        return v1;
    } else { 
        // ten halfedges
        Halfedge* h_1_2 = e->halfedge;
        Halfedge* h_2_1 = h_1_2->inv;
        Halfedge* h_2_3 = h_1_2->next;
        Halfedge* h_3_1 = h_2_3->next;
        Halfedge* h_3_2 = h_2_3->inv;
        Halfedge* h_1_3 = h_3_1->inv;
        Halfedge* h_1_4 = h_2_1->next;
        Halfedge* h_4_2 = h_1_4->next;
        Halfedge* h_4_1 = h_1_4->inv;
        Halfedge* h_2_4 = h_4_2->inv;
        // four vertices
        Vertex* v1 = h_1_2->from;
        Vertex* v2 = h_2_1->from;
        Vertex* v3 = h_3_1->from;
        Vertex* v4 = h_4_2->from;

        Edge* e1 = h_1_3->edge;
        Edge* e2 = h_3_2->edge;
        Edge* e3 = h_2_4->edge;
        Edge* e4 = h_1_4->edge;

        Face* f1 = h_1_2->face;
        Face* f2 = h_2_1->face;

        Vertex* v5 = new_vertex();

        v3->halfedge = h_3_2;
        v4->halfedge = h_4_1;
        v5->halfedge = h_1_3;
        v5->pos      = e->center();
        // h->set_neighbors(next, prev, inv, from, edge, face);
        h_1_3->set_neighbors(h_1_3->next, h_1_3->prev, h_3_2, v5, e1, h_1_3->face);
        h_3_2->set_neighbors(h_3_2->next, h_3_2->prev, h_1_3, v3, e1, h_3_2->face);
        h_2_4->set_neighbors(h_2_4->next, h_2_4->prev, h_4_1, v5, e3, h_2_4->face);
        h_4_1->set_neighbors(h_4_1->next, h_4_1->prev, h_2_4, v4, e3, h_4_1->face);

        h_4_1->next->from            = v5;
        h_3_2->next->from            = v5;
        h_3_2->next->inv->next->from = v5;
        h_4_1->next->inv->next->from = v5;

        Halfedge* H = h_4_1;
        while (H->next->inv->next != h_1_3) {
            H->next->from            = v5;
            H->next->inv->next->from = v5;
            H                        = H->next->inv;
        }
        H = h_3_2;
        while (H->next->inv->next != h_2_4) {
            H->next->from            = v5;
            H->next->inv->next->from = v5;
            H                        = H->next->inv;
        }

        erase(h_3_1);
        erase(h_2_3);
        erase(h_1_2);
        erase(h_2_1);
        erase(h_1_4);
        erase(h_4_2);
        erase(v1);
        erase(v2);
        erase(f1);
        erase(f2);
        erase(e);
        erase(e2);
        erase(e4);
        //erase(H);
        return v5;
    }
}



void HalfedgeMesh::loop_subdivide()
{
    optional<HalfedgeMeshFailure> check_result = validate();
    if (check_result.has_value()) {
        return;
    }
    logger->info(
        "subdivide object {} (ID: {}) with Loop Subdivision strategy", object.name, object.id
    );
    logger->info("original mesh: {} vertices, {} faces in total", vertices.size, faces.size);
    // Each vertex and edge of the original mesh can be associated with a vertex
    // in the new (subdivided) mesh.
    // Therefore, our strategy for computing the subdivided vertex locations is to
    // *first* compute the new positions using the connectivity of the original
    // (coarse) mesh. Navigating this mesh will be much easier than navigating
    // the new subdivided (fine) mesh, which has more elements to traverse.
    // We will then assign vertex positions in the new mesh based on the values
    // we computed for the original mesh.

    // Compute new positions for all the vertices in the input mesh using
    // the Loop subdivision rule and store them in Vertex::new_pos.
    //    At this point, we also want to mark each vertex as being a vertex of the
    //    original mesh. Use Vertex::is_new for this.
    for (Vertex* v = vertices.head; v != nullptr; v = v->next_node) {
        v->is_new = false;
        bool on_boundary = false;
        Halfedge* h = v->halfedge;
        Halfedge* start = h;
        // check if the vertex is on boundary
        do {
            if (h->edge->on_boundary()) {
                on_boundary = true;
                break;
            }
            h = h->inv->next;
        } while (h != start);
        // deal with boundary vertex
        if (on_boundary) {
            Vertex*   v1 = nullptr;
            Vertex*   v2 = nullptr;
            // find the two neighboring boundary vertices
            do {
                if (h->face->is_boundary) {
                    v1 = h->inv->from;
                }
                if (h->inv->face->is_boundary) {
                    v2 = h->inv->from;
                }
                h = h->inv->next;
            } while (h != v->halfedge);

            if (v1 && v2) {
                v->new_pos = 0.75f * v->pos + 0.125f * (v1->pos + v2->pos);
            } else {
                v->new_pos = v->pos;
            }
        } else {
            size_t n = v->degree();
            double u;
            if (n == 3) {
                u = 3.0 / 16.0;
            } else {
                u = 3.0 / (8.0 * (double)n);
            }
            v->new_pos = (1.0f - (double)n * u) * v->pos + u * n * v->neighborhood_center();
        }
    }

    // Next, compute the subdivided vertex positions associated with edges, and
    // store them in Edge::new_pos.
    for (Edge* e = edges.head; e != nullptr; e = e->next_node) {
        e->is_new = false;
        Halfedge* h     = e->halfedge;
        Halfedge* h_inv = h->inv;
        Vertex*    v1   = h->from;
        Vertex*    v2   = h_inv->from;
        if (e->on_boundary()) {
            e->new_pos = 0.5f * (v1->pos + v2->pos);
        } else {
            Vertex* v3 = h->next->next->from;
            Vertex* v4 = h_inv->next->next->from;
            e->new_pos = 0.375f * (v1->pos + v2->pos) + 0.125f * (v3->pos + v4->pos);
        }
    }
    // Next, we're going to split every edge in the mesh, in any order.
    // We're also going to distinguish subdivided edges that came from splitting
    // an edge in the original mesh from new edges by setting the boolean Edge::is_new.
    // Note that in this loop, we only want to iterate over edges of the original mesh.
    // Otherwise, we'll end up splitting edges that we just split (and the
    // loop will never end!)
    // I use a vector to store iterators of original because there are three kinds of
    // edges: original edges, edges split from original edges and newly added edges.
    // The newly added edges are marked with Edge::is_new property, so there is not
    // any other property to mark the edges I just split.
    std::vector<Edge*> original_edges;
    for (Edge* e = edges.head; e != nullptr; e = e->next_node) {
        original_edges.push_back(e);
    }
    for (Edge* e: original_edges) {
        auto v_op = split_edge(e);
        if (v_op.has_value()) {
            Vertex* v  = v_op.value();
            v->is_new  = true;
            v->new_pos = e->new_pos;
        }
    }


    // Now flip any new edge that connects an old and new vertex.
    Edge* e = edges.head;
    while (e != nullptr) {
        if (e->is_new && !e->on_boundary()) {
            bool v1_is_new  = e->halfedge->from->is_new;
            bool v2_is_new  = e->halfedge->inv->from->is_new;
            bool new_or_not = (v1_is_new ^ v2_is_new); 
            if (new_or_not) {
                flip_edge(e);
            }
        }
        e = e->next_node;
    }
    // Finally, copy the new vertex positions into final Vertex::pos.
    Vertex* v = vertices.head;
    while (v != nullptr) {
        v->pos = v->new_pos;
        v = v->next_node;
    }

    // Once we have successfully subdivided the mesh, set global_inconsistent
    // to true to trigger synchronization with GL::Mesh.
    global_inconsistent = true;
    logger->info("subdivided mesh: {} vertices, {} faces in total", vertices.size, faces.size);
    logger->info("Loop Subdivision done");
    logger->info("");
    validate();
}

void HalfedgeMesh::simplify()
{
    optional<HalfedgeMeshFailure> check_result = validate();
    if (check_result.has_value()) {
        return;
    }
    logger->info("simplify object {} (ID: {})", object.name, object.id);
    logger->info("original mesh: {} vertices, {} faces", vertices.size, faces.size);
    unordered_map<Vertex*, Matrix4f> vertex_quadrics;
    unordered_map<Face*, Matrix4f>   face_quadrics;
    unordered_map<Edge*, EdgeRecord> edge_records;
    set<EdgeRecord>                  edge_queue;

    // Compute initial quadrics for each face by simply writing the plane equation
    // for the face in homogeneous coordinates. These quadrics should be stored
    // in face_quadrics
    for (Face* f = faces.head; f != nullptr; f = f->next_node) {
        if (f->is_boundary) {
            continue;
        }

        Halfedge* h  = f->halfedge;
        Vertex*   v0 = h->from;
        Vertex*   v1 = h->next->from;
        Vertex*   v2 = h->next->next->from;
        Vector3f  p0 = v0->pos;
        Vector3f  p1 = v1->pos;
        Vector3f  p2 = v2->pos;

        Vector3f n = (p1 - p0).cross(p2 - p0);
        if (n.squaredNorm() == 0.0f) {
            face_quadrics[f] = Matrix4f::Zero();
            continue;
        }
        n.normalize();

        float    a = n.x(), b = n.y(), c = n.z();
        float    d = -n.dot(p0);
        Vector4f plane(a, b, c, d);

        Matrix4f K       = plane * plane.transpose(); // v v^T
        face_quadrics[f] = K;
    }
    // -> Compute an initial quadric for each vertex as the sum of the quadrics
    //    associated with the incident faces, storing it in vertex_quadrics
    for (Vertex* v = vertices.head; v != nullptr; v = v->next_node) {
        Matrix4f Q = Matrix4f::Zero();

        Halfedge* h = v->halfedge;
        if (h) {
            Halfedge* start = h;
            do {
                Face* f  = h->face;
                auto  it = face_quadrics.find(f);
                if (it != face_quadrics.end()) {
                    Q += it->second;
                }
                h = h->inv->next;
            } while (h != start);
        }

        vertex_quadrics[v] = Q;
    }
    // -> Build a priority queue of edges according to their quadric error cost,
    //    i.e., by building an Edge_Record for each edge and sticking it in the
    //    queue. You may want to use the above PQueue<Edge_Record> for this.
    for (Edge* e = edges.head; e != nullptr; e = e->next_node) {
        if (e->on_boundary()) {
            continue;
        }
        EdgeRecord rec(vertex_quadrics, e);
        edge_records[e] = rec;
        edge_queue.insert(rec);
    }

    // -> Until we reach the target edge budget, collapse the best edge. Remember
    //    to remove from the queue any edge that touches the collapsing edge
    //    BEFORE it gets collapsed, and add back into the queue any edge touching
    //    the collapsed vertex AFTER it's been collapsed. Also remember to assign
    //    a quadric to the collapsed vertex, and to pop the collapsed edge off the
    //    top of the queue.
    const size_t target_faces = faces.size / 4;
    auto         collect_vertex_neighbors = [](Vertex* v, set<Vertex*>& S) {
        Halfedge* h = v->halfedge;
        if (!h)
            return;

        Halfedge* start = h;
        do {
            Vertex* u = h->inv->from; 
            S.insert(u);
            h = h->inv->next;
        } while (h != start);
    };
    auto common_neighbor_count = [&](Vertex* v1, Vertex* v2) -> size_t {
        set<Vertex*> n1;
        collect_vertex_neighbors(v1, n1);

        size_t    cnt = 0;
        Halfedge* h   = v2->halfedge;
        if (!h)
            return 0;
        Halfedge* start = h;
        do {
            Vertex* u = h->inv->from;
            if (n1.find(u) != n1.end()) {
                ++cnt;
            }
            h = h->inv->next;
        } while (h != start);
        return cnt;
    };
    while (faces.size > target_faces && !edge_queue.empty()) {
        auto       it  = edge_queue.begin();
        EdgeRecord rec = *it;
        edge_queue.erase(it);

        Edge* e = rec.edge;
        if (!e || !e->halfedge) {
            continue; 
        }

        if (e->on_boundary()) {
            continue;
        }

        Halfedge* h     = e->halfedge;
        Halfedge* h_inv = h->inv;
        Vertex*   v1    = h->from;
        Vertex*   v2    = h_inv->from;

        if (common_neighbor_count(v1, v2) != 2) {
            continue;
        }

        Matrix4f Q_new = vertex_quadrics[v1] + vertex_quadrics[v2];

        set<Edge*> adj_edges;
        auto       collect_adj_edges = [&](Vertex* v) {
            Halfedge* hh = v->halfedge;
            if (!hh)
                return;
            Halfedge* start_h = hh;
            do {
                Edge* ee = hh->edge;
                if (ee != e && !ee->on_boundary()) {
                    adj_edges.insert(ee);
                }
                hh = hh->inv->next;
            } while (hh != start_h);
        };

        collect_adj_edges(v1);
        collect_adj_edges(v2);

        for (Edge* ae: adj_edges) {
            auto mit = edge_records.find(ae);
            if (mit != edge_records.end()) {
                edge_queue.erase(mit->second);
                edge_records.erase(mit);
            }
        }
        edge_records.erase(e);

        auto v_new_opt = collapse_edge(e);
        if (!v_new_opt.has_value()) {
            continue;
        }

        Vertex* v_new = v_new_opt.value();

        v_new->pos = rec.optimal_pos;

        vertex_quadrics.erase(v1);
        vertex_quadrics.erase(v2);
        vertex_quadrics[v_new] = Q_new;

        Halfedge* hh = v_new->halfedge;
        if (!hh) {
            continue;
        }
        Halfedge* start_h = hh;
        do {
            Edge* ee = hh->edge;
            if (!ee->on_boundary()) {
                EdgeRecord new_rec(vertex_quadrics, ee);
                edge_records[ee] = new_rec;
                edge_queue.insert(new_rec);
            }
            hh = hh->inv->next;
        } while (hh != start_h);
    }

    logger->info("simplified mesh: {} vertices, {} faces", vertices.size, faces.size);
    logger->info("simplification done\n");
    global_inconsistent = true;
    validate();
}

void HalfedgeMesh::isotropic_remesh()
{
    optional<HalfedgeMeshFailure> check_result = validate();
    if (check_result.has_value()) {
        return;
    }
    logger->info(
        "remesh the object {} (ID: {}) with strategy Isotropic Remeshing", object.name, object.id
    );
    logger->info("original mesh: {} vertices, {} faces", vertices.size, faces.size);
    // Compute the mean edge length.

    // Repeat the four main steps for 5 or 6 iterations
    // -> Split edges much longer than the target length (being careful about
    //    how the loop is written!)
    // -> Collapse edges much shorter than the target length.  Here we need to
    //    be EXTRA careful about advancing the loop, because many edges may have
    //    been destroyed by a collapse (which ones?)
    // -> Now flip each edge if it improves vertex degree
    // -> Finally, apply some tangential smoothing to the vertex positions
    static const size_t iteration_limit = 5;
    set<Edge*>          selected_edges;
    for (size_t i = 0; i != iteration_limit; ++i) {
        // Split long edges.

        // Collapse short edges.

        // Flip edges.

        // Vertex averaging.
    }
    logger->info("remeshed mesh: {} vertices, {} faces\n", vertices.size, faces.size);
    global_inconsistent = true;
    validate();
}

void HalfedgeMesh::smooth(float lambda)
{
    optional<HalfedgeMeshFailure> check_result = validate();
    if (check_result.has_value()) {
        return;
    }

    logger->info(
        "Laplacian smooth object {} (ID: {}) with lambda = {}", object.name, object.id, lambda
    );
    logger->info("original mesh: {} vertices, {} faces", vertices.size, faces.size);

    auto cotangent = [](const Vector3f& v, const Vector3f& vi, const Vector3f& opposite) {
        Vector3f a      = v - opposite;
        Vector3f b      = vi - opposite;
        Vector3f cross  = a.cross(b);
        float    denom  = cross.norm();
        float    result = 0.0f;
        if (denom > 1e-6f) {
            float cos_theta = a.dot(b);
            result          = cos_theta / denom;
        }
        return result;
    };

    // First pass: compute new positions using cotangent-weighted Laplacian.
    for (Vertex* v = vertices.head; v != nullptr; v = v->next_node) {
        Vector3f v_pos = v->pos;

        Halfedge* h0 = v->halfedge;
        if (!h0) {
            v->new_pos = v_pos;
            continue;
        }

        // Treat boundary vertices as fixed.
        bool      on_boundary = false;
        Halfedge* h           = h0;
        do {
            if (h->edge->on_boundary()) {
                on_boundary = true;
                break;
            }
            h = h->inv->next;
        } while (h != h0);

        if (on_boundary) {
            v->new_pos = v_pos;
            continue;
        }

        float    weight_sum = 0.0f;
        Vector3f weighted_neighbor_sum = Vector3f::Zero();

        h = h0;
        do {
            Halfedge* h_v_vi = h;
            Halfedge* h_vi_v = h_v_vi->inv;
            Vertex*   vi     = h_vi_v->from;

            float cot_alpha = 0.0f;
            float cot_beta  = 0.0f;

            if (!h_v_vi->face->is_boundary) {
                // Triangle on the left side: vertices are
                // v (h_v_vi->from), vi (h_v_vi->next->from),
                // and the opposite vertex is h_v_vi->next->next->from.
                Vertex* vl = h_v_vi->next->next->from;
                cot_alpha  = cotangent(v_pos, vi->pos, vl->pos);
            }

            if (!h_vi_v->face->is_boundary) {
                // Triangle on the right side: vertices are
                // vi (h_vi_v->from), v (h_vi_v->next->from),
                // and the opposite vertex is h_vi_v->next->next->from.
                Vertex* vr = h_vi_v->next->next->from;
                cot_beta   = cotangent(v_pos, vi->pos, vr->pos);
            }

            float w = 0.5f * (cot_alpha + cot_beta);

            if (w != 0.0f) {
                weight_sum += w;
                weighted_neighbor_sum += w * vi->pos;
            }

            h = h->inv->next;
        } while (h != h0);

        if (std::fabs(weight_sum) < 1e-8f) {
            v->new_pos = v_pos;
        } else {
            Vector3f avg_neighbor = weighted_neighbor_sum / weight_sum;
            Vector3f delta        = avg_neighbor - v_pos;
            v->new_pos            = v_pos + lambda * delta;
        }
    }

    // Second pass: write back new positions.
    for (Vertex* v = vertices.head; v != nullptr; v = v->next_node) {
        v->pos = v->new_pos;
    }

    global_inconsistent = true;
    logger->info("Laplacian smoothing done\n");
    validate();
}
