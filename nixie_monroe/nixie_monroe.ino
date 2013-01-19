#include <avr/io.h>
#include <avr/interrupt.h>

int numLines[10] = { 45, 44, 43, 42, 41, 40, 31, 30, 29, 28 };
int digitLines[12] = { 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6 };
int buttonA = 19;
int buttonB = 18;
int analogKnob = 0;

int digits[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1 };

void setup() {
  for (int i = 0; i < 10; i++) {
      pinMode(numLines[i],OUTPUT);
      digitalWrite(numLines[i],LOW);
  }
  for (int i = 0; i < 12; i++) {
      pinMode(digitLines[i],OUTPUT);
      digitalWrite(digitLines[i],LOW);
  }
  pinMode(buttonA,INPUT_PULLUP);
  pinMode(buttonB,INPUT_PULLUP);
  cli();
  // Clock 3 is for second timing
  TCCR3A = 0x00; // 0x04 is CTC
  TCCR3B = 0x0C; // 0x04 is clock prescaler
  TIMSK3 = 0x02; // OCR1A match interrupt
  OCR3A = 62500;
  // Clock 2 is for tube cycling
  TCCR2A = 0x00;
  TCCR2B = 0x04;
  TIMSK2 = 0x01;
  sei();
  Serial.begin(15200);
}

int lastDigit = -1;
int lastNum = -1;

unsigned long long epoch = 135840000;

void updateDigits() {
  // Update digits from epoch.
  // This is where various date modes would go.
  
  if (1) {
    // unix epoch; 10 digits long. Left justified.
    digits[11] = -1;
    digits[10] = -1;
    unsigned long long n = epoch;
    for (int i = 9; i >= 0; i--) {
      digits[i] = n % 10;
      n = n / 10;
    }
  }
}

void updateEpoch() {
  // Update epoch from digits.
  // This is where the various date modes would go.
  if (1) {
    // unix epoch editing mode
    epoch = 0;
    for (int i = 0; i < 10; i++) {
      epoch *= 10;
      epoch += digits[i];
    }
  }
}

void setTube(int digit, int num) {
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

int d = 0;

long long number = 123456789012LL;

boolean bstateA = true;
boolean bstateB = true;

int editDigit = -1;
int displayedDigit = 0;
int oldVal = 0;
int val = 0;
int blinkCycle = 0;

volatile boolean needsUpdate = true;
boolean valueChanged = false;

void loop() {
  if (needsUpdate) { needsUpdate = false; updateDigits(); }
  delayMicroseconds(100);
  boolean nbA = digitalRead(buttonA);
  boolean nbB = digitalRead(buttonB);
  if (!bstateA && nbA) {
    editDigit++;
    if (editDigit >= 12) {
      editDigit = -1;
    }
  }
  if (!bstateB && nbB) {
    if (editDigit >= 0) {
      editDigit--;
      if (editDigit < 0) editDigit = -1;
    } else {
      // mode change
    }
  }

  if ((editDigit == -1) && valueChanged) {
    valueChanged = false;
    updateEpoch();
  }
  
  bstateA = nbA; bstateB = nbB;
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
  //val = (input*(input/102))/102;// analogRead(0)/102;
  if ((editDigit != -1) && (val != oldVal)) {
    if (val > 10) { val = 9; }
    digits[editDigit] = val;
    blinkCycle = 0;
    valueChanged = true;
  }
}

const int BCYCLEN=80;

ISR(TIMER2_OVF_vect) {
  int d = digits[displayedDigit];
  if ((displayedDigit == editDigit) && (blinkCycle > (BCYCLEN/2))) {
    d = -1;
  }
  setTube(displayedDigit,d);
  displayedDigit++;
  if (displayedDigit == 12) {
    displayedDigit = 0;
    blinkCycle = (blinkCycle+1)%BCYCLEN;
  }
}

ISR(TIMER3_COMPA_vect) {
  if (editDigit == -1) {
    epoch++;
    needsUpdate = true;
  }
}
