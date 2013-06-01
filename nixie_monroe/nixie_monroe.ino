#include <avr/io.h>
#include <avr/interrupt.h>

// Arduino pin numbers for the low end lines (selecting the value displayed)
int numLines[10] = { 45, 44, 43, 42, 41, 40, 31, 30, 29, 28 };
// Arduino pin numbers for the high end lines (selecting the tube to light up)
int digitLines[12] = { 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6 };

// constants of TIME!!
int days_per_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// Arduino pin number for the top button
int buttonA = 19;
// Arduino pin number for the bottom button
int buttonB = 18;
// Arduino analog port for the knob
int analogKnob = 0;

// The values currently displayed on each tube. "-1" means the tube
// is off.
int digits[12] = { -1, -1, -1, -1, -1, -1, -1 , -1, -1, -1, -1, -1 };

void setup() {
  // Set all number and digit lines as outputs and turn them off
  for (int i = 0; i < 10; i++) {
      pinMode(numLines[i],OUTPUT);
      digitalWrite(numLines[i],LOW);
  }
  for (int i = 0; i < 12; i++) {
      pinMode(digitLines[i],OUTPUT);
      digitalWrite(digitLines[i],LOW);
  }
  // Buttons are active low
  pinMode(buttonA,INPUT_PULLUP);
  pinMode(buttonB,INPUT_PULLUP);
  // Turn off interupts during timer configuration
  cli();
  // Timer 3 is used for clock ticks at one-second intervals
  TCCR3A = 0x00; // 0x04 is CTC
  TCCR3B = 0x0C; // 0x04 is clock prescaler
  TIMSK3 = 0x02; // OCR1A match interrupt
  OCR3A = 62500;
  // Timer 2 is used to refresh each tube in turn
  TCCR2A = 0x02;
  TCCR2B = 0x04;
  TIMSK2 = 0x02;
  OCR2A = 0x7F;
  // Reenable interrupts
  sei();
  Serial.begin(15200);
}

// The index of the digit currently illuminated
int lastDigit = -1;
// The index of the number line currently asserted
int lastNum = -1;

// The default time, in seconds since the UNIX epoch
//unsigned long long epoch = 1358650000LL;
int hours = 12;
int minutes = 0;
int seconds = 0;
int years = 13;
int months = 05;
int days = 31;


// This is called every second. The array of digits should
// be updated to reflect the current time.
// This function is *not* called when the clock is in "set"
// mode (when the user is adjusting the time).
void updateDigits() {
  // Update digits from epoch.
  // This is where various date modes would go (swatch time,
  // military time, etc.).

  digits[0] = hours / 10;
  digits[1] = hours % 10;
  digits[2] = minutes / 10;
  digits[3] = minutes % 10;
  digits[4] = seconds / 10;
  digits[5] = seconds % 10;
  digits[6] = months / 10;
  digits[7] = months % 10;
  digits[8] = days / 10;
  digits[9] = days % 10;
  digits[10] = years / 10;
  digits[11] = years % 10;  

}

// This is called when "set" mode is exited. It's assumed
// the user has set the correct time; update the date value
// based on the values of the digits.
void updateDate() {
  hours = digits[0]*10 + digits[1];
  minutes = digits[2]*10 + digits[3];
  seconds = digits[4]*10 + digits[5];
  months = digits[6]*10 + digits[7];
  days = digits[8]*10 + digits[9];
  years = digits[10]*10 + digits[11];
}

// Turn off the previously lit tube and illuminate the
// given number on the specified tube. If -1 is specified,
// the tube is not lit.
void setTube(int digit, int num) {
  if (digit >= 12) {
    digit = -1;
  }
  if (lastDigit != -1) {
    digitalWrite(digitLines[lastDigit],LOW);
  }
  if (lastNum != -1) {
    digitalWrite(numLines[lastNum],LOW);
  }
  delayMicroseconds(20);
  if ((digit != -1) && (num != -1)) {
    digitalWrite(digitLines[digit],HIGH);
  }
  if (num != -1) {
    digitalWrite(numLines[num],HIGH);
  }
  lastNum = num;
  lastDigit = digit;
}

// Previous states of the A and B buttons;
// used for debouncing.
boolean bstateA = true;
boolean bstateB = true;

// The digit currently being set. If -1, then
// the clock is not in set time mode.
int editDigit = -1;
// The index of the currently illuminated tube.
int displayedDigit = 0;
// Previous value of the potentiometer
int oldVal = 0;
// Current value of the potentiometer
int val = 0;
// Scrap variable used to blink digits in set mode.
int blinkCycle = 0;

// Set to true once a second to indicate that the epoch has
// changed and the digits need to be updated to reflect the
// new time.
volatile boolean needsUpdate = true;
// The user has changed the values on the tubes in set time mode.
boolean valueChanged = false;

void loop() {
  if (needsUpdate) { needsUpdate = false; updateDigits(); }
  // Small delay to aid debouncing
  delayMicroseconds(50);
  boolean nbA = digitalRead(buttonA);
  boolean nbB = digitalRead(buttonB);
  if (!bstateA && nbA) {
    // On button A press, enter edit mode...
    editDigit++;
    if (editDigit >= 12) {
      // ... or leave edit mode if they cycle through the
      // last digit.
      editDigit = -1;
    }
  }
  if (!bstateB && nbB) {
    // On button B press, go back a digit. Eventually this may
    // be used to change display modes when not in set time mode.
    if (editDigit >= 0) {
      editDigit--;
      if (editDigit < 0) editDigit = -1;
    } else {
      // mode change?
    }
  }

  if ((editDigit == -1) && valueChanged) {
    // If we're not editing and the value has changed, update the epoch!
    valueChanged = false;
    updateDate();
  }
  
  // Save button states
  bstateA = nbA; bstateB = nbB;
  // Save potentiometer value state
  oldVal = val;
  // oh, great, used a log taper pot instead of linear.
  // luckily these are only marginally log. We'll hack it.
  int input = analogRead(0);
  const int elbow = 900;
  if (input < elbow) {
    val = (4*input)/elbow;
  } else {
    val = 4+((6*(input-elbow)) / (1024-elbow));
  }
  if ((editDigit != -1) && (val != oldVal)) {
    // The knob has been turned; update the digit
    if (val > 10) { val = 9; }
    digits[editDigit] = val;
    blinkCycle = 0;
    valueChanged = true;
  }
}

// Number of all-tube cycles per "blink" cycle
const int BCYCLEN=80;

// Interrupt for udating the tubes
//ISR(TIMER2_OVF_vect) {
ISR(TIMER2_COMPA_vect) {
  int d = digits[displayedDigit];
  if ((displayedDigit == editDigit) && (blinkCycle > (BCYCLEN/2))) {
    d = -1;
  }
  setTube(displayedDigit,d);
  displayedDigit++;
  if (displayedDigit == 24) {
    displayedDigit = 0;
    blinkCycle = (blinkCycle+1)%BCYCLEN;
  }
}

// Interrupt for clock ticks
ISR(TIMER3_COMPA_vect) {
  if (editDigit == -1) {
    seconds++;
    if (seconds > 59) {
      seconds = 0;
      minutes++;
      if (minutes > 59) {
        minutes = 0;
        hours++;
        if (hours > 23) {
          hours = 0;
          days++;
          if (days > days_per_month[months]) {
            days = 1;
            months++;
            if (months > 12) {
              months = 1;
              years++;
              if (years > 99) {
                years = 0;
              }
            }
          }
        }
      }
    }
    needsUpdate = true;
  }
}
