#include <wiringPi.h>
#include <bcm2835.h>
#include <stdio.h>
#include <queue.h>
#include "../MRF24J/mrf24j.h"

#define NUM_ATTEMPTS 3

char message_number;

typedef struct _message_list {
	char * message;
	uint64_t address;
	char number;
	char attempts;
} message_list;

typedef struct _message_lifetime {
	char number;
	char lifetime;
} message_lifetime;

std::queue<message_list> message_queue;

int handle_packets(void) {
	char routing_control = (mrf.get_rxinfo()->rx_data[0] & 0xc0) >> 6;
	message_number = mrf.get_rxinfo()->rx_data[0] & 0x3f;
	switch (routing_control) {
		case 0:
			handle_message();
			break;
		case 1: 
			handle_routing();
			break;
		case 2:
			handle_flooding();
			break;
		case 3:
			handle_fowarding();
			break;
		case 4:
			handle_ack();
			break;
		case 5:
			handle_scan();
			break;
	}
}

int handle_message(void) {
	if(mrf.address64_read() == mrf.get_dest_address64()) {
		
	} else {
		return 0;
	}
}

int handle_routing(void) {
	if(mrf.address64_read() == mrf.get_dest_address64()) {
		// handle message and return ack
	} else {
		if(new_message()) {
			message_list tmp_list;
			tmp_list.message = mrf.get_rxinfo()->rx_data;
			tmp_list.address = mrf.get_dest_address64();
			tmp_list.number = message_number ;
			tmp_list.attempts = NUM_ATTEMPTS;
			message_queue.push(tmp_list);
			// return node ack
		} else {
			// return flooding control
			return 0;
		}
	}
}

bool new_message() {
	
}

int handle_flooding(void) {
	uint64_t dest_addr = mrf.get_dest_address64();
	std::queue<message_list> tmp_queue;
	while(!message_queue.empty()) {
		message_list tmp_list = message_queue.front();
		message_queue.pop();
		if(tmp_list.address == dest_addr && tmp_list.number == message_number) {
			tmp_list.attempts--;
			if(tmp_list.attempts != 0) {
				tmp_queue.push(tmp_list);
			}
			break;
		}
		tmp_queue.push(tmp_list);
	}
	while(!tmp_queue.empty()) {
		message_queue.push(tmp_queue.front());
		tmp_queue.pop();
	}
}

int handle_fowarding(void) {
	uint64_t dest_addr = mrf.get_dest_address64();
	std::queue<message_list> tmp_queue;
	while(!message_queue.empty()) {
		message_list tmp_list = message_queue.front();
		message_queue.pop();
		if(tmp_list.address == dest_addr && tmp_list.number == message_number) {
			break;
		}
		tmp_queue.push(tmp_list);
	}
	while(!tmp_queue.empty()) {
		message_queue.push(tmp_queue.front());
		tmp_queue.pop();
	}
}

int handle_ack(void) {
	//remove messages from pending 
}

int handle_scan(void) {
	// retreive address (future use - maybe)
}
