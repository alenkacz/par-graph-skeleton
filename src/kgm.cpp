/**
 *  \file kgm.cpp
 *  \brief KGM
 *  \author krcallub, varkoale
 *  \version 0.1
 *
 *  --
 */

#include <boost/graph/adjacency_list.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/timer.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/program_options.hpp>

#include <algorithm>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <list>
#include <utility>
#include <fstream>
#include <stack>

#include <stdint.h>

using namespace std;
//using namespace boost::filesystem;
using namespace boost;

static int32_t KGM_GRAPH_SIZE;
static int32_t KGM_UPPER_BOUND = 30;
static int32_t KGM_LOWER_BOUND = 2;
static int32_t KGM_START_NODE = 0;
static uint64_t KGM_REPORT_INTERVAL = 0x10000000;
//static path graphSource;

struct kgm_vertex_properties {
    kgm_vertex_properties() :
        state(false),
        degree(0) {}
    bool state;
    uint16_t degree;
};

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
typedef std::stack<kgm_edge_descriptor> kgm_stack;
typedef std::vector<uint16_t> degree_stack;

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

void dfs_step(
        dfs_stack& stack,
        degree_stack& dstack,
        ugraph& graph,
        uint16_t& degreeLimit)
{
    if (stack.empty())
        return;

    if (degreeLimit <= KGM_LOWER_BOUND){
        stack.clear();
        return;
    }

    if (graph[*(stack.back().a_it)].state == false)
    {
        graph[*(stack.back().v_it)].degree += 1;
        graph[*(stack.back().a_it)].degree = 1;
        graph[*(stack.back().a_it)].state = true;

        dstack.push_back(std::max(graph[*(stack.back().v_it)].degree, dstack.back()));

        if (dstack.back() >= degreeLimit)
        {   // not going to be a better solution
            dstack.pop_back();
            graph[*(stack.back().v_it)].degree -= 1;
            graph[*(stack.back().a_it)].degree = 0;
            graph[*(stack.back().a_it)].state = false;

            if (!iterate_dfs_state(stack.back(), graph))
                stack.pop_back();
            return;
        }

        dfs_state newState;
        if (create_dfs_state(newState, graph))
            stack.push_back(newState);
        else
        {
            std::cout << "New spanning tree - degree: "<< dstack.back() << std::endl;
            degreeLimit = dstack.back();
            // TODO possible spanning tree improvement
            std::cout << stack << std::endl;
        }
        return;
    }

    dfs_state prev (stack.back());
    graph[*(prev.v_it)].degree -= 1;
    graph[*(prev.a_it)].degree = 0;
    graph[*(prev.a_it)].state = false;

    dstack.pop_back();

    if (!iterate_dfs_state(stack.back(), graph))
        stack.pop_back();
}

int main(int argc, char ** argv) {

    if (argc <= 1)
    {
        std::cerr << "Not enough arguments." << std::endl;
        return -1;
    }

    std::string filename (argv[1]);
    if (filename.empty())
        return -1;

    std::ifstream in(filename.c_str());
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string contents(buffer.str());

    std::vector<std::string> lines;
    boost::split(lines, contents, boost::is_any_of("\n"));

    int m = lines.size()-2, n = lines[lines.size()-2].length();
    std::cout << "loaded graph of size: " << m << "*" << n << std::endl;
    KGM_GRAPH_SIZE = n;
    if (m != n && m <= 1)
    {
        std::cerr << "Input data error" << std::endl;
        return -3;
    }

    ugraph g(KGM_GRAPH_SIZE);
    std::vector<std::string>::iterator it = lines.begin();
    ++it;
    for (int i = 0; it != lines.end(); ++it, ++i)
    {
        if ((*it).empty())
            continue;
        for (int j = 0; j <= i; ++j)
        {
            if ((*it)[j] == '1')
                add_edge(i,j,g);
        }
    }

    dfs_state firstState;
    g[KGM_START_NODE].state = true;
    if (!create_dfs_state(firstState,g))
    {
        std::cerr << "Failed to initialize first state" << std::endl;
        return -4;
    }

    dfs_stack stack;
    stack.push_back(firstState);
    degree_stack dstack;
    dstack.push_back(0);

    uint16_t minSPdegree = KGM_UPPER_BOUND;
    uint64_t steps = 0;
    uint64_t psteps = KGM_REPORT_INTERVAL;
    boost::scoped_ptr<boost::timer> timer (new boost::timer);
    while (!stack.empty())
    {
        dfs_step(stack, dstack, g, minSPdegree);
        if (steps >= psteps)
        {
            std::cout << "Stack at " << timer->elapsed() << ":" << std::endl
                    << make_pair(stack.front(),g) << std::endl;
            psteps += KGM_REPORT_INTERVAL;
        }
        ++steps;
    }
    double time = timer->elapsed();
    timer.reset();
    std::cout << "-------------" << std::endl;
    std::cout << "TOTAL STEPS: " << steps << std::endl;
    std::cout << "In time: " << time << std::endl;

    return 0;
}
