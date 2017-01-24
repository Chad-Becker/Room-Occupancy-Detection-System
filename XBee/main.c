/*
 * Muntaser Ahmed
 * Chad Becker
 * Usman Chaudhry
 * Venkata-Gautam Kanumuru
 * XBee
 * 11/19/14
 */

#include <msp430.h>

// Defines
#define redLED		BIT0	//"redLED" can now be used to represent BIT0.
#define greenLED	BIT6	//"greenLED" can now be used to represent BIT6.
#define SCK			BIT5	//"SCK" can now be used to represent BIT5. SCK is the serial clock. Port 2.
#define SO			BIT2	//"SO" can now be used to represent BIT2. SO is master input, slave output. Port 2.
#define SI			BIT7	//"SI" can now be used to represent BIT7. SI is master output, slave input. Port 1
#define nSSEL		BIT1	//"nSSEL" can now be used to represent BIT1. nSSEL is slave select. Active low. Port 2.
#define MSB			0x80	//"MSB" can now be used to represent 0x80. This is the most significant bit of a byte (10000000).
#define LSB			0x01	//"LSB" can now be used to represent 0x01. This is the least significant bit of a byte (00000001).
#define nRST		BIT0	//"nRST" can now be used to represent BIT0. This is the slave reset. Active low. Port 2.
#define nATTN		BIT4	//"nATTN" can now be used to represent BIT4. This is the attention bit that indicates the slave has data to send to the master. Active low. Port 2.
#define DOUT		BIT3	//"DOUT" can now be used to represent BIT3. It is used for setting the XBee to SPI mode. Port 2.
typedef unsigned char byte;

// Function Declarations
void init_CPU_CLK();						//Initializes CPU clock to 1MHz
void init_Ports();							//Initializes the LED and SPI interface pins of ports 1 and 2
void setup();								//Pre-instructions common to other functions (sets nSSEL and SCK)
void send_Instruction(byte instruction);	//Sends a byte via bit-banging
byte read_Byte();							//Reads a byte, bit-by-bit
void send_AT_Cmd();							//Sends an AT command to set the SSID of the network: wahoo
int check_Attention();						//Checks if the attention bit is asserted to determine if the slave has data to send to the master
byte int_To_Byte(unsigned int x);			//Converts an integer to a byte represented in hex
void init_Timers(void);						//Initializes the timer and its interrupt
void config_ADC(void);						//Configures the ADC for sampling the temperature from the internal sensor
void send_Temperature();					//Sends the temperature sample to the slave and ultimately to the server

// Global variables
byte ATCommandReturn[80];					//Saves the response of the AT command
unsigned int temperature = 1;				//Initializes temperature sample to 1

void main(void) {
	WDTCTL = WDTPW | WDTHOLD;				//Stops watchdog timer by providing the correct password and hold command.

	init_CPU_CLK();							//Initializes CPU clock to 1MHz
	init_Ports();							//Initializes the LED and SPI interface pins of ports 1 and 2
	init_Timers();							//Initializes the timer and its interrupt
	config_ADC();							//Configures the ADC for sampling the temperature from the internal sensor

	ADC10CTL0 |= ENC + ADC10SC;				//Enables and starts the conversion and sampling process.

	send_AT_Cmd();							//Sends an AT command to set the SSID of the network: wahoo

	__bis_SR_register(GIE);					//General interrupt enable. Enables maskable interrupts.

	while (1) {								//Keeps the program running
		if ((P2IN & nATTN) == 0) {			//True if the slave has data to send to the master
			P1OUT |= greenLED;
			unsigned int checkingAttentionArray = 0;
			while ((P2IN & nATTN) == 0) {	//Occurs if the slave still has data to send to the master
				ATCommandReturn[checkingAttentionArray] = read_Byte();		//Reads and saves the response
				checkingAttentionArray++;
			}
			P1OUT &= ~greenLED;
		}
	}
}

void init_Timers(void) { 							//Initializes the timers and its interrupt
	TACCR0 = 0xF424;	 							//Sets the upper bound of the timer for requesting an interrupt. 2 samples per second with 1/8MHz clock.
	TACCTL0 = CM_0 | CCIE;							//Disables the timer's capture mode and enables the capture/compare interrupt for compare mode.
	TACTL = TASSEL_2 | ID_3 | MC_1 | TACLR;			//Sets the SMCLK to drive the timer, divides the SMCLK frequency by 8 (1/8MHz), sets the timer for counting up to TACCR0 and then resetting, and clears the timer
}

void config_ADC(void) {												//Function that sets the ADC's reference voltages, sets the ADC's clock speed, and selects the temperature sensor as the source that is converted
	ADC10CTL0 &= ~ENC;												//Disables the ADC while initializing it
	ADC10CTL0 &= ~ADC10IE;											//Disables the ADC interrupt flag while initializing
	ADC10CTL0 = SREF_1 + ADC10SHT_3 + REFON + ADC10ON;				//Sets the positive reference voltage and makes the negative voltage reference ground, sets the sample-and-hold time for 64 × ADC10CLKs, turns the reference generator on, and turns the ADC10 on
	__delay_cycles(1000);											//Waits for ADC reference voltages to settle
	ADC10CTL1 = INCH_10 + ADC10SSEL_0 + ADC10DIV_3 + CONSEQ_0;		//Selects the temperature sensor as the input channel, chooses the internal oscillator clock source, divides the clock source by 4, and chooses the single channel single conversion option
}

void init_CPU_CLK() {						//Initializes CPU clock to 1MHz
	BCSCTL1 = CALBC1_1MHZ;					//Calibrates the basic clock to 1MHz. Sets the range.
	DCOCTL = CALDCO_1MHZ;					//Calibrates the DCO to 1MHz. Sets the DCO step and modulation.
}

void init_Ports() {							//Initializes the LED and SPI interface pins of ports 1 and 2
	P1OUT &= ~greenLED;						//Green LED is off if pin is made to be an output.
	P1OUT &= ~redLED;   					//Red LED is off if pin is made to be an output.
	P1DIR |= greenLED + redLED;				//Makes the red and green LED pins outputs

	P2OUT &= ~SCK;				//Sets SCK low
	P2DIR |= SCK;				//Sets SCK as output
	P1OUT &= ~SI;				//Sets SI low
	P1DIR |= SI;				//Sets SI as output
	P2OUT |= nSSEL;				//Sets nSSEL high to disable slave (because it is active low)
	P2DIR |= nSSEL;				//Sets nSSEL as output

	P2REN |= SO;				//Enables the pullup/pulldown resistor for SO
	P2OUT &= ~SO;				//Pulls the resistor low (SO is active high)
	P2DIR &= ~SO;				//Sets SO as input

	P2OUT |= nRST;				//Sets RST high (active low)
	P2DIR |= nRST;				//Sets RST as output

	P2REN |= nATTN;				//Enables the pullup/pulldown resistor for nATTN
	P2OUT |= nATTN;				//Pulls the pull up/pull down resistor high (nATTN is active low)
	P2DIR &= ~nATTN;			//Sets nATTN as input

	//Set the XBee to SPI mode
	P2OUT &= ~DOUT;				//Sets DOUT low
	P2DIR |= DOUT;				//Sets DOUT as output

	P2OUT &= ~nRST;				//Sets RST low
	P2OUT |= nRST;				//Sets RST high

	while ((P2IN & nATTN) != 0) {					//Waits for the nATTN bit to assert, indicating that the XBee is now in SPI mode and not UART mode
	}
	//P1OUT |= greenLED;

	unsigned int xArray = 0;
	while ((P2IN & nATTN) == 0) {					//While the slave still has data to send to the master
		ATCommandReturn[xArray] = read_Byte();		//Reads and saves the response
		xArray++;
	}
	//P1OUT &= ~greenLED;

}

void setup() {			//Generic function called before SPI communication to make sure the slave is enabled and the clock is low.
	P2OUT |= nSSEL;		//Disables slave
	P2OUT &= ~nSSEL;	//Enables slave
	P2OUT &= ~SCK;		//Sets clock low
	__delay_cycles(8);
}

void send_Instruction(byte instruction) {			//Sends a byte via bit-banging
	int i;											//Counter variable for loop
	for (i = 0; i < 8; i++) {						//Loops through each bit in the byte (from MSB to LSB)
													//Check the MSB, and send the relevant value
		if ((instruction & MSB) == MSB) {			//True if MSB is a 1
			P1OUT |= SI;							//Sends a 1 to the slave
		} else {									//True if MSB is a 0
			P1OUT &= ~SI;							//Sends a 0 to the slave
		}
		instruction <<= 1;							//Shifts byte left by 1 so that the next bit to be sent is the MSB

		//Toggles clock to send the bit. Data is input to slave on the rising edge of the clock.
		__delay_cycles(8);
		P2OUT |= SCK;
		__delay_cycles(8);
		P2OUT &= ~SCK;
		__delay_cycles(8);
	}
}

byte read_Byte() {		//Reads a byte, bit-by-bit
	P2OUT &= ~nSSEL;	//Enables slave
	P2OUT &= ~SCK;		//Sets clock low
	__delay_cycles(8);

	byte returnByte = 0;			//Byte to be returned from the function
	int i;							//Counter variable for loop
	int x = 0;						//Counter variable that represents the number of bits that have been passed so far.
	for (i = 0; i < 8; i++) {		//Loops through each bit in the byte (from MSB to LSB)
		x++;						//An additional bit has been passed

		//Bit-banging logic
		if ((P2IN & SO) == SO) {	//True if the bit being read is high
			returnByte |= 0x01;		//puts a 1 in the byte to be returned
		}

		if (x != 8) {				//End-case check. Don't shift if we have all 8 bits.
			returnByte <<= 1;		//Shift bits left since we are constructing it from MSB to LSB
		}

		//Output of slave is sent on falling edge of SCK.
		__delay_cycles(8);
		P2OUT |= SCK;
		__delay_cycles(8);
		P2OUT &= ~SCK;
		__delay_cycles(8);
	}
	P2OUT |= nSSEL;					//Disables slave

	return returnByte;				//Returns the byte that was sent from the slave
}

void send_AT_Cmd() {				//Sends an AT command to set the SSID of the network: wahoo
	setup();
	send_Instruction(0x7E);			//Start delimiter
	send_Instruction(0x00);			//MSB of length
	send_Instruction(0x09);			//LSB of length
	send_Instruction(0x08);			//API frame identifier
	send_Instruction(0x01);			//Frame ID
	send_Instruction(0x49);			//MSB of AT command
	send_Instruction(0x44);			//LSB of AT command
	send_Instruction(0x77);			//Parameter values: wahoo
	send_Instruction(0x61);
	send_Instruction(0x68);
	send_Instruction(0x6F);
	send_Instruction(0x6F);
	byte x = 0xFF
			- (0x08 + 0x01 + 0x49 + 0x44 + 0x77 + 0x61 + 0x68 + 0x6F + 0x6F);
	send_Instruction(x);			//Checksum
	__delay_cycles(1000000);		//1s delay for connection to establish
}

void send_Temperature() {								//Sends the temperature sample to the slave and ultimately to the server
	unsigned int tempX = temperature;					//Most recent temperature sample
	byte FirstByte = int_To_Byte((tempX / 1000));		//Converts the thousandth's place to hex
	tempX = tempX % 1000;
	byte SecondByte = int_To_Byte((tempX / 100));		//Converts the hundredth's place to hex
	tempX = tempX % 100;
	byte ThirdByte = int_To_Byte((tempX / 10));			//Converts the tenth's place to hex
	tempX = tempX % 10;
	byte FourthByte = int_To_Byte(tempX);				//Converts the one's place to hex

	__delay_cycles(1000000);		//1s delay
	setup();
	send_Instruction(0x7E);			//Start delimiter
	send_Instruction(0x00);			//MSB of length
	send_Instruction(0x10);			//LSB of length
	send_Instruction(0x20);			//API frame identifier
	send_Instruction(0x01);			//Frame ID
	send_Instruction(0x36);			//IPV4 MSB
	send_Instruction(0xAD);			//IPV4
	send_Instruction(0x06);			//IPV4
	send_Instruction(0xC5);			//IPV4 LSB
	send_Instruction(0x26);			//Destination Port MSB
	send_Instruction(0x16);			//Destination Port LSB
	send_Instruction(0x26);			//Source Port MSB (Same as Destination Port for UDP)
	send_Instruction(0x16);			//Source Port LSB (Same as Destination Port for UDP)
	send_Instruction(0x00);     	//Protocol: UDP
	send_Instruction(0x00);     	//Transmit Options Bitfield
	send_Instruction(FirstByte);    //Payload
	send_Instruction(SecondByte);	//Payload
	send_Instruction(ThirdByte);	//Payload
	send_Instruction(FourthByte);	//Payload
	byte x = 0xFF
			- (0x20 + 0x01 + 0x36 + 0xAD + 0x06 + 0xC5 + 0x26 + 0x16 + 0x26
					+ 0x16 + 0x00 + 0x00 + FirstByte + SecondByte + ThirdByte
					+ FourthByte);
	send_Instruction(x);			//Checksum
	__delay_cycles(1000000);		//1s delay
}

byte int_To_Byte(unsigned int x) {					//Converts an integer to a byte represented in hex
	return (char) (((unsigned int) '0') + x);
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void timer_Routine(void) {		//Timer A's interrupt service routine for sampling temperature
	P1OUT |= redLED;						//Turns the red LED on to indicate that a sample is being taken from ADC10MEM and stored in RAM.
	temperature = ADC10MEM;					//A result from the conversion (a sample), which is stored in ADC10MEM, is now represented by variable "temperature."
	send_Temperature();						//Sends the temperature sample to the slave and ultimately to the server
	ADC10CTL0 |= ENC + ADC10SC;				//Enables and starts the conversion process for continued sampling.
	P1OUT &= ~redLED;						//Turns the red LED off to indicate that a sample has been taken from ADC10MEM and stored in RAM.
}
