#ifndef LIB_ROUTING_H
#define LIB_ROUTING_H

#include <wiringPi.h>
#include <bcm2835.h>
#include <stdio.h>
#include <queue>
#include <unordered_map>
#include <utility>
#include "../MRF24J/mrf24j.h"

#define NUM_ATTEMPTS 3
#define MSG_LIFETIME 60

typedef struct _message_list {
	uint8_t message[116];
	uint64_t address;
	char number;
	char attempts;
	boolean self, active = true;
} message_list;

typedef struct _message_lifetime {
	char number;
	char lifetime;
} message_lifetime;

void handle_packets(Mrf24j& mrf);
void handle_message(void);
void handle_routing(Mrf24j& mrf);
bool new_message();
void handle_flooding(void);
void handle_nack(void);
void handle_ack(Mrf24j& mrf);
int handle_scan(void);
void send_flood(Mrf24j& mrf, uint64_t src_addr, uint64_t msg_address);
void send_nack(Mrf24j& mrf, uint64_t src_address, uint64_t msg_address);
void send_ack(Mrf24j& mrf, uint64_t dest_addr, uint64_t msg_address);
std::queue<message_list> get_queue(void);
uint64_t routed_dest_address64(void);
void update_timer (void);

#endif  /* LIB_ROUTING_H */
