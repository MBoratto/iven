#include <wiringPi.h>
#include <bcm2835.h>
#include <stdio.h>
#include "../MRF24J/mrf24j.h"

int handle_packets(void) {
	char routing_control mrf.get_rxinfo()->rx_data[0];
	switch (routing_control) {
		case 0b00000000: 
			handle_message();
			break;
		case 0b00000001: 
			handle_routing();
			break;
	}
}

int handle_message(void) {
	if(address64_read() == get_dest_address64()) {
		
	} else {
		return 0;
	}
}

int handle_routing(void) {
	if(address64_read() == get_dest_address64()) {
		
	} else {
		mrf.send64(get_dest_address64(), mrf.get_rxinfo()->rx_data);
	}
}
