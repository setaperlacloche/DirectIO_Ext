/*
  ___  ___  ___  ___    _    _      _____ __  __
 / __|| __|| _ \|_ _|  /_\  | |    |_   _|\ \/ /
 \__ \| _| |   / | |  / _ \ | |__    | |   >  < 
 |___/|___||_|_\|___|/_/ \_\|____|___|_|  /_/\_\
                                 |___|          

A software serial TX only 

Allows to transmit data over one GPIO in 1N8 format without dedicated hardware (UART, USI ...).
Minimum baud time is 3tcy. It can be extended via SERIAL_TX_DELAY_ASM macro to put before #include "serial_tx.h".
Example :
    #define SERIAL_TX_DELAY_ASM SERIAL_TX_DELAY1_ASM
	#include "serial_tx.h"
==> Add 1tcy, so 4tcy per bit ie 4Mps @ 16MHz.

Example of usage :

    #define SERIAL_TX_DELAY_ASM SERIAL_TX_DELAY4_ASM SERIAL_TX_DELAY3_ASM		// 1Mbps @ 10MHz
	#include "serial_tx.h"
	Serial_TX<2> tx;		// GPIO 2 will be defined as output. Default HIGH level.

	atomic {				// Only required if an interrupt can occur
		tx.putbyte( 0x55 );
		tx.putbyte( 0xAA );
	}
*/


#include "DirectIO.h"
#ifdef DIRECTIO_FALLBACK
#error "DIRECTIO_FALLBACK is defined, it's mean that DirectIO is not activated. Possible reason ARDUINO_AVR_NANO is not defined due to configuration mistake in boards.txt (missing XXXX.build.board=AVR_NANO)."
#endif

#ifndef SERIAL_TX_DELAY_ASM	
#define SERIAL_TX_DELAY_ASM ""
#endif

// Constant delay to ensure correct timings at minimum code costs
#define SERIAL_TX_DELAY1_ASM	"nop		\n"
#define SERIAL_TX_DELAY2_ASM	"rjmp	.+0	\n"
#define SERIAL_TX_DELAY3_ASM	"rjmp	.+0	\nnop			\n"
#define SERIAL_TX_DELAY4_ASM	"rjmp .+0	\nrjmp .+0		\n"

#define _SERIAL_SEND_BIT(n)				\
"\tBST	%[B], " #n "				\n" \
"\tBLD	__tmp_reg__, %p[BIT]		\n" \
"\tOUT	%p[OUT], __tmp_reg__ 		\n" \
SERIAL_TX_DELAY_ASM


template <u8 pin>
class Serial_TX {
	public:
		Serial_TX(boolean initial_value=HIGH) {
            digitalWrite(pin, initial_value);
            pinMode(pin, OUTPUT);
        }

	static void putbyte(byte b) {
		asm volatile(
			"\tIN		__tmp_reg__, %p[OUT]	\n"
		: /* Empty */								// Output variables
		: [OUT]"I"(_pins<pin>::out-__SFR_OFFSET)	// Input variables
		: 											// Clobbered register
		);
		//		
		// Start bit
		// 
		_pins<pin>::output_write(LOW);
		asm volatile(
		SERIAL_TX_DELAY_ASM
		);
		//
		// Data bits from 0 to 7
		//
		asm volatile(
			_SERIAL_SEND_BIT(0)
			_SERIAL_SEND_BIT(1)
			_SERIAL_SEND_BIT(2)
			_SERIAL_SEND_BIT(3)
			_SERIAL_SEND_BIT(4)
			_SERIAL_SEND_BIT(5)
			_SERIAL_SEND_BIT(6)
			_SERIAL_SEND_BIT(7)
			"\tNOP						 	\n"
		: /* Empty */								// Output variables
		: [BIT]"I"(_pins<pin>::bit),				// Input variables
		  [OUT]"I"(_pins<pin>::out-__SFR_OFFSET),
		  [B]"r"(b)
		: "r0"										// Clobbered register
		);
		//
		// Stop bit
		//
		_pins<pin>::output_write(HIGH);
		asm volatile(
		SERIAL_TX_DELAY_ASM
		);
	}
    
    static void putbuf( byte* pt, byte len ) {
    	for( ; len > 0; len--, pt++ )
        	putbyte( *pt );
    }

	static void put02X( byte b ) {
		byte t = b >> 4;
		putbyte( t + ( (t >= 10) ? 'A' - 10 : '0' ) );
		t = b & 0x0F;
		putbyte( t + ( (t >= 10) ? 'A' - 10 : '0' ) );
	}

	__attribute__ ((always_inline))
	static inline void LF() {
		putbyte( '\n' );
	}

	__attribute__ ((always_inline))
	static inline void CR() {
		putbyte( '\r' );
	}
};
