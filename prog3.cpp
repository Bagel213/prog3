#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <cstdlib>
#include <vector>
#include <deque>

// Packet structure
struct packet {
	int pkt_id;
	int src_node;
	int dst_node;
	int pkt_size;
	int time;
	int nav;
	int cw;
	int finish;
	bool operator<(const packet& rhs) const {
		return time < rhs.time;
	}
};

// Node structure for experimentation log
struct node {
	
	int node_num;
	// logging info
	int = pktsSent;
};

// Global packets queue
std::priority_queue<packet> pktQ;


/****************************
 *          DCF             *
 ****************************/
void DCF(struct node *nodeList) {
	std::ofstream outFile;
	int clock = 0, i;
	bool busy = 0;
	std::deque<packet> ready, transmitting;
	struct packet temp;
	int dif = 28;
	int finishTime = 0;
	
	// Logging variables
	int collisions = 0, transmissions = 0;
	int freeTime = 0;
	
	while (pktQ.size() != 0){		
				
		/* check transmitting finish time against clock*/
		if (finishTime > clock)
			busy = 1;
		else {
			busy = 0;
			freeTime += 1;
		}

		//print out any that finish transmitting and remove from tranmitting deque

		/* Add all ready packets to ready deque */ 
		do {
			temp = pktQ.top();
			if (temp.time <= clock) {
				temp.time = temp.time + dif; // account for dif in start time
				temp.cw = 2 ^ (4 + collision); // adjust collision window based on number of collisions
				if (temp.cw > 1024)
					temp.cw = 1024;
				ready.push_back(temp);
				pktQ.pop();
			}
			if (temp.time > clock)
				break;
		} while (pktQ.size() != 0 && temp.time <= clock);
		
		/* For all ready packets determine action */	
		i = 0;
		do{
			// line idle, dif done, decrement cw
			if (busy == 0 && ready[i].time < clock && ready[i].cw != 0) { 
				if (clock%9 == 0) // time slots multiples of 9
					ready[i].cw--;
				ready[i].time = clock;
			}
			
			// dif and cw complete, line idle, add to send deque
			if (busy == 0 && ready[i].time < clock && ready[i].cw == 0) {
				ready[i].finish = ready[i].time + ready[i].nav;
				transmitting.push_back(ready[i]);
				transmissions += 1;
				ready.erase(ready.begin() + i);
			}
			
			// Dif complete line goes busy put back in queue with decremented cw and updated start time
			else if (busy == 1 && ready[i].time < clock && ready[i].cw != 0){
				ready[i].time = ready[i].time = finishTime;
				pktQ.push(ready[i]);
				ready.erase(ready.begin() + i);
			}

			// dif has not finished line goes busy, change start time and put back in queue
			else if (busy == 1 && ready[i].time >= clock) {  
				ready[i].time = finishTime;
				pktQ.push(ready[i]);
				ready.erase(ready.begin() + i);
			}
			
			i++;
		} while (i < ready.size());

		
		/* Message transmision handling	*/
		if (transmitting.size() > 1) { //collision
			// change start time put back in queue
			collisions += transmitting.size() - 1;
			finishTime = transmitting[0].finish;
			
		}
		else if (transmitting.size() == 1) {
			//transmit message
			finishTime = transmitting[0].finish;	
		}
		
		else if (transmitting.size() == 0)
			finishTime = 0;

		/* Clock to next slot time */
		clock += 1;


	}
	return;
}

void RTSCTS(struct node *nodeList) {
	std::ofstream outFile;
	return;
}

/********************************************
 *  Read from traffic file and add to queue *
 ********************************************/
int main(int argc, char *argv[]) {
	
	std::ifstream inFile;
	int temp;
	inFile.open(argv[2]);
	struct packet pktTemp;
	int size, nodes = 0;

	// Still need to build array of node structs for logging purposes
	
	// Read in # of packets
	inFile >> size;

	// Read in packets and set defaults
	while (inFile.good()) {
		inFile >> pktTemp.pkt_id;
		inFile >> pktTemp.src_node;
		inFile >> pktTemp.dst_node;
		inFile >> pktTemp.pkt_size;
		inFile >> pktTemp.time;
		pktTemp.nav = 44 + 10 + ((pktTemp.pkt_size / 6000000) * 1000000);
		pktTemp.cw = 16;
		pktTemp.finish = 0;
		pktQ.push(pktTemp);
		if (pktTemp.src_node > nodes)
			nodes = pktTemp.src_node;
	}
	struct node nodeList[nodes];
	inFile.close();
	
	// Call appropriate simulator function
	if (argv[1] == "DCF" || argv[1] == "dcf")
		DCF(nodeList);
	else if (argv[1] == "RTS" || argv[1] == "rts")
		RTSCTS(nodeList);
	else
		std::cout << "Invalid simulator selection.";

	return 0;
}

