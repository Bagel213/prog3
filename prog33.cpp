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
	double time, difStart;                         // time is time post dif for sending, difstart is start of dif countdown
	int nav;                                       // Total time for send and ACK
	int cw;                                        // Window counter
	int collisions;                                // Collision tracker for cw backoff            
	bool cwPause, cwStarted;                       // Trackers for cw counter
	double org_start;                              // maintain original start time for logging
	bool operator < (const packet& rhs) const {    // Sort with difStart time as key
		return rhs.difStart < difStart;
	}
};

/* Node structure */
struct node {

	std::priority_queue<packet> pktQ;
	bool pkt_in_ready;                  // Track if node already has packet trying to send

	//for logging
	int pkts_transmitted;
	int pkts_attempted;
	double latency;
};


/***************************
*           DCF            *
****************************/
void DCF(std::vector<node> &nodes, int size, char *file) {
	std::ofstream outFile;
	std::deque<packet> ready, transmitting;
	struct packet temp;
	bool busy = 0;
	bool check;
	double clock = 0;
	int i, j=0;
	int dif = 28;
	int finishTime=0;
	int successful_transmissions = 0;
	outFile.open(file);
	//logging variables
	int total_collisions = 0;
	int total_transmissions = 0;
	double time_free = 0;
	double time_busy = 0;
	double total_bits_sent = 0;
	long double throughput = 0;
	long double free_fraction = 0;
	int latcalc;
	long double avg_latency;
   
	do {
		/* Add packets to ready list*/
		i = 0;
		do {
			if (nodes[i].pktQ.size() != 0) {
				do {
					temp = nodes[i].pktQ.top();
					if (temp.time <= clock ) {                               
						if (nodes[temp.src_node].pkt_in_ready == 1)          // Check if node already has a packet attempting to send
							break;
						nodes[i].pktQ.pop();                                 // Remove from node's queue
						temp.time += dif + temp.difStart;                    // Account for dif
						temp.cw = rand() % 16;                               // Randomize the cw countdown [0-15] for initial DIF + [0-15]
						nodes[temp.src_node].pkt_in_ready = 1;               // Set node flag pkt_in_ready to 1
						ready.push_back(temp);                               // Add to ready deque
					}
				} while (nodes[i].pktQ.size() != 0 && temp.time <= clock);
			}
			i++;
		} while (i < nodes.size());
		
		/* Set busy flag */
		if (finishTime > clock) {
			busy = 1;
			time_busy += 1;
		}
		else {
			busy = 0;
			time_free += 1;
		}
		
		/* Packet Events */
		if (ready.size() != 0) {
			i = 0;
			do {
				
				// Output appropriate message depending on status of packet
				if (busy == 0) {
					if (ready[i].difStart == clock)
						outFile << "Time: " << clock << ": Node " << ready[i].src_node << " started waiting for DIFS\n";
					else if (ready[i].time == clock && ready[i].cwStarted == 0 && ready[i].cwPause == 0)
						outFile << "Time: " << clock << ": Node " << ready[i].src_node << " finished waiting for DIFS and started waiting for "
						<< ready[i].cw << " slots\n";
					else if (ready[i].time == clock && ready[i].cwStarted == 1 && ready[i].cwPause == 1) {
						outFile << "Time: " << clock << ": Node " << ready[i].src_node << " finished waiting for DIFS and started waiting for "
							<< ready[i].cw << " slots (counter was freezed!)\n";
						ready[i].cwPause = 0;  // Set paused flag to "unpaused"
					}

					// line idle, dif done, decrement cw
					if (ready[i].time <= clock) {
						if (ready[i].cw != 0) {
							ready[i].cwStarted = 1;  // Set flag for countdown started
							ready[i].cw--;
							ready[i].time += 9;
						}

						// dif and cw complete, line idle, add to send deque */
						else {
							outFile << "Time: " << clock << " : Node " << ready[i].src_node << " finished waiting and is ready to send the packet\n";
							transmitting.push_back(ready[i]);                // Add to tranmitting list and remove from ready
							ready.erase(ready.begin() + i);
							nodes[ready[i].src_node].pkts_attempted += 1;    // node transmissions
							total_transmissions += 1;                        // network wide transmissions
							i--;                                             // Account for removal
						}
					}
				}

				else if(busy == 1) {
					// dif has not finished line goes busy, change start time, keep in ready deque 
					if (ready[i].time < clock) {
						ready[i].difStart = finishTime;
						ready[i].time = finishTime + dif;
					}

					// dif has finished line goes busy, change start time, pause cw(set flag), and keep in ready deque 
					else if (ready[i].difStart < clock && ready[i].cwPause != 1) {
						ready[i].difStart = finishTime;
						ready[i].time = finishTime + dif;
						ready[i].cwPause = 1;             // Set count paused flag
						ready[i].cw += 1;                 // Account for cw being decremented when other is transmitting due to order of operations
						outFile << "Time: " << clock << ": Node " << ready[i].src_node << " had to wait for "
							<< ready[i].cw << " more slots that channel became busy!\n";
					}
				}
				i++;
			} while (i < ready.size());
		}
		
		/* Transmit packets */
		// No collision
		if (transmitting.size() == 1) {
			finishTime = transmitting[0].time + transmitting[0].nav;
			outFile << "Time: " << finishTime << ": Node " << transmitting[0].src_node << " sent "
				<< transmitting[0].pkt_size << " bits\n";
			successful_transmissions += 1;                                                        // count total successful transmissions
			total_bits_sent = total_bits_sent + transmitting[0].pkt_size;                         // for throughput calculation
			nodes[transmitting[0].src_node].pkt_in_ready = 0;                                     // Reset pkt_in_ready flag for node to 0
			nodes[transmitting[0].src_node].pkts_transmitted += 1;                                // Total successful transmissions for each node
			latcalc = (clock - transmitting[0].org_start);                                        // for average latency calculation
			nodes[transmitting[0].src_node].latency += latcalc;                                   /* calculate latency.
			                                                                                        Time of actual send - original ready time */
			transmitting.clear();
		}

		// With collision
		else if (transmitting.size() > 1) {
			//std::cout << "collision\n";
			do {
				outFile << "Time: " << clock << ": Node " << transmitting[0].src_node << "attempted to send "
					<< transmitting[0].pkt_size << " bits, but had a collision\n";
				if (finishTime < (transmitting[0].time + transmitting[0].nav))                          // Use longest transmission time for finish time
					finishTime = transmitting[0].time + transmitting[0].nav;                            // Finish Time for busy flag
				transmitting[0].collisions += 1;                                                        // current packet collisions
				total_collisions += 1;                                                                  // track network wide collisions
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
		if (successful_transmissions < size)
				check = true;
		

	} while (check);
	
	/* Logging calculations and output */
	throughput = (total_bits_sent / 1000)/(clock / 1000000);
	outFile << "Network throughput: " << throughput << " kbps\n";
	outFile << "Total number of attempted transmissions: " << total_transmissions << "\n";
	outFile << "Total number of collisions: " << total_collisions << "\n";
	free_fraction = time_free / clock;
	outFile << "Fraction of time medium was free: " << free_fraction << "\n";
	for (i = 0; i < nodes.size(); i++) {
		avg_latency = nodes[i].latency / nodes[i].pkts_transmitted;
		outFile << "Node: " << i << " Avg latency: " << avg_latency << "\n";
	}
	return; 
	outFile.close();
} // end DCF


/*****************************
*         RTS/CTS           *
*****************************/
void RTSCTS(std::vector<node> &nodes, int size, char *file) {
	std::ofstream outFile;
	/*std::deque<packet> ready, transmitting;
	struct packet temp;
	bool busy = 0;
	bool check;
	double clock = 0;
	int i, j = 0;
	int dif = 28;
	int finishTime;
	int rts_cts = 30 + 10 + 30 + 10;
	outFile.open(file);
	//logging variables
	int total_collisions = 0;
	int total_transmissions = 0;
	double time_free = 0;
	double time_busy = 0;
	double total_bits_sent = 0;
	long double throughput = 0;
	long double free_fraction = 0;
	int latcalc;
	long double avg_latency;

	//do {
	i = 0;
	do {
	if (nodes[i].pktQ.size() != 0) {
	do {
	temp = nodes[i].pktQ.top();
	if (temp.time <= clock) {
	if (nodes[temp.src_node].pkt_in_ready == 1)          // Check if node already has a packet attempting to send
	break;
	nodes[i].pktQ.pop();                                 // Remove from node's queue
	temp.time += dif + temp.difStart;                    // Account for dif
	nodes[temp.src_node].pkt_in_ready = 1;               // Set node flag pkt_in_ready to 1
	ready.push_back(temp);                               // Add to ready deque
	}
	} while (nodes[i].pktQ.size() != 0 && temp.time <= clock);
	}
	i++;
	} while (i < nodes.size());

	/* Set busy flag
	if (finishTime > clock) {
	busy = 1;
	time_busy += 1;
	}
	else {
	busy = 0;
	time_free += 1;
	}
	std::cout << "rs " << ready.size() << "\n";
	if (ready.size() != 0) {
	i = 0;
	do {
	if (busy == 0 && ready[i].difStart == clock)
	outFile << "Time: " << clock << ": Node " << ready[i].src_node << " started waiting for DIFS\n";
	else if (busy == 0 && ready[i].time == clock && ready[i].cwStarted == 0 && ready[i].cwPause == 0)
	outFile << "Time: " << clock << ": Node " << ready[i].src_node << " finished waiting for DIFS and is sending RTS\n";
	std::cout << "clock " << clock << "\n";
	if (busy == 0 && ready[i].time <= clock) {
	outFile << "Time: " << clock << ": Node " << ready[i].src_node << " finished waiting and is ready to send the packet\n";
	transmitting.push_back(ready[i]);                // Add to tranmitting list and remove from ready
	ready.erase(ready.begin() + i);
	nodes[ready[i].src_node].pkts_attempted += 1;    // node transmissions
	total_transmissions += 1;                        // network wide transmissions
	i--;                                             // Account for removal
	}

	if (busy == 1 && clock > ready[i].time) {
	outFile << "Time: " << clock << ": Node " << ready[i].src_node << " has to wait channel is busy\n";
	ready[i].difStart = finishTime;
	ready[i].time = finishTime + dif;
	}
	i += 1;
	} while (i < ready.size());
	}

	/* Transmit packets
	// No collision
	if (transmitting.size() == 1) {
	finishTime = transmitting[0].time + transmitting[0].nav;
	outFile << "Time: " << finishTime << ": Node " << transmitting[0].src_node << " sent "
	<< transmitting[0].pkt_size << " bits\n";
	total_bits_sent = total_bits_sent + transmitting[0].pkt_size;                         // for throughput calculation
	nodes[transmitting[0].src_node].pkt_in_ready = 0;                                     // Reset pkt_in_ready flag for node to 0
	nodes[transmitting[0].src_node].pkts_transmitted += 1;                                // Total successful transmissions for each node
	latcalc = (clock - transmitting[0].org_start);                                        // for average latency calculation
	nodes[transmitting[0].src_node].latency += latcalc;                                   /* calculate latency.
	Time of actual send - original ready time */
	//transmitting.clear();
	//}

	/* With collision
	else if (transmitting.size() > 1) {
	//std::cout << "collision\n";
	do {
	outFile << "Time: " << clock << ": Node " << transmitting[0].src_node << "attempted to send "
	<< transmitting[0].pkt_size << " bits, but had a collision\n";
	if (finishTime < (transmitting[0].time + transmitting[0].nav + rts_cts))                // Use longest transmission time for finish time
	finishTime = transmitting[0].time + transmitting[0].nav + rts_cts;                  // Finish Time for busy flag
	transmitting[0].collisions += 1;                                                        // current packet collisions
	total_collisions += 1;                                                                  // track network wide collisions
	transmitting[0].difStart = finishTime;                                                  // Set DIF start to line idle time
	transmitting[0].time = transmitting[0].difStart + dif;                                  // Set countdown start time
	ready.push_back(transmitting[0]);                                                       // Put back in ready deque
	transmitting.pop_front();
	} while (transmitting.size() != 0);

	}

	/* Advance Clock
	clock += 1;

	/* Check to see if all packets sent
	check = false;
	for (i = 0; i < nodes.size(); i++) {
	if (nodes[i].pktQ.size() != 0 || ready.size() != 0)
	check = true;
	}

	} while (check);
	outFile.close();*/
	return;
} // end RTS/CTS


/**************************************************
*  main: Read from traffic file and add to queue *
**************************************************/
int main(int argc, char *argv[]) {

	std::ifstream inFile;
	int temp;
	inFile.open(argv[2]);
	struct packet pktTemp;
	std::deque<packet> pktList;
	int size, i=0, nodeTemp = 0;
	std::string select;
	std::vector<node> nodes;
	
	/* Read in # of packets */
	inFile >> size;

	/* Read in packets and set defaults */
	while (inFile.good() && i < size) {
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
		pktTemp.org_start = pktTemp.difStart;
		pktList.push_back(pktTemp);
		if (pktTemp.src_node > nodeTemp)                                  // Determine number of nodes
			nodeTemp = pktTemp.src_node;
		i++;
	}
	
	nodes.resize(nodeTemp + 1); // Resize node vector to fit number of nodes
	inFile.close();

	/* Build node queues*/
	do {
		nodes[pktList[0].src_node].pktQ.push(pktList[0]);
		nodes[pktList[0].src_node].pkt_in_ready = 0;
		nodes[pktList[0].src_node].pkts_transmitted = 0;
		nodes[pktList[0].src_node].pkts_attempted = 0;
		pktList.pop_front();
	} while (pktList.size() != 0);
	
	/* Call appropriate simulator function */
	select = argv[1];
	if (select.compare("DCF") == 0 || select.compare("dcf") == 0)
		DCF(nodes, size, argv[3]);

	else if (select.compare("RTS") == 0 || select.compare("rts") == 0)
		RTSCTS(nodes, size, argv[3]);
	else
		std::cout << "Invalid simulator selection.";

	return 0;
}

