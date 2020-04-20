#include <Arduino.h>

struct LedCycle {
  int eCount;                       // event count
  unsigned long eTime[10];          // next event time
  unsigned char r[10],g[10],b[10];  // rgb values at event start
};

LedCycle operator + (const LedCycle& c1, const LedCycle& c2) {
  LedCycle lc = c1;
  // this breaks if counts are too hign
  unsigned long last = c1.eTime[c1.eCount-1];
  for(int i = 0; i < c2.eCount; i++) {
    lc.eTime[lc.eCount] = c2.eTime[i] + last;
    lc.r[lc.eCount] = c2.r[i];
    lc.g[lc.eCount] = c2.g[i];
    lc.b[lc.eCount] = c2.b[i];
    lc.eCount++;
  }
  return lc;
}

LedCycle operator * (float multiplier, const LedCycle& c) {
  LedCycle lc = c;
  for(int i =0; i < c.eCount; i++) {
    lc.eTime[i] *= multiplier;
  }
}

const LedCycle OFF          = {1, {100}, {0x00}, {0x00}, {0x00}};
const LedCycle RED          = {1, {100}, {0xFF}, {0x00}, {0x00}};
const LedCycle RED_LOW      = {1, {100}, {0x0F}, {0x00}, {0x00}};
const LedCycle GREEN        = {1, {100}, {0x00}, {0xFF}, {0x00}};
const LedCycle GREEN_LOW    = {1, {100}, {0x00}, {0x0F}, {0x00}};
const LedCycle BLUE         = {1, {100}, {0x00}, {0x00}, {0xFF}};
const LedCycle BLUE_LOW     = {1, {100}, {0x00}, {0x00}, {0x0F}};
const LedCycle SUCCESS = {4, {100,200,300,1000}, {}, {0xFF,0,0xFF,0}, {}};
const LedCycle FAILURE = {4, {100,200,300,1000}, {0xFF,0,0xFF,0}, {}, {}};
const LedCycle VICTORY = {6, {100,200,300,400,500,1600}, {0xFF,0,0xFF,0,0xFF,0}, {}, {0x7F,0,0x7F,0,0x7F,0}};

const LedCycle BLINKFO = BLUE + OFF + BLUE + 5*OFF;

struct ButtonActivity {
  int aCount;
  unsigned long aTime[10];  // next activity time
  unsigned char state[10];  // 0 nothing, 1 press, 2 release
};

const ButtonActivity NONE = {0};
const ButtonActivity PRESS = {1, {}, {1}};

struct ToyCycle {
  int sCount;
  const LedCycle *l1[10], *l2[10];
  const ButtonActivity *bRed[10], *bGreen[10], *bBlue[10];
};

const ToyCycle TOYCYCLE = {
  5,
  {&RED,&GREEN,&BLUE,&BLINKFO,&VICTORY},
  {&OFF,&OFF,&OFF,&BLINKFO,&VICTORY},
  {&PRESS,&NONE,&NONE,&PRESS,&PRESS},
  {&NONE,&PRESS,&NONE,&PRESS,&PRESS},
  {&NONE,&NONE,&PRESS,&PRESS,&PRESS},
};

class Led {
  public:
    Led(int rPin, int gPin, int bPin): rPin(rPin), gPin(gPin), bPin(bPin) {
      cycle = &OFF;
      cycleStart = 0;
      pinMode(rPin, OUTPUT);
      pinMode(rPin, OUTPUT);
      pinMode(rPin, OUTPUT);
    }
    void setCycle(const LedCycle* s) {
      cycle = s;
      cycleStart = millis();
    }
    void loopEvent() {
      unsigned long currentTime = millis();
      unsigned long cycleTime = (currentTime - cycleStart) % cycle->eTime[cycle->eCount-1];
      for (int i = 0; i < cycle->eCount; i++) {
        if (cycleTime < cycle->eTime[i]) {
          setColor(cycle->r[i], cycle->g[i], cycle->b[i]);
          break;
        }
      }
    }
  private:
    void setColor(unsigned char r, unsigned char g, unsigned char b) {
      rIntensity = r;
      gIntensity = g;
      bIntensity = b;
      analogWrite(rPin, 255-r);
      analogWrite(gPin, 255-g);
      analogWrite(bPin, 255-b);
    }
  private:
    int rPin,gPin,bPin;
    unsigned long cycleStart;
    const LedCycle* cycle;
    unsigned char rIntensity,gIntensity,bIntensity;
};

class Button {
  public:
    Button(int pin): pin(pin), prevDown(0), prevUp(0), pendingDownEvent(false), pendingUpEvent(false) {
      pinMode(pin, INPUT_PULLUP);
    }
    void loopEvent() {
      int val = digitalRead(pin);
      unsigned long currentTime = millis();
      if (val == LOW && prevDown <= prevUp) {
        prevDown = currentTime;
        pendingDownEvent = true;
      } else if (val == HIGH && prevUp <= prevDown) {
        prevUp = currentTime;
        pendingUpEvent = true;
      }
    }
    bool pressEvent() {
      if (!pendingDownEvent) {
        return false;
      }
      pendingDownEvent = false;
      return true;
    }
    bool releaseEvent() {
      if (!pendingUpEvent) {
        return false;
      }
      pendingUpEvent = false;
      return true;
    }
  private:
    int pin;
    unsigned long prevDown, prevUp;
    bool pendingDownEvent, pendingUpEvent;
};


struct ToyState {
  public:
    ToyState(const ToyCycle* c) : l1(5,3,6), l2(10,9,11), bRed(2), bGreen(4), bBlue(7), stateCycle(c) {
      stateStep = 0;
      previousStepCompletionTime = 0;
      newStateSet = false;
    }
    void loopEvent() {
      l1.loopEvent();
      l2.loopEvent();
      bRed.loopEvent();
      bBlue.loopEvent();
      bGreen.loopEvent();

      const ButtonActivity* expectedActivity[] = {stateCycle->bRed[stateStep], stateCycle->bGreen[stateStep], stateCycle->bBlue[stateStep]};
      bool press[] = {bRed.pressEvent(), bGreen.pressEvent(), bBlue.pressEvent()};
      bool release[] = {bRed.releaseEvent(), bGreen.releaseEvent(), bBlue.releaseEvent()};
      unsigned long currentTime = millis();

      // If the previous state just finished, return so there's time to flash "success" before going to the next state.
      if (currentTime < previousStepCompletionTime + 1000) {
        return;
      }

      // If the new state is not computed yet, compute it now.
      if (!newStateSet) {
        newStateSet = true;
        if (stateStep < 0) {
          l1.setCycle(&OFF);
          l2.setCycle(&OFF);
        } else if (stateStep >= stateCycle->sCount) {
          l1.setCycle(&VICTORY);
          l2.setCycle(&VICTORY);
        } else {
          l1.setCycle(stateCycle->l1[stateStep]);
          l2.setCycle(stateCycle->l2[stateStep]);
        }
      }

      for (int i = 0; i < 3; i++) {
        unsigned long actualTime = currentTime;
        unsigned long actualAction = press[i] ? 1 : release[i] ? 2 : 0;
        if (actualAction == 0) {
          continue; // no action
        }
        if (buttonStep[i] >= expectedActivity[i]->aCount && press[i]) {
          Serial.print("Failures step "); Serial.print(stateStep); Serial.println();
          stateStep = 0;
          previousStepCompletionTime = currentTime;
          buttonStep[0] = buttonStep[1] = buttonStep[2] = 0;
          newStateSet = false;
          l1.setCycle(&FAILURE);
          l2.setCycle(&FAILURE);
          return;
        }
        if (press[i]) {
          Serial.print("event press ");
          Serial.print(i);
          Serial.println();
        }
        if (release[i]) {
          Serial.print("event release ");
          Serial.print(i);
          Serial.println();
        }
        unsigned long expectedActionTime = currentTime;
        if (buttonStep[i] != 0) {
          expectedActionTime = buttonPrevTime[i] + expectedActivity[i]->aTime[buttonStep[i]];
        }
        unsigned char expectedAction = expectedActivity[i]->state[buttonStep[i]];
        if (actualAction == 1) { 
          buttonStep[i] = buttonStep[i]+1; //temp; later check timing
        }
        // todo fuckup reset
      }
      // if all steps, new state
      int done = 0;
      for (int i = 0; i < 3; i++) {
        if (buttonStep[i] >= expectedActivity[i]->aCount) {
          done++;
        }
      }
      if (done == 3) {
        Serial.print("Success step "); Serial.print(stateStep); Serial.println();
        stateStep++;
        previousStepCompletionTime = currentTime;
        buttonStep[0] = buttonStep[1] = buttonStep[2] = 0;
        newStateSet = false;
        l1.setCycle(&SUCCESS);
        l2.setCycle(&SUCCESS);
      }
    }
  private:
    Led l1, l2;
    Button bRed, bGreen, bBlue;
    const ToyCycle* stateCycle;
    int stateStep;
    unsigned long previousStepCompletionTime;
    bool newStateSet;
    int buttonStep[3];
    unsigned long buttonPrevTime[3];
};

ToyState *ts;

void setup() {
  ts = new ToyState(&TOYCYCLE);
  // digitalWrite(testPin, HIGH);
  Serial.begin(9600);
  Serial.print("Reset");
  Serial.println();
}

void loop() {
  ts->loopEvent();
}