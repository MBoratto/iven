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
	printf("Packet received! Handling...\n");
	rx_data = mrf.get_rxinfo()->rx_data;
	char routing_control = (rx_data[0] & 0xe0) >> 5;
	message_number = rx_data[0] & 0x1f;
	printf("Control packet: %i; Routing Control: %i; Msg #: %i\n", rx_data[0], routing_control, message_number);

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
			handle_routing(mrf);
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

void handle_routing(Mrf24j& mrf) {
	if(self_address == dest_address) {
		// handle message and return ack
		printf("\n\n Message Arrived! \n\n");
		uint64_t dest_addr = routed_dest_address64();
		send_ack(mrf, dest_addr, self_address);
	} else {
		if(new_message()) {
			message_list tmp_list;
			tmp_list.message = rx_data;
			tmp_list.address = dest_address;
			tmp_list.number = message_number ;
			tmp_list.attempts = NUM_ATTEMPTS;
			message_queue.push(tmp_list);
			
			send_nack(mrf, src_address, dest_address);// return node ack
			printf("Nack Sent! \n");
		} else {
			printf("\n\n Flood! \n\n");
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
	if(self_address == dest_address) {
		uint64_t dest_addr = routed_dest_address64();
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
}

uint64_t routed_dest_address64(void) {
	uint64_t dest_addr = 0;
	
	for(int i = 0; i < 8; i++) {
		
			dest_addr |= rx_data[i+1] << 8*i; // recebe e armazena endereço da fonte
		
	}
	
	return dest_addr;
}

void handle_ack(void) {
	
	if(self_address == dest_address) {
		auto range = message_map.equal_range(self_address);
		if(range.first != range.second) {
			for(auto it = range.first; it != range.second; it++) {
				if(it->second.number == message_number) {
					message_map.erase(it);
					break;
				} 
			}
		}
	} else {
		// Remove corresponding message from routing queue
		std::queue<message_list> tmp_queue;
		uint64_t msg_addr = routed_dest_address64();
		printf("MSG addr: %lld  Number: %i\n\n", msg_addr, message_number);
		while(!message_queue.empty()) {
			message_list tmp_list = message_queue.front();
			message_queue.pop();
			printf("Addr: %lld Number: %i", tmp_list.address, tmp_list.number);
			if(tmp_list.address == msg_addr && tmp_list.number == message_number) {
				printf("Pop messsage from queue");
				break;
			}
			tmp_queue.push(tmp_list);
		}
		while(!tmp_queue.empty()) {
			message_queue.push(tmp_queue.front());
			tmp_queue.pop();
		}
		// Add final ack message to queue for routing to destination
	}
}

int handle_scan(void) {
	// retreive address (future use - maybe)
	return 0;
}

void send_nack(Mrf24j& mrf, uint64_t src_addr, uint64_t msg_address) {
	printf("Sending nack. dest:%lld, msg:%lld", src_addr, msg_address);
	char nack_msg[] = {(char)(0b01100000 | message_number), (char)((msg_address>>56) & 0xff), (char)((msg_address>>48) & 0xff), (char)((msg_address>>40) & 0xff), (char)((msg_address>>32) & 0xff), (char)((msg_address>>24) & 0xff), (char)((msg_address>>16) & 0xff), (char)((msg_address>>8) & 0xff), (char)(msg_address & 0xff), '\0'};
	mrf.send64(src_addr, nack_msg);
}

void send_ack(Mrf24j& mrf, uint64_t dest_addr, uint64_t msg_address) {
	printf("Sending ack. dest:%lld, msg:%lld", dest_addr, msg_address);
	char ack_msg[] = {(char)(0b10000000 | message_number), (char)((msg_address>>56) & 0xff), (char)((msg_address>>48) & 0xff), (char)((msg_address>>40) & 0xff), (char)((msg_address>>32) & 0xff), (char)((msg_address>>24) & 0xff), (char)((msg_address>>16) & 0xff), (char)((msg_address>>8) & 0xff), (char)(msg_address & 0xff), '\0'};
	mrf.send64(dest_addr, ack_msg);
}
