#include "routing.h"

std::queue<message_list> message_queue;
std::unordered_multimap<uint64_t, message_lifetime> message_map;
//std::unordered_multimap<uint64_t, message_block> message_path;

char message_number;
char clearPath = 0;
uint8_t rx_data[116];
uint8_t data_length;
uint64_t self_address, dest_address, src_address;

void routing_init(uint64_t self) {
	self_address = self;
}

bool number_used(uint8_t msg_number) {
	auto range = message_map.equal_range(self_address);
	if(range.first != range.second) {
		for(auto it = range.first; it != range.second; it++) {
			if(it->second.number == msg_number) {
				return true;
			} 
		}
	}
	return false;
}

std::queue<message_list> get_queue(void) {
	return message_queue;
}

bool message_send64(Mrf24j& mrf, uint64_t dest64, char * data) {

	char msg_num = data[0] & 0x1f;
	
	auto range = message_map.equal_range(self_address);
	if(range.first != range.second) {
		for(auto it = range.first; it != range.second; it++) {
			if(it->second.number == msg_num) {
				return false;
			} 
		}
	}
	message_lifetime tmp_message;
	tmp_message.number = msg_num;
	tmp_message.lifetime = MSG_LIFETIME;
	message_map.insert({self_address, tmp_message});
	
	uint8_t len = strlen(data);
	message_list tmp_list;
	for(uint8_t i = 0; i < len; i++) {
		tmp_list.message[i] = data[i];
	}
	tmp_list.message[len] = '\0';
	tmp_list.address = dest64;
	tmp_list.number = msg_num;
	tmp_list.attempts = NUM_ATTEMPTS;
	tmp_list.self = true;
	message_queue.push(tmp_list);
	
	return true;
}

void handle_packets(Mrf24j& mrf, void (*msg_handler)(void)) {
	printf("====================================\n");
	printf("Pacote recebido! Tratando...\n\n");
	
	data_length = mrf.get_rxinfo()->frame_length - 23;
	
	for(int i = 0; i < data_length; i++) {
		rx_data[i] = mrf.get_rxinfo()->rx_data[i];
	}
	rx_data[data_length] = '\0';
	
	char routing_control = (rx_data[0] & 0xe0) >> 5;
	message_number = rx_data[0] & 0x1f;
	//printf("Control packet: %i\tRouting Control: %i\tMsg #: %i\n\n", rx_data[0], routing_control, message_number);

	dest_address = mrf.get_dest_address64();
	src_address = mrf.get_src_address64();
	
	printf("Remetente: %X\tDestinatário: %X\tMeu endereço: %X\tNumero: %i\t Tipo: %i", (int)(routed_dest_address64() & 0xff), (int)(dest_address & 0xff), (int)(self_address & 0xff), message_number, routing_control);
	printf("\n------------------------------------\n");
	
	switch (routing_control) {
		case 0:
			printf("\nTratando mensagem\n");
			handle_message(msg_handler);
			break;
		case 1: 
			printf("\nTratando roteamento\n");
			handle_routing(mrf, msg_handler);
			break;
		case 2:
			printf("\nTratando enchente\n");
			handle_flooding();
			break;
		case 3:
			printf("\nTratando confirmação de nó\n");
			handle_nack();
			break;
		case 4:
			printf("\nTratando confirmação final\n");
			handle_ack(mrf);
			break;
		case 5:
			printf("\nHandling scan \n");
			handle_scan();
			break;
	}
	printf("------------------------------------\n");
	printf("====================================\n\n");
}

void handle_message(void (*msg_handler)(void)) {
	if(self_address == dest_address) {
		msg_handler();
	}
}

void handle_routing(Mrf24j& mrf, void (*msg_handler)(void)) {
	if(self_address == dest_address) {
		if(new_message()) {
			// handle message and return ack
			printf("\nMensagem recebida!\n\n");
			uint64_t dest_addr = routed_dest_address64();
			send_ack(mrf, dest_addr, self_address);
			handle_message(msg_handler);
		} else {
			printf("\n Enchente!\n\n");
			send_flood(mrf, src_address, self_address);
		}
	} else {
		if(new_message()) {
			printf("\nRoteando mensagem para destino final\n\n");
			message_list tmp_list;
			for(int i = 0; i < data_length; i++) {
				tmp_list.message[i] = rx_data[i];
			}
			tmp_list.message[data_length] = '\0';
			tmp_list.address = dest_address;
			tmp_list.number = message_number ;
			tmp_list.attempts = NUM_ATTEMPTS;
			tmp_list.self = false;
			message_queue.push(tmp_list);
			//printf("\nMessage on queue");
			
			/*message_block tmp_block;
			tmp_block.address = dest_address;
			tmp_block.number = message_number ;
			
			message_path.insert({src_address, tmp_block});*/
			
			send_nack(mrf, src_address, dest_address);// return node ack
			//printf("\nNack Sent!\n");
		} else {
			printf("\nEnchente!\n\n");
			send_flood(mrf, src_address, dest_address);
		}
	}
}

bool new_message(void) {
	uint64_t dest_addr = routed_dest_address64();
	auto range = message_map.equal_range(dest_addr);
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
	message_map.insert({dest_addr, tmp_message});
	return true;
}

/*bool self_path(void) {
	auto range = message_path.equal_range(src_address);
	if(range.first != range.second) {
		for(auto it = range.first; it != range.second; it++) {
			if(it->second.number == message_number) {
				return true;
			} 
		}
	}
	return false;
}*/

void handle_flooding(void) {
	if(self_address == dest_address) {
		uint64_t dest_addr = routed_dest_address64();
		std::queue<message_list> tmp_queue;
		//printf("\nMSG addr: %X\tNumber: %i\n", (int)(dest_addr & 0xff), message_number);
		//printf("\n***********Message Queue***********\n");
		while(!message_queue.empty()) {
			message_list tmp_list = message_queue.front();
			message_queue.pop();
			//printf("\nAddr: %X\tNumber: %i\tAttempts: %i", (int)(tmp_list.address & 0xff), tmp_list.number, tmp_list.attempts);
			if(tmp_list.address == dest_addr && tmp_list.number == message_number) {
				if(tmp_list.active) {
					tmp_list.attempts--;
					if(tmp_list.attempts == 0) {
						if(tmp_list.self == true && (tmp_list.number % 2) == 0) {
							tmp_list.active = false;
							tmp_queue.push(tmp_list);
							break;
						} else {
							break;
						}
					}
				}
			}
			tmp_queue.push(tmp_list);
		}
		while(!tmp_queue.empty()) {
			message_queue.push(tmp_queue.front());
			tmp_queue.pop();
		}
	}
}

void handle_nack(void) {
	//check if is to me
	if(self_address == dest_address) {
		uint64_t dest_addr = routed_dest_address64();
		std::queue<message_list> tmp_queue;
		//printf("\nMSG addr: %X\tNumber: %i\n", (int)(dest_addr & 0xff), message_number);
		//printf("\n***********Message Queue***********\n");
		while(!message_queue.empty()) {
			message_list tmp_list = message_queue.front();
			message_queue.pop();
			//printf("\nAddr: %X\tNumber: %i", (int)(tmp_list.address & 0xff), tmp_list.number);
			if(tmp_list.address == dest_addr && tmp_list.number == message_number) {
				if(tmp_list.active) {
					if(tmp_list.self == true && (tmp_list.number % 2) == 0) {
						tmp_list.active = false;
						tmp_queue.push(tmp_list);
						break;
					} else {
						//printf("\n\nPop messsage from queue\n");
						break;
					}
				}
			}
			tmp_queue.push(tmp_list);
		}
		while(!tmp_queue.empty()) {
			message_queue.push(tmp_queue.front());
			tmp_queue.pop();
		}
		//printf("\n***********************************\n");
	}
}

uint64_t routed_dest_address64(void) {
	uint64_t dest_addr = 0;
	for(int i = 0; i < 8; i++) {
		dest_addr |= (uint64_t)rx_data[8-i] << 8*i; // recebe e armazena endereço da fonte
	}
	return dest_addr;
}

void handle_ack(Mrf24j& mrf) {
	if(self_address == dest_address) {
		printf("\nConfirmação de mensagem recebida!\n");
		auto range = message_map.equal_range(self_address);
		if(range.first != range.second) {
			for(auto it = range.first; it != range.second; it++) {
				if(it->second.number == (message_number - 1)) {
					message_map.erase(it);
					break;
				} 
			}
		}
		uint64_t msg_addr = routed_dest_address64();
		std::queue<message_list> tmp_queue;
		while(!message_queue.empty()) {
			message_list tmp_list = message_queue.front();
			message_queue.pop();
			if(tmp_list.address == msg_addr && tmp_list.number == (message_number - 1)) {
				//printf("\n\nPop messsage from queue\n");
				break;
			}
			tmp_queue.push(tmp_list);
		}
		while(!tmp_queue.empty()) {
			message_queue.push(tmp_queue.front());
			tmp_queue.pop();
		}
		
		send_nack(mrf, src_address, dest_address);// return node ack
		//printf("Nack Sent! \n");
	} else {
		if(new_ack_message()) {
			printf("\nRoteando confirmação de mensagem final\n");
			// Remove corresponding message from routing queue
			std::queue<message_list> tmp_queue;
			uint64_t msg_addr = routed_dest_address64();
			//printf("\nMSG addr: %X\tNumber: %i\n", (int)(msg_addr & 0xff), message_number);
			//printf("\n***********Message Queue***********\n");
			while(!message_queue.empty()) {
				message_list tmp_list = message_queue.front();
				message_queue.pop();
				//printf("\nAddr: %X\tNumber: %i", (int)(tmp_list.address & 0xff), tmp_list.number);
				if(tmp_list.address == msg_addr && tmp_list.number == (message_number - 1)) {
					//printf("\n\nPop messsage from queue\n");
					break;
				}
				tmp_queue.push(tmp_list);
			}
			while(!tmp_queue.empty()) {
				message_queue.push(tmp_queue.front());
				tmp_queue.pop();
			}
			//printf("\n***********************************\n");
			// Add final ack message to queue for routing to destination
			message_list tmp_list;
			for(int i = 0; i < data_length; i++) {
				tmp_list.message[i] = rx_data[i];
			}
			tmp_list.message[data_length] = '\0';
			tmp_list.address = dest_address;
			tmp_list.number = message_number ;
			tmp_list.attempts = NUM_ATTEMPTS;
			tmp_list.self = false;
			message_queue.push(tmp_list);
			
			/*message_block tmp_block;
			tmp_block.address = dest_address;
			tmp_block.number = message_number ;
			
			message_path.insert({src_address, tmp_block});*/

			send_nack(mrf, src_address, dest_address);// return node ack
			//printf("\nNack Sent!\n");
		} else {
			printf("\n\nEnchente!\n\n");
			send_flood(mrf, src_address, dest_address);
		}
	}
}

bool new_ack_message(void) {
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

int handle_scan(void) {
	// retreive address (future use - maybe)
	return 0;
}

void send_flood(Mrf24j& mrf, uint64_t src_addr, uint64_t msg_address) {
	printf("\nEnviando enchente...\nDestinatário: %X\tEndereço de mensagem: %X\tNumero: %i\n", (int)(src_addr & 0xff), (int)(msg_address & 0xff), message_number);
	char flood_msg[] = {(char)(0b01000000 | message_number), (char)((msg_address>>56) & 0xff), (char)((msg_address>>48) & 0xff), (char)((msg_address>>40) & 0xff), (char)((msg_address>>32) & 0xff), (char)((msg_address>>24) & 0xff), (char)((msg_address>>16) & 0xff), (char)((msg_address>>8) & 0xff), (char)(msg_address & 0xff), '\0'};
	mrf.send64(src_addr, flood_msg);
}

void send_nack(Mrf24j& mrf, uint64_t src_addr, uint64_t msg_address) {
	printf("\n\nEnviando confirmação de no...\nDestinatário: %X\tEndereço de mensagem: %X\tNumero: %i\n", (int)(src_addr & 0xff), (int)(msg_address & 0xff), message_number);
	char nack_msg[] = {(char)(0b01100000 | message_number), (char)((msg_address>>56) & 0xff), (char)((msg_address>>48) & 0xff), (char)((msg_address>>40) & 0xff), (char)((msg_address>>32) & 0xff), (char)((msg_address>>24) & 0xff), (char)((msg_address>>16) & 0xff), (char)((msg_address>>8) & 0xff), (char)(msg_address & 0xff), '\0'};
	mrf.send64(src_addr, nack_msg);
}

void send_ack(Mrf24j& mrf, uint64_t dest_addr, uint64_t msg_address) {
	message_lifetime tmp_message;
	tmp_message.number = message_number + 1;
	tmp_message.lifetime = MSG_LIFETIME;
	message_map.insert({dest_addr, tmp_message});

	printf("\nEnviando confirmação final...\nDestinatário: %X\tEndereço de mensagem: %X\tNumero: %i\n", (int)(dest_addr & 0xff), (int)(msg_address & 0xff), message_number);
	char ack_msg[] = {(char)(0b10000000 | (message_number + 1)), (char)((msg_address>>56) & 0xff), (char)((msg_address>>48) & 0xff), (char)((msg_address>>40) & 0xff), (char)((msg_address>>32) & 0xff), (char)((msg_address>>24) & 0xff), (char)((msg_address>>16) & 0xff), (char)((msg_address>>8) & 0xff), (char)(msg_address & 0xff), '\0'};
	
	//mrf.send64(dest_addr, ack_msg);
	
	message_list tmp_list;
	for(int i = 0; i < 10; i++) {
		tmp_list.message[i] = ack_msg[i];
	}
	tmp_list.address = dest_addr;
	tmp_list.number = message_number + 1;
	tmp_list.attempts = NUM_ATTEMPTS;
	tmp_list.self = true;
	message_queue.push(tmp_list);
}

void update_timer (void) {
	/*clearPath++;
	if(clearPath == 10) {
		message_path.clear();
		clearPath = 0;
	}*/
	for (std::unordered_multimap<uint64_t, message_lifetime>::iterator it = message_map.begin(); it != message_map.end(); it++) {
		it->second.lifetime--;
		if(it->second.lifetime == 0) {
			printf("\nTratando timeout!\n");
			//printf("\n%X - %i\n", (char)(it->first & 0xff), it->second.number);
			if(it->first == self_address) {
				if((it->second.number % 2) == 0) {
					printf("\nMensagem %i sem confirmação! Reenviando...\n", it->second.number);
					it->second.lifetime = MSG_LIFETIME;
					std::queue<message_list> tmp_queue;
					while(!message_queue.empty()) {
						message_list tmp_list = message_queue.front();
						message_queue.pop();
						if(tmp_list.self == true && tmp_list.number == it->second.number) {
							tmp_list.active = true;
							tmp_list.attempts = NUM_ATTEMPTS;
							tmp_queue.push(tmp_list);
							break;
						}
						tmp_queue.push(tmp_list);
					}
					while(!tmp_queue.empty()) {
						message_queue.push(tmp_queue.front());
						tmp_queue.pop();
					}
				}
			} else {
				message_map.erase(it);
				break;
			}
		}
	}
}

