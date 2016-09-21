#ifndef LIB_ROUTING_H
#define LIB_ROUTING_H

#include <wiringPi.h>
#include <bcm2835.h>
#include <stdio.h>
#include <queue.h>
#include <unordered_map.h>
#include "../MRF24J/mrf24j.h"

#define NUM_ATTEMPTS 3
#define MSG_LIFETIME 60

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

int handle_packets(void);
int handle_message(void);
int handle_routing(void);
bool new_message(void);
int handle_flooding(void)
int handle_nack(void);
int handle_ack(void);
int handle_scan(void);
int send_nack(uint64_t);
int send_ack(uint64_t);

#endif  /* LIB_ROUTING_H */
