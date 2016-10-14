/**
 * mrf24j.cpp, Karl Palsson, 2011, karlp@tweak.net.au
 * modified bsd license / apache license
 */

#include "mrf24j.h"

// aMaxPHYPacketSize = 127, from the 802.15.4-2006 standard.
static uint8_t rx_buf[127];

// essential for obtaining the data frame only
// bytes_MHR = 2 Frame control + 1 sequence number + 2 panid + 2 shortAddr Destination + 2 shortAddr Source
static int bytes_MHR = 9;
static int bytes_MHR_64 = 21;
static int bytes_FCS = 2; // FCS length = 2
static int bytes_nodata = bytes_MHR + bytes_FCS; // no_data bytes in PHY payload,  header length + FCS
static int bytes_nodata_64 = bytes_MHR_64 + bytes_FCS;

static int ignoreBytes = 0; // bytes to ignore, some modules behaviour.

static boolean bufPHY = false; // flag to buffer all bytes in PHY Payload, or not

volatile uint8_t flag_got_rx;
volatile uint8_t flag_got_tx;

static rx_info_t rx_info;
static tx_info_t tx_info;


/**
 * Constructor MRF24J Object.
 * @param pin_reset, @param pin_chip_select, @param pin_interrupt
 */
Mrf24j::Mrf24j(int pin_reset, int pin_chip_select, int pin_interrupt) {
    _pin_reset = pin_reset;
    _pin_cs = pin_chip_select;
    _pin_int = pin_interrupt;
}

void Mrf24j::reset(void) {
    digitalWrite(_pin_reset, LOW);
    delay(10);  // just my gut
    digitalWrite(_pin_reset, HIGH);
    delay(20);  // from manual
}

byte Mrf24j::read_short(byte address) {
    digitalWrite(_pin_cs, LOW);
    // 0 top for short addressing, 0 bottom for read
    bcm2835_spi_transfer(address<<1 & 0b01111110);
    byte ret = bcm2835_spi_transfer(0x00);
    digitalWrite(_pin_cs, HIGH);
    return ret;
}

byte Mrf24j::read_long(word address) {
    digitalWrite(_pin_cs, LOW);
    byte ahigh = address >> 3;
    byte alow = address << 5;
    bcm2835_spi_transfer(0x80 | ahigh);  // high bit for long
    bcm2835_spi_transfer(alow);
    byte ret = bcm2835_spi_transfer(0);
    digitalWrite(_pin_cs, HIGH);
    return ret;
}


void Mrf24j::write_short(byte address, byte data) {
    digitalWrite(_pin_cs, LOW);
    // 0 for top short address, 1 bottom for write
    bcm2835_spi_transfer((address<<1 & 0b01111110) | 0x01);
    bcm2835_spi_transfer(data);
    digitalWrite(_pin_cs, HIGH);
}

void Mrf24j::write_long(word address, byte data) {
    digitalWrite(_pin_cs, LOW);
    byte ahigh = address >> 3;
    byte alow = address << 5;
    bcm2835_spi_transfer(0x80 | ahigh);  // high bit for long
    bcm2835_spi_transfer(alow | 0x10);  // last bit for write
    bcm2835_spi_transfer(data);
    digitalWrite(_pin_cs, HIGH);
}

word Mrf24j::get_pan(void) {
    byte panh = read_short(MRF_PANIDH);
    return panh << 8 | read_short(MRF_PANIDL);
}

void Mrf24j::set_pan(word panid) {
    write_short(MRF_PANIDH, panid >> 8);
    write_short(MRF_PANIDL, panid & 0xff);
}

void Mrf24j::address16_write(word address16) {
    write_short(MRF_SADRH, address16 >> 8);
    write_short(MRF_SADRL, address16 & 0xff);
}

word Mrf24j::address16_read(void) {
    byte a16h = read_short(MRF_SADRH);
    return a16h << 8 | read_short(MRF_SADRL);
}

void Mrf24j::address64_write(uint64_t address64) {
    write_short(MRF_EADR0, address64 & 0xff);
    write_short(MRF_EADR1, (address64 >> 8) & 0xff);
    write_short(MRF_EADR2, (address64 >> 16) & 0xff);
    write_short(MRF_EADR3, (address64 >> 24) & 0xff);
    write_short(MRF_EADR4, (address64 >> 32) & 0xff);
    write_short(MRF_EADR5, (address64 >> 40) & 0xff);
    write_short(MRF_EADR6, (address64 >> 48) & 0xff);
    write_short(MRF_EADR7, (address64 >> 56) & 0xff);
}

uint64_t Mrf24j::address64_read(void) {
    uint64_t a64h = read_short(MRF_EADR7);
    a64h = a64h << 8 | read_short(MRF_EADR6);
    a64h = a64h << 8 | read_short(MRF_EADR5);
    a64h = a64h << 8 | read_short(MRF_EADR4);
    a64h = a64h << 8 | read_short(MRF_EADR3);
    a64h = a64h << 8 | read_short(MRF_EADR2);
    a64h = a64h << 8 | read_short(MRF_EADR1);
    return a64h << 8 | read_short(MRF_EADR0);
}

uint64_t Mrf24j::get_src_address64(void) {
	uint64_t src_addr = 0;
	
	for(int i = 0; i < 8; i++) {
		
			src_addr |= (uint64_t)read_long(0x30e + i) << 8*i; // recebe e armazena endereço da fonte
		
	}
	
	return src_addr;
}

uint64_t Mrf24j::get_dest_address64(void) {
	uint64_t dest_addr = 0;
	
	for(int i = 0; i < 8; i++) {
		
			dest_addr |= (uint64_t)read_long(0x306 + i) << 8*i; // recebe e armazena endereço da fonte
		
	}
	
	return dest_addr;
}

/**
 * Simple send 16, with acks, not much of anything.. assumes src16 and local pan only.
 * @param data
 */
void Mrf24j::send16(word dest16, char * data) {
    byte len = strlen(data); // get the length of the char* array
    int i = 0;
    write_long(i++, bytes_MHR); // header length
    // +ignoreBytes is because some module seems to ignore 2 bytes after the header?!.
    // default: ignoreBytes = 0;
    write_long(i++, bytes_MHR+ignoreBytes+len);

    // 0 | pan compression | ack | no security | no data pending | data frame[3 bits]
    write_long(i++, 0b01100001); // first byte of Frame Control
    // 16 bit source, 802.15.4 (2003), 16 bit dest,
    write_long(i++, 0b10001000); // second byte of frame control
    write_long(i++, 1);  // sequence number 1

    word panid = get_pan();

    write_long(i++, panid & 0xff);  // dest panid
    write_long(i++, panid >> 8);
    write_long(i++, dest16 & 0xff);  // dest16 low
    write_long(i++, dest16 >> 8); // dest16 high

    word src16 = address16_read();
    write_long(i++, src16 & 0xff); // src16 low
    write_long(i++, src16 >> 8); // src16 high

    // All testing seems to indicate that the next two bytes are ignored.
    //2 bytes on FCS appended by TXMAC
    i+=ignoreBytes;
    for (int q = 0; q < len; q++) {
        write_long(i++, data[q]);
    }
    // ack on, and go!
    write_short(MRF_TXNCON, (1<<MRF_TXNACKREQ | 1<<MRF_TXNTRIG));
}

/**
 * Simple send 16, with acks, not much of anything.. assumes src16 and local pan only.
 * @param data
 */
void Mrf24j::send64(uint64_t dest64, char * data) {
    byte len = strlen(data); // get the length of the char* array
    int i = 0;
    write_long(i++, bytes_MHR_64); // header length
    // +ignoreBytes is because some module seems to ignore 2 bytes after the header?!.
    // default: ignoreBytes = 0;
    write_long(i++, bytes_MHR_64+ignoreBytes+len);

    // 0 | pan compression | ack | no security | no data pending | data frame[3 bits]
    write_long(i++, 0b01000001); // first byte of Frame Control
    // 16 bit source, 802.15.4 (2003), 16 bit dest,
    write_long(i++, 0b11001100); // second byte of frame control
    write_long(i++, 1);  // sequence number 1

    word panid = get_pan();

    write_long(i++, panid & 0xff);  // dest panid
    write_long(i++, panid >> 8);
    
    write_long(i++, dest64 & 0xff);  // dest16 low
    write_long(i++, (dest64 >> 8) & 0xff); // dest16 high
    write_long(i++, (dest64 >> 16) & 0xff);
    write_long(i++, (dest64 >> 24) & 0xff);
    write_long(i++, (dest64 >> 32) & 0xff);
    write_long(i++, (dest64 >> 40) & 0xff);
    write_long(i++, (dest64 >> 48) & 0xff);
    write_long(i++, (dest64 >> 56) & 0xff);

    uint64_t src64 = address64_read();
    write_long(i++, src64 & 0xff); // src16 low
    write_long(i++, (src64 >> 8) & 0xff); // src16 high
    write_long(i++, (src64 >> 16) & 0xff);
    write_long(i++, (src64 >> 24) & 0xff);
    write_long(i++, (src64 >> 32) & 0xff);
    write_long(i++, (src64 >> 40) & 0xff);
    write_long(i++, (src64 >> 48) & 0xff);
    write_long(i++, (src64 >> 56) & 0xff);

    // All testing seems to indicate that the next two bytes are ignored.
    //2 bytes on FCS appended by TXMAC
    i+=ignoreBytes;
    for (int q = 0; q < len; q++) {
        write_long(i++, data[q]);
    }
    // ack on, and go!
    write_short(MRF_TXNCON, (1<<MRF_TXNTRIG));
}

void Mrf24j::set_interrupts(void) {
    // interrupts for rx and tx normal complete
    write_short(MRF_INTCON, 0b11110110);
}

/** use the 802.15.4 channel numbers..
 */
void Mrf24j::set_channel(byte channel) {
    write_long(MRF_RFCON0, (((channel - 11) << 4) | 0x03));
}

void Mrf24j::init(void) {
	pinMode(_pin_reset, OUTPUT);
    pinMode(_pin_cs, OUTPUT);
    pinMode(_pin_int, INPUT);

    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS_NONE);
    /*
    // Seems a bit ridiculous when I use reset pin anyway
    write_short(MRF_SOFTRST, 0x7); // from manual
    while (read_short(MRF_SOFTRST) & 0x7 != 0) {
        ; // wait for soft reset to finish
    }
    */
    write_short(MRF_PACON2, 0x98); // – Initialize FIFOEN = 1 and TXONTS = 0x6.
    write_short(MRF_TXSTBL, 0x95); // – Initialize RFSTBL = 0x9.

    write_long(MRF_RFCON0, 0x03); // – Initialize RFOPT = 0x03.
    write_long(MRF_RFCON1, 0x01); // – Initialize VCOOPT = 0x02.
    write_long(MRF_RFCON2, 0x80); // – Enable PLL (PLLEN = 1).
    write_long(MRF_RFCON6, 0x90); // – Initialize TXFIL = 1 and 20MRECVR = 1.
    write_long(MRF_RFCON7, 0x80); // – Initialize SLPCLKSEL = 0x2 (100 kHz Internal oscillator).
    write_long(MRF_RFCON8, 0x10); // – Initialize RFVCO = 1.
    write_long(MRF_SLPCON1, 0x21); // – Initialize CLKOUTEN = 1 and SLPCLKDIV = 0x01.

    //  Configuration for nonbeacon-enabled devices (see Section 3.8 “Beacon-Enabled and
    //  Nonbeacon-Enabled Networks”):
    write_short(MRF_BBREG2, 0x80); // Set CCA mode to ED
    write_short(MRF_CCAEDTH, 0x60); // – Set CCA ED threshold.
    write_short(MRF_BBREG6, 0x40); // – Set appended RSSI value to RXFIFO.
    set_interrupts();
    set_channel(12);
    // max power is by default.. just leave it...
    write_long(MRF_RFCON3, 0xf8); // Set transmitter power - See “REGISTER 2-62: RF CONTROL 3 REGISTER (ADDRESS: 0x203)”.
    write_short(MRF_RFCTL, 0x04); //  – Reset RF state machine.
    write_short(MRF_RFCTL, 0x00); // part 2
    delay(1); // delay at least 192usec
}

void Mrf24j::eraseRxBuff(void) {
	for(int i = 0; i < 116 ; i++) {
		rx_info.rx_data[i] = 0;
	}
}

/**
 * Call this from within an interrupt handler connected to the MRFs output
 * interrupt pin.  It handles reading in any data from the module, and letting it
 * continue working.
 * Only the most recent data is ever kept.
 */
void Mrf24j::interrupt_handler(void) {
    uint8_t last_interrupt = read_short(MRF_INTSTAT);
    if (last_interrupt & MRF_I_RXIF) {
        
        // read out the packet data...
        rx_disable();
        // read start of rxfifo for, has 2 bytes more added by FCS. frame_length = m + n + 2
        uint8_t frame_length = read_long(0x300);

        // buffer all bytes in PHY Payload
        if(bufPHY){
            int rb_ptr = 0;
            for (int i = 0; i < frame_length; i++) { // from 0x301 to (0x301 + frame_length -1)
                rx_buf[rb_ptr++] = read_long(0x301 + i);
            }
        }
        
        int bytes_MHR_R, bytes_nodata_R;
 
        //Check MHR size through frame control
        byte frame_control_H = read_long(0x302);
        if(frame_control_H == 0xcc) {
			bytes_MHR_R = bytes_MHR_64;
			bytes_nodata_R = bytes_nodata_64;
		}
        else if(frame_control_H == 0x88) {
			bytes_MHR_R = bytes_MHR;
			bytes_nodata_R = bytes_nodata;
		} else if(frame_control_H == 0x00) {
			bytes_nodata_R = frame_length;
		}
		
		eraseRxBuff();

        // buffer data bytes
        int rd_ptr = 0;
        // from (0x301 + bytes_MHR) to (0x301 + frame_length - bytes_nodata - 1)
        
        for (int i = 0; i < (frame_length - bytes_nodata_R); i++) {
            rx_info.rx_data[rd_ptr++] = read_long(0x301 + bytes_MHR_R + i);
        }

        rx_info.frame_length = frame_length;
        // same as datasheet 0x301 + (m + n + 2) <-- frame_length
        rx_info.lqi = read_long(0x301 + frame_length);
        // same as datasheet 0x301 + (m + n + 3) <-- frame_length + 1
        rx_info.rssi = read_long(0x301 + frame_length + 1);
        
        flag_got_rx++;

        rx_enable();
    }
    if (last_interrupt & MRF_I_TXNIF) {
        
        uint8_t tmp = read_short(MRF_TXSTAT);
        // 1 means it failed, we want 1 to mean it worked.
        tx_info.tx_ok = !(tmp & ~(1 << TXNSTAT));
        tx_info.retries = tmp >> 6;
        tx_info.channel_busy = (tmp & (1 << CCAFAIL));
        
        flag_got_tx++;
    }
}


/**
 * Call this function periodically, it will invoke your nominated handlers
 */
void Mrf24j::check_flags(void (*rx_handler)(void), void (*tx_handler)(void)){
    // TODO - we could check whether the flags are > 1 here, indicating data was lost?
    if (flag_got_rx) {
        flag_got_rx = 0;
        rx_handler();
    }
    if (flag_got_tx) {
        flag_got_tx = 0;
        tx_handler();
    }
}

/**
 * Set RX mode to promiscuous (without auto ack), or normal
 */
void Mrf24j::set_promiscuous(boolean enabled) {
    if (enabled) {
        write_short(MRF_RXMCR, 0x21);
    } else {
        write_short(MRF_RXMCR, 0x00);
    }
}

rx_info_t * Mrf24j::get_rxinfo(void) {
    return &rx_info;
}

tx_info_t * Mrf24j::get_txinfo(void) {
    return &tx_info;
}

uint8_t * Mrf24j::get_rxbuf(void) {
    return rx_buf;
}

int Mrf24j::rx_datalength(void) {
    return rx_info.frame_length - bytes_nodata;
}

void Mrf24j::set_ignoreBytes(int ib) {
    // some modules behaviour
    ignoreBytes = ib;
}

/**
 * Set bufPHY flag to buffer all bytes in PHY Payload, or not
 */
void Mrf24j::set_bufferPHY(boolean bp) {
    bufPHY = bp;
}

boolean Mrf24j::get_bufferPHY(void) {
    return bufPHY;
}

/**
 * Set PA/LNA external control
 */
void Mrf24j::set_palna(boolean enabled) {
    if (enabled) {
        write_long(MRF_TESTMODE, 0x07); // Enable PA/LNA on MRF24J40MB module.
    }else{
        write_long(MRF_TESTMODE, 0x00); // Disable PA/LNA on MRF24J40MB module.
    }
}

void Mrf24j::rx_flush(void) {
    write_short(MRF_RXFLUSH, 0x01);
}

void Mrf24j::rx_disable(void) {
    write_short(MRF_BBREG1, 0x04);  // RXDECINV - disable receiver
}

void Mrf24j::rx_enable(void) {
    write_short(MRF_BBREG1, 0x00);  // RXDECINV - enable receiver
}
