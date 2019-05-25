/*
    AUTHOR: Shyam
    Domain: Embedded and Electronics
    Functions: initPins, initADC, readADC, initTimer1, startTimer1, stopTimer1, getMilliseconds,
        initUSART, USART_sendByte, USART_sendData, calculateCapacitance, calculateResistance, checkMode,
        updateLED, ISR (TIMER1_OVF_vect), main
    Macros: PIN_LOW, PIN_HIGH
    Global variables: pin_capacitance, pin_resistance, pin_voltage, pin_charge, pin_curent, pin_ts,
        pin_led[4], timer1_overflow, mode
*/

#include<avr/interrupt.h>
#include<avr/io.h>

#define PIN_LOW(port, pin) PORT##port &= !(1 << pin)
#define PIN_HIGH(port, pin) PORT##port |= (1 << pin)

#define MODE_IDLE 0
#define MODE_CAPACITANCE 3
#define MODE_RESISTANCE 2
#define MODE_VOLTAGE 1
#define MODE_CURRENT 4

const int pin_capacitance = 0; // A0
const int pin_resistance = 1; // A1
const int pin_voltage = 2; // A2
const int pin_charge = 4; // PORTB 
const int pin_current = 3; //A3
const int pin_ts = 5; //PORTB : On board led for trouble shooting

const int pin_led[4] = {4,5,6,7}; //PORTD

float timer1_overflow = 0; //to handle overflow of TCNT1
uint8_t mode = MODE_IDLE;

/*
    Function name: initPins
    Input: none
    Output: none
    Logic: Initialize all pins using the DDRX and PORTX registers
    Example call: initPins();
*/
void initPins() {
    //PORT C
    DDRC = 0; //INPUT

    //PORT B
    DDRB |= (1 << pin_ts) | (1 << pin_charge); //OUTPUT

    //PORTD 
    DDRD |= (1 << pin_led[0]) | (1 << pin_led[1]) | (1 << pin_led[2]) | (1 << pin_led[3]); //OUTPUT

    //PIN INITIAL OUTPUTS
    PIN_LOW(B, pin_charge);
    PIN_HIGH(B, pin_ts);
    PIN_LOW(D, pin_led[0]);
    PIN_LOW(D, pin_led[1]);
    PIN_LOW(D, pin_led[2]);
    PIN_LOW(D, pin_led[3]);
}

/*
    Function name: initADC
    Input: none
    Output: none
    Logic: Initialize ADC, use the VCC=5v reference, right shifted data, 128 prescalar
    Example call: initADC();
*/
void initADC() {
    ADMUX = (1 << REFS0);   //5V ref, ADC0
    ADCSRA = (1<<ADEN)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
}

/*
    Function name: readADC
    Input: {
        uint8_t pin: the analog input pin
    }
    Output: data read at pin <pin>
    Logic: Start conversion 
    Example call: 
        uint16_t value = readADC(0); //reads input voltage at pin ADC0
*/
uint16_t readADC(uint8_t pin)  
{
    ADMUX |= (pin); //select the input pin
    ADCSRA |= (1<<ADSC); //start conversion
    while (!(ADCSRA & (1 << ADIF))); //wait till conversion is complete
    ADCSRA |= (1<<ADIF); //clear the flag
    return ADC; //return the data read
}

/*
    Function name: initTimer1
    Input: none
    Output: none
    Logic: Initialize timers used: timer1 (16-bit)
    Example call: initTimer1();
*/
void initTimer1() {
    TCCR1A = 0; //default configuration
    TIMSK1 |= 1; //overflow interrupt enable
    timer1_overflow = 0;
}

/*
    Function name: startTimer1
    Input: none
    Output: none
    Logic: Start timer1 with 64 prescalar
    Example call: startTimer1();
*/
void startTimer1() {
    initTimer1();
    TCCR1B = (1 << CS11) | (1 << CS10); //64 prescalar
    TCNT1 = 0;
}

/*
    Function name: stopTimer1
    Input: none
    Output: none
    Logic: Stop timer1
    Example call: stopTimer1();
*/
void stopTimer1() {
    TCCR1B = 0; //no clock
    timer1_overflow = 0;
    TCNT1 = 0;
}

/*
    Function name: getMilliseconds
    Input: none
    Output: float - milliseconds elapsed since the timer1 started
    Logic: Get the time elapsed since timer1 started
    Example call: float time = getMilliseconds();
*/
float getMilliseconds() {
    //250 ticks = 1 millisecond
    float t = timer1_overflow + (float)(TCNT1)/250.0f;
    return t;
}

/*
    Function name: initUSART
    Input: none
    Output: none
    Logic: Initialize serial communication
    Example call: initUSART();
*/
void initUSART() {
    UCSR0C = (1 << UCSZ00) | (1 << UCSZ01); //asynchronous, 8 bit data
    UBRR0 = 103; //set baud rate = 9600 bps
    UCSR0B = (1 << RXEN0) | (1 << TXEN0); //enable RX and TX
}

/*
    Function name: USART_sendByte
    Input: {
        uint8_t data: the data to be sent
    }
    Output: none
    Logic: Send 1 byte of data to the serial monitor
    Example call: USART_sendByte('A');
*/
void USART_sendByte(uint8_t data) {
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = data;
}

/*
    Function name: USART_sendData
    Input: {
        char* data: the data (string) to be sent
    }
    Output: none
    Logic: Send a string to the serial monitor
    Example call: USART_sendData("Hello world!\n");
*/
void USART_sendData(char* data) {
    while(*data) {
        USART_sendByte(*data);
        data++;
    }
}

/*
    Function name: USART_sendData
    Input: {
        float data: the data to be sent
    }
    Output: none
    Logic: Send a float to the serial monitor
    Example call: USART_sendData(10.0);
*/
void USART_sendData(float data) {
    char temp[10];
    dtostrf(data, 8, 2, temp);
    USART_sendData(temp);
}

/*
    Function name: USART_sendData
    Input: {
        uint16_t data: the data to be sent
    }
    Output: none
    Logic: Send a uint16_t to the serial monitor
    Example call: USART_sendData(10.0);
*/
void USART_sendData(uint16_t data) {
    char temp[10];
    sprintf(temp, "%d", data);
    USART_sendData(temp);
}

/*
    Function name: calculateCapacitance
    Input: none
    Output: float : the capacitance of the capacitor connected
    Logic: calculate the time to reach 0.63 Vcc == 648 ADC0 value and hence capacitance
    Example call: float C = calculateCapacitance();
*/
float calculateCapacitance() {
    //let the capacitor discharge
    PIN_LOW(B, pin_charge); 
    while (readADC(pin_capacitance) != 0);
    
    //calculate the time till capacitor is charged to 0.63Vcc (ADC reads 648)
    startTimer1();
    PIN_HIGH(B, pin_charge);
    while(readADC(pin_capacitance) < 647);

    float tc = getMilliseconds();
    PIN_LOW(B, pin_charge);
    stopTimer1();
    return tc/10.0; //uF
}

/*
    Function name: calculateResistance
    Input: none
    Output: float : the resistance connected
    Logic: calculate the time to reach 0.63 Vcc == 648 ADC0 value and hence the resistance
    Example call: float R = calculateResistance();
*/
float calculateResistance() {
    //let the capacitor discharge
    PIN_LOW(B, pin_charge); 
    while (readADC(pin_resistance) != 0);
    
    //calculate the time till capacitor is charged to 0.63Vcc (ADC reads 648)
    startTimer1();
    PIN_HIGH(B, pin_charge);
    while(1023 - readADC(pin_resistance) < 647);

    float tc = getMilliseconds();
    PIN_LOW(B, pin_charge);
    stopTimer1();
    return tc/100.0f; //kOhms
}

/*
    Function name: checkMode
    Input: none
    Output: none
    Logic: read input from the serial monitor and set the mode
    Example call: checkMode();
*/
void checkMode() {
    USART_sendData("SHYAM\'S MULTIMETER\n");
    USART_sendData("KEY\t\tFUNCTION\n");
    USART_sendData("1\t\tVOLTMETER\n");
    USART_sendData("2\t\tRESISTANCE METER\n");
    USART_sendData("3\t\tCAPACITANCE METER\n");
    USART_sendData("4\t\tAMMETER\n");
    while(!(UCSR0A & (1 << RXC0)));
    mode = UDR0;
    mode -= '0';
}

/*
    Function name: updateLED
    Input: none
    Output: none
    Logic: burn one of the four LEDs based on the input
    Example call: checkMode();
*/
void updateLED() {
    PIN_LOW(D, pin_led[0]);
    PIN_LOW(D, pin_led[1]);
    PIN_LOW(D, pin_led[2]);
    PIN_LOW(D, pin_led[3]);
    if(mode >= 1 && mode <= 4)
        PIN_HIGH(D, pin_led[mode-1]);
}

/*
    Function name: ISR (TIMER1_OVF_vect)
    Input: none
    Output: none
    Logic: Update timer1_overflow whenever TCNT1 overflows
    Example call: <implicitly called>
*/
ISR (TIMER1_OVF_vect) {
    //250 ticks = 1 millisecond
    timer1_overflow += (float)(0xFFFF)/250.0f;
}

/*
    Function name: main
    Input: none
    Output: int : success/error code
    Example call: <called implicitly>
 */
int main() {
    initPins();
    sei(); //enable global interrupts
    initUSART();
    initADC();


    checkMode();
    updateLED();
    USART_sendData("MODE: ");
    
    float value = 0;

    switch(mode) {
        case MODE_CAPACITANCE:
            USART_sendData("Capacitance meter\n");
            USART_sendData("Calculating time constant...\n");
            value = calculateCapacitance();
            USART_sendData("The capacitance is: ");
            USART_sendData(value); 
            USART_sendData(" uF\n");
            USART_sendByte('\n');
        break;

        case MODE_RESISTANCE:
            USART_sendData("Resistance meter\n");
            USART_sendData("Calculating time constant...\n");
            value = calculateResistance();
            USART_sendData("The resistance is: ");
            USART_sendData(value); 
            USART_sendData(" k ohm\n");
            USART_sendByte('\n');
        break;

        case MODE_VOLTAGE:
            USART_sendData("Volt meter\n");
            value = readADC(pin_voltage)*0.00488f;
            USART_sendData("The Voltage is: ");
            USART_sendData(value); 
            USART_sendData(" V\n");
            USART_sendByte('\n');
        break;

        case MODE_CURRENT:
            USART_sendData("Ammeter\n");
            value = readADC(pin_current)*0.488f;
            USART_sendData("The current is: ");
            USART_sendData(value); 
            USART_sendData(" mA\n");
            USART_sendByte('\n');
        break;

        default:
            USART_sendData("Invalid\n");
    }

    USART_sendData("\nRESET to reuse");

    return 0;
}