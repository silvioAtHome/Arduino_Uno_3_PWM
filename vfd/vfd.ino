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

  PIN A1 is Analogread(23) Poti Frequency
  PIN A2 is Analogread(24) Poti Amplitude
  PIN A3 is Analogread(25) Poti Phasediff

  PWM Timer0 8 bit  (Arduino-Uno Pin 5,6, PD5/PD6) UNO 5,6
  ISR Tic-Timer1 16bit (Arduino-Pin 9,10 .. PB1,PB2) pins are not used
  PWM Timer2 8bit, (analogWrite) , (Arduino-Pin 3,11 .. PB3/PD3) PD3-> UNO 3

*/

#include <SPI.h>

#define LEDGREEN 2
#define PPP 192
#define ISRTIMERFREQUENCY 1000000.0f
// 1 = 2-phase, 0 = 3 phase with 120 degrees
#define TWOPHASEGENERATION 0

// manuell rechnen ISRTIMDIV = ISRTIMERFREQUENCY / PPP;
// #define ISRTIMDIV 31250.0f
#define ISRTIMDIV 5208.3333

#define MINSPEED 1.0f
#define MAXSPEED 100.0f

#define POTIHYSTERESIS 20

// manuell rechnen. Beschleunigungsfaktor
/*p0=acc*2.0/((float) PPP);
  p0*=0.98; // Korrektur, woher auch immer!?
  ppp:        24       32
  acc:1   0.081667 0.061250
  acc:5   0.408333 0.306250
  acc:50  4.083333 3.062500
  acc:100 8.166667 6.125000
*/
#define P0 1.0416667

uint8_t LCD[4];
uint8_t tic_cnt;
uint16_t analog0raw,analog1raw, analog0 = 0, analog1 = 0,analog2 = 0, analog1scaled = 0;

volatile float speed = MINSPEED;
volatile uint16_t isr_cnt = 0, timer_after_isr;;
float speed_command = 0.0f;
const float LUT[PPP] = {
  0.0000f, 0.0327f, 0.0654f, 0.0980f, 0.1305f,
  0.1629f, 0.1951f, 0.2271f, 0.2588f, 0.2903f,
  0.3214f, 0.3523f, 0.3827f, 0.4127f, 0.4423f,
  0.4714f, 0.5000f, 0.5281f, 0.5556f, 0.5825f,
  0.6088f, 0.6344f, 0.6593f, 0.6836f, 0.7071f,
  0.7299f, 0.7518f, 0.7730f, 0.7934f, 0.8128f,
  0.8315f, 0.8492f, 0.8660f, 0.8819f, 0.8969f,
  0.9109f, 0.9239f, 0.9359f, 0.9469f, 0.9569f,
  0.9659f, 0.9739f, 0.9808f, 0.9866f, 0.9914f,
  0.9952f, 0.9979f, 0.9995f, 1.0000f, 0.9995f,
  0.9979f, 0.9952f, 0.9914f, 0.9866f, 0.9808f,
  0.9739f, 0.9659f, 0.9569f, 0.9469f, 0.9359f,
  0.9239f, 0.9109f, 0.8969f, 0.8819f, 0.8660f,
  0.8492f, 0.8315f, 0.8128f, 0.7934f, 0.7730f,
  0.7518f, 0.7299f, 0.7071f, 0.6836f, 0.6593f,
  0.6344f, 0.6088f, 0.5825f, 0.5556f, 0.5281f,
  0.5000f, 0.4714f, 0.4423f, 0.4127f, 0.3827f,
  0.3523f, 0.3214f, 0.2903f, 0.2588f, 0.2271f,
  0.1951f, 0.1629f, 0.1305f, 0.0980f, 0.0654f,
  0.0327f, 0.0000f, -0.0327f, -0.0654f, -0.0980f,
  -0.1305f, -0.1629f, -0.1951f, -0.2271f, -0.2588f,
  -0.2903f, -0.3214f, -0.3523f, -0.3827f, -0.4127f,
  -0.4423f, -0.4714f, -0.5000f, -0.5281f, -0.5556f,
  -0.5825f, -0.6088f, -0.6344f, -0.6593f, -0.6836f,
  -0.7071f, -0.7299f, -0.7518f, -0.7730f, -0.7934f,
  -0.8128f, -0.8315f, -0.8492f, -0.8660f, -0.8819f,
  -0.8969f, -0.9109f, -0.9239f, -0.9359f, -0.9469f,
  -0.9569f, -0.9659f, -0.9739f, -0.9808f, -0.9866f,
  -0.9914f, -0.9952f, -0.9979f, -0.9995f, -1.0000f,
  -0.9995f, -0.9979f, -0.9952f, -0.9914f, -0.9866f,
  -0.9808f, -0.9739f, -0.9659f, -0.9569f, -0.9469f,
  -0.9359f, -0.9239f, -0.9109f, -0.8969f, -0.8819f,
  -0.8660f, -0.8492f, -0.8315f, -0.8128f, -0.7934f,
  -0.7730f, -0.7518f, -0.7299f, -0.7071f, -0.6836f,
  -0.6593f, -0.6344f, -0.6088f, -0.5825f, -0.5556f,
  -0.5281f, -0.5000f, -0.4714f, -0.4423f, -0.4127f,
  -0.3827f, -0.3523f, -0.3214f, -0.2903f, -0.2588f,
  -0.2271f, -0.1951f, -0.1629f, -0.1305f, -0.0980f,
  -0.0654f, -0.0327f
};

void setup() {

  /* set system clock to 8 MHz to archive 15.6 kHz PWm at the end */
  CLKPR = (1 << CLKPCE);
  CLKPR = (1 << CLKPS0);
  pinMode(8, INPUT_PULLUP); /* Umschalter 7-Segement */
  pinMode(LEDGREEN, OUTPUT); /* green LED, debug or */
  pinMode(4, INPUT_PULLUP); /* close button */

  /* SPI for 7-Seg */
  digitalWrite(SS, HIGH);
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV128);
  SPI.setDataMode(SPI_MODE3);

  /* http://www.openmusiclabs.com/learning/digital/synchronizing-timers/index.html
    timer synchronization */
  /* init Timer */
  noInterrupts(); /* disable all interrupts */
  /* Timer 1 for ISR */
  /* stop all timer */
  GTCCR = (1 << TSM) | (1 << PSRASY) | (1 << PSRSYNC); // halt all timers
  /* set to: Clear Timer on Compare Match (CTC) Mode */
  TCCR1A = (1 << COM1A0);   /* toggle OC1A, but it is just enable */
  TCCR1B =  (1 << WGM12);   /* enable CTC mode */
  OCR1A = 65535;            /* initinal step until the ISR writes a new value */
  TCCR1B |=  (1 << CS11);   /* prescaler = 8 -> 1MHz Zählgeschwindigkeit bei clock = 8MHz */
  TIMSK1 |= (1 << OCIE1A);  /* enable timer interrupt on output compare A */
  TCNT1 = 0;

  /* PWM Timer0 */
  /*OC0A ist Pin PD6, OC0B ist Pin PD5 */
  DDRD |= (1 << PD5) | (1 << PD6); /* enable outputs */
  TCCR0A = (1 << WGM00) | (1 << COM0B1) | (1 << COM0A1); /* phase corrected mode */
  TCNT0 = 0;
  TCCR0B = (1 << CS00);  /* prescaler = 1 */
  /* PWM Timer2 */
  /* OC2B ist Pin PD3 */
  DDRD |= (1 << PD3); /* enable output */
  TCCR2A = (1 << WGM20) | (1 << COM2B1);
  TCNT2 = 0;
  /* enable clock for pwm, no prescaler f_pwm = 8e6/510 = 15686.275 Hz, DS p 124 */
  TCCR2B = (1 << CS20);  /* prescaler = 1 */
  interrupts();  // enable all interrupts
  /* start all timer synchronuously */
  GTCCR = 0; // release all timers
}

/* timer 1 ISR for setting the PWM channels */
ISR(TIMER1_COMPA_vect)
{
  uint8_t pwm0, pwm1, pwm2;
  #if TWOPHASEGENERATION
  static uint8_t ti0 = 0, ti1 = PPP / 4, ti2 = PPP / 2;
  #else
  static uint8_t ti0 = 0, ti1 = PPP / 3 , ti2 = PPP / 3 * 2;
  #endif
  static uint8_t ppp_frac = 1; // Schnellste Drehzahl = 4, geringste Drehzahl = 1
  // set the new output compare value of current speed. page 101
  isr_cnt = ((int) (((float)ppp_frac) * ISRTIMDIV / speed)) - 1 ;
  OCR1A = isr_cnt;

  //toggle green LED
  //digitalWrite(LEDGREEN, digitalRead(LEDGREEN) ^ 1);
  digitalWrite(LEDGREEN, 1);

  pwm0 = 128 + (int8_t) ((float) analog1scaled * LUT[ti0]);
  pwm1 = 128 + (int8_t) ((float) analog1scaled * LUT[ti1]);
  pwm2 = 128 + (int8_t) ((float) analog1scaled * LUT[ti2]);


  OCR2B = pwm0; /* pin PD3 */ // Haupt Phase
  OCR0B = pwm1; /* pin PD5 */ // Mittelpunkt
  OCR0A = pwm2; /* pin PD6 */ // Hilfswicklung

  ti0 += ppp_frac;
  
  #if TWOPHASEGENERATION
  ti1 = ti0 - analog2;
  if(ti1 > 191) ti1 += 192; // check if underflow 
  #else
  ti1 += ppp_frac;
  #endif
  
  ti2 += ppp_frac;

  // w= analog / 192 * 360
  // Haupt-Mittel = sqrt(2-2*cos(w/180*%pi))
  // Hilfs-Mittel = sqrt(2+2*cos(w/180*%pi))

  ti0 %= PPP;
  ti1 %= PPP;
  ti2 %= PPP;

  if (speed_command > speed)
  {
    speed += ((float) ppp_frac) * P0 / (2.0 * speed);
    if (speed > speed_command)
    {
      speed = speed_command;
    }
  }
  if (speed_command < speed)
  {
    speed -= ((float) ppp_frac) * P0 / (2.0 * speed);
    if (speed < speed_command)
    {
      speed = speed_command;
    }
  }
  if (speed < MINSPEED) speed = MINSPEED;

  if (ppp_frac <= 8 && isr_cnt < 400)
  {
    ppp_frac *= 2;
  }


  if (ppp_frac > 1 && isr_cnt > 1000)
  {
    ppp_frac /= 2;
  }

  timer_after_isr = TCNT1;

  digitalWrite(LEDGREEN, 0);
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
  /*
    7-Segment display bit assignment
      0
    5   1
      6
    4   2
      3   7
  */
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
  analog1raw = analogRead(24);
  analog2 = analogRead(25)/21;  // 0..48
  /* speed 1..125 Hz mit ein bisschen Hysterese */
  if (analog0raw > analog0 + POTIHYSTERESIS)
  {
    analog0 = analog0raw - POTIHYSTERESIS;
  }
  if (analog0raw < analog0)
  {
    analog0 = analog0raw;
  }
  // Amplitude poti
  if (analog1raw > analog1 + POTIHYSTERESIS)
  {
    analog1 = analog1raw - POTIHYSTERESIS;
  }
  if (analog1raw < analog1)
  {
    analog1 = analog1raw;
  }
  analog1scaled= analog1/9;
  speed_command = MINSPEED + (MAXSPEED - MINSPEED) * ((float) analog0) / (1023.0f - (float) POTIHYSTERESIS);

  switch ((int) readSwitch())
  {
    case 0:
      if (readButtonClose())
      {
        Tm74HC595((uint16_t) (ISRTIMERFREQUENCY / (float) (isr_cnt + 1)), LCD);
      }
      else
      {
        //Tm74HC595(isr_cnt, LCD);
        Tm74HC595((uint16_t) analog2*360/192, LCD);
      }
      break;
    case 1:
      if (readButtonClose())
      {
        if (tic_cnt++ == 0)
        {
          Tm74HC595(timer_after_isr, LCD);
        }
      }
      else
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
