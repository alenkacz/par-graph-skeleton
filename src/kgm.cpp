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

static const uint32_t KGM_GRAPH_SIZE = 20;

struct vertex_kgm_state {
    typedef vertex_property_tag kind;
};

enum VERTEX_KGM_STATE {
	NEW,
	OPEN,
	__VERTEX_KGM_STATE_SIZE
};

struct uint8_t_d {
	uint8_t_d() : value(0) {}
	uint8_t value;
};

//typedef property<vertex_kgm_state, uint8_t_d> vertex_kgm_state_property;
typedef property<vertex_kgm_state, uint8_t> vertex_kgm_state_property;
typedef adjacency_list<
				listS, // std::list
				vecS, // std::vector
				undirectedS,
				vertex_kgm_state_property> ugraph;
typedef graph_traits<ugraph>::adjacency_iterator adj_iterator;
typedef std::pair<adj_iterator, adj_iterator> kgm_state;
typedef std::vector<kgm_state> kgm_stack;
typedef property_map<ugraph, vertex_kgm_state>::type pmap_node_states;

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
		pmap_node_states& nodeStates,
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
		nodeStates[*(stack.back().first)] = 0;
		++(stack.back().first);
		return;
	}

	std::cout << "state of node " << *(stack.back().first) << ": "
			<< (unsigned int)nodeStates[*(stack.back().first)] << std::endl;

	if ((unsigned int)nodeStates[*(stack.back().first)] > 0)
	{	// node is already a part of current skeleton
		++(stack.back().first);
		return;
	}

	if (stack.back().first != stack.back().second)
	{   // enter not currently visited node
		std::cout << "entering: " << *(stack.back().first) << std::endl;
		nodeStates[*(stack.back().first)] = 1;
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



	ugraph g(KGM_GRAPH_SIZE);
	add_edge(0,1,g);
	add_edge(0,2,g);
	add_edge(1,1,g);
	add_edge(2,1,g);
	add_edge(2,3,g);
	add_edge(3,5,g);
	add_edge(2,6,g);
	add_edge(5,6,g);

	pmap_node_states nodeStates = get(vertex_kgm_state(), g);

	kgm_state firstState = adjacent_vertices(0, g);
	nodeStates[0] = 1;
	kgm_stack stack;
	stack.push_back(firstState);

	uint_fast32_t maxDeg = 0, steps = 0;
//	while (!stack.empty() && limit > 0)
	while (!stack.empty())
	{
		std::cout << "-------------" << std::endl;
//		std::cout << "steps remaining: " << limit << std::endl;
		dfs_step(stack,g,nodeStates,maxDeg);
		++steps;
	}

	std::cout << "-------------" << std::endl;
	cout << "TOTAL STEPS: " << steps << std::endl;

	return 0;
}
