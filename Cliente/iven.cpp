#include <wiringPi.h>
#include <bcm2835.h>
#include <stdio.h>
#include "mrf24j.h"

#define BUTTON_KEY 0
#define DEBOUNCE_TIME 100

void interrupt_routine(void);
void handle_rx(void);
void handle_tx(void);

const int pin_button = 17;
static volatile int txTriggered = 0;

PI_THREAD (buttonTrigger) {
	unsigned int debounceTime = 0;
	
	for(;;) {
		while(!digitalRead(pin_button))
			delay(1);
		if(millis() < debounceTime) {
			debounceTime = millis() + DEBOUNCE_TIME;
			continue;
		}
		
		piLock(BUTTON_KEY);
			txTriggered = 1;
		piUnlock(BUTTON_KEY);
		
		while(digitalRead(pin_button))
			delay(1);
			
		debounceTime = millis() + DEBOUNCE_TIME;
	}
}

const int pin_reset = 23;
const int pin_cs = 24; // default CS pin on ATmega8/168/328
const int pin_interrupt = 25; // default interrupt pin on ATmega8/168/328

Mrf24j mrf(pin_reset, pin_cs, pin_interrupt);

int main() {
	wiringPiSetupGpio();
	
	piThreadCreate (buttonTrigger);
	
	bcm2835_init();
	bcm2835_spi_begin();
	
	pinMode(pin_button, INPUT);
	pullUpDnControl(pin_button, PUD_DOWN);
	
	mrf.reset();
	mrf.init();
	  
	mrf.set_pan(0xcafe);
	// This is _our_ address
	mrf.address16_write(0x4202); 
	
	printf("%X\n", mrf.address16_read());
	printf("%X\n\n", mrf.get_pan());

	// uncomment if you want to receive any packet on this channel
	mrf.set_promiscuous(true);
	  
	// uncomment if you want to enable PA/LNA external control
	//mrf.set_palna(true);
	  
	// uncomment if you want to buffer all PHY Payload
	//mrf.set_bufferPHY(true);

	wiringPiISR(pin_interrupt, INT_EDGE_BOTH, &interrupt_routine); // interrupt 0 equivalent to pin 2(INT0) on ATmega8/168/328
	
	for(;;) {
		mrf.check_flags(&handle_rx, &handle_tx);
		if (txTriggered) {
			piLock(BUTTON_KEY);
				txTriggered = 0;
			piUnlock(BUTTON_KEY);
			printf("txxxing...\n");
			mrf.send16(0x6001, "Jessica");
		}
	}
}

void interrupt_routine() {
    mrf.interrupt_handler(); // mrf24 object interrupt routine
}

void handle_rx() {
    printf("received a packet ");
    printf("%d", mrf.get_rxinfo()->frame_length);
    printf(" bytes long\n");
    
    if(mrf.get_bufferPHY()){
      printf("Packet data (PHY Payload):");
      for (int i = 0; i < mrf.get_rxinfo()->frame_length; i++) {
          printf("%c", mrf.get_rxbuf()[i]);
      }
    }
    
    printf("\r\nASCII data (relevant data):");
    for (int i = 0; i < mrf.rx_datalength(); i++) {
        printf("%c", mrf.get_rxinfo()->rx_data[i]);
    }
    
    printf("\r\nLQI/RSSI=");
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
