#include "led.h"

Led::Led(int pin_led) {
    _pin_led = pin_led;
}

void Led::init(void) {
	pinMode(_pin_led, OUTPUT);
}

void Led::on(void) {
    digitalWrite(_pin_led, HIGH);
}

void Led::off(void) {
    digitalWrite(_pin_led, LOW);
}
