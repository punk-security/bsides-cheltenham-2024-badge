#include <avr/io.h>-
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <EEPROM.h>
#include <tinyNeoPixel_Static.h>


// PINS
#define MORSE_CODE_PIN PIN_PA7

// COLOURS
#define OFF 0,0,0
#define RED 50,0,0
#define ORANGE 40,10,0
#define YELLOW 30,20,0
#define GREEN 0,50,0
#define BLUE 0,0,50
#define PURPLE 10,0,20
#define PINK 20,0,10



// MORSE CODE
/*
 *
 * MORSE CODE is between 1 and 5 pulses long
 *
 * To store this in one byte we have the length, (3 bits) and each position as either 0 (dot) or 1 (dash)
 * i.e.
 * A = 010 01 000
 * B = 100 1000 0
 * C = 100 1010 0
 */
// A .-
#define m_A B01001000
// B -...
#define m_B B10010000
// C -.-.
#define m_C B10010100
// D -..
#define m_D B01110000
// E .
#define m_E B00100000
// F ..-.
#define m_F B10000100
// G --.
#define m_G B01111000
// H ....
#define m_H B10000000
// I ..
#define m_I B01000000
// J .---
#define m_J B10001110
// K -.-
#define m_K B01110100
// L .-..
#define m_L B10001000
// M --
#define m_M B01011000
// N -.
#define m_N B01010000
// O ---
#define m_O B01111100
// P .--.
#define m_P B10001100
// Q --.-
#define m_Q B10011010
// R .-.
#define m_R B01101000
// S ...
#define m_S B01100000
// T -
#define m_T B00110000
// U ..-
#define m_U B01100100
// V ...-
#define m_V B10000010
// W .--
#define m_W B01101100
// X -..-
#define m_X B10010010
// Y -.--
#define m_Y B10010110
// Z --..
#define m_Z B10011000
// 1 .----
#define m_1 B10101111
// 2 ..---
#define m_2 B10100111
// 3 ...--
#define m_3 B10100011
// 4 ....-
#define m_4 B10100001
// 5 .....
#define m_5 B10100000
// 6 -....
#define m_6 B10110000
// 7 --...
#define m_7 B10111000
// 8 ---..
#define m_8 B10111100
// 9 ----.
#define m_9 B10111110
// 0 -----
#define m_0 B10111111


// MORSE Code timing intervals

#define MORSE_CODE_MAX_DOT_MS 400 // LOW - Any longer than this and its a dash
#define MORCE_CODE_MAX_DASH_MS 2000 // LOW - Any longer than this and its an error
#define MORSE_CODE_MAX_CHAR_INTERVAL_MS 800 // LOW - Any longer than this and we end the character
#define MORSE_CODE_MAX_WORD_INTERVAL_MS MORSE_CODE_MAX_CHAR_INTERVAL_MS * 2 // LOW - Any longer than this and we end the secret code
#define MORSE_CODE_MAX_HIGH_MS MORCE_CODE_MAX_DASH_MS

#define MORSE_CODE_CHAR_ERROR 0
#define MAX_SECRET_CODE_LENGTH 4
#define NUMLEDS 5
#define WAKE_TIME_MS 300000

uint16_t time_pin_low(uint16_t max_ms)
{
  // blocking for up to max_ms
  if (digitalRead(MORSE_CODE_PIN) == HIGH)
  {
    return(0);
  }
  delay(50); //debounce-
  uint16_t t = 50;
  while(digitalRead(MORSE_CODE_PIN) == LOW)
  {
    delay(5);
    t = t + 5;
    if ( t > max_ms )
      return(max_ms);
  }
  return(t);
}

uint16_t time_pin_high(uint16_t max_ms)
{
  // blocking for up to max_ms
  if (digitalRead(MORSE_CODE_PIN) == LOW)
  {
    return(0);
  }
  delay(50); //debounce-
  uint16_t t = 50;
  while(digitalRead(MORSE_CODE_PIN) == HIGH)
  {
    delay(5);
    t = t + 5;
    if ( t > max_ms )
      return(max_ms);
  }
  return(t);
}

byte read_morse_char()
{
  // this function blocks
  byte code = B0000000;
  uint16_t lastPulseLength;
  uint8_t pos = 0;
  while(pos < 5)
  {
    lastPulseLength = time_pin_low(MORCE_CODE_MAX_DASH_MS);
    if (lastPulseLength == 0 || lastPulseLength == MORCE_CODE_MAX_DASH_MS) {return MORSE_CODE_CHAR_ERROR;} // return null byte on error
    if (lastPulseLength < MORSE_CODE_MAX_DOT_MS)
    {
      // set dot as the LOW was less than a dash
      // dots are zero so nothing to set but bump the pos
      pos++;
    }
    else
    {
      // set dash
      bitSet(code, (4 - pos)); // bits in byte are 76543210, we start at 4 and move right
      pos++;
    }
    // At this point we have read the low dash or dot but we dont know if its the end of the char.
    lastPulseLength = time_pin_high(MORSE_CODE_MAX_CHAR_INTERVAL_MS);
    if (lastPulseLength == MORSE_CODE_MAX_CHAR_INTERVAL_MS)
    {
      // The low time was higher than the intra-character spacing max so must end this character
     code |= pos << 5; // set the pos bits
     return(code); 
    }
  }
    // We have already got all 5 dots/dashes, cant RX any more
    return MORSE_CODE_CHAR_ERROR; //return Error
}

void read_morse_word(char code[])
{
  uint8_t pos = 0;
  pos = 0;
  byte lastChar;
  int8_t lastPulseLength;
  while(pos < MAX_SECRET_CODE_LENGTH)
  {
    lastChar = read_morse_char();
    if (lastChar == MORSE_CODE_CHAR_ERROR)
      return "";
    // We should still be HIGH here - we left the function because the wait was longer than the intra char interval
    code[pos] = lastChar;
    pos++;
    // Should we end the string early or capture the next character?
    if (time_pin_high(MORSE_CODE_MAX_WORD_INTERVAL_MS) == MORSE_CODE_MAX_WORD_INTERVAL_MS)
    {
      // The char interval has already passed inside the read_morse_char func.
      // If we go that length AGAIN twice, the code is over
      // i.e.  CHAR INT = 1s, WORD INT = 2s, 1s has elapsed so wait up to 1s for the next LOW to start
      return;
    }
  }
  // At MAX code length, so return
  return;
}

byte pixels[NUMLEDS * 3];
tinyNeoPixel strip = tinyNeoPixel(NUMLEDS, PIN_PA3, NEO_GRB, pixels);

void setAllPixels(int r, int g, int b, bool show = false)
{
  for (int i = 0; i < NUMLEDS; i++) 
  {
    strip.setPixelColor(i,r,g,b);
  }
  if(show)
    strip.show();
}

int police(int x)
{
  if( x % 2 == 0)
  {
    strip.setPixelColor(0,RED);
    strip.setPixelColor(1,BLUE);
    strip.setPixelColor(2,RED);
    strip.setPixelColor(3,BLUE);
    strip.setPixelColor(4,RED);
  }
  else
  {
    strip.setPixelColor(0,BLUE);
    strip.setPixelColor(1,RED);
    strip.setPixelColor(2,BLUE);
    strip.setPixelColor(3,RED);
    strip.setPixelColor(4,BLUE);
  }
  strip.show();
  return 200;
}

int fill(int x, int r, int g, int b)
{
  int step = x % 10;
  switch(step)
  {
    case 0:
      setAllPixels(OFF);
      break;
    case 1 ... 6:
      strip.setPixelColor(step - 1, r,g,b);
      break;
    default:
      break;
  }
  strip.show();
  return 100;
}

void sleep()
{
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  PORTA.PIN7CTRL = PORT_PULLUPEN_bm | PORT_ISC_LEVEL_gc; // enable pullup and interrupt
  digitalWrite(PIN_PA1, LOW); // turn off LED power rail
  sleep_enable();
  sleep_cpu();
  // sleep resumes here
  PORTA.PIN7CTRL = PORT_PULLUPEN_bm; // renable pullup but no interrupt
  digitalWrite(PIN_PA1, HIGH);  // turn on LED power rail
}

ISR(PORTA_PORT_vect) {
  PORTA.INTFLAGS = PORT_INT7_bm; // Clear Pin 7 interrupt flag otherwise keep coming back here
}

uint8_t state;

void setup()
{
  // Power save
  // http://www.technoblogy.com/show?2RA3
  // https://github.com/SpenceKonde/megaTinyCore/blob/master/megaavr/extras/PowerSave.md
  ADC0.CTRLA &= ~ADC_ENABLE_bm; // Disable ADC
  pinMode(PIN_PA1, OUTPUT);
  pinMode(PIN_PA2, OUTPUT);
  pinMode(PIN_PA3, OUTPUT);
  pinMode(PIN_PA6, OUTPUT);
  // UPDI does not need setting (PA0)

  // Pin setup
  pinMode(PIN_PA7, INPUT_PULLUP);
  digitalWrite(PIN_PA1, HIGH);

  // Read eeprom to state
  state = EEPROM.read(0);
  if (state == B11111111)
  {
    EEPROM.update(0, 0);
    state = EEPROM.read(0);
  }
  // state = B11111111; // Comment out before flash
  
}

bool c_array(char array_1[],char array_2[])
{
  for (int i = 0;  i < MAX_SECRET_CODE_LENGTH; i++)
  {
  if( array_1[i] != array_2[i] ) 
    return false;
  }
  return true;
}

void success()
{
    EEPROM.update(0, state);
    setAllPixels(0,5,0,true);
    delay(500);
}

void fail()
{
    setAllPixels(5,0,0,true);
    delay(500);
}

void c_morse()
{
  // SECRET CODES
  char secret1[MAX_SECRET_CODE_LENGTH] = {m_R, m_G, m_B, 0}; //STROBE
  char secret2[MAX_SECRET_CODE_LENGTH] = {m_S, m_O, m_S, 0}; //POLICE
  char secret3[MAX_SECRET_CODE_LENGTH] = {m_1, m_J, m_U, m_N};  //VEGAS
  char secret4[MAX_SECRET_CODE_LENGTH] = {m_C, m_I, m_S, m_O}; //RED
  char secret5[MAX_SECRET_CODE_LENGTH] = {m_U, m_F, m_O, 0};  // BLUE
  char secret6[MAX_SECRET_CODE_LENGTH] = {m_S, m_E, m_C, 0}; // YELLOW
  char secret7[MAX_SECRET_CODE_LENGTH] = {m_P, m_U, m_N, m_K}; // PUNK
  char secret_reset[MAX_SECRET_CODE_LENGTH] = {m_R, m_S, m_T, 0};
  char secret_all[MAX_SECRET_CODE_LENGTH] = {m_1, m_3, m_3, m_7};
  // BUFFER TO READ TO
  char i[MAX_SECRET_CODE_LENGTH] = {0};
  // SET PIXELS TO SHOW WE ARE READIING
  setAllPixels(0,0,5,true);
  // READ INPUT
  while(digitalRead(MORSE_CODE_PIN) == LOW)
    delay(10);
  while(digitalRead(MORSE_CODE_PIN) == HIGH)
    delay(10);
  read_morse_word(i);
  //flash_morse_word(i);
  // TEST FOR A MATCH
  if (c_array(i, secret1))
  {
    state = state | B00000001;
    return success();
  }
  if (c_array(i, secret2))
  {
    state = state | B00000010;
    return success();
  }
  if (c_array(i, secret3))
  {
    state = state | B00000100;
    return success();
  }
  if (c_array(i, secret4))
  {
    state = state | B00001000;
    return success();
  }
  if (c_array(i, secret5))
  {
    state = state | B00010000;
    return success();
  }
  if (c_array(i, secret6))
  {
    state = state | B00100000;
    return success();
  }
  if (c_array(i, secret7))
  {
    state = state | B01000000;
    return success();
  }
  if (c_array(i, secret_reset))
  {
    state = B0;
    return success();
  }
  if (c_array(i, secret_all))
  {
    state = B11111111;
    return success();
  }
  return fail();
}

int vegas(int x)
{
  setAllPixels(0,0,0);
  if (x % 2)
  {
    strip.setPixelColor(0,YELLOW);
    strip.setPixelColor(2,YELLOW);
    strip.setPixelColor(4,YELLOW);
  }
  else
  {
    strip.setPixelColor(1,YELLOW);
    strip.setPixelColor(3,YELLOW);
  }
  strip.show();
  return 300;
}

int loading(int x)
{
  int i = x % 25;
  if ( i < 5 )
  {
    setAllPixels(0,0,0);
    strip.setPixelColor(i,RED);
    strip.show();
  }
  else if ( i < 9 )
  {
    setAllPixels(0,0,0);
    strip.setPixelColor(4,RED);
    strip.setPixelColor(i-6,ORANGE);
    strip.show();
  }
  else if ( i < 13 )
  {
    setAllPixels(0,0,0);
    strip.setPixelColor(4,RED);
    strip.setPixelColor(3,ORANGE);
    strip.setPixelColor(i-11,YELLOW);
    strip.show();
  }
  else if ( i < 16 )
  {
    setAllPixels(0,0,0);
    strip.setPixelColor(4,RED);
    strip.setPixelColor(3,ORANGE);
    strip.setPixelColor(2,YELLOW);
    strip.setPixelColor(i-15,GREEN);
    strip.show();
  }
  else if ( i < 18 )
  {
    setAllPixels(0,0,0);
    strip.setPixelColor(4,RED);
    strip.setPixelColor(3,ORANGE);
    strip.setPixelColor(2,YELLOW);
    strip.setPixelColor(1,GREEN);
    strip.setPixelColor(0,BLUE);
    strip.show();
  }
  return (100 + (i * 10));
}

int cycle(int n)
{
  int i = n % 6;
  switch(i) {
   case 0:
    setAllPixels(RED,true);
    break;
   case 1:
    setAllPixels(ORANGE,true);
    break;
   case 2:
    setAllPixels(YELLOW,true);
    break;
   case 3:
    setAllPixels(GREEN,true);
    break;
   case 4:
    setAllPixels(BLUE,true);
    break;
   case 5:
    setAllPixels(PURPLE,true);
    break;
  }
  return 200;
}

int fill_cycle(int x)
{
  int color_step = x % 60;
  switch (color_step)
  {
    case 1 ... 9:
      return fill(color_step,RED);    
    case 11 ... 19:
      return fill(color_step,ORANGE);
    case 21 ... 29:
      return fill(color_step,YELLOW);
    case 31 ... 39:
      return fill(color_step,GREEN);
    case 41 ... 49:
      return fill(color_step,BLUE);    
    case 51 ... 59:
      return fill(color_step,PURPLE);
  }
  return 0;
}

int knightrider(int i, int r, int g, int b)
{
  int n = i % 9;
  setAllPixels(OFF);
  if (n > 4)
  {
    n = 4 - (n - 5);
  }
  if (n == 2)
  {
    r = r * 4;
    g = g * 4;
    b = b * 4;
  }
  strip.setPixelColor(n,r,g,b);
  strip.show();
  return 100;
}

void loop()
{
  int mode = 0;
  int interval;
  uint16_t button_low_time = 0;
  uint32_t total_interval = 0;
  strip.begin();
  int i = 0;
  while(true)
  {
    if ( mode == 0 )
    {
      interval = vegas(i);
    }
    else if ( mode == 1 && (state & B00000001 ))
    {
      interval = cycle(i);
    }
    else if ( mode == 2 && (state & B00000010 ))
    {
      interval = police(i);
    }
    else if ( mode == 3 && (state & B00000100 ))
    {
      interval = loading(i);
    }
    else if ( mode == 4 && (state & B00001000 ))
    {
      interval = knightrider(i,20,0,0);
    }
    else if ( mode == 5 && (state & B00010000 ))
    {
      interval = fill(i,0,0,255);
    }
    else if ( mode == 6 && (state & B00100000 ))
    {
      interval = fill(i,30,50,0);
    }
    else if ( mode == 7 && (state & B01000000 ))
    {
      interval = fill_cycle(i);
    }
    else
    {
      mode = (mode +1) % 8;
      continue;
    }
    i++;
    // This section breaks down the sleep interval to catch button presses
    total_interval = total_interval + interval;
    while(interval > 0)
    {
      delay(10);
      interval = interval - 10;
      button_low_time = time_pin_low(2000);
      if (button_low_time > 200)
      {
       /*
       * MAIN MENU
       * NO PRESS = CONTINUE
       * SHORT PRESS = CHANGE FLASHY MODE
       * LONG PRESS = ENTER MORSE CODE MODE
       */
        if (button_low_time > 1200)
        {
          c_morse();
        }
        else
        {
          mode = (mode +1) % 8;
        }
        // reset timer
        i = 0;
        total_interval = 0;
      }

    }
    // At the end of each interval, see if we need to sleep
    if (total_interval > WAKE_TIME_MS)
    {
      total_interval = 0;
      i = 0;
      sleep();
    }
  }
}
