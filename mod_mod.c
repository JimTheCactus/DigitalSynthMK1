#include <avr/io.h>
#include <math.h>

#define baseperiod (1.0/8000000.0*8.0)
#define A440 2273
#define A440by2 (A440>>1)
#define scaletop 85
#define scalebottom 24
#define scalerange (float)(scaletop-scalebottom)/(float)(1024)
#define reffreq (float)440.0

int main(void) {
	DDRB=1<<DDB1;
	DDRB=1<<DDB1;

	// Initialize the PWM
	OCR1A = A440by2; // Set the initial toggle period to half the A440 frequency
	TCCR1A =  1 << WGM11 | 1 << WGM10 | 0 <<COM1A1 | 1<<COM1A0; // Enable toggle mode, Fast PWM
	TCCR1B = 1 << WGM13 | 1 << WGM12 | 0 << CS12 | 1 << CS11 | 0 << CS10; // Enable /8 prescaler, Fast PWM
	
	//Initialize the ADC
	ADMUX = 1 << REFS0 | 0 << ADLAR | 0 << MUX0; // Use AVCC for reference, left align result, select pin 0
	ADCSRA = 1 << ADEN | 0 << ADATE | 0 << ADIE | 6 << ADPS0; // Enable ADC, no auto trigger or interrupts, use /64 prescaler
	DIDR0 = 1 << ADC0D;
	while (1) {
		ADCSRA |= (1 << ADSC);
		while (ADCSRA & (1 << ADSC)) {			
			// Wait for conversion to finish
		}
		float volt = (float) ADC;
//		float note = floor(volt*scalerange+(float)scalebottom);
		float note = volt*scalerange+(float)scalebottom;
		float freq = reffreq*pow(2,((note-57.0)/12.0));
		float period = 1.0/freq/2.0;
		unsigned int counts = (period/baseperiod);		
		OCR1A = counts; // Write the calculated 16 bit number into the register.
	}
}
