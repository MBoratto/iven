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
	uint8_t * message;
	uint64_t address;
	char number;
	char attempts;
} message_list;

typedef struct _message_lifetime {
	char number;
	char lifetime;
} message_lifetime;

void handle_packets(Mrf24j& mrf);
void handle_message(void);
void handle_routing(void);
bool new_message(void);
void handle_flooding(void);
void handle_nack(void);
int handle_ack(void);
int handle_scan(void);
int send_nack(uint64_t);
int send_ack(uint64_t);
std::queue<message_list> get_queue(void);

#endif  /* LIB_ROUTING_H */
