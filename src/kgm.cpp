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
#include <string>
#include <iostream>
#include <stdint.h>
#include <vector>
#include <list>
#include <utility>

using namespace std;
using namespace boost::filesystem;
using namespace boost;

static const uint32_t KGM_GRAPH_SIZE = 7;
static const uint32_t KGM_START_NODE = 0;

struct kgm_vertex_properties {
	kgm_vertex_properties() :
		state(false),
		degree(0) {}
    bool state;
    uint_fast16_t degree;
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
typedef graph_traits<ugraph>::adjacency_iterator adj_iterator;
typedef std::pair<adj_iterator, adj_iterator> kgm_state;
typedef std::vector<kgm_state> kgm_stack;
//typedef property_map<ugraph, kgm_vertex_properties>::type pmap_vertex_properties;

ostream& operator<< (ostream& out, const adj_iterator& ai)
{
	out << *ai;
	return out;
}

ostream& operator<< (ostream& out, const kgm_state& state )
{
	out << "[";
	kgm_state tmp (state);
	while (tmp.first != tmp.second)
	{
		out << *(tmp.first) << " ";
		++tmp.first;
	}
    out << "]";
	return out;
}

ostream& operator<< (ostream& out, const kgm_stack& stack)
{
	for (kgm_stack::const_iterator it = stack.begin();
			it != stack.end(); ++it)
		out << *it;
	return out;
}

void dfs_step(
		kgm_stack& stack,
		ugraph& graph,
		uint_fast32_t& maxDeg)
{
	if (stack.empty())
		return;
	std::cout << "stack: " << stack << std::endl;

	if (stack.back().first == stack.back().second)
	{	// no nodes remaining in this level
		std::cout << "step back" << std::endl;
		stack.pop_back();
		if (stack.empty())
			return;
		graph[*(stack.back().first)].state = false;
		++(stack.back().first);
		return;
	}

	std::cout << "state of node " << *(stack.back().first) << ": "
			<< (unsigned int)graph[*(stack.back().first)].state << std::endl;

	if ((unsigned int)graph[*(stack.back().first)].state == true)
	{	// node is already a part of current skeleton
		++(stack.back().first);
		return;
	}

	if (stack.back().first != stack.back().second)
	{   // enter not currently visited node
		uint_fast32_t currDegree = graph[*(stack.back().first)].degree;
		std::cout << "degree of " << *(stack.back().first) << ": "
				<< currDegree << std::endl;
		if (currDegree > maxDeg)
			maxDeg = currDegree;

		std::cout << "entering: " << *(stack.back().first) << std::endl;
		graph[*(stack.back().first)].state = true;
		kgm_state newState = adjacent_vertices(*(stack.back().first), graph);
		stack.push_back(newState);
		return;
	}
}

int main() {
	path graphSource("u20.graph");
//	if (!is_regular_file(graphSource))
//	{
//		cout << "Input file not regular." << endl;
//		return -1;
//	}
//	if (boost::filesystem::is_empty(graphSource))
//	{
//		cout << "Input file is empty" << endl;
//		return -2;
//	}

	// TODO
	// load graph from file
	// load property: degree

	ugraph g(KGM_GRAPH_SIZE);
	add_edge(0,1,g);
	add_edge(0,2,g);
	add_edge(1,1,g);
	add_edge(2,1,g);
	add_edge(2,3,g);
	add_edge(3,5,g);
	add_edge(2,6,g);
	add_edge(5,6,g);

	kgm_state firstState = adjacent_vertices(KGM_START_NODE, g);
	g[KGM_START_NODE].state = true;
	kgm_stack stack;
	stack.push_back(firstState);

	uint_fast32_t steps = 0;
	uint_fast16_t maxDegree = 1;
	while (!stack.empty())
	{
		std::cout << "-------------" << std::endl;
		dfs_step(stack, g, maxDegree);
		++steps;
	}

	std::cout << "-------------" << std::endl;
	cout << "TOTAL STEPS: " << steps << std::endl;

	return 0;
}
