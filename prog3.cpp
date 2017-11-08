#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <cstdlib>
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
	bool operator<(const packet& rhs) const {
		return time < rhs.time;
	}
};
// Global packets queue
std::priority_queue<packet> pktQ;


/****************************
 *			DCF             *
 ****************************/
void DCF() {
	std::ofstream outFile;
	int clock = 0, i;
	bool busy = 0;
	std::deque<packet> ready, transmitting;
	struct packet temp;
	int dif = 28;
	int collision = 0;
	int finishTime = 0;
	
	while (pktQ.size() != 0){		
		
		/* check transmitting finish time against clock*/
		if (finishTime > clock)
			busy = 1;

		/* Add all ready packets to ready deque */ 
		do {
			temp = pktQ.top();
			if (temp.time <= clock) {
				temp.time = temp.time + dif; // account for dif in start time
				ready.push_back(temp);
				pktQ.pop();
			}
			if (temp.time > clock)
				break;
		}while (pktQ.size() != 0 && temp.time <= clock)
		
		/* For all ready packets determine action */	
		i = 0;
		do{
			// ready.time > clock means dif done, decrement cw
			if (busy == 0 && ready[i].time > clock && cw != 0) { 
				cw--;
				ready[i].time = clock;
			}
			
			// dif and cw complete and idle, add to send deque
			else if (busy == 0 && ready[i].time > clock && cw == 0) { 
				ready[i].finish = ready[i].time + ready[i].nav;
				transmitting.push_back(ready[i]);
				ready.erase(i);
			}
			
			// Dif complete but channel busy put back in queue with decremented cw
			else if (busy == 1 && ready[i].time >= clock && cw != 0){ //Dif complete but channel busy put back in queue with decremented cw
				ready[i].time = ready[i].time = finishTime + difs;
				pktQ.push(ready[i]);
				ready.erase(i);
			}

			// ready.time > clock dif has not finished and busy, change start time
			else if (busy == 1 && ready[i].time < clock) {  
				ready[i].time = finishTime + difs;
				pktQ.push(ready[i]);
				ready.erase(i);
			}
			
			
			i++;
		} while (i < ready.size())

		
		/* Message transmision handling	*/
		if (transmitting.size() > 1){ //collision
			// change start time put back in queue
			// add to collision	count`
			finishTime = transmitting[0].finish;
			//remove from transmitting

		else if (transmitting.size() == 1) {
			//transmit message
			finishTime = transmitting[0].finish;
			//remove from transmitting
		}
		
		else if (transmitting.size() == 0)
			finishTime = 0;

		/* Clock to next slot time */
		clock += 9;


	}
	return;
}

void RTSCTS() {
	std::ofstream outFile;
	return;
}

/********************************************
 *	Read from traffic file and add to queue *
 ********************************************/
int main(int argc, char *argv[]) {
	
	std::ifstream inFile;
	int temp;
	inFile.open(argv[2]);
	struct packet pktTemp;
	int size;
	
	// Read in # of packets
	inFile >> size;

	// Read in packets and set defaults
	while (inFile.good()) {
		inFile >> pktTemp.pkt_id;
		inFile >> pktTemp.src_node;
		inFile >> pktTemp.dst_node;
		inFile >> pktTemp.pkt_size;
		inFile >> pktTemp.time;
		pktTemp.nav = 44 + 10 + ((pkt_size / 6000000) * 1000000);
		pktTemp.cw = 32;
		pktTemp.finish = 0;
		pktQ.push(pktTemp);
	}
	
	inFile.close();
	
	// Call appropriate simulator function
	if (argv[1] == "DCF" || argv[1] == "dcf")
		DCF();
	else if (argv[1] == "RTSCTS" || argv[1] == "rtscts")
		RTSCTS();
	else
		std::cout << "Invalid simulator selection.";

	return 0;
}

