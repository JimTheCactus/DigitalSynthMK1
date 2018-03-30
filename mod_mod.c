#include <avr/io.h>
#include <math.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

#define PRESCALER 2 // 2 selects the /8 prescaler
#define PRESCALER_RATIO 8
#define TICK_PERIOD (1.0/(float)(F_CPU)*(float)(PRESCALER_RATIO))
#define A440 2273 // Ticks assuming 8MHz clock and /8 prescaler
#define A440_BY_2 (A440>>1) // Half so we can toggle
#define SEMITONES_PER_OCTAVE 12 // From music theory
#define SCALE_BOTTOM (24+12) // Note number on a scale where A440 is 57
#define SCALE_TOP (SCALE_BOTTOM+SEMITONES_PER_OCTAVE*2) // 2 octaves
#define SCALE_RANGE (float)(SCALE_TOP-SCALE_BOTTOM)/(float)(1024)
#define REF_FREQENCY (float)440.0 // Call A440 our reference frequency
#define REF_NOTE 57 // Call 57 the reference note number for A440.
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
		6 << ADPS0;  // use /64 prescaler
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
		// Check if the button is pressed
		if (PINB & 0x01) { // If not (inverse logic)
			TCCR1B &= OSC_OFF; // Kill the oscillator
		}
		else { // Otherwise
			TCCR1B |= PRESCALER; // Turn the oscillator on.
		}
		// Take a nap to save power till the next conversion finishes.
		sleep_mode();
	}
}

ISR(ADC_vect) {
	// Initialize to the reference note.
	static float oldnote = REF_NOTE; 
		
	float input = (float) ADC;  // Get the input value
	// Calculate the new note number
	float note = (input * SCALE_RANGE + (float)SCALE_BOTTOM);
	// Average the new note with the old one with 1/3rd feedback
	float note_with_feedback = round((note * 2.0 + oldnote) / 3.0);
	// Cache the old note for hysteresis. 
	oldnote = note_with_feedback;
	// Get the delta relative to our reference note offset by half for tuning.
	float note_offset = note_with_feedback - ((float)REF_NOTE - 0.5);
	// Calculate the exact fractional octave for our note
	float octave = note_offset / (float) SEMITONES_PER_OCTAVE;
	// Calculate the frequency of the note given it's octave value
	float freq = REF_FREQENCY * pow(2, octave);
	// Get the note's period and cut it in half since we're toggling.
	float period = (1.0 / freq) / 2.0;
	// Get the number of ticks it's we need to wait between toggles
	unsigned int counts = (period / TICK_PERIOD);
	// Write the calculated 16 bit number into the register.	
	OCR1A = counts; 
}
