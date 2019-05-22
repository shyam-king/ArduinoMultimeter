/*
    AUTHOR: Shyam
    Domain: Embedded and Electronics
    Functions: initPins, initADC, initTimers, readADC, main
    Global variables: pin_charge, pin_capacitor, time_elapsed
*/

#include<avr/interrupt.h>
#include<avr/io.h>

#define PIN_LOW(port, pin) PORT##port &= !(1 << pin)
#define PIN_HIGH(port, pin) PORT##port |= (1 << pin)


const int pin_capacitor = 0; // A0
const int pin_charge = 2; // PORTD 
float timer1_overflow; //to handle overflow of TCNT1
float time_constant = 0; //callibrated timeConstant

/*
    Function name: initPins
    Input: none
    Output: none
    Logic: Initialize all pins using the DDRX and PORTX registers
    Example call: initPins();
*/
void initPins() {
    DDRD |= (1 << pin_charge);
    DDRC &= !(1 << pin_capacitor);
    PIN_LOW(D, pin_charge);
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
    Function name: initTimers
    Input: none
    Output: none
    Logic: Initialize timers used: timer1 (16-bit)
    Example call: initTimers();
*/
void initTimers() {
    TCCR1A = 0; //default configuration
    TIMSK1 |= 1; //overflow interrupt enable
}

/*
    Function name: startTimer1
    Input: none
    Output: none
    Logic: Start timer1 with 64 prescalar
    Example call: startTimer1();
*/
void startTimer1() {
    TCCR1B = (1 << CS11) | (1 << CS10); //64 prescalar
    timer1_overflow = 0;
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
    return timer1_overflow + (float)TCNT1/250;
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
    Function name: ISR (TIMER1_OVF_vect)
    Input: none
    Output: none
    Logic: Update timer1_overflow whenever TCNT1 overflows
    Example call: <implicitly called>
*/
ISR (TIMER1_OVF_vect) {
    //250 ticks = 1 millisecond
    timer1_overflow += (float)TCNT1/250;
}

/*
    Function name: getTimeConstant
    Input: none
    Output: float : the time constant
    Logic: calculate the time to reach 0.63 Vcc == 645 ADC0 value
    Example call: float tc = getTimeConstant();
*/
float getTimeConstant() {
   // USART_sendData("Starting callibration\n");

    //let the capacitor discharge
    PIN_LOW(D, pin_charge); 
    while (readADC(0) != 0);

    //calculate the time till capacitor is charged to 0.63Vcc (ADC reads 645)
    startTimer1();
    PIN_HIGH(D, pin_charge);
    while (readADC(0) < 645);
    float tc = getMilliseconds();
    stopTimer1();
    PIN_LOW(D, pin_charge);

    USART_sendData("Callibration complete\n");
    return tc;
}

int main() {
    initPins();
    sei(); //enable global interrupts
    initADC();
    initTimers();
    initUSART();

    time_constant = getTimeConstant();

    while (1) {
        USART_sendData("Time constant: ");
        USART_sendData(time_constant);
        USART_sendByte('\n');
        /*
        uint16_t value = readADC(0);
        USART_sendData(value);
        USART_sendByte('\n');*/
    }
}
