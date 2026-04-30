#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <SevSeg.h>
#include <Wire.h>

// --- Enums ---
enum MainMode { WORKFLOW_CODING, WORKFLOW_STUDY, WORKFLOW_MOVIES };

enum PomodoroState { POM_STOPPED, POM_WORK, POM_BREAK };

enum IconType {
  ICON_NONE,
  ICON_SPOTIFY,
  ICON_PLAYPAUSE,
  ICON_DEBUG,
  ICON_PREV,
  ICON_NEXT,
  ICON_TIMER,
  ICON_SKIPFWD,
  ICON_SKIPBWD,
  ICON_MODE_CODING,
  ICON_MODE_STUDY,
  ICON_MODE_MOVIES
};

// --- Function Prototypes ---
void updateOLED();
void updateDisplays();
void handleTimerEnd();
void handleInputRelease(int idx, unsigned long dur);
void switchMode(int newMode);
void setAction(String msg, IconType icon);
void doButton1Press();
void doButton2Press();
void doButton3Press();
void doNextAction();
void doPreviousAction();
void startPomodoro();

// --- Hardware Objects ---
Adafruit_SSD1306 display(128, 64, &Wire, -1);
SevSeg sevseg;

// --- Global Variables ---
MainMode workflow = WORKFLOW_CODING;
PomodoroState pomState = POM_STOPPED;

String actionMsg = "READY";
IconType currentActionIcon = ICON_NONE;
unsigned long actionTimer = 0;
bool actionUiActive = false;

// Pomodoro Timer
long remainingSecs = 0;
bool isPaused = true;
int sessionsCompleted = 0;

// Time & Notifications
int pcTime = 1200; // Updated via Serial
unsigned long blinkTimer = 0;
int blinkCount = 0;

// Touch Configuration
const int pins[] = {15, 2, 4}; // Using GPIO 15, 2, 4
float filtered[] = {100.0, 100.0, 100.0};
unsigned long touchStart[3] = {0, 0, 0};
bool pressed[3] = {false, false, false};
bool btn1_mode_switched = false; // Tracks if B1 modifier was used
bool btn3_mode_switched = false; // Tracks if B3 modifier was used

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;)
      ;
  }
  display.setRotation(2);

  byte numDigits = 4;
  byte dPins[] = {19, 18, 5, 17};
  byte sPins[] = {16, 23, 25, 26, 14, 27, 33, 32};
  sevseg.begin(COMMON_CATHODE, numDigits, dPins, sPins, false, false, false,
               false);
  sevseg.setBrightness(90);

  setAction("SYSTEM ONLINE", ICON_MODE_CODING);
}

void loop() {
  static unsigned long lastTick = 0;

  // 1. Sync Time & Notifications from PC
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    data.trim();
    if (data.startsWith("TIME:")) {
      pcTime = data.substring(5).toInt();
    } else if (data == "NOTIFY") {
      blinkCount = 6;
      blinkTimer = millis();
    }
  }

  // 2. Update Touch Sensors
  for (int i = 0; i < 3; i++) {
    filtered[i] = (filtered[i] * 0.9) + (touchRead(pins[i]) * 0.1);
    bool isTouched = filtered[i] < 30;

    if (isTouched && !pressed[i]) {
      touchStart[i] = millis();
      pressed[i] = true;

      // --- CHORDED INPUTS ---
      // Hold B1 + Press B2 -> Coding
      if (i == 1 && pressed[0]) {
        switchMode(WORKFLOW_CODING);
        btn1_mode_switched = true;
        touchStart[1] = 0;
      }
      // Hold B1 + Press B3 -> Movies
      else if (i == 2 && pressed[0]) {
        switchMode(WORKFLOW_MOVIES);
        btn1_mode_switched = true;
        touchStart[2] = 0;
      }
      // Hold B3 + Press B1 -> Next
      else if (i == 0 && pressed[2]) {
        doNextAction();
        btn3_mode_switched = true;
        touchStart[0] = 0;
      }
      // Hold B3 + Press B2 -> Previous
      else if (i == 1 && pressed[2]) {
        doPreviousAction();
        btn3_mode_switched = true;
        touchStart[1] = 0;
      }
    } else if (!isTouched && pressed[i]) {
      handleInputRelease(i, millis() - touchStart[i]);
      pressed[i] = false;
    }
  }

  // 3. Timer Ticking
  if (pomState != POM_STOPPED && !isPaused && millis() - lastTick >= 1000) {
    lastTick = millis();
    if (remainingSecs > 0) {
      remainingSecs--;
    } else {
      handleTimerEnd();
    }
    // Update display if showing idle timer
    if (!actionUiActive && workflow == WORKFLOW_STUDY) {
      updateOLED();
    }
  }

  if (isPaused || pomState == POM_STOPPED) {
    lastTick = millis();
  }

  // 4. Update Displays
  if (millis() - actionTimer < 2000) {
    actionUiActive = true;
  } else if (actionUiActive) {
    actionUiActive = false;
    updateOLED(); // Clear action UI
  }

  updateDisplays(); // Refresh 7-seg continuously
}

void setAction(String msg, IconType icon) {
  actionMsg = msg;
  currentActionIcon = icon;
  actionTimer = millis();
  actionUiActive = true;
  updateOLED();
}

void switchMode(int newMode) {
  workflow = (MainMode)newMode;
  if (workflow == WORKFLOW_CODING) {
    Serial.println("MODE_CODING");
    setAction("CODING MODE", ICON_MODE_CODING);
    pomState = POM_STOPPED;
  } else if (workflow == WORKFLOW_STUDY) {
    Serial.println("MODE_STUDY");
    setAction("STUDY MODE", ICON_MODE_STUDY);
    startPomodoro();
  } else if (workflow == WORKFLOW_MOVIES) {
    Serial.println("MODE_MOVIES");
    setAction("MOVIES MODE", ICON_MODE_MOVIES);
    pomState = POM_STOPPED;
  }
}

void handleInputRelease(int idx, unsigned long dur) {
  if (touchStart[idx] == 0)
    return; // Action invalidated by modifier logic

  bool isHold = (dur > 800);

  if (idx == 0) { // Button 1
    if (!btn1_mode_switched) {
      if (isHold) {
        switchMode(WORKFLOW_STUDY);
      } else {
        doButton1Press();
      }
    }
    btn1_mode_switched = false;
  } else if (idx == 1) { // Button 2
    if (!isHold) {
      doButton2Press();
    }
  } else if (idx == 2) { // Button 3
    if (!btn3_mode_switched) {
      if (!isHold) {
        doButton3Press();
      }
    }
    btn3_mode_switched = false;
  }
}

// --- Mode-Specific Actions ---

void doButton1Press() {
  if (workflow == WORKFLOW_CODING || workflow == WORKFLOW_STUDY) {
    Serial.println("MEDIA_SPOTIFY");
    setAction("SPOTIFY", ICON_SPOTIFY);
  } else if (workflow == WORKFLOW_MOVIES) {
    Serial.println("KEY_RIGHT");
    setAction("SKIP FWD", ICON_SKIPFWD);
  }
}

void doButton2Press() {
  Serial.println("MEDIA_PLAY_PAUSE");
  setAction("PLAY / PAUSE", ICON_PLAYPAUSE);
}

void doButton3Press() {
  if (workflow == WORKFLOW_CODING) {
    Serial.println("KEY_DEBUGGER");
    setAction("DEBUGGER", ICON_DEBUG);
  } else if (workflow == WORKFLOW_STUDY) {
    if (pomState != POM_STOPPED) {
      isPaused = !isPaused;
      setAction(isPaused ? "PAUSED" : "RESUMED", ICON_TIMER);
    } else {
      startPomodoro();
      setAction("POMODORO", ICON_TIMER);
    }
  } else if (workflow == WORKFLOW_MOVIES) {
    Serial.println("KEY_LEFT");
    setAction("SKIP BWD", ICON_SKIPBWD);
  }
}

void doNextAction() {
  Serial.println("MEDIA_NEXT");
  setAction("NEXT", ICON_NEXT);
}

void doPreviousAction() {
  Serial.println("MEDIA_PREVIOUS");
  setAction("PREVIOUS", ICON_PREV);
}

// --- Pomodoro Logic ---

void startPomodoro() {
  pomState = POM_WORK;
  remainingSecs = 25 * 60;
  isPaused = false;
  sessionsCompleted = 0;
}

void handleTimerEnd() {
  if (pomState == POM_WORK) {
    pomState = POM_BREAK;
    remainingSecs = 5 * 60;
    sessionsCompleted++;
    Serial.println("NOTIFY");
    blinkCount = 6;
    blinkTimer = millis();
    setAction("BREAK TIME", ICON_TIMER);
  } else if (pomState == POM_BREAK) {
    startPomodoro();
    Serial.println("NOTIFY");
    blinkCount = 6;
    blinkTimer = millis();
    setAction("WORK TIME", ICON_TIMER);
  }
}

// --- Display Rendering ---

void updateDisplays() {
  if (blinkCount > 0) {
    if (millis() - blinkTimer > 250) {
      blinkTimer = millis();
      blinkCount--;
    }
  }

  bool showBlank = (blinkCount % 2 != 0);

  if (showBlank) {
    sevseg.blank();
  } else {
    if (workflow == WORKFLOW_STUDY && pomState != POM_STOPPED) {
      int mins = remainingSecs / 60;
      int secs = remainingSecs % 60;
      sevseg.setNumber(mins * 100 + secs, 2);
    } else {
      sevseg.setNumber(pcTime, 2);
    }
  }
  sevseg.refreshDisplay();
}

// --- Icons ---
void drawIconPlayPause(int x, int y) {
  display.fillTriangle(x + 2, y + 4, x + 2, y + 28, x + 14, y + 16, WHITE);
  display.fillRect(x + 18, y + 6, 4, 20, WHITE);
  display.fillRect(x + 26, y + 6, 4, 20, WHITE);
}

void drawIconNext(int x, int y) {
  display.fillTriangle(x + 2, y + 6, x + 2, y + 26, x + 14, y + 16, WHITE);
  display.fillTriangle(x + 14, y + 6, x + 14, y + 26, x + 26, y + 16, WHITE);
  display.fillRect(x + 26, y + 6, 3, 20, WHITE);
}

void drawIconPrev(int x, int y) {
  display.fillRect(x + 3, y + 6, 3, 20, WHITE);
  display.fillTriangle(x + 18, y + 6, x + 18, y + 26, x + 6, y + 16, WHITE);
  display.fillTriangle(x + 30, y + 6, x + 30, y + 26, x + 18, y + 16, WHITE);
}

void drawIconSpotify(int x, int y) {
  display.fillCircle(x + 16, y + 16, 14, WHITE);
  display.drawLine(x + 8, y + 10, x + 24, y + 10, BLACK);
  display.drawLine(x + 9, y + 11, x + 23, y + 11, BLACK);
  display.drawLine(x + 10, y + 16, x + 22, y + 16, BLACK);
  display.drawLine(x + 11, y + 17, x + 21, y + 17, BLACK);
  display.drawLine(x + 12, y + 22, x + 20, y + 22, BLACK);
}

void drawIconDebug(int x, int y) {
  display.fillCircle(x + 16, y + 18, 8, WHITE);
  display.fillCircle(x + 16, y + 8, 5, WHITE);
  display.drawLine(x + 16, y + 18, x + 4, y + 12, WHITE);
  display.drawLine(x + 16, y + 18, x + 28, y + 12, WHITE);
  display.drawLine(x + 16, y + 20, x + 2, y + 20, WHITE);
  display.drawLine(x + 16, y + 20, x + 30, y + 20, WHITE);
  display.drawLine(x + 16, y + 22, x + 6, y + 28, WHITE);
  display.drawLine(x + 16, y + 22, x + 26, y + 28, WHITE);
}

void drawIconTimer(int x, int y) {
  display.drawCircle(x + 16, y + 16, 14, WHITE);
  display.drawCircle(x + 16, y + 16, 15, WHITE);
  display.drawLine(x + 16, y + 16, x + 16, y + 6, WHITE);
  display.drawLine(x + 16, y + 16, x + 24, y + 16, WHITE);
}

void drawIconSkipFwd(int x, int y) {
  display.drawCircle(x + 16, y + 16, 14, WHITE);
  display.fillTriangle(x + 26, y + 12, x + 26, y + 20, x + 32, y + 16, WHITE);
  display.setCursor(x + 9, y + 13);
  display.setTextSize(1);
  display.setTextColor(BLACK, WHITE);
  display.print("10");
  display.setTextColor(WHITE);
}

void drawIconSkipBwd(int x, int y) {
  display.drawCircle(x + 16, y + 16, 14, WHITE);
  display.fillTriangle(x + 6, y + 12, x + 6, y + 20, x + 0, y + 16, WHITE);
  display.setCursor(x + 11, y + 13);
  display.setTextSize(1);
  display.setTextColor(BLACK, WHITE);
  display.print("10");
  display.setTextColor(WHITE);
}

void drawIconCoding(int x, int y) {
  display.setTextSize(3);
  display.setCursor(x - 2, y + 6);
  display.print("<>");
}

void drawIconStudy(int x, int y) {
  display.fillRect(x + 4, y + 8, 10, 16, WHITE);
  display.fillRect(x + 18, y + 8, 10, 16, WHITE);
  display.drawLine(x + 14, y + 6, x + 14, y + 26, WHITE);
}

void drawIconMovie(int x, int y) {
  display.fillRect(x + 4, y + 4, 24, 24, WHITE);
  for (int i = 6; i < 26; i += 6) {
    display.fillRect(x + 6, y + i, 4, 3, BLACK);
    display.fillRect(x + 22, y + i, 4, 3, BLACK);
  }
}

void drawIcon(IconType icon, int x, int y) {
  switch (icon) {
  case ICON_SPOTIFY:
    drawIconSpotify(x, y);
    break;
  case ICON_PLAYPAUSE:
    drawIconPlayPause(x, y);
    break;
  case ICON_DEBUG:
    drawIconDebug(x, y);
    break;
  case ICON_PREV:
    drawIconPrev(x, y);
    break;
  case ICON_NEXT:
    drawIconNext(x, y);
    break;
  case ICON_TIMER:
    drawIconTimer(x, y);
    break;
  case ICON_SKIPFWD:
    drawIconSkipFwd(x, y);
    break;
  case ICON_SKIPBWD:
    drawIconSkipBwd(x, y);
    break;
  case ICON_MODE_CODING:
    drawIconCoding(x, y);
    break;
  case ICON_MODE_STUDY:
    drawIconStudy(x, y);
    break;
  case ICON_MODE_MOVIES:
    drawIconMovie(x, y);
    break;
  default:
    break;
  }
}

void updateOLED() {
  display.clearDisplay();
  display.setTextColor(WHITE);

  if (actionUiActive && currentActionIcon != ICON_NONE) {
    // --- ZONE: ACTION (Pop-up UI) ---
    drawIcon(currentActionIcon, 48, 8); // Center 32x32 icon

    // Center label below icon
    int16_t x1, y1;
    uint16_t w, h;
    display.setTextSize(1);
    display.getTextBounds(actionMsg, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, 48);
    display.print(actionMsg);

  } else {
    // --- ZONE: IDLE MODE ---
    IconType modeIcon = (workflow == WORKFLOW_CODING)  ? ICON_MODE_CODING
                        : (workflow == WORKFLOW_STUDY) ? ICON_MODE_STUDY
                                                       : ICON_MODE_MOVIES;

    drawIcon(modeIcon, 0, 8);

    display.setCursor(44, 8);
    display.setTextSize(2);
    if (workflow == WORKFLOW_CODING) {
      display.println("CODING");
      display.setTextSize(1);
      display.setCursor(44, 26);
      display.println("Ready.");
    } else if (workflow == WORKFLOW_STUDY) {
      display.println("STUDY");
      display.setTextSize(1);
      display.setCursor(44, 26);
      if (pomState == POM_WORK)
        display.print("WORK");
      else if (pomState == POM_BREAK)
        display.print("BREAK");
      else
        display.print("IDLE");

      if (pomState != POM_STOPPED) {
        display.setCursor(0, 44);
        display.setTextSize(2);
        int mins = remainingSecs / 60;
        int secs = remainingSecs % 60;
        if (mins < 10)
          display.print("0");
        display.print(mins);
        display.print(":");
        if (secs < 10)
          display.print("0");
        display.print(secs);
        if (isPaused) {
          display.setTextSize(1);
          display.print(" PAUSED");
        }
      }
    } else if (workflow == WORKFLOW_MOVIES) {
      display.println("MOVIES");
      display.setTextSize(1);
      display.setCursor(44, 26);
      display.println("Enjoy.");
    }

    // --- ZONE: LEGEND ---
    display.drawFastHLine(0, 54, 128, WHITE);
    display.setTextSize(1);
    display.setCursor(0, 56);
    if (workflow == WORKFLOW_CODING) {
      display.print("SPO | PLAY | DBG");
    } else if (workflow == WORKFLOW_STUDY) {
      display.print("SPO | PLAY | POM");
    } else if (workflow == WORKFLOW_MOVIES) {
      display.print("FWD | PLAY | BWD");
    }
  }

  display.display();
}
