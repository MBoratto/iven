#ifndef LIB_LED_H
#define LIB_LED_H

#include <bcm2835.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define OUTPUT BCM2835_GPIO_FSEL_OUTP
#define INPUT BCM2835_GPIO_FSEL_INPT

#define pinMode(x,y) bcm2835_gpio_fsel(x,y)
#define digitalWrite(x,y) bcm2835_gpio_write(x,y)

class Led
{
    public:
        Led(int pin_led);
		void init(void);
		
        void on(void);
        void off(void);

    private:
        int _pin_led;
};

#endif  /* LIB_LED_H */
