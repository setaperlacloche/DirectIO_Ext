#ifndef _I2C_H
#define _I2C_H 1

/*
Some I2C basics :
	- SDA must be steady while SCL is HIGH.
	- SDA must be read just before SCL falling edge.
	- SDA must be written just after SCL falling edge.
*/
#include "DirectIO.h"
#ifdef DIRECTIO_FALLBACK
#error "DIRECTIO_FALLBACK is defined, it's mean that DirectIO is not activated. Possible reason ARDUINO_AVR_NANO is not defined due to configuration mistake in boards.txt (missing XXXX.build.board=AVR_NANO)."
#endif

// Global delay for quick setup
#ifndef I2C_DELAY
#define I2C_DELAY
#endif

// Example of short delay macro :
//#ifndef I2C_WRITE_DELAY
//#define I2C_WRITE_DELAY __asm__ __volatile__ ("nop\n\t")
//#endif

// Minimum delay before SCL raising. This is required for pull up to take effect.
#ifndef I2C_SCL_LOW_DELAY
#define I2C_SCL_LOW_DELAY	I2C_DELAY
#endif

// Minimum delay before SCL falling.
#ifndef I2C_SCL_HIGH_DELAY
#define I2C_SCL_HIGH_DELAY	I2C_DELAY
#endif

#ifndef I2C_START_DELAY
#define	I2C_START_DELAY	I2C_DELAY
#endif

#ifndef I2C_STOP_DELAY
#define	I2C_STOP_DELAY	I2C_DELAY
#endif

#ifndef I2C_READ_SCL_LOW_EXTRA_DELAY		// This delay only applies when SDA is an input (Read data ou read ACK from device)
#define I2C_READ_SCL_LOW_EXTRA_DELAY asm( "rjmp	.+0		;2tcy	\n" )
#endif


#define I2C_SCL_RAISE		{ I2C_SCL_LOW_DELAY; _scl = HIGH; I2C_SCL_HIGH_DELAY; }
#define I2C_SCL_FALL		{ _scl = LOW; }
#define I2C_SCL_PULSE		{ I2C_SCL_RAISE; I2C_SCL_FALL; }


#define _I2C_WRITE_BIT asm volatile( 		\
		"rol	%[b]	; MSB first	\n"		\
		"brcs	1f		; 			\n"		\
		"nop			; 1tcy		\n"		\
		: : [b] "r" (b) : );				\
		_sda=LOW;							\
	asm volatile( 							\
		"rjmp	2f					\n"		\
		"1:							\n" );	\
		_sda=HIGH;							\
	asm volatile( 							\
		"rjmp	.+0		; 2tcy		\n"		\
		"2:							\n" );


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma GCC push_options
#pragma GCC optimize ("O3")		// By default is -Os ==> pin changes are moved to a sub-routine. O3 disable code size optimisation
template <byte scl, byte sda>
class I2C {
	protected:
	Output<scl> _scl;
	Output<sda> _sda;

	public:
	I2C() : _scl(HIGH), _sda(HIGH) {
		_scl = HIGH;
		_sda = HIGH;
	}
	
	#pragma GCC push_options
	#pragma GCC optimize ("-O3")

	__attribute__((always_inline))
	static inline void _start() {
		Output<sda>::write( LOW );
		I2C_START_DELAY;
		Output<scl>::write( LOW );
	}
	
	__attribute__((always_inline))
	static inline void _stop() {
		Output<scl>::write( HIGH );
		I2C_STOP_DELAY;
		Output<sda>::write( HIGH );
	}
	
	//__attribute__((always_inline))  // Alonge à 7 le timing entre D7 et D6.
	// Minimum timings (I2C_READ_SCL_LOW_EXTRA_DELAY = 0tcy) : 
	//     +--+    +--+    +-...    +--+          +--+                 +-...
	//     | 3|  5 | 3|  5 |        | 3|  4 +  5  | 2|     >=17        |
	//  ---+  +----+  +----+    ...-+  +----+-----+  +-----------------+
	//      D7       D6              D0     GND    ACK   (Next Call to same function)
	// Timing customization : TODO Specify a delay using I2C_READ_DELAY macro.
	// Bus must be busy before call (SDA=SCL=0).
	// Bus is busy after call (SDA=SCL=0).
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wuninitialized"
	__attribute__((noinline))
	byte readByte() {
		byte ret;				// Un-initialised variable to guarantee no optimisation when first used, so read SDA timings are guarantee.
		byte x;

		_sda.inputMode(true);   // SDA as input with pull-up
		I2C_SCL_RAISE; x = _sda.read(); I2C_SCL_FALL; ret <<= 1; if (x) ret++; I2C_READ_SCL_LOW_EXTRA_DELAY;
		I2C_SCL_RAISE; x = _sda.read(); I2C_SCL_FALL; ret <<= 1; if (x) ret++; I2C_READ_SCL_LOW_EXTRA_DELAY;
		I2C_SCL_RAISE; x = _sda.read(); I2C_SCL_FALL; ret <<= 1; if (x) ret++; I2C_READ_SCL_LOW_EXTRA_DELAY;
		I2C_SCL_RAISE; x = _sda.read(); I2C_SCL_FALL; ret <<= 1; if (x) ret++; I2C_READ_SCL_LOW_EXTRA_DELAY;
		I2C_SCL_RAISE; x = _sda.read(); I2C_SCL_FALL; ret <<= 1; if (x) ret++; I2C_READ_SCL_LOW_EXTRA_DELAY;
		I2C_SCL_RAISE; x = _sda.read(); I2C_SCL_FALL; ret <<= 1; if (x) ret++; I2C_READ_SCL_LOW_EXTRA_DELAY;
		I2C_SCL_RAISE; x = _sda.read(); I2C_SCL_FALL; ret <<= 1; if (x) ret++; I2C_READ_SCL_LOW_EXTRA_DELAY;
		I2C_SCL_RAISE; x = _sda.read(); I2C_SCL_FALL; ret <<= 1; if (x) ret++; I2C_READ_SCL_LOW_EXTRA_DELAY;

		_sda.outputMode(LOW);	// SDA = 0, ie send ACK
		I2C_SCL_PULSE;			// Send SCL pulse for ACK
		return ret;
	}
	#pragma GCC diagnostic pop

	// Returns : Success indicator (true ==> ACK by device, false ==> No-Ack)
	// Final state : Bus is busy (i.e. SCL=SDA=0) if ACK. Bus is freed (i.e. SCL=SDA=1) if NACK.
	// Timings from function boolean write(byte addr) (so with a START).
	//  ---+            +----------+----------+---------/ /---+-------+
	// SDA |     13     |    11    |    11    |         / /   |   8   |               
	//     +------------+----------+----------+---------/ /---+-------+ZZZZTZZZZZZ---
	//     <2>          <-4->      <-4->      <-4->           <-4-> <2>
	// ------+              +-+        +-+        +-+   / /       +-+     +--+      
	// SCL   |      15      |2|    9   |2|    9   |2|   / /       |2| 6   | 3|   
	//       +--------------+ +--------+ +--------+ +---/ /-------+ +-----+  +-------
	__attribute__((noinline))
	boolean _writeByte(byte b) {
		boolean nack;

		asm volatile( "nop	\n" );
		
		_I2C_WRITE_BIT; I2C_SCL_PULSE;
		_I2C_WRITE_BIT; I2C_SCL_PULSE;
		_I2C_WRITE_BIT; I2C_SCL_PULSE;
		_I2C_WRITE_BIT; I2C_SCL_PULSE;
		_I2C_WRITE_BIT; I2C_SCL_PULSE;
		_I2C_WRITE_BIT; I2C_SCL_PULSE;
		_I2C_WRITE_BIT; I2C_SCL_PULSE;
		_I2C_WRITE_BIT; I2C_SCL_PULSE;		// A conflict can occured here, if SDA is HIGH and data is acknowledged
		_sda.inputMode(true);				// SDA as input with pull-up
		I2C_READ_SCL_LOW_EXTRA_DELAY;		// Add some delay in High-Z
		I2C_SCL_RAISE; nack = _sda.read(); I2C_SCL_FALL;
		_sda.outputMode(LOW);	// SDA set to LOW output
		if (nack) _stop();
		return !nack;
	}
	#pragma GCC pop_options
	
	boolean readU8(byte addr, uint8_t* out) {
		_start();
		if (!_writeByte((addr<<1) | 0x01)) return false;
		*out   = readByte();
		_stop();
		return true;
	}
	boolean readU16(byte addr, uint16_t* out) {
		byte msb, lsb;
		
		_start();
		if (!_writeByte((addr<<1) | 0x01)) return false;
		msb = readByte();
		lsb = readByte();
		_stop();
		*out = (msb << 8) + lsb;
		return true;
	}
#pragma GCC push_options
#pragma GCC optimize ("Os")		// By default is -Os ==> pin changes are moved to a sub-routine. O3 disable code size optimisation
	boolean readBuffer(byte addr, uint8_t* out, uint8_t len) {
		asm( ";COUCOU" );
		_start();
		if (!_writeByte((addr<<1) | 0x01)) return false;
		for( ; len > 0; len-- ) 
			*out++ = readByte();
		_stop();
		return true;
	}
#pragma GCC pop_options
	
	boolean write(byte addr) {
		_start();
		if (!_writeByte(addr << 1)) return false;
		_stop();
		return true;
	}
	boolean write(byte addr, byte n1) {
		_start();
		if (!_writeByte(addr << 1)) return false;
		if (!_writeByte(n1)) return false;
		_stop();
		return true;
	}
	boolean write(byte addr, byte n1, byte n2) {
		_start();
		if (!_writeByte(addr << 1)) return false;
		if (!_writeByte(n1)) return false;
		if (!_writeByte(n2)) return false;
		_stop();
		return true;
	}
	boolean write(byte addr, byte n1, byte n2, byte n3) {
		_start();
		if (!_writeByte(addr << 1)) return false;
		if (!_writeByte(n1)) return false;
		if (!_writeByte(n2)) return false;
		if (!_writeByte(n3)) return false;
		_stop();
		return true;
	}
};
#pragma GCC pop_options

#endif // _I2C_H
