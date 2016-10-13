#include "routing.h"

std::queue<message_list> message_queue;
std::unordered_multimap<uint64_t, message_lifetime> message_map;

char message_number;
uint8_t * rx_data;
uint64_t self_address, dest_address, src_address;

std::queue<message_list> get_queue(void) {
	return message_queue;
}

void handle_packets(Mrf24j& mrf) {
	rx_data = mrf.get_rxinfo()->rx_data;
	char routing_control = (rx_data[0] & 0xc0) >> 6;
	message_number = rx_data[0] & 0x3f;
	
	self_address = mrf.address64_read();
	dest_address = mrf.get_dest_address64();
	src_address = mrf.get_src_address64();
	
	switch (routing_control) {
		case 0:
			printf("\n Handling Message \n");
			handle_message();
			break;
		case 1: 
			printf("\n Handling Routing \n");
			handle_routing();
			break;
		case 2:
			printf("\n Handling Flooding \n");
			handle_flooding();
			break;
		case 3:
			printf("\n Handling Nack \n");
			handle_nack();
			break;
		case 4:
			printf("\n Handling Ack \n");
			handle_ack();
			break;
		case 5:
			printf("\n Handling scan \n");
			handle_scan();
			break;
	}
}

void handle_message(void) {
	if(self_address == dest_address) {
		
	} else {

	}
}

void handle_routing(void) {
	if(self_address == dest_address) {
		// handle message and return ack
		printf("\n\n Message Arrived! \n\n");
		send_ack(src_address);
	} else {
		if(new_message()) {
			message_list tmp_list;
			tmp_list.message = rx_data;
			tmp_list.address = dest_address;
			tmp_list.number = message_number ;
			tmp_list.attempts = NUM_ATTEMPTS;
			message_queue.push(tmp_list);
			
			send_nack(src_address);// return node ack
		} else {
			// return flooding control
		}
	}
}

bool new_message(void) {
	auto range = message_map.equal_range(dest_address);
	if(range.first != range.second) {
		for(auto it = range.first; it != range.second; it++) {
			if(it->second.number == message_number) {
				return false;
			} 
		}
	}
	message_lifetime tmp_message;
	tmp_message.number = message_number;
	tmp_message.lifetime = MSG_LIFETIME;
	message_map.insert({dest_address, tmp_message});
	return true;
}

void handle_flooding(void) {
	uint64_t dest_addr = 0; //routed_dest_address64(); // correct!
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

void handle_nack(void) {
	//check if is to me
	uint64_t dest_addr = 0; //routed_dest_address64(); // correct!
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
	return 0;
}

int handle_scan(void) {
	// retreive address (future use - maybe)
	return 0;
}

int send_nack(uint64_t src_address) {
	return 0;
}

int send_ack(uint64_t src_address) {
	return 0;
}
