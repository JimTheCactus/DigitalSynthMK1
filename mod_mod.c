#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include "note_table.h"

#define PRESCALER 2 // 2 selects the /8 prescaler
#define A440 2273 // Ticks assuming 8MHz clock and /8 prescaler
#define A440_BY_2 (A440>>1) // Half so we can toggle
#define OSC_OFF (unsigned char) ~(3<< CS10); // All 0s for the CS regs.


int main(void) {
	// Set OCA1 (Pin B1 on the PDIP package) to be an output.
	DDRB=1<<DDB1;

	// Turn of all the stuff we're not using.
	PRR =
		1 << PRTWI | // Shutdown the I2C Interface
		1 << PRTIM2 | // Shutdown Timer2
		1 << PRTIM0 | // Shutdown Timer0
		0 << PRTIM1 | // Leave Timer1 on
		1 << PRSPI | // Shutdown the SPI interface
		1 << PRUSART0 | // Shutdown the UART interface
		0 << PRADC; // Leave the ADC on
		
	// Initialize the PWM
	OCR1A = A440_BY_2; // Set the initial toggle period to half the A440 period
	                   // (Half because it will toggle after this period)
	TCCR1A = 
		3 << WGM10 | // Set Fast PWM (lower 2 bits)
		1 << COM1A0; // Enable toggle mode
	TCCR1B =
		3 << WGM12 |       // Set Fast PWN (upper 2 bits)
		PRESCALER << CS10; // Enable /8 prescaler, Fast PWM
	
	// Initialize the ADC
	ADMUX =
		1 << REFS0 | // Use AVCC for reference
		0 << ADLAR | // Right align result
		0 << MUX0;   // Select pin 0
	ADCSRA =
		1 << ADEN |  // Enable ADC
		1 << ADATE | // Enable auto trigger
		1 << ADIE |  // Enable interrupt
		7 << ADPS0;  // use /128 prescaler
	ADCSRB = 0 << ADTS0; // Select free running auto trigger.
	DIDR0 = 1 << ADC0D; // Disable digital IO buffer on ADC0

	// Set IDLE sleep mode. Not much power savings,
	// but we need Timer1 to be alive.
	set_sleep_mode(SLEEP_MODE_IDLE);
	
	// Start the ADC
	ADCSRA |= (1 << ADSC);
	
	// Enable interrupts so we can wake up after conversions.
	sei();	
	
	while (1) {
		// Take a nap to save power till the next conversion finishes.
		sleep_mode();
	}
}

ISR(ADC_vect) {
	// Check if the button is pressed
	if (PINB & 0x01) { // If not (inverse logic)
		TCCR1B &= OSC_OFF; // Kill the oscillator
	}
	else { // Otherwise
		unsigned int input = ADC; // Get the input
		unsigned int period = get_period(input); // Lookup the oscillator period
		OCR1A = period; // Write it to the oscillator
		TCCR1B |= PRESCALER; // Turn the oscillator on
	}	
}
