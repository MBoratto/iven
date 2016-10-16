#include <wiringPi.h>
#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include "../MRF24J/mrf24j.h"
#include "../Routing/routing.h"

#define BUTTON_KEY 0
#define TIMER_KEY 1
#define DEBOUNCE_TIME 100
#define TIMER_INTERVAL 1000

void interrupt_routine(void);
void handle_rx(void);
void handle_tx(void);
void client_handler(void);

const int pin_button1 = 26;
const int pin_button2 = 21;
const int pin_button3 = 19;
const int pin_button4 = 13;

static volatile int txTriggered = 0;

PI_THREAD (buttonTrigger)
{
	unsigned int debounceTime = 0;
	
	for(;;) {
		while(digitalRead(pin_button1)) {
			if(!digitalRead(pin_button1))
				break;
			if(!digitalRead(pin_button2))
				break;
			if(!digitalRead(pin_button3))
				break;
			if(!digitalRead(pin_button4))
				break;
			
			delay(1);
		}
		if(millis() < debounceTime) {
			debounceTime = millis() + DEBOUNCE_TIME;
			continue;
		}
		
		piLock(BUTTON_KEY);
			txTriggered = 1;
		piUnlock(BUTTON_KEY);
		
		while(!digitalRead(pin_button1)) {
			if(digitalRead(pin_button1))
				break;
			if(digitalRead(pin_button2))
				break;
			if(digitalRead(pin_button3))
				break;
			if(digitalRead(pin_button4))
				break;
			
			delay(1);
		}
			
		debounceTime = millis() + DEBOUNCE_TIME;
	}
}

PI_THREAD (routingTimer)
{
	unsigned int lastTime = 0;
	
	for(;;) {

		if(millis() > lastTime) {
			piLock(TIMER_KEY);
				update_timer();
			piUnlock(TIMER_KEY);
			lastTime = millis() + TIMER_INTERVAL;
		}
		
	}
}

const int pin_reset = 6;
const int pin_cs = 17; // default CS pin on ATmega8/168/328
const int pin_interrupt = 5; // default interrupt pin on ATmega8/168/328

Mrf24j mrf(pin_reset, pin_cs, pin_interrupt);

int main() {
	wiringPiSetupGpio();
	
	bcm2835_init();
	bcm2835_spi_begin();
	
	pinMode(pin_button1, INPUT);
	pullUpDnControl(pin_button1, PUD_UP);
	pinMode(pin_button2, INPUT);
	pullUpDnControl(pin_button2, PUD_UP);
	pinMode(pin_button3, INPUT);
	pullUpDnControl(pin_button3, PUD_UP);
	pinMode(pin_button4, INPUT);
	pullUpDnControl(pin_button4, PUD_UP);
	
	piThreadCreate (buttonTrigger);
	piThreadCreate (routingTimer);
	
	mrf.reset();
	mrf.init();
	  
	mrf.set_pan(0xcafe);
	// This is _our_ address
	mrf.address64_write(0x1111111111111112); 
	
	uint64_t addr64 = mrf.address64_read();
	
	printf("%X%X%X%X\n", (word)((addr64>>48) & 0xffff), (word)((addr64>>32) & 0xffff), (word)((addr64>>16) & 0xffff), (word)(addr64 & 0xffff));
	printf("%X\n\n", mrf.get_pan());

	// uncomment if you want to receive any packet on this channel
	mrf.set_promiscuous(true);
	  
	// uncomment if you want to enable PA/LNA external control
	//mrf.set_palna(true);
	  
	// uncomment if you want to buffer all PHY Payload
	//mrf.set_bufferPHY(true);

	wiringPiISR(pin_interrupt, INT_EDGE_BOTH, &interrupt_routine); // interrupt 0 equivalent to pin 2(INT0) on ATmega8/168/328
	
	unsigned int sendTime = 0;
	
	for(;;) {
		mrf.check_flags(&handle_rx, &handle_tx);
		if (txTriggered) {
			piLock(BUTTON_KEY);
				txTriggered = 0;
			piUnlock(BUTTON_KEY);
			printf("\ntxxxing...\n");
			char msg[] = {0b00100000, (char)((addr64>>56) & 0xff), (char)((addr64>>48) & 0xff), (char)((addr64>>40) & 0xff), (char)((addr64>>32) & 0xff), (char)((addr64>>24) & 0xff), (char)((addr64>>16) & 0xff), (char)((addr64>>8) & 0xff), (char)(addr64 & 0xff), '\0'};
			message_send64(mrf, 0x1111111111111111, msg);
		}
		if(millis() > sendTime) {
			std::queue<message_list> message_queue = get_queue();

			while(!message_queue.empty()) {
				message_list tmp_list = message_queue.front();
				message_queue.pop();
				if(tmp_list.active) {
					printf("\n##############txxxing queue...##############\n");
					printf("\nAddr: %X\tNumber: %i\t MSG: %i\t From: %X", (int)(tmp_list.address & 0xff), tmp_list.number, tmp_list.message[0], tmp_list.message[8]);
					mrf.send64(tmp_list.address, (char *)tmp_list.message);
				}
			}
			sendTime = millis() + 500;
		}
	}
}

void interrupt_routine() {
    mrf.interrupt_handler(); // mrf24 object interrupt routine
}

void handle_rx() {
	
	piLock(TIMER_KEY);
		handle_packets(mrf, &client_handler);
	piUnlock(TIMER_KEY);
	
    printf("\nreceived a packet ");
    printf("%d", mrf.get_rxinfo()->frame_length);
    printf(" bytes long\n");
    
    if(mrf.get_bufferPHY()){
      printf("Packet data (PHY Payload):");
      for (int i = 0; i < mrf.get_rxinfo()->frame_length; i++) {
          printf("%c", mrf.get_rxbuf()[i]);
      }
    }
    
    printf("ASCII data (relevant data):");
    for (int i = 0; i < mrf.rx_datalength(); i++) {
        printf("%c", mrf.get_rxinfo()->rx_data[i]);
    }
    
    printf("\nLQI/RSSI=");
    printf("%d", mrf.get_rxinfo()->lqi);
    printf("/");
    printf("%d\n", mrf.get_rxinfo()->rssi);
}

void handle_tx() {
    if (mrf.get_txinfo()->tx_ok) {
        printf("TX went ok, got ack\n");
    } else {
        printf("TX failed after ");
        printf("%d", mrf.get_txinfo()->retries);
        printf(" retries\n");
    }
}

void client_handler(void) {
	printf("Handling Client Messages");
}
