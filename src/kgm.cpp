/**
 *  \file kgm.cpp
 *  \brief KGM
 *  \author krcallub, varkoale
 *  \version 0.1
 *
 *  --
 */

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/tuple/tuple.hpp>
#include <string>
#include <iostream>
#include <stdint.h>
#include <vector>
#include <list>
#include <utility>

using namespace std;
using namespace boost::filesystem;
using namespace boost;

static const int32_t KGM_GRAPH_SIZE = 7;
static const int32_t KGM_START_NODE = 0;

struct kgm_vertex_properties {
    kgm_vertex_properties() :
        state(false),
        degree(0) {}
    bool state;
    uint16_t degree;
};

//struct kgm_skeleton_node {
//	kgm_skeleton_node() :
//		node(0),
//		maxDegree(INT32_MAX) {}
//	int32_t node;
//	uint16_t maxDegree;
//};

enum VERTEX_KGM_STATE {
    NEW,
    OPEN,
    __VERTEX_KGM_STATE_SIZE
};

typedef adjacency_list<
                listS, // std::list
                vecS, // std::vector
                undirectedS,
                kgm_vertex_properties> ugraph;

typedef graph_traits<ugraph>::edge_descriptor       kgm_edge_descriptor;
typedef graph_traits<ugraph>::vertex_descriptor     kgm_vertex_descriptor;
typedef graph_traits<ugraph>::edge_iterator         kgm_edge_iterator;
typedef graph_traits<ugraph>::vertex_iterator       kgm_vertex_iterator;
typedef graph_traits<ugraph>::adjacency_iterator    kgm_adjacency_iterator;

struct dfs_state {
    kgm_vertex_iterator v_it;
    kgm_vertex_iterator v_it_end;
    kgm_adjacency_iterator a_it;
    kgm_adjacency_iterator a_it_end;
};
typedef std::vector<dfs_state> dfs_stack;

ostream& operator<< (ostream& out, const kgm_adjacency_iterator& ai);
ostream& operator<< (ostream& out, const kgm_vertex_iterator& ai);
ostream& operator<< (ostream& out, const dfs_state& state);
ostream& operator<< (ostream& out, const std::pair<dfs_state,ugraph>& state);
ostream& operator<< (ostream& out, const dfs_stack& stack);

bool iterate_dfs_state(dfs_state& dfsState, const ugraph& graph)
{
    for (++dfsState.a_it; dfsState.a_it != dfsState.a_it_end; ++dfsState.a_it)
    {
        if (graph[*(dfsState.a_it)].state == false)
            return true;
    }
    for (++dfsState.v_it; dfsState.v_it != dfsState.v_it_end; ++dfsState.v_it)
    {
        if (graph[*(dfsState.v_it)].state == false)
            continue;

        for(tie(dfsState.a_it, dfsState.a_it_end) = adjacent_vertices(*(dfsState.v_it),graph);
                dfsState.a_it != dfsState.a_it_end; ++dfsState.a_it)
        {
            if (graph[*(dfsState.a_it)].state == false)
                return true;
        }
    }
    return false;
}

bool create_dfs_state(dfs_state& newState, const ugraph& graph)
{
    for (tie(newState.v_it, newState.v_it_end) = vertices(graph);
            newState.v_it != newState.v_it_end; ++newState.v_it)
    {
        if (graph[*(newState.v_it)].state == false)
            continue;

        for(tie(newState.a_it, newState.a_it_end) = adjacent_vertices(*(newState.v_it),graph);
                newState.a_it != newState.a_it_end; ++newState.a_it)
        {
            if (graph[*(newState.a_it)].state == false)
                return true;
        }
    }
    return false;
}

ostream& operator<< (ostream& out, const kgm_adjacency_iterator& ai)
{
    out << *ai;
    return out;
}

ostream& operator<< (ostream& out, const kgm_vertex_iterator& ai)
{
    out << *ai;
    return out;
}

ostream& operator<< (ostream& out, const dfs_state& state)
{
    out << "[" << *(state.v_it) << "->" << *(state.a_it) << "]";
    return out;
}

ostream& operator<< (ostream& out, const std::pair<dfs_state,ugraph>& state)
{
    out << "[";
    dfs_state tmp (state.first);
    bool first = true;
    do {
        if (first)
            first = false;
        else
            out << ", ";
        out << *(tmp.v_it) << "->" << *(tmp.a_it);
    }
    while (iterate_dfs_state(tmp, state.second));
    out << "]";
    return out;
}

ostream& operator<< (ostream& out, const dfs_stack& stack)
{
    for (dfs_stack::const_iterator it = stack.begin();
            it != stack.end(); ++it)
        out << *it;
    return out;
}

//void dfs_step(
//        dfs_stack& stack,
//        ugraph& graph,
//        uint_fast32_t& maxDeg)
//{
//    if (stack.empty())
//        return;
//    std::cout << "stack: " << stack << std::endl;
//
//    if (stack.back().first == stack.back().second)
//    {    // no nodes remaining in this level
//        std::cout << "step back" << std::endl;
//        stack.pop_back();
//        if (stack.empty())
//            return;
//        graph[*(stack.back().first)].state = false;
//        ++(stack.back().first);
//        return;
//    }
//
//    std::cout << "state of node " << *(stack.back().first) << ": "
//            << (unsigned int)graph[*(stack.back().first)].state << std::endl;
//
//    if ((unsigned int)graph[*(stack.back().first)].state == true)
//    {    // node is already a part of current skeleton
//        ++(stack.back().first);
//        return;
//    }
//
//    if (stack.back().first != stack.back().second)
//    {   // enter not currently visited node
//        uint_fast32_t currDegree = graph[*(stack.back().first)].degree;
//        std::cout << "degree of " << *(stack.back().first) << ": "
//                << currDegree << std::endl;
//        if (currDegree > maxDeg)
//            maxDeg = currDegree;
//
//        std::cout << "entering: " << *(stack.back().first) << std::endl;
//        graph[*(stack.back().first)].state = true;
//        dfs_state newState = adjacent_vertices(*(stack.back().first), graph);
//        stack.push_back(newState);
//        return;
//    }
//}

int main() {
    path graphSource("u20.graph");
//    if (!is_regular_file(graphSource))
//    {
//        cout << "Input file not regular." << endl;
//        return -1;
//    }
//    if (boost::filesystem::is_empty(graphSource))
//    {
//        cout << "Input file is empty" << endl;
//        return -2;
//    }

    // TODO
    // load graph from file

    ugraph g(KGM_GRAPH_SIZE);
    add_edge(0,1,g);
    add_edge(0,2,g);
    add_edge(1,1,g);
    add_edge(2,1,g);
    add_edge(2,3,g);
    add_edge(3,5,g);
    add_edge(2,6,g);
    add_edge(5,6,g);

    dfs_state firstState;
    if (create_dfs_state(firstState,g))
//        std::cout << firstState << std::endl;
    std::cout << make_pair(firstState,g) << std::endl;

    g[KGM_START_NODE].state = true;
    if (create_dfs_state(firstState,g))
//        std::cout << firstState << std::endl;
    std::cout << make_pair(firstState,g) << std::endl;

    g[1].state = true;
    if (create_dfs_state(firstState,g))
//        std::cout << firstState << std::endl;
    std::cout << make_pair(firstState,g) << std::endl;

    g[2].state = true;
    if (create_dfs_state(firstState,g))
//        std::cout << firstState << std::endl;
    std::cout << make_pair(firstState,g) << std::endl;

    g[5].state = true;
    if (create_dfs_state(firstState,g))
//        std::cout << firstState << std::endl;
    std::cout << make_pair(firstState,g) << std::endl;

//    do {std::cout << firstState << std::endl;}
//    while (iterate_dfs_state(firstState,g));


//    adjacent_vertices(KGM_START_NODE, g);
//    g[KGM_START_NODE].state = true;
//    dfs_stack stack;
//    stack.push_back(firstState);
//
//    uint_fast32_t steps = 0;
//    uint_fast16_t maxDegree = 1;
//    while (!stack.empty())
//    {
//        std::cout << "-------------" << std::endl;
////        dfs_step(stack, g, maxDegree);
//        ++steps;
//    }
//
    std::cout << "-------------" << std::endl;
//    cout << "TOTAL STEPS: " << steps << std::endl;

    return 0;
}
