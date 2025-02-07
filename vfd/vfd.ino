
/*
  13 / PB7 ist LED "L" vergeben durch SPI
  8 button kippschalter für Anzeigen-Umschaltung 7Seg

  2 green LED
  3,5,6 red LEDs

  SPI auf Arduino Uno
  SCK 13
  MISO 12
  MOSI 11
  CS 10

  A1 ist Analogread(23) Poti

  Timer0 8 bit  (Arduino-Pin ??, PD5/PD6) UNO 5,6 .. PD4 ist Timer0 Input (UNO4)
  Tic-Timer1 16bit (Arduino-Pin 9,10 .. PB1,PB2)
  PWM Timer2 8bit, (analogWrite) , (Arduino-Pin 3,11 .. PB3/PD3) PD3-> UNO 3

*/

#include <SPI.h>

#define LEDGREEN 2
#define PPP 32
#define ISRTIMERFREQUENCY 2000000.0f
// manuell rechnen ISRTIMERFREQUENCY / PPP;
#define ISRTIMDIV 62500.0f
// manuell rechnen. Beschleunigungsfaktor
//p0=acc*2.0/((float) PPP);
//p0*=0.98; // Korrektur, woher auch immer!?
// für 1 Hz/s
//#define P0 0.06125f
// für 5 Hz/s
#define P0 0.30625f
// für 50 Hz/s
#define P0 3.0625f

// 0.025s
/*(2^16 -59285)/(8e6/32)
  ans  =

    0.025004
*/
#define COUNTERPRESET 59285

uint8_t LCD[4];
uint16_t analog0raw;
volatile bool tic = false;
volatile float speed = 0.0f;
volatile uint16_t isr_cnt = 0;
float speed_command = 0.0f;
const float LUT[PPP] = {
  0.0000f, 0.1951f, 0.3827f, 0.5556f, 0.7071f,
  0.8315f, 0.9239f, 0.9808f, 1.0000f, 0.9808f,
  0.9239f, 0.8315f, 0.7071f, 0.5556f, 0.3827f,
  0.1951f, 0.0000f, -0.1951f, -0.3827f, -0.5556f,
  -0.7071f, -0.8315f, -0.9239f, -0.9808f, -1.0000f,
  -0.9808f, -0.9239f, -0.8315f, -0.7071f, -0.5556f,
  -0.3827f, -0.1951f
};

void setup() {
  pinMode(8, INPUT_PULLUP); // Umschalter 7-Segement
  pinMode(LEDGREEN, OUTPUT);

  digitalWrite(SS, HIGH);
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV128);
  SPI.setDataMode(SPI_MODE3);
  // PWM

  noInterrupts();                       // disable all interrupts
  TCCR1A = 0;
  TCCR1B = 0;
  //65535-16e6*0.025/64
  TCNT1 = COUNTERPRESET;                        // preload timer
  TCCR1B |=  (1 << CS11);  //  prescaler = 8 -> 2MHz Zählgeschwindigkeit
  TIMSK1 |= (1 << TOIE1);               // enable timer overflow interrupt ISR
  

  //PWM Timer2, prescaler 8 -> 3.9 kHz, DS p 124
  //OC0A ist Pin PD6, OC0B ist Pin PD5
  DDRD |= (1 << PD5) | (1 << PD6);
  TCCR0A = (1 << WGM00) | (1 << COM0B1) | (1 << COM0A1); // phase corrected mode
  TCCR0B |= (1 << CS01);  // prescaler = 8
  
  //OC2B ist Pin PD3
  DDRD |= (1 << PD3);
  TCCR2A = (1 << WGM20) | (1 << COM2B1);
  TCCR2B |= (1 << CS21);  // prescaler = 8
  interrupts();                         // enable all interrupts
}

/* timer 1 ISR for setting the PWM channels */
ISR(TIMER1_OVF_vect)
{
  static uint8_t ti0 = 0, ti1 = PPP / 8, ti2 = PPP / 4;
  isr_cnt = ((int) (ISRTIMDIV / speed));
  TCNT1 = 65535 - isr_cnt;
  tic = true;

  //toggle green LED
  //digitalWrite(LEDGREEN, digitalRead(LEDGREEN) ^ 1);
  digitalWrite(LEDGREEN, 1);

  // pin PD3
  OCR2B=(uint16_t) (30.0 * (1.0f + LUT[ti2++]));
  
  // pin PD5
  OCR0B=(uint16_t) (30.0 * (1.0f + LUT[ti1++]));
  
  // pin PD6
  OCR0A=(uint16_t) (30.0 * (1.0f + LUT[ti0++]));
    
  if (speed_command > speed)
  {
    speed += P0 / (2.0 * speed);
  }
  if (speed_command < speed - 0.1)
  {
    speed -= P0 / (2.0 * speed);
  }
  if (speed < 1.0) speed = 1.0;
  ti0 %= PPP;
  ti1 %= PPP;
  ti2 %= PPP;
  digitalWrite(LEDGREEN, 0);
}

bool readSwitch(void)
{
  return !digitalRead(8);
}

void Tm74HC595(int16_t number_to_print, uint8_t *copy_to)
{
  uint8_t n[4];
  uint16_t h;
  bool negative = false;
  if (number_to_print < 0)
  {
    negative = true;
  }

  number_to_print = abs(number_to_print);

  if (number_to_print > 9999)
  {
    number_to_print = 9999;
  }

  //  0
  //5   1
  //  6
  //4   2
  //  3   7
  const uint16_t digitH[13] =      //seven segment digits in bits
  {
    0b11000000, // 0
    0B11111001, // 1
    0B10100100, // 2
    0B10110000, // 3
    0B10011001, // 4
    0B10010010, // 5
    0B10000010, // 6
    0B11111000, // 7
    0B10000000, // 8
    0B10011000, // 9
    0b11111111, // blank
    0b01111111, //Dot
    0b10111111 //minus
  };

  h = number_to_print;
  n[3] = number_to_print / 1e3;
  h -= n[3] * 1000;
  n[2] = h / 1e2;
  h -= n[2] * 100;
  n[1] = h / 1e1;
  h -= n[1] * 10;
  n[0] = h;

  if (n[3] == 0)
  {
    n[3] = 10;
  }
  if (n[2] == 0 && n[3] == 10)
  {
    n[2] = 10;
  }

  if (n[2] == 10 && negative)
  {
    n[2] = 12;
    negative = false;
  }

  if (n[3] == 10 && negative)
  {
    n[3] = 12;
    negative = false;
  }

  for (h = 0; h < 4; h++)
  {
    if (h == 3)
    {
      //copy_to[h] = digitH[n[3 - h]] & digitH[11];
      copy_to[h] = digitH[n[3 - h]];
    }
    else
    {
      copy_to[h] = digitH[n[3 - h]];
    }
  }
}

void addComma(uint8_t digit, uint8_t *copy_to)
{
  copy_to[3 - digit] = 0b01111111 & copy_to[3 - digit];
}

void loop() {

  if (tic)
  {
    tic = false;
    analog0raw = analogRead(23);
    /* speed 1..70 Hz */
    speed_command = 1.0f + 69.0 * ((float) analog0raw) / 1023.0f;

    switch ((int) readSwitch())
    {
      case 0:
        Tm74HC595(isr_cnt, LCD);
        break;
      case 1:
        Tm74HC595((uint16_t)( 10.0f * speed), LCD);
        addComma(1, LCD);
    }

    digitalWrite(SS, LOW);
    SPI.transfer (LCD[3]);
    SPI.transfer (LCD[2]);
    SPI.transfer (LCD[1]);
    SPI.transfer (LCD[0]);
    digitalWrite(SS, HIGH);

  }
}
