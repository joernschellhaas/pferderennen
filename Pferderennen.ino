// (c) Jörn Schellhaas

// Board: Sparkfun Pro Micro 5V/16MHz (https://github.com/sparkfun/Arduino_Boards)
// Make sure you set it correctly in the IDE - otherwise you might brick the device
#define VERSION "0.0trunk"

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
  // Seems to be not required
#endif

typedef struct{
  uint8_t pin;
  uint8_t channel;
  uint8_t number;
} t_inputmap;
typedef struct{
  uint8_t state;
  uint32_t until;
  bool polarity;
} t_inputstate;

#define PLAYERS 2
#define STEPS 16
#define LED_PIN 14
#define LED_COUNT (PLAYERS * STEPS)
#define T_BLOCK_HIT 2000
#define T_BLOCK_WIN 5000
#define T_INPUT_TIMEOUT (10 * 60 * 1000)
#define T_DEBOUNCE 25
#define COUNTOF(arr) (sizeof(arr) / sizeof(arr[0]))
const uint32_t COLORS[] = {
  0xFF0000,
  0x00FF00
};
const t_inputmap INPUTMAP[] = {
  {2, 0, 1},
  {A2 /*3*/, 0, 2},
  {A1 /*4*/, 0, 3},
  {5, 1, 1},
  {16 /*6*/, 1, 2},
  {7, 1, 3},
};
#define DEBUG(var) Serial.print(var);

Adafruit_NeoPixel leds = Adafruit_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
uint8_t counter[PLAYERS];
uint8_t state;
#define STATE_IDLE 0
#define STATE_RUN 1
#define STATE_DONE 2
#define STATE_DONE_WAIT 3
uint32_t stateSince;
uint32_t lastInput;
uint8_t winner;
uint8_t input_state[PLAYERS] = {};
t_inputstate inputs[COUNTOF(INPUTMAP)];

void reset(){
  for(uint8_t p = 0; p < PLAYERS; p++) counter[p] = 0;
}

void setState(uint8_t s){
  if(state == s) return;
  state = s;
  stateSince = millis();
  if(s == STATE_RUN) reset();
}

void hit(uint8_t player, uint8_t number){
  lastInput = millis();
  switch(state){
    case STATE_DONE_WAIT:
      return;
      break;
    default:
      setState(STATE_RUN);
      counter[player] += number;
      if(counter[player] >= STEPS){
        // Player wins...
        winner = player;
        setState(STATE_DONE_WAIT);
      }
      break;
  }
}

void logic(){
  switch(state){
    case STATE_RUN:
      if(millis() - lastInput > T_INPUT_TIMEOUT){
        setState(STATE_DONE);
      }
      break;
    
    case STATE_DONE_WAIT:
      if(millis() - stateSince > T_BLOCK_WIN){
        setState(STATE_IDLE);
      }
      break;
  }
}

void setupInput(){
  for(uint8_t i = 0; i < COUNTOF(INPUTMAP); i++){
    pinMode(INPUTMAP[i].pin, INPUT);
    bool start_value = digitalRead(INPUTMAP[i].pin);
    inputs[i].state = start_value;
    // Auto detect polarity at startup. It does not really matter if we get it wrong; in this case we trigger when the ball leaves the detector.
    inputs[i].polarity = start_value;
    inputs[i].until = millis();
  }
}

void input(){
  for(uint8_t i = 0; i < COUNTOF(inputs); i++){
    uint32_t t = millis();
    uint8_t current = digitalRead(INPUTMAP[i].pin);
    if(inputs[i].state != current){
      if(t - inputs[i].until > T_DEBOUNCE){ // Only compare durations!
        // Input had changed value for some time
        inputs[i].state = current;
        inputs[i].until = t;
        DEBUG("Pin ") DEBUG(INPUTMAP[i].pin) DEBUG(" changed to ") DEBUG(current) DEBUG("\n")
        if(current != inputs[i].polarity) {
          hit(INPUTMAP[i].channel, INPUTMAP[i].number);
        }
      }
    }else{
      // Input is the same, so reset timer
      inputs[i].until = t;
    }
  }
}

void inputPrint(){
  for(uint8_t i = 0; i < COUNTOF(inputs); i++){
    DEBUG("Pin ") DEBUG(INPUTMAP[i].pin) DEBUG(": ") DEBUG(digitalRead(INPUTMAP[i].pin)) DEBUG("\n")
  }  
}

uint8_t getLEDIndex(uint8_t player, uint8_t number) {
  return player + number * PLAYERS;
}

void output(){
  // Print player progress
  for(uint8_t p = 0; p < PLAYERS; p++) {
    for(uint8_t n = 0; n < STEPS; n++) {
      leds.setPixelColor(getLEDIndex(p, n), counter[p] > n ? COLORS[p] : 0x000000);
    }
  }
  
  switch(state){
    case STATE_DONE_WAIT:
      if((millis() - stateSince) % 1000 > 500){
        for(uint8_t n = 0; n < STEPS; n++) {
          leds.setPixelColor(getLEDIndex(winner, n), 0xFFFFFF);
        }
      }
      break;
 
    case STATE_IDLE:
    case STATE_DONE:
      int pixel_offset = millis() >> 5;
      for(int i=0; i<leds.numPixels() / 2; i++) { // For each pixel in strip...
        // Offset pixel hue by an amount to make one full revolution of the
        // color wheel (range of 65536) along the length of the strip
        // (strip.numPixels() steps):
        int pixelHue = i * 65536L / leds.numPixels() * 2 / 3;
        // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
        // optionally add saturation and value (brightness) (each 0 to 255).
        // Here we're using just the single-argument hue variant. The result
        // is passed through strip.gamma32() to provide 'truer' colors
        // before assigning to each pixel:
        auto color = leds.gamma32(leds.ColorHSV(pixelHue));
        leds.setPixelColor((pixel_offset + i) % leds.numPixels(), color);
        leds.setPixelColor((pixel_offset - i - 1) % leds.numPixels(), color);
      }
      break;
  }
  leds.show();
}

void setupOutput(){
  leds.begin();
  leds.show();
}

void setupDebug(){
  Serial.begin(115200);
  DEBUG("This is Pferderennen\n");
}

void setup() {
  setupInput();
  reset();
  setupOutput();
  setupDebug();
}

void loop() {
  /*inputPrint(); delay(100);*/
  input();
  logic();
  output();
}
