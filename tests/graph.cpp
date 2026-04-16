#include <algorithm>

#include <catch2/catch.hpp>

#include <nautypp/nautypp>

static inline uint64_t binom2(uint64_t n) {
    return (n*(n-1)) / 2;
}

template <typename T>
static inline bool contains(const std::vector<T>& v, const T& x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

using namespace nautypp;

TEST_CASE("empty graph is empty") {
    constexpr size_t V{5};
    Graph G(V);
    REQUIRE(G.V() == V);
    REQUIRE(G.E() == 0);

    size_t nb_edges{0};
    for(auto _ : G.edges())
        ++nb_edges;
    REQUIRE(nb_edges == 0);

    nb_edges = 0;
    for(Vertex v{0}; v < G.V(); ++v)
        for(Vertex _ : G.neighbors_of(v))
            ++nb_edges;
    REQUIRE(nb_edges == 0);

    for(Vertex v{0}; v < G.V(); ++v)
        REQUIRE(G.degree(v) == 0);
}

TEST_CASE("Complete graph is complete") {
    size_t n = GENERATE(range(1, 10));
    auto Kn{Graph::make_complete(n)};
    REQUIRE(Kn.V() == n);
    REQUIRE(Kn.E() == binom2(n));
    for(Vertex v{0}; v < Kn.V(); ++v) {
        REQUIRE(Kn.degree(v) == n-1);
        std::vector<Vertex> Nv{Kn.neighbours_of(v)};
        REQUIRE(Nv.size() == n-1);
        for(Vertex w{0}; w < Kn.V(); ++w) {
            if(w == v)
                continue;
            REQUIRE(contains(Nv, w));
        }
        REQUIRE(not contains(Nv, v));
    }
}

TEST_CASE("Complete bipartite is complete bipartite") {
    size_t s = GENERATE(range(1, 6));
    size_t t = GENERATE(range(1, 6));
    auto Kst{Graph::make_complete_bipartite(s, t)};

    std::vector<Vertex> first_part;
    first_part.reserve(s);
    for(size_t v{0}; v < s; ++v)
        first_part.push_back(v);
    std::vector<Vertex> second_part;
    for(size_t v{0}; v < t; ++v)
        second_part.push_back(s+v);
    second_part.reserve(t);

    REQUIRE(Kst.V() == s+t);
    REQUIRE(Kst.E() == s*t);

    for(Vertex v{0}; v < Kst.V(); ++v) {
        std::vector<Vertex> Nv{Kst.neighbours_of(v)};
        auto& expected{
            contains(first_part, v) ? second_part : first_part
        };
        REQUIRE(Nv == expected);
    }
}

TEST_CASE("Disjoint union") {
    constexpr size_t n{5};
    auto G1{Graph::make_complete_bipartite(n, n)};
    auto G2{Graph::make_complete(n)};
    auto G{Graph::disjoint_union(G1, G2)};
    REQUIRE(G.V() == 15);
    REQUIRE(G.E() == G1.E() + G2.E());
    REQUIRE(G.nb_connected_components() == 2);

    std::vector<Vertex> first_component;
    std::vector<Vertex> second_component;
    for(size_t v{0}; v < n; ++v) {
        first_component.push_back(2*v + 0);
        first_component.push_back(2*v + 1);
        second_component.push_back(2*n + v);
    }
    auto components{G.get_connected_components()};
    REQUIRE(components.get_component_of(0) == first_component);
    REQUIRE(components.get_component_of(G.V()-1) == second_component);
}

TEST_CASE("Directed graph: basic arc operations") {
    constexpr size_t V{4};
    auto G{Graph::make_directed(V)};
    REQUIRE(G.is_directed());
    REQUIRE(G.V() == V);
    REQUIRE(G.E() == 0);

    // Add arc 0->1
    G.add_arc(0, 1);
    REQUIRE(G.E() == 1);
    REQUIRE(G.are_linked(0, 1));
    REQUIRE(!G.are_linked(1, 0));  // directed: asymmetric

    // Degree checks
    REQUIRE(G.out_degree(0) == 1);
    REQUIRE(G.in_degree(0) == 0);
    REQUIRE(G.out_degree(1) == 0);
    REQUIRE(G.in_degree(1) == 1);

    // More arcs: 1->2, 2->0
    G.add_arc(1, 2);
    G.add_arc(2, 0);
    REQUIRE(G.E() == 3);

    REQUIRE(G.out_degree(0) == 1);
    REQUIRE(G.in_degree(0) == 1);
    REQUIRE(G.out_degree(1) == 1);
    REQUIRE(G.in_degree(1) == 1);
    REQUIRE(G.out_degree(2) == 1);
    REQUIRE(G.in_degree(2) == 1);
    REQUIRE(G.out_degree(3) == 0);
    REQUIRE(G.in_degree(3) == 0);
}

TEST_CASE("Directed graph: remove_arc") {
    auto G{Graph::make_directed(3)};
    G.add_arc(0, 1);
    G.add_arc(1, 2);
    REQUIRE(G.E() == 2);

    G.remove_arc(0, 1);
    REQUIRE(G.E() == 1);
    REQUIRE(!G.are_linked(0, 1));
    REQUIRE(G.are_linked(1, 2));
}

TEST_CASE("Directed graph: in/out neighbours") {
    // Cycle: 0->1->2->0
    auto G{Graph::make_directed(3)};
    G.add_arc(0, 1);
    G.add_arc(1, 2);
    G.add_arc(2, 0);

    std::vector<Vertex> out_N0{G.out_neighbours_of(0)};
    REQUIRE(out_N0 == std::vector<Vertex>{1});

    std::vector<Vertex> in_N0{G.in_neighbours_of(0)};
    REQUIRE(in_N0 == std::vector<Vertex>{2});

    std::vector<Vertex> out_N1{G.out_neighbours_of(1)};
    REQUIRE(out_N1 == std::vector<Vertex>{2});

    std::vector<Vertex> in_N1{G.in_neighbours_of(1)};
    REQUIRE(in_N1 == std::vector<Vertex>{0});
}

TEST_CASE("Directed graph: arc enumeration counts all arcs") {
    auto G{Graph::make_directed(3)};
    G.add_arc(0, 1);
    G.add_arc(0, 2);
    G.add_arc(2, 1);

    size_t arc_count{0};
    for(auto [v, w] : G.edges())
        ++arc_count;
    REQUIRE(arc_count == 3);
    REQUIRE(G.E() == 3);

    // Arcs alias
    arc_count = 0;
    for(auto [v, w] : G.arcs())
        ++arc_count;
    REQUIRE(arc_count == 3);
}

TEST_CASE("Directed graph: strongly connected components") {
    // 0->1->2->0 (one SCC), 3->2 (3 is its own SCC)
    auto G{Graph::make_directed(4)};
    G.add_arc(0, 1);
    G.add_arc(1, 2);
    G.add_arc(2, 0);
    G.add_arc(3, 2);

    auto scc{G.get_strongly_connected_components()};
    REQUIRE(scc.get_nb_components() == 2);
    REQUIRE(scc.get_component_identifier_of(0) == scc.get_component_identifier_of(1));
    REQUIRE(scc.get_component_identifier_of(1) == scc.get_component_identifier_of(2));
    REQUIRE(scc.get_component_identifier_of(3) != scc.get_component_identifier_of(0));
}

TEST_CASE("Directed graph: complement") {
    // Tournament on 3 vertices: 0->1, 1->2, 0->2
    auto G{Graph::make_directed(3)};
    G.add_arc(0, 1);
    G.add_arc(1, 2);
    G.add_arc(0, 2);
    REQUIRE(G.E() == 3);

    auto Gc{G.complement()};
    REQUIRE(Gc.is_directed());
    // complement arcs: 1->0, 2->1, 2->0
    REQUIRE(Gc.E() == 3);
    REQUIRE(Gc.are_linked(1, 0));
    REQUIRE(Gc.are_linked(2, 1));
    REQUIRE(Gc.are_linked(2, 0));
    REQUIRE(!Gc.are_linked(0, 1));
}

TEST_CASE("Directed graph: disjoint union") {
    auto G1{Graph::make_directed(2)};
    G1.add_arc(0, 1);
    auto G2{Graph::make_directed(2)};
    G2.add_arc(0, 1);

    auto G{Graph::disjoint_union(G1, G2)};
    REQUIRE(G.is_directed());
    REQUIRE(G.V() == 4);
    REQUIRE(G.E() == 2);
    REQUIRE(G.are_linked(0, 1));
    REQUIRE(G.are_linked(2, 3));
    REQUIRE(!G.are_linked(1, 0));
    REQUIRE(!G.are_linked(3, 2));
}

TEST_CASE("Directed graph: from_adjacency_matrix") {
    // 3-cycle: 0->1, 1->2, 2->0
    std::vector<int> A{
        0, 1, 0,
        0, 0, 1,
        1, 0, 0
    };
    auto G{Graph::from_adjacency_matrix(A, 3, true, true)};
    REQUIRE(G.is_directed());
    REQUIRE(G.E() == 3);
    REQUIRE(G.are_linked(0, 1));
    REQUIRE(G.are_linked(1, 2));
    REQUIRE(G.are_linked(2, 0));
    REQUIRE(!G.are_linked(1, 0));
    REQUIRE(!G.are_linked(2, 1));
    REQUIRE(!G.are_linked(0, 2));
}

TEST_CASE("Directed graph: copy preserves directed flag") {
    auto G{Graph::make_directed(3)};
    G.add_arc(0, 1);
    auto H{G.copy()};
    REQUIRE(H.is_directed());
    REQUIRE(H.are_linked(0, 1));
    REQUIRE(!H.are_linked(1, 0));
}

TEST_CASE("Directed graph: disjoint union of directed and undirected throws") {
    auto Gd{Graph::make_directed(2)};
    auto Gu{Graph(2)};
    REQUIRE_THROWS(Graph::disjoint_union(Gd, Gu));
}

TEST_CASE("Directed graph: is_planar throws") {
    auto G{Graph::make_directed(3)};
    G.add_arc(0, 1);
    REQUIRE_THROWS(G.is_planar());
}
    auto k = GENERATE(values({6, 8, 10, 12, 14}));
    auto n{1ull << (k-1)};
    auto K2n{Graph::make_complete(n)};  // K_{2^n}
    REQUIRE(K2n.V() == n);
    REQUIRE(K2n.E() == binom2(n));

    std::vector<Vertex> expected_neighbours;
    expected_neighbours.reserve(n-1);
    for(Vertex v{0}; v < n; ++v) {
        expected_neighbours.clear();
        for(Vertex w{0}; w < n; ++w)
            if(w != v)
                expected_neighbours.push_back(w);
        std::vector<Vertex> Nv{K2n.neighbours_of(v)};
        REQUIRE(expected_neighbours == Nv);
    }
}
