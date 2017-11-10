#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <cstdlib>
#include <cmath>
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
	int collisions;
	bool cwPause;
	bool operator < (const packet& rhs) const {
		return rhs.time < time;
	}
};

// Node structure for experimentation log
struct node {
	
	int node_num;
	// logging info
	int pktsSent;
};

// Global packets queue
std::priority_queue<packet> pktQ;


/****************************
 *          DCF             *
 ****************************/
void DCF(struct node *nodeList) {
	
	std::ofstream outFile;
	std::deque<packet> ready, transmitting;
	struct packet temp;
	bool busy = 0;
	int clock = 0;
	int i, j = 0;
	int randomizer, colRand;
	int dif = 28;
	int finishTime = 0;
	int global_collisions = 0;

	do {
		
		/* Put all ready packets into ready deque */
		do {
			if (pktQ.size() != 0) {
				temp = pktQ.top();
				if (temp.time <= clock) {
					temp.time = temp.time + dif;                          // account for DIF
					if (temp.cwPause == 0) {                              // check if cw countdown is paused
						colRand = temp.collisions;                        // determine cw based on packet collisions
						if (colRand > 6)                                  // ensure range for randomization <= 1024
							colRand = 6;
						randomizer = pow(2, (4 + colRand));               // calculate randomization range
						temp.cw = rand() % static_cast<int>(randomizer);  // randomize cw based on # of collisions
					}
					ready.push_back(temp);
					pktQ.pop();
				}
			}
		} while (pktQ.size() != 0 && temp.time <= clock);
		
		//std::cout << pktQ.size() << "\n";
		//std::cout << ready.size() << "\n";
		//std::cout << ready[0].time << " " << ready[0].cw << " " << clock <<"\n";
		
		if (finishTime > clock)
			busy = 1;
		else
			busy = 0;
		
		/* Packet Events */
		if (ready.size() != 0) {
			i = 0;
			do {
				// line idle, dif done, decrement cw
				if (busy == 0 && ready[i].time < clock && ready[i].cw != 0) {
					ready[i].cw--;
					ready[i].time = clock;
				}

				// dif and cw complete, line idle, add to send deque */
				else if (busy == 0 && ready[i].time < clock && ready[i].cw == 0) {
					ready[i].finish = ready[i].time + ready[i].nav;
					transmitting.push_back(ready[i]);
					ready.erase(ready.begin() + i);
				}

				// dif has not finished line goes busy, change start time, pause cw, and put back in queue 
				else if (busy == 1 && ready[i].time >= clock) {
					ready[i].time = finishTime;
					ready[i].cwPause = 1;
					pktQ.push(ready[i]);
					ready.erase(ready.begin() + i);
				}

				// dif has finished line goes busy, change start time, pause cw, and put back in queue 
				else if (busy == 1 && ready[i].time < clock) {
					ready[i].time = finishTime;
					ready[i].cwPause = 1;
					pktQ.push(ready[i]);
					ready.erase(ready.begin() + i);
				}

				i++;
			} while (i < ready.size());
		}

		/* Transmit packets */
		// No collision
		if (transmitting.size() == 1) {
			std::cout << "packet sent " << transmitting[0].time << "\n";
			finishTime = transmitting[0].time + transmitting[0].nav;
			std::cout << "finishTime = " << finishTime << "\n";
			transmitting.clear();
			j = j + 1;
		}
		
		// With collision
		else if (transmitting.size() > 1) {
			std::cout << "collision\n";
			finishTime = transmitting[0].time + transmitting[0].nav;
			do{
				transmitting[0].cwPause = 0;
				transmitting[0].collisions += 1;
				pktQ.push(transmitting[0]);
				transmitting.pop_front();
				
			} while (transmitting.size() != 0);
		}
		
		/* Advance clock to next slot */
		clock += 9;
	
	} while (pktQ.size() != 0 || ready.size() != 0);
	std::cout << "total packets sent for debugging " << j << "\n";
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
	std::string select;
		
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
		pktQ.push(pktTemp);
		if (pktTemp.src_node > nodes)
			nodes = pktTemp.src_node;
	}
	
	struct node nodeList[nodes];
	inFile.close();
	
	// Call appropriate simulator function
	select = argv[1];
	if (select.compare("DCF") == 0 || select.compare("dcf") == 0) 
		DCF(nodeList);
	
	else if (select.compare("RTS") == 0 || select.compare("rts") == 0)
		RTSCTS(nodeList);
	else
		std::cout << "Invalid simulator selection.";

	return 0;
}

