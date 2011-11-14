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

#include <mpi.h>
#include <pthread.h>

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
#define MSG_NOT_ViSITED 1000

uint32_t KGM_GRAPH_SIZE;
uint16_t KGM_UPPER_BOUND = 30;
uint32_t KGM_ACTUAL_MIN_BOUND = KGM_UPPER_BOUND; // min degree
uint32_t KGM_LOWER_BOUND = 2;
uint32_t KGM_START_NODE = 0;
uint64_t KGM_REPORT_INTERVAL = 0x10000000;
uint64_t KGM_REPORT_NEXT = KGM_REPORT_INTERVAL;
uint64_t KGM_STEPS = 0;
uint32_t KGM_MINIMAL_SUBPROBLEM = 3;
boost::scoped_ptr<boost::timer> KGM_TIMER;
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

bool isValid_dfs_state(const dfs_state& dfsState, const ugraph& graph)
{
    if (graph[*(dfsState.v_it)].state == false ||
        graph[*(dfsState.a_it)].state == true)
        return false;
    return true;
}

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

void sendFinish() {
	int message = 1;
	for(int i = 1; i < MPI_PROCESSES; i++) {
		MPI_Send (&message, 1, MPI_INT, i, MSG_FINISH, MPI_COMM_WORLD);
	}
	running = false;
}

void broadcastNewSolution(uint16_t& degreeLimit) {
    std::cout << MPI_MY_RANK << ": broadcasting new solution of degree = " << degreeLimit << std::endl;
	for( int i = 0; i < MPI_PROCESSES; i++ ) {
		if(i != MPI_MY_RANK) {
			MPI_Send ((int*)(&degreeLimit), 1, MPI_INT, i, MSG_NEW_SOLUTION, MPI_COMM_WORLD);
		}
	}
}

void dfs_step(
        dfs_stack& stack,
        degree_stack& degStack,
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

        degStack.push_back(std::max(graph[*(stack.back().v_it)].degree, degStack.back()));

        if (degStack.back() >= degreeLimit)
        {   // not going to be a better solution
            degStack.pop_back();
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
            degreeLimit = degStack.back();
            // TODO possible spanning tree improvement
            std::cout << MPI_MY_RANK << ": New spanning tree of degree: "<< degreeLimit << std::endl
                    << stack << std::endl;
	    if(MPI_MY_RANK == 0 && degreeLimit == KGM_LOWER_BOUND) { 
	    	sendFinish();
            } else {
		broadcastNewSolution(degreeLimit);
	    }
        }

        return;
    }

    dfs_state prev (stack.back());
    graph[*(prev.v_it)].degree -= 1;
    graph[*(prev.a_it)].degree = 0;
    graph[*(prev.a_it)].state = false;

    degStack.pop_back();

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
    for (dfs_stack::iterator it = dfsStack.begin(); it != (dfsStack.end()-1); ++it)
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
    for (it = dfsStack.begin(); it != (dfsStack.end()-1); ++it)
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

    char* outputBuffer = new char [sizeof(uint16_t)*(KGM_GRAPH_SIZE*2 + 5)];
    uint16_t* outputBuffer16 = (uint16_t*) outputBuffer;
    int i = 1, cnt = 0;
    std::cout << MPI_MY_RANK << ": sending work - degree at level: " << it-dfsStack.begin()
            <<"( of"<< degreeStack.size() << ") = " << degreeStack.at(it-dfsStack.begin()) << std::endl;
    outputBuffer16[i++] = (uint16_t)(degreeStack.at(it-dfsStack.begin())); // spanning tree degree
    outputBuffer16[i++] = (uint16_t)(*newVIt); // starting state - v_it
    outputBuffer16[i++] = (uint16_t)(*newVIt_end); // starting state - v_it_end
    dfs_stack::iterator git;
    ++it;
    for (git = dfsStack.begin(); git != it; ++git)
    {
        outputBuffer16[i++] = (uint16_t)(*((*git).v_it));
        outputBuffer16[i++] = (uint16_t)(*((*git).a_it));
        ++cnt;
    }
    outputBuffer16[0] = (uint16_t)(cnt); // number of visited states - depth
    outputBuffer16[i++] = (uint16_t) 0; // terminating 00 bytes

    std::stringstream ss;
    ss << "[ ";
    for (int j = 0; j < i; ++j)
    {
        ss << outputBuffer16[j] << " ";
    }
    ss << "]";
    std::cout << MPI_MY_RANK << ": sent work - length (of uint16_t): " << i
            << " to " << processNumber << std::endl
            << ss.str() << std::endl;

    MPI_Send (outputBuffer, i*sizeof(uint16_t), MPI_CHAR, processNumber, MSG_WORK_SENT, MPI_COMM_WORLD);
    delete[] outputBuffer;
}

void acceptWork(MPI_Status& status) {
    int maxInputBufferSize = sizeof(uint16_t)*(KGM_GRAPH_SIZE*2 + 5), receivedNum;
    char* inputBuffer = new char [maxInputBufferSize];
    uint16_t* inputBuffer16 = (uint16_t*) inputBuffer;

    MPI_Recv(inputBuffer, maxInputBufferSize, MPI_CHAR, status.MPI_SOURCE, MSG_WORK_SENT, MPI_COMM_WORLD, &status);
    MPI_Get_count(&status, MPI_CHAR, &receivedNum);

    int total16 = receivedNum / sizeof(uint16_t); // uint16_t index
    std::stringstream ss;
    ss << "[ ";
    for (int i = 0; i < total16; ++i)
    {
        ss << inputBuffer16[i] << " ";
    }
    ss << "]";
    std::cout << MPI_MY_RANK << ": received work - length (of uint16_t): " << total16
            << " from " << status.MPI_SOURCE << std::endl
            << ss.str() << std::endl;

    if (inputBuffer16[total16-1] != 0)
    {
        std::cout << MPI_MY_RANK <<
                ": ERROR - acceptWork char* bad format - LAST UINT16_T != 0" << std::endl
                << "inputBuffer[total16-1] = " << inputBuffer[total16-1] << std::endl
                << "total16 = " << total16 << std::endl;
        throw("ERROR - acceptWork char* bad format - LAST UINT16_T != 0");
    }
    else {
        std::cout << MPI_MY_RANK <<
                ": OK - acceptWork char* valid format - LAST UINT16_T == 0" << std::endl;
    }

    int numberOfEdgesBefore = inputBuffer16[0];

    // Clear degree stack, put first state's degree into the stack
    degreeStack.clear();
    degreeStack.push_back(inputBuffer16[1]);

    // Clear graph state
    kgm_vertex_iterator vi, vi_end;
    for (tie(vi, vi_end) = vertices(g); vi != vi_end; ++vi)
    {
        g[(*vi)].state = false;
        g[(*vi)].degree = 0;
    }

    // Initialize graph state from message
    int visitedEdgesCnt = 0;
    for (int j = 4, j_end = total16-1; j < j_end; j+=2)
    {
        g[inputBuffer16[j]].state = true;
        g[inputBuffer16[j]].degree++;
        g[inputBuffer16[j+1]].state = true;
        g[inputBuffer16[j+1]].degree++;
        ++visitedEdgesCnt;
    }
    if (numberOfEdgesBefore != visitedEdgesCnt)
    {
        std::cout << MPI_MY_RANK <<
                ": ERROR - acceptWork char* bad format - inputBuffer16[0] != visited nodes" << std::endl
                << "numberOfEdgesBefore = inputBuffer16[0] = " << inputBuffer16[0] << std::endl
                << "visitedEdgesCnt = " << visitedEdgesCnt << std::endl;
        throw("ERROR - acceptWork char* bad format - inputBuffer16[0] != visited edges");
    }
    else
    {
        std::cout << MPI_MY_RANK <<
                ": OK - acceptWork char* valid format - inputBuffer16[0] == visited edges" << std::endl;
    }

    // Clear dfs stack and initialize first state
    dfsStack.clear();
    dfs_state firstState;
    tie(firstState.v_it, firstState.v_it_end) = vertices(g);
    firstState.v_it += inputBuffer16[2];
    firstState.v_it_end = firstState.v_it + (inputBuffer16[3] - inputBuffer16[2]);
    tie(firstState.a_it, firstState.a_it_end) = adjacent_vertices(*(firstState.v_it),g);
    if (!isValid_dfs_state(firstState, g))
        iterate_dfs_state(firstState, g);
    dfsStack.push_back(firstState);

        std::cout << MPI_MY_RANK << ": successfully accepted work" << std::endl
                << "   dfs stack size: " << dfsStack.size() << std::endl
                << "   degree: " << degreeStack.back() << std::endl
                << "   edges before this state: " << visitedEdgesCnt << std::endl;
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
		++processNumber;
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

void updateDegree(char* buffer, int source) {
    int* newDegree = (int*) buffer;

    std::cout << MPI_MY_RANK << " received MSG_NEW_SOLUTION from " << source
             << " of degree " << (*newDegree) << std::endl
             << "   while this process has a degree " << KGM_UPPER_BOUND << std::endl;

    if ((*newDegree) < KGM_UPPER_BOUND)
        KGM_UPPER_BOUND = (*newDegree);

    if (KGM_UPPER_BOUND == KGM_LOWER_BOUND) {
    	if(MPI_MY_RANK == 0) {
    		sendFinish();
    	}
    }
}

void receiveMessage() {
	int flag = 0;
	MPI_Status status;

//	std::cout << MPI_MY_RANK << ": receiving messages..." << std::endl;
	MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
	if (flag)
	{
	  std::cout << MPI_MY_RANK << ": received message of type: " << status.MPI_TAG << std::endl;
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
			 acceptWork(status);
			 std::cout << MPI_MY_RANK << " work successfully accepted from " << status.MPI_SOURCE << std::endl;
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
			 MPI_Recv(buffer, MSG_LENGTH, MPI_INT, status.MPI_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			 updateDegree(buffer, status.MPI_SOURCE);
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
	else
	{
	    int milisec = 100;
	    struct timespec req = {0};
	    req.tv_sec = 0;
	    req.tv_nsec = milisec * 1000000L;
	    nanosleep(&req, (struct timespec *)NULL);
	}
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

	while (!dfsStack.empty())
	{
		++KGM_STEPS;
		if ((KGM_STEPS % CHECK_MSG_AMOUNT)==0)
		{
			receiveMessage();
			return;
		}

		if(PROCESS_STATE == WARMUP && KGM_STEPS >= KGM_GRAPH_SIZE) {
			// time to divide work to other processes
			std::cout << MPI_MY_RANK << ": Attemp to divide work" << std::endl;
			divideWork();
			return;
		}

		dfs_step(dfsStack, degreeStack, g, KGM_UPPER_BOUND);
		if (KGM_STEPS >= KGM_REPORT_NEXT)
		{
			std::cout << MPI_MY_RANK << "Stack report at " << KGM_TIMER->elapsed() << ":" << std::endl
					<< make_pair(dfsStack.front(),g) << std::endl;
			KGM_REPORT_NEXT += KGM_REPORT_INTERVAL;
		}
	}


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
    	std::cout << "1: Initializing stack" << std::endl;
    	initStack();
    }
    std::cout << MPI_MY_RANK << ": Working..." << std::endl;

    KGM_TIMER.reset(new boost::timer);

    work();

    std::cout << MPI_MY_RANK << ": -------------" << std::endl;
    std::cout << MPI_MY_RANK << ": TOTAL STEPS: " << KGM_STEPS << std::endl;
    std::cout << MPI_MY_RANK << ": In time: " << KGM_TIMER->elapsed() << std::endl;
    MPI_Finalize();

    return 0;
}
