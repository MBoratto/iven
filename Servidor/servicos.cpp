#include <wiringPi.h>
#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../MRF24J/mrf24j.h"
#include "../Routing/routing.h"

#define BUTTON_KEY 0
#define TIMER_KEY 1
#define DEBOUNCE_TIME 100
#define TIMER_INTERVAL 1000

void interrupt_routine(void);
void handle_rx(void);
void handle_tx(void);
void server_handler(uint8_t * msg);

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
	mrf.address64_write(0x1111111111111111); 
	
	uint64_t addr64 = mrf.address64_read();
	
	routing_init(addr64);
	
	printf("Endereço: %X%X%X%X\n", (word)((addr64>>48) & 0xffff), (word)((addr64>>32) & 0xffff), (word)((addr64>>16) & 0xffff), (word)(addr64 & 0xffff));
	printf("PAN: %X\n\n", mrf.get_pan());

	// uncomment if you want to receive any packet on this channel
	mrf.set_promiscuous(true);
	  
	// uncomment if you want to enable PA/LNA external control
	//mrf.set_palna(true);
	  
	// uncomment if you want to buffer all PHY Payload
	//mrf.set_bufferPHY(true);

	wiringPiISR(pin_interrupt, INT_EDGE_FALLING, &interrupt_routine); // interrupt 0 equivalent to pin 2(INT0) on ATmega8/168/328
	
	unsigned int sendTime = 0;
	uint8_t message_n = 0;
	
	for(;;) {
		mrf.check_flags(&handle_rx, &handle_tx);
		if (txTriggered) {
			piLock(BUTTON_KEY);
				txTriggered = 0;
			piUnlock(BUTTON_KEY);
			printf("\nAção recebida. Enviando mensagem...\n");
			while(number_used(message_n)) {
				message_n += 2;
				if (message_n == 32) message_n = 0;
			}
			
			char msg[] = {(char)(0b00100000 | message_n), (char)((addr64>>56) & 0xff), (char)((addr64>>48) & 0xff), (char)((addr64>>40) & 0xff), (char)((addr64>>32) & 0xff), (char)((addr64>>24) & 0xff), (char)((addr64>>16) & 0xff), (char)((addr64>>8) & 0xff), (char)(addr64 & 0xff), 0b00000010, '\0'};
			message_send64(mrf, 0x1111111111111112, msg);

		}
		if(millis() > sendTime) {
			std::queue<message_list> message_queue = get_queue();

			while(!message_queue.empty()) {
				message_list tmp_list = message_queue.front();
				message_queue.pop();
				if(tmp_list.active) {
					printf("\n##############Fila de envio...##############\n");
					printf("\nRemetente: %X\tDestinatario: %X\tNumero: %i\t Tipo: %i", tmp_list.message[8], (int)(tmp_list.address & 0xff), tmp_list.number, tmp_list.message[0]);
					mrf.send64(tmp_list.address, (char *)tmp_list.message);
				}
				delay(300);
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
		handle_packets(mrf, &server_handler);
	piUnlock(TIMER_KEY);
	
    /*printf("\nreceived a packet ");
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
    printf("%d\n", mrf.get_rxinfo()->rssi);*/
}

void handle_tx() {
    /*if (mrf.get_txinfo()->tx_ok) {
        printf("TX went ok, got ack\n");
    } else {
        printf("TX failed after ");
        printf("%d", mrf.get_txinfo()->retries);
        printf(" retries\n");
    }*/
}

void server_handler(uint8_t * msg) {
	printf("Tratando mensagem de cliente\n");
	char location[64];
	char timestamp[33];
	sprintf(timestamp, "%u", millis());
	strcpy(location, "/var/www/html/iven_status/");
	strcat(location, timestamp);
	strcat(location, ".txt");
	FILE * file = fopen(location, "w+");
	uint64_t destino = mrf.get_dest_address64();
	uint64_t origem = routed_dest_address64();
	fprintf(file, "%X%X%X%X\t%X%X%X%X\t%i", (word)((origem>>48) & 0xffff), (word)((origem>>32) & 0xffff), (word)((origem>>16) & 0xffff), (word)(origem & 0xffff), (word)((destino>>48) & 0xffff), (word)((destino>>32) & 0xffff), (word)((destino>>16) & 0xffff), (word)(destino & 0xffff), msg[9]);
	fclose(file);
	printf("Dados postados e notificações enviadas!");
	//system("sudo python ../Notifications/sinistronotifica.py");
}
