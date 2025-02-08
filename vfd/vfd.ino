
/*
  PIN 13 / PB7 is LED "L" and already taken for SPI
  PIN 8 switch for display select 7Seg

  PIN 2 green LED
  PIN 3,5,6 red LEDs, PWM outputs

  SPI on Arduino Uno
  SCK 13
  MISO 12
  MOSI 11
  CS 10

  PIN A1 is Analogread(23) Poti

  Timer0 8 bit  (Arduino-Uno Pin 5,6, PD5/PD6) UNO 5,6
  Tic-Timer1 16bit (Arduino-Pin 9,10 .. PB1,PB2) pins are not used
  PWM Timer2 8bit, (analogWrite) , (Arduino-Pin 3,11 .. PB3/PD3) PD3-> UNO 3

*/

#include <SPI.h>

#define LEDGREEN 2
#define PPP 24
#define ISRTIMERFREQUENCY 1000000.0f

// manuell rechnen ISRTIMERFREQUENCY / PPP;
// #define ISRTIMDIV 31250.0f
#define ISRTIMDIV 41666.667f

// manuell rechnen. Beschleunigungsfaktor
//p0=acc*2.0/((float) PPP);
//p0*=0.98; // Korrektur, woher auch immer!?

// für acc=1 Hz/s
//#define P0 0.06125f

// für acc=5 Hz/s
//#define P0 0.30625f

// für acc=50 Hz/s
//#define P0 3.0625f

//#define P0 4.1666667f

// für acc=100 Hz/s
#define P0 8.3333333

uint8_t LCD[4];
uint8_t tic_cnt;
uint16_t analog0raw, analog0 = 0;

volatile float speed = 0.0f;
volatile uint16_t isr_cnt = 0, timer_after_isr;;
float speed_command = 0.0f;
const float LUT[PPP] = {
  1.0000f, 1.2588f, 1.5000f, 1.7071f, 1.8660f,
  1.9659f, 2.0000f, 1.9659f, 1.8660f, 1.7071f,
  1.5000f, 1.2588f, 1.0000f, 0.7412f, 0.5000f,
  0.2929f, 0.1340f, 0.0341f, 0.0000f, 0.0341f,
  0.1340f, 0.2929f, 0.5000f, 0.7412f
};

void setup() {

  //set system clock to 8 MHz to archive 15.6 kHz PWm at the end
  CLKPR = (1 << CLKPCE);
  CLKPR = (1 << CLKPS0);
  pinMode(8, INPUT_PULLUP); // Umschalter 7-Segement
  pinMode(LEDGREEN, OUTPUT);
  pinMode(4, INPUT_PULLUP); //close button

  // SPI for 7-Seg
  digitalWrite(SS, HIGH);
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV128);
  SPI.setDataMode(SPI_MODE3);
  // PWM

  noInterrupts();              // disable all interrupts
  //Timer 1 for ISR
  //set to: Clear Timer on Compare Match (CTC) Mode
  TCCR1A = (1 << COM1A0); // toggle OC1A, but it is just enable
  TCCR1B =  (1 << WGM12); // CTC mode
  OCR1A = 65535;         // initinal step until the ISR writes a new value
  TCCR1B |=  (1 << CS11);  //  prescaler = 8 -> 1MHz Zählgeschwindigkeit bei clock = 8MHz
  TIMSK1 |= (1 << OCIE1A);               // enable timer interrupt on output compare A

  //PWM Timer2, prescaler 8 -> 3.9 kHz, DS p 124
  //OC0A ist Pin PD6, OC0B ist Pin PD5
  DDRD |= (1 << PD5) | (1 << PD6);
  TCCR0A = (1 << WGM00) | (1 << COM0B1) | (1 << COM0A1); // phase corrected mode


  //PWM Timer0
  //OC2B ist Pin PD3
  DDRD |= (1 << PD3);
  TCCR2A = (1 << WGM20) | (1 << COM2B1);

  // enable clock for pwm, f_pwm = 8e6/510 = 15686.275 Hz
  TCCR0B = (1 << CS00);  // prescaler = 1
  TCCR2B = (1 << CS20);  // prescaler = 1
  interrupts();                         // enable all interrupts
}

/* timer 1 ISR for setting the PWM channels */
ISR(TIMER1_COMPA_vect)
{
  uint8_t pwm0, pwm1, pwm2;
  static uint8_t ti0 = 0, ti1 = PPP / 8, ti2 = PPP / 4;
  //static uint8_t ti0 = 0, ti1 = PPP / 3 , ti2 = 2* PPP / 3;
  // set the new output compare value of current speed
  isr_cnt = ((int) (ISRTIMDIV / speed));
  OCR1A = isr_cnt; // vielleicht noch +1  hier.

  //toggle green LED
  digitalWrite(LEDGREEN, digitalRead(LEDGREEN) ^ 1);
  //digitalWrite(LEDGREEN, 1);

  pwm0 = 0 + (uint16_t) (speed * LUT[ti2++]);
  pwm1 = 0 + (uint16_t) (speed * LUT[ti1++]);
  pwm2 = 0 + (uint16_t) (speed * LUT[ti0++]);

  OCR2B = pwm0; // pin PD3
  OCR0B = pwm1; // pin PD5
  OCR0A = pwm2; // pin PD6

  ti0 %= PPP;
  ti1 %= PPP;
  ti2 %= PPP;

  if (speed_command > speed)
  {
    speed += P0 / (2.0 * speed);
    if (speed > speed_command)
    {
      speed = speed_command;
    }
  }
  if (speed_command < speed)
  {
    speed -= P0 / (2.0 * speed);
    if (speed < speed_command)
    {
      speed = speed_command;
    }
  }
  if (speed < 1.0) speed = 1.0;

  timer_after_isr = TCNT1;

  //digitalWrite(LEDGREEN, 0);
}

bool readSwitch(void)
{
  return !digitalRead(8);
}

bool readButtonClose(void)
{
  return !digitalRead(4);
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
  analog0raw = analogRead(23);
  /* speed 1..100 Hz mit ein bisschen Hysterese */
  if (analog0raw > analog0 + 4)
  {
    analog0 = analog0raw - 4;
  }
  if (analog0raw < analog0)
  {
    analog0 = analog0raw;
  }
  speed_command = 1.0f + 120.0 * ((float) analog0) / 1019.0f;

  switch ((int) readSwitch())
  {
    case 0:
      Tm74HC595(isr_cnt, LCD);
      break;
    case 1:
      if (readButtonClose())
      {
        if (tic_cnt++ == 0)
        {
          Tm74HC595(timer_after_isr, LCD);
        }
      } else
      {
        Tm74HC595((uint16_t)( 10.0f * speed), LCD);
        addComma(1, LCD);
      }
  }

  digitalWrite(SS, LOW);
  SPI.transfer (LCD[3]);
  SPI.transfer (LCD[2]);
  SPI.transfer (LCD[1]);
  SPI.transfer (LCD[0]);
  digitalWrite(SS, HIGH);
}
