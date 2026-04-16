#ifndef NAUTYPP_IMPL_HPP
#define NAUTYPP_IMPL_HPP

#include <bit>

#include <nautypp/algorithms.hpp>
#include <nautypp/graph.hpp>
#include <nautypp/iterators.hpp>
#include <nautypp/properties.hpp>

namespace nautypp {
void DegreeProperty::compute() {
    const auto m{graph->__get_m()};
    auto gv{GRAPHROW(static_cast<const nauty_graph_t*>(*graph), v, m)};
    value = 0;
    for(size_t i{0}; i < m; ++i, ++gv)
        value += std::popcount(*gv);
}

void EdgeProperty::compute() {
    value = 0;
    for(size_t v{0}; v < graph->V(); ++v)
        value += graph->degree(v);
    if(!graph->is_directed())
        value >>= 1;  // sum(d(v)) == 2E for undirected
}

void _CliquerGraphProperty::compute() {
    clear();
    value = graph_new(graph->V());
    for(auto [v, w] : graph->edges()) {
        GRAPH_ADD_EDGE(value, v, w);
    }
}

ConnectedComponents::ConnectedComponents(const Graph& graph):
        G{graph}, ids(graph.V(), UNVISITED), nb_components{0} {
    _run();
}

void ConnectedComponents::_run() {
    for(Vertex v{0}; v < G.V(); ++v) {
        if(ids[v] == UNVISITED) {
            _run(v);
            ++nb_components;
        }
    }
}

StronglyConnectedComponents::StronglyConnectedComponents(const Graph& graph):
        G{graph}, ids(graph.V(), UNVISITED), nb_components{0} {
    _run();
}

#ifdef OSTREAM_LL_GRAPH
static std::ostream& operator<<(std::ostream& os, const nautypp::Graph& G) {
    os << "Graph(";
    bool first{true};
    for(auto [v, w] : G.edges()) {
        if(first)
            first = false;
        else
            os << ", ";
        os << v << "-" << w;
    }
    return os << ")";
}
#endif
#ifdef OSTREAM_LL_CLIQUE
static std::ostream& operator<<(std::ostream& os, const nautypp::Cliquer::Set& S) {
    os << '{';
    bool first{true};
    for(auto v : S) {
        if(first)
            first = false;
        else
            os << ", ";
        os << v;
    }
    return os << '}';
}
#endif
}

#endif
