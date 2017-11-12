#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <cstdlib>
#include <cmath>
#include <deque>
#include <vector>
#include <algorithm>

/* Packet structure */
struct packet {
	int pkt_id;
	int src_node;
	int dst_node;
	int pkt_size;
	int time, difStart;                            // time is time post dif for sending, difstart is start of dif countdown
	int nav;                                       // Total time for send and ACK
	int cw;                                        // Window counter
	int collisions;                                // Collision tracker for cw backoff            
	bool cwPause, cwStarted;                       // Trackers for cw counter
	bool operator < (const packet& rhs) const {    // Sort with difStart time as key
		return rhs.difStart < difStart;
	}
};

/* Order of ready deque by cw */
struct cwCompartor
{
	bool operator()(const packet &first, const packet &sec)
	{
		if (first.cw < sec.cw)
			return true;
		else
			return false;
	}
};

/* Node structure */
struct node {

	std::priority_queue<packet> pktQ;
	bool pkt_in_ready;                  // Track if node already has packet trying to send
};


/***************************
*           DCF            *
****************************/
void DCF(std::vector<node> &nodes) {
	//std::ofstream outFile;
	std::deque<packet> ready, transmitting;
	struct packet temp;
	bool busy = 0;
	bool check;
	int clock = 0;
	int i, j=0;
	int dif = 28;
	int finishTime;
   
	do {
		/* Add packets to ready list*/
		i = 0;
		do {
			if (nodes[i].pktQ.size() != 0) {
				do {
					temp = nodes[i].pktQ.top();
					if (temp.time <= clock ) {                               // Check for nodes with ready packets
						if (nodes[temp.src_node].pkt_in_ready == 1)          // Check if node already has a packet attempting to send
							break;
						nodes[i].pktQ.pop();                                 // Remove from node's queue
						temp.time += dif + temp.difStart;                    // Account for dif
						temp.cw = rand() % 17;                               // Randomize the cw countdown
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
			std::sort(ready.begin(), ready.end(), cwCompartor());  // Sort ready deque by cw
			i = 0;
			do {
				
				// Output appropriate message depending on status of packet
				if (busy == 0 && ready[i].difStart == clock)
					std::cout << "Time: " << clock << ": Node " << ready[i].src_node << " started waiting for DIFS\n";
				else if (busy == 0 && ready[i].time == clock && ready[i].cwStarted == 0 && ready[i].cwPause == 0)
					std::cout << "Time: " << clock << ": Node " << ready[i].src_node << " finished waiting for DIFS and started waiting for "
					<< ready[i].cw << " slots\n";
				else if (busy == 0 && ready[i].time == clock && ready[i].cwStarted == 1 && ready[i].cwPause == 1) {
					std::cout << "Time: " << clock << ": Node " << ready[i].src_node << " finished waiting for DIFS and started waiting for "
						<< ready[i].cw << " slots (counter was freezed!)\n";
					ready[i].cwPause = 0;  // Set paused flag to "unpaused"
				}
				
				// line idle, dif done, decrement cw
				if (busy == 0 && ready[i].time <= clock && ready[i].cw != 0) {
					ready[i].cwStarted = 1;  // Set flag for countdown started
					ready[i].cw--;
					ready[i].time += 9;
				}

				// dif and cw complete, line idle, add to send deque */
			    if (busy == 0 && ready[i].time <= clock && ready[i].cw == 0) {
					std::cout << "Time: " << clock << " : Node " << ready[i].src_node << " finished waiting and is ready to send the packet\n";
				    transmitting.push_back(ready[i]);   // Add to tranmitting list and remove from ready
					ready.erase(ready.begin() + i);
					i--;                                // Account for removal
				}

				// dif has not finished line goes busy, change start time, keep in ready deque 
				if (busy == 1 && ready[i].time < clock) {
					ready[i].difStart = finishTime;
					ready[i].time = finishTime + dif;                  
				}

				// dif has finished line goes busy, change start time, pause cw(set flag), and keep in ready deque 
				if (busy == 1 && ready[i].time >= clock && ready[i].difStart < clock && ready[i].cwPause != 1) {
					ready[i].difStart = finishTime;
					ready[i].time = finishTime + dif;
					ready[i].cwPause = 1;             // Set count paused flag
					ready[i].cw += 1;                 // Account for pausing
					std::cout << "Time: " << clock << ": Node " << ready[i].src_node << " had to wait for "
						<< ready[i].cw << " more slots that channel became busy!\n";
				}

				i++;
			} while (i < ready.size());
		}
		
		/* Transmit packets */
		// No collision
		if (transmitting.size() == 1) {
			finishTime = transmitting[0].time + transmitting[0].nav;
			std::cout << "Time: " << clock << ": Node " << transmitting[0].src_node << "sent "
				<< transmitting[0].pkt_size << " bits\n";
			nodes[transmitting[0].src_node].pkt_in_ready = 0;  // Reset pkt_in_ready flag for node to 0
			transmitting.clear();
			// For troubleshooting
			//j = j + 1;                                         
			//std::cout << j << "\n";
		}

		// With collision
		else if (transmitting.size() > 1) {
			//std::cout << "collision\n";
			do {
				finishTime = transmitting[0].time + transmitting[0].nav;                                // Finish Time for busy flag
				transmitting[0].collisions += 1;                                                        // current packet collisions
				transmitting[0].cw = rand() % static_cast<int>(pow(2, 4 + transmitting[0].collisions)); // Calculate new cw based on collisions
				transmitting[0].difStart = finishTime;                                                  // Set DIF start to line idle time
				transmitting[0].time = transmitting[0].difStart + dif;                                  // Set countdown start time
				transmitting[0].cwStarted = 0;                                                          // Reset defaults for cw flags
				transmitting[0].cwPause = 0;			
				// for troubleshooting
				//std::cout << transmitting[0].src_node << " " << transmitting[0].time << "\n";
			    ready.push_back(transmitting[0]);                                                       // Put back in ready deque
				transmitting.pop_front();
			} while (transmitting.size() != 0);
			
		}

		/* Advance Clock*/
		clock += 1;
		
		/* Check to see if all packets sent */
		check = false;
		for (i = 0; i < nodes.size(); i++) {
			if (nodes[i].pktQ.size() != 0 || ready.size() != 0)
				check = true;
		}

	} while (check);
	
	
	return; 
	
} // end DCF


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
	
	/* Read in # of packets */
	inFile >> size;

	/* Read in packets and set defaults */
	while (inFile.good()) {
		inFile >> pktTemp.pkt_id;
		inFile >> pktTemp.src_node;
		inFile >> pktTemp.dst_node;
		inFile >> pktTemp.pkt_size;
		inFile >> pktTemp.difStart;
		pktTemp.nav = 44 + 10 + ((pktTemp.pkt_size / 6000000) * 1000000); // Transmit time + SIFS + ACK
		pktTemp.time = 0;
		pktTemp.cw = 0;
		pktTemp.cwPause = 0;
		pktTemp.collisions = 0;
		pktTemp.cwStarted = 0;
		pktTemp.cwPause = 0;
		pktList.push_back(pktTemp);
		if (pktTemp.src_node > nodeTemp)                                  // Determine number of nodes
			nodeTemp = pktTemp.src_node;
	}
	
	nodes.resize(nodeTemp + 1); // Resize node vector to fit number of nodes
	inFile.close();

	/* Build node queues*/
	do {
		nodes[pktList[0].src_node].pktQ.push(pktList[0]);
		nodes[pktList[0].src_node].pkt_in_ready = 0;
		pktList.pop_front();
	} while (pktList.size() != 0);
	
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

