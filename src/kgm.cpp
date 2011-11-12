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
#include "mpi.h"

using namespace std;
using namespace boost;

int32_t MPI_MY_RANK; // my process number
int32_t MPI_PROCESSES; // number of processes
int32_t MPI_REQUEST_PROCESS; // process from which we will be requesting work
int32_t MPI_WORK_REQUEST_FAILS_NUMBER = 0;  // number of times that the work request failed

enum kgm_process_state { // states of the process
	LISTEN,
	WARMUP, // only form process 0
	WORKING,
	NEED_WORK,
	TERMINATING,
	FINISHED
};

#define CHECK_MSG_AMOUNT  100

#define MSG_WORK_REQUEST 1000
#define MSG_WORK_SENT    1001
#define MSG_WORK_NOWORK  1002
#define MSG_FINISH       1003
#define MSG_NEW_SOLUTION 1004
#define MSG_TOKEN_WHITE  1005
#define MSG_TOKEN_BLACK  1006

#define MSG_LENGTH 2048 // maximum length of message


uint32_t KGM_GRAPH_SIZE;
uint32_t KGM_UPPER_BOUND = 30;
uint32_t KGM_ACTUAL_MIN_BOUND = KGM_UPPER_BOUND; // min degree
uint32_t KGM_LOWER_BOUND = 2;
uint32_t KGM_START_NODE = 0;
uint64_t KGM_REPORT_INTERVAL = 0x10000000;
uint64_t KGM_STEPS = 0;
kgm_process_state PROCESS_STATE = LISTEN;
bool running = true;




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

ugraph g;
dfs_stack dfsStack;
degree_stack degreeStack;

ostream& operator<< (ostream& out, const kgm_adjacency_iterator& ai);
ostream& operator<< (ostream& out, const kgm_vertex_iterator& ai);
ostream& operator<< (ostream& out, const dfs_state& state);
ostream& operator<< (ostream& out, const std::pair<dfs_state,ugraph>& state);
ostream& operator<< (ostream& out, const dfs_stack& stack);

bool isValid_dfs_state(const dfs_state& dfsState, const ugraph& graph);

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
            newState.v_it != newState.v_it_end; ++newState.v_it) // nodes that are part of the skeleton
    {
        if (graph[*(newState.v_it)].state == false)
            continue;

        for(tie(newState.a_it, newState.a_it_end) = adjacent_vertices(*(newState.v_it),graph); // neighbours
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

void broadcastNewSolution(uint16_t& degreeLimit) {
	for( int i = 0; i < MPI_PROCESSES; i++ ) {
		if(i != MPI_MY_RANK) {
			MPI_Send (&degreeLimit, 1, MPI_INT, i, MSG_NEW_SOLUTION, MPI_COMM_WORLD);
		}
	}
}

void dfs_step(
        dfs_stack& stack,
        degree_stack& dstack,
        ugraph& graph,
        uint16_t& degreeLimit) // so far the lowest skeleton degree
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

            broadcastNewSolution(degreeLimit);
        }

        return;
    }

    dfs_state prev (stack.back());
    graph[*(prev.v_it)].degree -= 1;
    graph[*(prev.a_it)].degree = 0;
    graph[*(prev.a_it)].state = false;

    dstack.pop_back();

    if (!iterate_dfs_state(stack.back(), graph)) // not able to iterate further, branch is removed from the stack
        stack.pop_back();
}

void readInputFromFile(std::string filename) {
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
		exit(-3);
	}

	g = ugraph(KGM_GRAPH_SIZE);
	std::vector<std::string>::iterator it = lines.begin();
	++it;
	for (uint32_t i = 0; it != lines.end(); ++it, ++i)
	{
		if ((*it).empty())
			continue;
		for (uint32_t j = 0; j <= i; ++j)
		{
			if ((*it)[j] == '1')
				add_edge(i,j,g);
		}
	}
}

bool hasExtraWork() {
    for (dfs_stack::iterator it = dfsStack.begin(); it != dfsStack.end(); ++it)
    {
        if ((*it).v_it != (*it).v_it_end)
            return true;
    }
	return false;
}

void sendWork(int processNumber) {
    int difference, diffRatio;
    kgm_vertex_iterator newVIt, newVIt_end;
    dfs_stack::iterator it;
    bool ok = false;
    for (it = dfsStack.begin(); it != dfsStack.end(); ++it)
    {
        difference = (*it).v_it_end-(*it).v_it;
        if (difference > 1)
        {
            diffRatio = ((*it).v_it_end-(*it).v_it+1)/2;
            newVIt = (*it).v_it+diffRatio;
            newVIt_end = (*it).v_it_end;
            ok = true;
            break;
        }
    }
    if (!ok)
        return;
    (*it).v_it_end = newVIt;

    kgm_vertex_iterator gi, gi_end;
    char* outputBuffer = new char [sizeof(uint32_t)*(KGM_GRAPH_SIZE + 4)];
    uint32_t* outputBuffer32 = (uint32_t*) outputBuffer;
    int i = 1, cnt = 0;
    outputBuffer32[i++] = (uint32_t)(*newVIt); // starting state - v_it
    outputBuffer32[i++] = (uint32_t)(*newVIt_end); // starting state - v_it_end
    for (tie (gi, gi_end) = vertices(g); gi != gi_end; ++gi)
    {
        if (g[(*gi)].state == true)
        {
            outputBuffer32[i++] = (uint32_t)(*gi);
            ++cnt;
        }
    }
    outputBuffer32[0] = (uint32_t)(cnt); // number of visited states - depth
    outputBuffer32[i++] = (uint32_t) 0; // terminating 0000 bytes

    MPI_Send (outputBuffer, i*sizeof(uint32_t)+1, MPI_CHAR, processNumber, MSG_WORK_SENT, MPI_COMM_WORLD);
    delete[] outputBuffer;
}

void acceptWork(char* _buffer) {
	// TODO - asi Lubos? :o) deserializovat a prijmout
}

void requestWork() {
	if(MPI_PROCESSES == 1) {
		running = false;
		PROCESS_STATE = FINISHED;

		return;
	}

	int message = 1;
	std::cout << MPI_MY_RANK << " is requesting work from " << MPI_REQUEST_PROCESS << std::endl;

	MPI_Send(&message, 1, MPI_INT, MPI_REQUEST_PROCESS, MSG_WORK_REQUEST, MPI_COMM_WORLD);

	MPI_REQUEST_PROCESS = (MPI_REQUEST_PROCESS + 1) % MPI_PROCESSES;
	if(MPI_REQUEST_PROCESS == MPI_MY_RANK) {
		MPI_REQUEST_PROCESS = (MPI_REQUEST_PROCESS+1)  % MPI_PROCESSES;
	}
}

void divideWork() {
	int processNumber = 1; // starting with the next process
	while(hasExtraWork() && processNumber < MPI_PROCESSES) {
		sendWork(processNumber);
	}
	PROCESS_STATE = WORKING;
}

void sendWhiteToken() {
	// sends white token to the next
	int message = 1;
	MPI_Send (&message, 1, MPI_INT, (MPI_MY_RANK + 1) % MPI_PROCESSES, MSG_TOKEN_WHITE, MPI_COMM_WORLD);
}

void sendBlackToken() {
	// sends white token to the next
	int message = 1;
	MPI_Send (&message, 1, MPI_INT, (MPI_MY_RANK + 1) % MPI_PROCESSES, MSG_TOKEN_BLACK, MPI_COMM_WORLD);
}

void sendFinish() {
	int message = 1;
	for(int i = 1; i < MPI_PROCESSES; i++) {
		MPI_Send (&message, 1, MPI_INT, i, MSG_FINISH, MPI_COMM_WORLD);
	}
	running = false;
}

void receiveMessage() {
	int flag = 0;
	MPI_Status status;

	MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
	if (flag)
	{
	  char* buffer = new char[MSG_LENGTH];
	  switch (status.MPI_TAG)
	  {
		 case MSG_WORK_REQUEST :
			 buffer = new char[1];
			 std::cout << MPI_MY_RANK << " received MSG_WORK_REQUEST from " << status.MPI_SOURCE << std::endl;
			 MPI_Recv(buffer, 1, MPI_INT, status.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			 if(hasExtraWork()) {
				 sendWork(status.MPI_SOURCE);
			 } else {
				 // not enough work to send, sending refusal
				 std::cout << MPI_MY_RANK << " sends NOWORK to " << status.MPI_SOURCE << std::endl;
				 MPI_Send (buffer, 1, MPI_INT, status.MPI_SOURCE, MSG_WORK_NOWORK, MPI_COMM_WORLD);
			 }
			 break;
		 case MSG_WORK_SENT :
			 acceptWork(buffer);
			 PROCESS_STATE = WORKING;
			 MPI_WORK_REQUEST_FAILS_NUMBER = 0;
			 break;
		 case MSG_WORK_NOWORK :
			 buffer = new char[1];
			 std::cout << MPI_MY_RANK << " received MSG_NOWORK from " << status.MPI_SOURCE << " it failed " << MPI_WORK_REQUEST_FAILS_NUMBER << " times " << std::endl;
			 MPI_Recv(buffer, 1, MPI_INT, status.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			 ++MPI_WORK_REQUEST_FAILS_NUMBER;
			 if(MPI_MY_RANK == 0 && (MPI_WORK_REQUEST_FAILS_NUMBER/2) >= MPI_PROCESSES) {
				 std::cout << MPI_MY_RANK << " is terminating" << std::endl;
				 PROCESS_STATE = TERMINATING;
				 sendWhiteToken();
			 } else {
				 PROCESS_STATE = NEED_WORK;
			 }
			 break;
		 case MSG_TOKEN_WHITE :
			 std::cout << MPI_MY_RANK << " received MSG_TOKEN_WHITE from " << status.MPI_SOURCE << std::endl;
			 MPI_Recv(buffer, MSG_LENGTH, MPI_INT, status.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

			 if(MPI_MY_RANK == 0 && dfsStack.empty()) {
				 std::cout << MPI_MY_RANK << " is sending MSG_TOKEN_FINISH to " << status.MPI_SOURCE << std::endl;
				 sendFinish();
			 } else if(dfsStack.empty()) {
				 PROCESS_STATE = TERMINATING;
				 sendWhiteToken(); // pass it to the next process
			 } else {
				 sendBlackToken(); // received white token but still has work
			 }
			 break;
		 case MSG_TOKEN_BLACK :
			 std::cout << MPI_MY_RANK << " received MSG_TOKEN_BLACK from " << status.MPI_SOURCE << std::endl;
			 MPI_Recv(buffer, MSG_LENGTH, MPI_INT, status.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			 if(MPI_MY_RANK == 0) {
				 PROCESS_STATE = NEED_WORK;
			 } else {
				 sendBlackToken();
			 }
			 break;
		 case MSG_NEW_SOLUTION :
			 std::cout << MPI_MY_RANK << " received MSG_NEW_SOLUTION from " << status.MPI_SOURCE << std::endl;
			 MPI_Recv(buffer, MSG_LENGTH, MPI_INT, status.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			 // TODO process new solution - not sure what to do :o)
			 // is it OK to just update the minDegree on the current process?
			 // shouldn't we send the best skeleton as well, not just the rank?
			 break;
		 case MSG_FINISH :
			  std::cout << MPI_MY_RANK << " received MSG_FINISH from " << status.MPI_SOURCE << std::endl;
			  MPI_Recv(buffer, MSG_LENGTH, MPI_INT, status.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			  running = false;
		      break;
		 default : // error
			 break;
	  }
	}
}

void printResult(double time, uint64_t steps) {
	std::cout << "-------------" << std::endl;
	std::cout << "TOTAL STEPS: " << steps << std::endl;
	std::cout << "In time: " << time << std::endl;
}

void initStack() {
	dfs_state firstState;
	g[KGM_START_NODE].state = true;
	if (!create_dfs_state(firstState,g))
	{
		std::cerr << "Failed to initialize first kgm_process_state" << std::endl;
		exit(-4);
	}
	dfsStack.push_back(firstState);
}

void iterateStack() {
	degreeStack.push_back(0);

	uint16_t minSPdegree = KGM_UPPER_BOUND;
	uint64_t psteps = KGM_REPORT_INTERVAL;
	boost::scoped_ptr<boost::timer> timer (new boost::timer);

	while (!dfsStack.empty())
	{
		if ((KGM_STEPS % CHECK_MSG_AMOUNT)==0)
		{
			receiveMessage();
			return;
		}

		if(PROCESS_STATE == WARMUP && KGM_STEPS == KGM_GRAPH_SIZE) {
			// time to divide work to other processes
			std::cout << "Attemp to divide work" << std::endl;
			divideWork();
			return;
		}

		dfs_step(dfsStack, degreeStack, g, minSPdegree);
		if (KGM_STEPS >= psteps)
		{
			std::cout << "Stack at " << timer->elapsed() << ":" << std::endl
					<< make_pair(dfsStack.front(),g) << std::endl;
			psteps += KGM_REPORT_INTERVAL;
		}
		++KGM_STEPS;
	}
	double time = timer->elapsed();
	timer.reset();

	PROCESS_STATE = NEED_WORK;
}

void work() {
	while(running) {
		switch(PROCESS_STATE) {
			case WORKING:
			case WARMUP:
				iterateStack();
				break;
			case LISTEN:
			case TERMINATING:
				receiveMessage();
				break;
			case NEED_WORK:
				requestWork();
				PROCESS_STATE = LISTEN;
				break;
			case FINISHED:
				exit(0);
				break;
			default:
				throw "Unknown kgm_process_state detected";
		}
	}

	if(MPI_MY_RANK == 0) {
		// TODO print result
		return;
	}
}

int main(int argc, char ** argv) {

    if (argc <= 1)
    {
        std::cerr << "Not enough arguments." << std::endl;
        return -1;
    }

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &MPI_MY_RANK); // my rank
    MPI_Comm_size(MPI_COMM_WORLD, &MPI_PROCESSES); // number of processes
    MPI_Barrier(MPI_COMM_WORLD); // loading all processes
    MPI_REQUEST_PROCESS = (MPI_MY_RANK + 1) % MPI_PROCESSES;

    std::string filename (argv[1]);
	if (filename.empty())
		return -1;

	readInputFromFile(filename);

    if(MPI_MY_RANK == 0) {
    	PROCESS_STATE = WARMUP;

    	initStack();
    }
    work();

    MPI_Finalize();

    return 0;
}
