#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <cstdlib>
#include <cmath>
#include <deque>
#include <vector>

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
	int collisions;
	bool cwPause, sent;
	bool operator < (const packet& rhs) const {
		return rhs.time < time;
	}
};

// Node structure for experimentation log
struct node {

	std::priority_queue<packet> pktQ;
	bool pkt_in_ready;
};


/***************************
*           DCF            *
****************************/
void DCF(std::vector<node> &nodes) {
	std::cout << "DCF\n";
	//std::ofstream outFile;
	std::deque<packet> ready, transmitting;
	struct packet temp;
	bool busy = 0;
	int clock = 0;
	int i, j=0;
	int dif = 28;
	int finishTime;
	int lastCW = 0;
   
	do {
		/* Add packets to ready list*/
		i = 0;
		do {
			if (nodes[i].pktQ.size() != 0) {
				do {
					temp = nodes[i].pktQ.top();
					if (temp.time <= clock ) {                              // Check for nodes with ready packets
						if (nodes[temp.src_node].pkt_in_ready == 1)         // Check if node already has a packet attempting to send
							break;
						nodes[i].pktQ.pop();
						temp.time += dif;                                    // Account for dif
						nodes[temp.src_node].pkt_in_ready = 1;               // Set node flag pkt_in_ready to 1
						ready.push_back(temp);                               // Add to ready deque
					}
				} while (nodes[i].pktQ.size() != 0 && temp.time <= clock);
			}
			i++;
		} while (i < nodes.size());
		
		/* Set busy flag */
		if (finishTime > clock)
			busy = 1;
		else
			busy = 0;
		
		/* Packet Events */
		if (ready.size() != 0) {
			i = 0;
			do {
				// line idle, dif done, decrement cw
				if (busy == 0 && ready[i].time <= clock && ready[i].cw != 0) {
					//std::cout << "node " << ready[i].src_node << " cw " << ready[i].cw << "\n";
					ready[i].cw--;
					ready[i].time += 9;
				}

				// dif and cw complete, line idle, add to send deque */
			    if (busy == 0 && ready[i].time <= clock && ready[i].cw == 0) {
					ready[i].finish = ready[i].time + ready[i].nav;
					transmitting.push_back(ready[i]);
					ready.erase(ready.begin() + i);
					i--;
				}

				// dif has not finished line goes busy, change start time put back in queue 
				if (busy == 1 && ready[i].time < clock) {
					//std::cout << "dif not finished\n";
					ready[i].time = finishTime + dif;
				}

				// dif has finished line goes busy, change start time, pause cw, and put back in queue 
				if (busy == 1 && ready[i].time >= clock) {
					//std::cout << "dif finished\n";
					ready[i].time = finishTime + dif;
				}

				i++;
			} while (i < ready.size());
		}
		
		/* Transmit packets */
		// No collision
		if (transmitting.size() == 1) {
			std::cout << "sending node " << transmitting[0].src_node << " " << transmitting[0].time << "\n";
			finishTime = transmitting[0].time + transmitting[0].nav;
			std::cout << "finishTime = " << finishTime << "\n";
			nodes[transmitting[0].src_node].pkt_in_ready = 0;  // Reset pkt_in_ready flag for node to 0
			transmitting.clear();
			// For troubleshooting
			j = j + 1;                                         
			std::cout << j << "\n";
		}

		// With collision
		else if (transmitting.size() > 1) {
			std::cout << "collision\n";
			do {
				finishTime = transmitting[0].time + transmitting[0].nav;                                // Finish Time for busy flag
				transmitting[0].collisions += 1;                                                        // current packet collisions
				transmitting[0].cw = rand() % static_cast<int>(pow(2, 4 + transmitting[0].collisions)); // Calculate new cw
				transmitting[0].time = finishTime + dif;                                                // New start time
				// for troubleshooting
				std::cout << transmitting[0].cw << " " << transmitting[0].time << "\n";
				if (transmitting[0].cw < lastCW)                                                        // Attempt to give some order
					ready.push_front(transmitting[0]);                                                  // May  or need change to
				else                                                                                    // better inserstion into ready
					ready.push_back(transmitting[0]);
				lastCW = transmitting[0].cw;
				transmitting.pop_front();
			} while (transmitting.size() != 0);
			lastCW = 0;
		}

		clock += 1;

	} while (clock < 20000);
	
	
	return;
	
}


/*****************************
*         RTS/CTS           *
*****************************/
void RTSCTS(std::vector<node> &nodes) {
	std::ofstream outFile;
	return;
}

/**************************************************
*  main: Read from traffic file and add to queue *
**************************************************/
int main(int argc, char *argv[]) {

	std::ifstream inFile;
	int temp;
	inFile.open(argv[2]);
	struct packet pktTemp;
	std::deque<packet> pktList;
	int size, i=0, nodeTemp;
	std::string select;
	std::vector<node> nodes;
	
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
		pktTemp.cwPause = 0;
		pktTemp.collisions = 0;
		pktTemp.sent = 0;
		pktList.push_back(pktTemp);
		if (pktTemp.src_node > nodeTemp)
			nodeTemp = pktTemp.src_node;
	}
	
	nodes.resize(nodeTemp + 1);
	inFile.close();

	/* Build node queues*/
	do {
		nodes[pktList[0].src_node].pktQ.push(pktList[0]);
		nodes[pktList[0].src_node].pkt_in_ready = 0;
		pktList.pop_front();
	} while (pktList.size() != 0);
	std::cout << nodes.size() << "\n";
	/* Call appropriate simulator function */
	select = argv[1];
	if (select.compare("DCF") == 0 || select.compare("dcf") == 0)
		DCF(nodes);

	else if (select.compare("RTS") == 0 || select.compare("rts") == 0)
		RTSCTS(nodes);
	else
		std::cout << "Invalid simulator selection.";

	return 0;
	
		
}

