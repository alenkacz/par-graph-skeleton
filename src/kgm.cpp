//============================================================================
// Name        : kgm.cpp
// Author      : krcallub
// Version     : 0.1
//============================================================================

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

typedef property<vertex_kgm_state, uint8_t_d> vertex_kgm_state_property;
typedef adjacency_list<
				listS, // std::list
				vecS, // std::vector
				undirectedS,
				vertex_kgm_state_property> ugraph;



typedef graph_traits<ugraph>::adjacency_iterator adj_iterator;

typedef std::pair<adj_iterator, adj_iterator> kgm_state;

typedef std::vector<kgm_state> kgm_stack;

ostream& operator<< (ostream& out, const adj_iterator& ai)
{
	out << *ai;
	return out;
}

ostream& operator<< (ostream& out, const kgm_state& state )
{
	out << "[" << *(state.first) << "," << *(state.second) << "]";
	return out;
}

ostream& operator<< (ostream& out, const kgm_stack& stack)
{
	for (kgm_stack::const_iterator it = stack.begin();
			it != stack.end(); ++it)
		out << *it;
	return out;
}

void dfs(kgm_stack& stack, uint_fast16_t& maxDeg)
{

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
//	adjacency_list<std::list<unsigned int>, std::vector<unsigned int>, undirectedS> g;
//	add_edge((uint32_t)0,(uint32_t)0,g);
	add_edge(0,1,g);
	add_edge(0,2,g);
	add_edge(1,1,g);
	add_edge(2,1,g);
	add_edge(2,3,g);
	add_edge(3,5,g);



	std::cout << "adjacent vertices: ";
	graph_traits<ugraph>::adjacency_iterator ai;
	graph_traits<ugraph>::adjacency_iterator ai_end;
	for (tie(ai, ai_end) = adjacent_vertices(2, g);
	   ai != ai_end; ++ai)
	{
		graph_traits<ugraph>::adjacency_iterator copy_ai(ai);
		kgm_state tmpState(copy_ai, ai_end);
		std::cout << tmpState;
	}
	std::cout << std::endl;

	kgm_state state1(ai,ai_end);

	std::cout << "(" << *(state1.first) << "," << *(state1.second) << ")" << std::endl;


	return 0;
}
