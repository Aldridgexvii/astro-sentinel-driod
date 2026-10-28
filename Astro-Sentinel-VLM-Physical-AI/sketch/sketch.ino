#include <Arduino.h>
#include <Arduino_RouterBridge.h>
#include <Arduino_LED_Matrix.h>
#include <Modulino.h>
#include <math.h>
#include <string.h>

// State IDs are mirrored by python/main.py.
enum VisualState : int {
  STATE_IDLE = 0,
  STATE_LABELING = 1,
  STATE_THINKING = 2,
  STATE_HAPPY = 3,
  STATE_CURIOUS = 4,
  STATE_CONFUSED = 5,
  STATE_OFFLINE = 6,
  STATE_IDENTIFIED = 7,
  STATE_CAMERA_ERROR = 8
};

Arduino_LED_Matrix matrix;
ModulinoDistance distanceSensor;
ModulinoBuzzer buzzer;
ModulinoVibro vibro;

constexpr int MATRIX_W = 13;
constexpr int MATRIX_H = 8;
uint8_t frame[MATRIX_W * MATRIX_H];
volatile int currentState = STATE_IDLE;
volatile bool cameraActive = false;
uint32_t animationTick = 0;
uint32_t lastRenderAt = 0;

bool distanceReady = false;
bool buzzerReady = false;
bool vibroReady = false;

void setPixel(int x, int y, uint8_t brightness) {
  if (x < 0 || x >= MATRIX_W || y < 0 || y >= MATRIX_H) return;
  frame[y * MATRIX_W + x] = brightness;
}

void clearFrame() {
  memset(frame, 0, sizeof(frame));
}

float getDistanceMm() {
  if (!distanceReady) return -1.0f;
  distanceSensor.available();
  const float mm = distanceSensor.get();
  if (!isfinite(mm) || mm <= 0.0f) return -1.0f;
  return mm;
}

String getHardwareStatus() {
  String result = "distance=";
  result += distanceReady ? "ok" : "missing";
  result += ",buzzer=";
  result += buzzerReady ? "ok" : "missing";
  result += ",vibro=";
  result += vibroReady ? "ok" : "missing";
  result += ",led_matrix=ok,camera_indicator=ok";
  return result;
}

void setSystemState(int state) {
  currentState = constrain(state, STATE_IDLE, STATE_CAMERA_ERROR);
  animationTick = 0;
}

void setCameraActive(bool active) {
  cameraActive = active;
}

void drawBotFace(bool happy, bool curious) {
  const bool blink = !happy && !curious && ((animationTick % 46) >= 43);
  if (blink) {
    setPixel(3, 3, 255); setPixel(4, 3, 140);
    setPixel(8, 3, 140); setPixel(9, 3, 255);
  } else if (curious) {
    setPixel(3, 2, 255); setPixel(3, 3, 255);
    setPixel(9, 1, 95); setPixel(9, 2, 180); setPixel(9, 3, 255);
  } else {
    setPixel(3, 2, 255); setPixel(4, 2, 190);
    setPixel(8, 2, 190); setPixel(9, 2, 255);
  }

  if (happy) {
    // Curved droid grin.
    setPixel(3, 5, 90); setPixel(4, 6, 160); setPixel(5, 6, 220);
    setPixel(6, 6, 255); setPixel(7, 6, 255); setPixel(8, 6, 220); setPixel(9, 5, 90);
  } else if (curious) {
    setPixel(5, 5, 100); setPixel(6, 6, 220); setPixel(7, 5, 100);
    setPixel(10, 3, 95);
  } else {
    setPixel(5, 5, 120); setPixel(6, 5, 180); setPixel(7, 5, 120);
  }
  setPixel(1, 3, 55); setPixel(11, 3, 55);
}

void renderIdle() {
  drawBotFace(false, false);
  setPixel(6, 7, 55);
  setPixel(5, 7, 25); setPixel(7, 7, 25);
}

void renderLabeling() {
  // A bright sweep with two fading echoes: droid is looking at the scene.
  const int x = (animationTick / 2) % (MATRIX_W + 4) - 2;
  for (int y = 1; y < 7; ++y) {
    setPixel(x, y, 255);
    setPixel(x - 1, y, 85);
    setPixel(x + 1, y, 125);
    setPixel(x - 2, y, 35);
  }
  setPixel(1, 0, 55); setPixel(5, 0, 55); setPixel(7, 0, 55);
}

void renderThinking() {
  const int xs[8] = {6, 8, 10, 11, 6, 4, 2, 1};
  const int ys[8] = {0, 1, 2, 4, 7, 6, 4, 2};
  for (int i = 0; i < 8; ++i) setPixel(xs[i], ys[i], 45);
  const int hot = (animationTick / 2) % 8;
  setPixel(xs[hot], ys[hot], 255);
  setPixel(xs[(hot + 7) % 8], ys[(hot + 7) % 8], 145);
  setPixel(5, 3, 80); setPixel(6, 3, 180); setPixel(7, 3, 80);
  setPixel(6, 4, 110);
}

void renderIdentified() {
  // Box + unmistakable checkmark.
  for (int x = 2; x <= 10; ++x) { setPixel(x, 1, 75); setPixel(x, 6, 75); }
  for (int y = 1; y <= 6; ++y) { setPixel(2, y, 75); setPixel(10, y, 75); }
  setPixel(4, 3, 255); setPixel(5, 4, 255); setPixel(6, 5, 255);
  setPixel(7, 4, 255); setPixel(8, 3, 255);
}

void renderCurious() {
  drawBotFace(false, true);
  // Keep the question-like expression inside the face area; the camera glyph
  // permanently owns the top-right three columns.
  setPixel(7, 5, 110); setPixel(8, 5, 75);
}

void renderConfused() {
  // Large question mark plus asymmetrical eyes.
  setPixel(2, 2, 140); setPixel(3, 1, 235); setPixel(4, 1, 235);
  setPixel(5, 2, 235); setPixel(5, 3, 210); setPixel(6, 4, 210); setPixel(6, 6, 255);
  setPixel(8, 2, 120); setPixel(10, 2, 120);
  setPixel(8, 4, 50); setPixel(9, 4, 50);
}

void renderOffline() {
  for (int i = 1; i < 7; ++i) {
    setPixel(i, i, 230);
    setPixel(12 - i, i, 230);
  }
  setPixel(6, 3, 65); setPixel(6, 4, 65);
}

void renderCameraError() {
  drawBotFace(false, true);
  setPixel(4, 0, 180); setPixel(5, 0, 180); setPixel(6, 0, 180);
  setPixel(4, 1, 180); setPixel(6, 1, 180);
  setPixel(4, 2, 220); setPixel(5, 1, 255); setPixel(6, 2, 220);
}

void renderCameraIndicator() {
  // Persistent 3x3 camera glyph in the top-right corner, overlaid on every state.
  // A pulsing lens means fresh frames are arriving; a dim crossed camera means stale
  // frames. This takes only x=10..12, so it no longer covers the droid's right eye.
  if (cameraActive) {
    setPixel(10, 0, 180); setPixel(11, 0, 180);
    setPixel(10, 1, 180); setPixel(11, 1, 90);
    setPixel(10, 2, 180); setPixel(11, 2, 180);
    setPixel(12, 1, ((animationTick / 3) % 2 == 0) ? 255 : 45);
  } else {
    setPixel(10, 0, 55); setPixel(11, 0, 55);
    setPixel(10, 1, 55); setPixel(11, 1, 25);
    setPixel(10, 2, 55); setPixel(11, 2, 55);
    setPixel(10, 0, 170); setPixel(11, 2, 170);
    setPixel(12, 1, 30);
  }
}

void renderMatrix() {
  clearFrame();
  switch (currentState) {
    case STATE_IDLE: renderIdle(); break;
    case STATE_LABELING: renderLabeling(); break;
    case STATE_THINKING: renderThinking(); break;
    case STATE_HAPPY: drawBotFace(true, false); break;
    case STATE_CURIOUS: renderCurious(); break;
    case STATE_CONFUSED: renderConfused(); break;
    case STATE_OFFLINE: renderOffline(); break;
    case STATE_IDENTIFIED: renderIdentified(); break;
    case STATE_CAMERA_ERROR: renderCameraError(); break;
    default: renderConfused(); break;
  }
  renderCameraIndicator();
  matrix.draw(frame);
  ++animationTick;
}

void chirp(uint16_t frequency, uint16_t durationMs) {
  if (!buzzerReady) return;
  buzzer.tone(frequency, durationMs);
  delay(durationMs + 15);
  buzzer.noTone();
}

void playMood(String mood) {
  mood.trim();
  mood.toUpperCase();

  if (mood == "SCAN") {
    chirp(880, 65);
    return;
  }

  if (mood == "HAPPY") {
    const uint16_t notes[] = {1320, 1760, 1480, 1960};
    for (size_t i = 0; i < (sizeof(notes) / sizeof(notes[0])); ++i) {
      chirp(notes[i], 55);
    }
    if (vibroReady) { vibro.on(170, GENTLE); delay(150); vibro.off(); }
    return;
  }

  if (mood == "CURIOUS") {
    chirp(780, 70); chirp(1120, 70); chirp(930, 95);
    if (vibroReady) { vibro.on(120, GENTLE); delay(100); vibro.off(); }
    return;
  }

  // CONFUSED / fallback: friendly descending pair, not an alarm.
  chirp(880, 100); chirp(590, 160);
  if (vibroReady) { vibro.on(135, MEDIUM); delay(120); vibro.off(); }
}

void setup() {
  matrix.begin();
  matrix.setGrayscaleBits(8);
  renderMatrix();

  if (!Bridge.begin()) setSystemState(STATE_OFFLINE);

  Modulino.begin();
  delay(100);
  distanceReady = distanceSensor.begin();
  buzzerReady = buzzer.begin();
  vibroReady = vibro.begin();

  Bridge.provide("get_distance_mm", getDistanceMm);
  Bridge.provide("get_hardware_status", getHardwareStatus);
  Bridge.provide("play_mood", playMood);
  Bridge.provide("set_state", setSystemState);
  Bridge.provide("set_camera_active", setCameraActive);
}

void loop() {
  const uint32_t now = millis();
  if (now - lastRenderAt >= 75) {
    lastRenderAt = now;
    renderMatrix();
  }
  delay(4);
}
