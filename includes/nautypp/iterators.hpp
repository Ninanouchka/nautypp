#ifndef NAUTYPP_ITERATORS_HPP
#define NAUTYPP_ITERATORS_HPP

#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

#include <nauty/nauty.h>

#include <nautypp/aliases.hpp>


namespace nautypp {

class Graph;
class AllEdgeIterator;

/*      *************** Iterables and Iterators ***************      */

/// \class EdgeIterator
/// \brief Iterator over edges incident to some vertex
///
/// **Example**:
/// \include iterators/neighbours.cpp
class EdgeIterator {
public:
    EdgeIterator() = delete;
    EdgeIterator(const Graph&, Vertex vertex, bool end=false);
    //EdgeIterator(const Graph&, Vertex vertex, setword Nv);
    inline bool operator==(const EdgeIterator& other) const {
        return std::addressof(graph) == std::addressof(other.graph)
            and v == other.v
            and gv == other.gv
            and w == other.w;
    }
    inline bool operator!=(const EdgeIterator& other) const {
        return not (*this == other);
    }
    inline EdgeIterator& operator=(EdgeIterator&& other) {
        if(std::addressof(graph) != std::addressof(other.graph))
            throw std::runtime_error(
                    "Cannot reassign EdgeIterator to a new graph"
            );
        v = other.v;
        w = other.w;
        gv = other.gv;
        return *this;
    }
    EdgeIterator& operator++();
    inline std::pair<Vertex, Vertex> operator*() const {
        return {v, w};
    }
private:
    const Graph& graph;
    Vertex v, w;
    setword* gv;

    friend AllEdgeIterator;
    inline void goto_after_v() {
        while(w != NO_VERTEX and w < v)
            ++*this;
    }
};

/// \class AllEdgeIterator
/// \brief Iterator over all edges of a graph
///
/// See Graph::edges()
/// **Example**:
/// \include iterators/neighbours.cpp
class AllEdgeIterator {
public:
    AllEdgeIterator() = delete;
    AllEdgeIterator(const Graph&, bool end=false);
    inline bool operator==(const AllEdgeIterator& other) const {
        return it == other.it;
    }
    inline bool operator!=(const AllEdgeIterator& other) const {
        return not (*this == other);
    }
    inline AllEdgeIterator& operator++() {
        next();
        return *this;
    }
    inline std::pair<Vertex, Vertex> operator*() const {
        return *it;
    }
private:
    const Graph& graph;
    Vertex v;
    EdgeIterator it;
    EdgeIterator it_end;

    void next();

    /// MASKS[i] = (0xFF...FF) >> (i+1)
    static constexpr std::array<setword, WORDSIZE> MASKS{[]{
        std::array<setword, WORDSIZE> ret{};
        auto value{static_cast<setword>(-1)};
        for(size_t i{0}; i < WORDSIZE; ++i)
            ret[i] = (value >>= 1);
        //setword value{-1};
        return ret;
    }()};
};

/// \class Edges
/// \brief Iterable over all edges incident to some vertex
///
/// See EdgeIterator and Graph::edges_incident_to()
class Edges {
public:
    Edges() = delete;
    Edges(const Graph& G, Vertex vertex):
            graph{G}, v{vertex} {
    }

    inline EdgeIterator begin() const {
        return {graph, v};
    }
    inline EdgeIterator end() const {
        return {graph, v, true};
    }
private:
    const Graph& graph;
    Vertex v;
};

/// \class AllEdges
/// \brief Iterable over all edges of a graph
///
/// See AllEdgeIterator and Graph::edges()
class AllEdges {
public:
    AllEdges() = delete;
    AllEdges(const Graph& G):
            graph{G} {
    }

    inline AllEdgeIterator begin() const {
        return {graph, false};
    }
    inline AllEdgeIterator end() const {
        return {graph, true};
    }

    operator std::vector<std::pair<Vertex, Vertex>>() const;

    inline std::vector<std::pair<Vertex, Vertex>> as_vector() const {
        return *this;
    }
private:
    const Graph& graph;
};

class NeighbourIterator {
public:
    NeighbourIterator() = delete;
    NeighbourIterator(const Graph& G, Vertex vertex, bool end=false):
            it(G, vertex, end) {
    }
    NeighbourIterator(const Graph& G, Vertex vertex, setword Nv):
            it(G, vertex, Nv) {
    }
    inline bool operator==(const NeighbourIterator& other) const {
        return it == other.it;
    }
    inline bool operator!=(const NeighbourIterator& other) const {
        return it != other.it;
    }
    inline NeighbourIterator& operator=(NeighbourIterator&& other) {
        it = std::move(other.it);
        return *this;
    }
    inline NeighbourIterator& operator++() {
        ++it;
        return *this;
    }
    inline Vertex operator*() const {
        return (*it).second;
    }
private:
    EdgeIterator it;
};

class Neighbours {
public:
    Neighbours() = delete;
    Neighbours(const Graph& G, Vertex vertex):
            graph{G}, v{vertex} {
    }

    inline NeighbourIterator begin() const {
        return {graph, v};
    }
    inline NeighbourIterator end() const  {
        return {graph, v, true};
    }

    operator std::vector<Vertex>() const;
private:
    const Graph& graph;
    Vertex v;
};

/// \class InNeighbourIterator
/// \brief Iterator over in-neighbours (predecessors) of a vertex in a directed graph.
///
/// Iterates over all vertices `u` such that an arc `u → target` exists.
class InNeighbourIterator {
public:
    InNeighbourIterator() = delete;
    InNeighbourIterator(const Graph& G, Vertex target, bool end=false);

    inline bool operator==(const InNeighbourIterator& other) const {
        return u == other.u and target == other.target;
    }
    inline bool operator!=(const InNeighbourIterator& other) const {
        return not (*this == other);
    }
    inline InNeighbourIterator& operator++() {
        advance();
        return *this;
    }
    inline Vertex operator*() const {
        return u;
    }
private:
    const Graph& graph;
    Vertex target;
    Vertex u;  ///< current in-neighbour (NO_VERTEX = end)

    void advance();
};

/// \class InNeighbours
/// \brief Iterable over all in-neighbours (predecessors) of a vertex.
///
/// See InNeighbourIterator and Graph::in_neighbours_of()
class InNeighbours {
public:
    InNeighbours() = delete;
    InNeighbours(const Graph& G, Vertex target):
            graph{G}, target{target} {
    }

    inline InNeighbourIterator begin() const {
        return {graph, target, false};
    }
    inline InNeighbourIterator end() const {
        return {graph, target, true};
    }

    operator std::vector<Vertex>() const;
private:
    const Graph& graph;
    Vertex target;
};

}

#endif
