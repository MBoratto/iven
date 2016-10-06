#include <bcm2835.h>
#include <stdio.h>
#include "led.h"

const int pin_led = 26;

Led led1(pin_led);

int main() {	
	bcm2835_init();
	led1.init();
	pinMode(21, OUTPUT);
	for(;;) {
		led1.on();
		digitalWrite(21, HIGH);
		delay(500);
		led1.off();
		digitalWrite(21, LOW);
		delay(500);
	}
}
