#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// pins for LEDs
const int blueLED = 4;
const int yellowLED = 6;
const int redLED = 8;
const int greenLED = 10;
const int modeLED = 11;

// pins for buttons
const int blueButton = 3;
const int yellowButton = 5;
const int redButton = 7;
const int greenButton = 9;
const int smallButton = 2;

// pin for buzzer
const int buzzerPin = 13;

const int maxSequenceLength = 100;
int sequence[maxSequenceLength];
int currentLevel = 0;
bool hardMode = false;
bool waitForNext = false;
int highestScore = 0;
int hardHighestScore = 0;

enum GameState {
  WAIT_FOR_START,
  GENERATE_SEQUENCE,
  DISPLAY_SEQUENCE,
  HARD_DISPLAY_SEQUENCE,
  WAIT_FOR_BUTTON_PRESS,
  GAME_OVER,
  LEVEL_UP
};

GameState gameState;
unsigned long lastMillis = 0;
int sequenceIndex = 0;
int displayState = 0;
int buttonPressIndex = 0;

unsigned long gameOverMillis = 0;
int gameOverPhase = 0;
unsigned long levelUpMillis = 0;
unsigned long hardModeToggleMillis = 0;
volatile bool hardModeChanged = false;

void smallButtonISR() {
  static unsigned long lastDebounceTime = 0;
  unsigned long debounceDelay = 100;

  if (millis() - lastDebounceTime > debounceDelay) {
    hardModeChanged = true;
    lastDebounceTime = millis();
  }
}

void setup() {
  pinMode(blueLED, OUTPUT);
  pinMode(yellowLED, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(modeLED, OUTPUT);

  pinMode(blueButton, INPUT_PULLUP);
  pinMode(yellowButton, INPUT_PULLUP);
  pinMode(redButton, INPUT_PULLUP);
  pinMode(greenButton, INPUT_PULLUP);
  pinMode(smallButton, INPUT_PULLUP);

  pinMode(buzzerPin, OUTPUT);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Memory Game!");
  lcd.setCursor(0, 1);
  lcd.print("Highest Score ");
  lcd.print(highestScore);

  randomSeed(analogRead(0));
  gameState = WAIT_FOR_START;
  attachInterrupt(digitalPinToInterrupt(smallButton), smallButtonISR, FALLING);
}

void generateSequence() {
  sequence[currentLevel] = random(0, 4);
  sequenceIndex = 0;
  gameState = hardMode ? HARD_DISPLAY_SEQUENCE : DISPLAY_SEQUENCE;
}

void fadeLED(int ledPin, int startBrightness, int endBrightness, int duration) {
  int stepDelay = duration / abs(endBrightness - startBrightness);
  for (int brightness = startBrightness; brightness != endBrightness; brightness += (startBrightness < endBrightness ? 1 : -1)) {
    analogWrite(ledPin, brightness);
    delay(stepDelay);
  }
  analogWrite(ledPin, endBrightness);
}

void displaySequence(bool hard) {
  int leds[] = {blueLED, yellowLED, redLED, greenLED};
  int brightness = hard ? max(255 - 30 * currentLevel, 0) : 255;
  unsigned long currentMillis = millis();
  
  int basePauseDuration = 600;
  int minPauseDuration = 400;
  int pauseDuration = hard ? max(basePauseDuration - 20 * currentLevel, minPauseDuration) : basePauseDuration;

  if (displayState == 0 && currentMillis - lastMillis >= pauseDuration) {
    lastMillis = currentMillis;
    fadeLED(leds[sequence[sequenceIndex]], 0, brightness, 300); // Fade in LED
    tone(buzzerPin, 1000 + (leds[sequence[sequenceIndex]] * 200), 300); // Play tone
    displayState = 1;
  } else if (displayState == 1 && currentMillis - lastMillis >= 300) {
    lastMillis = currentMillis;
    fadeLED(leds[sequence[sequenceIndex]], brightness, 0, 300); // Fade out LED
    sequenceIndex++;
    displayState = 0;
    if (sequenceIndex > currentLevel) {
      gameState = WAIT_FOR_BUTTON_PRESS;
      buttonPressIndex = 0;
    }
  }
}

int checkButtonPress() {
  for (int i = 3; i <= 9; i += 2) {
    if (digitalRead(i) == LOW) {
      digitalWrite(i + 1, HIGH);
      unsigned long debounceStart = millis();
      while (millis() - debounceStart < 30) {
        if (digitalRead(i) == HIGH) {
          tone(buzzerPin, 1000 + ((i + 1) * 200), 30);
          digitalWrite(i + 1, LOW);
          return -1;
        }
      }
      while (digitalRead(i) == LOW);
      debounceStart = millis();
      while (millis() - debounceStart < 30) {
        if (digitalRead(i) == LOW) {
          return -1;
        }
      }
      digitalWrite(i + 1, LOW);
      return i;
    }
  }
  return -1;
}

void gameOver() {
  unsigned long currentMillis = millis();
  if (gameOverPhase == 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Game Over!");
    if (hardMode && hardHighestScore < currentLevel) {
      hardHighestScore = currentLevel;
    } else if (highestScore < currentLevel) {
      highestScore = currentLevel;
    }
    currentLevel = 0;
    for (int i = 4; i <= 10; i += 2) {
      digitalWrite(i, HIGH);
    }
    tone(buzzerPin, 1000, 500);
    gameOverMillis = currentMillis;
    gameOverPhase = 1;
  } else if (gameOverPhase == 1 && currentMillis - gameOverMillis >= 500) {
    for (int i = 4; i <= 10; i += 2) {
      digitalWrite(i, LOW);
    }
    gameOverMillis = currentMillis;
    gameOverPhase = 2;
  } else if (gameOverPhase == 2 && currentMillis - gameOverMillis >= 300) {
    gameOverPhase = 0;
    gameState = WAIT_FOR_START;
    hardMode = false; // Reset hard mode
    digitalWrite(modeLED, LOW); // Turn off mode LED
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Memory Game!");
    lcd.setCursor(0, 1);
    lcd.print("Highest Score ");
    lcd.print(highestScore);
  }
}

void levelUp() {
  currentLevel++;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Level ");
  lcd.print(currentLevel + 1); // Show the next level number
  levelUpMillis = millis();
  waitForNext = true;
}

void loop() {
  switch (gameState) {
    case WAIT_FOR_START:
      if (digitalRead(blueButton) == LOW || digitalRead(yellowButton) == LOW ||
          digitalRead(redButton) == LOW || digitalRead(greenButton) == LOW) {
        gameState = GENERATE_SEQUENCE;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Memory Game!");
        lcd.setCursor(0, 1);
        lcd.print("Highest Score ");
        lcd.print(highestScore);
      } else if (hardModeChanged) {
        if (millis() - hardModeToggleMillis >= 500) {
          hardMode = !hardMode;
          digitalWrite(modeLED, hardMode ? HIGH : LOW);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(hardMode ? "Hard Mode" : "Memory Game");
          lcd.setCursor(0, 1);
          lcd.print("Highest Score ");
          lcd.print(hardMode ? hardHighestScore : highestScore);
        }
        hardModeChanged = false;
      }
      break;

    case GENERATE_SEQUENCE:
      generateSequence();
      break;

    case DISPLAY_SEQUENCE:
      displaySequence(false);
      break;

    case HARD_DISPLAY_SEQUENCE:
      displaySequence(true);
      break;

    case WAIT_FOR_BUTTON_PRESS:
      if (buttonPressIndex <= currentLevel) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Press for LED ");
        lcd.print(buttonPressIndex);
        int buttonPressed = checkButtonPress();
        if (buttonPressed != -1) {
          int expectedButton = 2 * (sequence[buttonPressIndex] + 1) + 1;
          if (buttonPressed != expectedButton) {
            gameState = GAME_OVER;
            gameOverMillis = millis();
            gameOverPhase = 0;
          } else {
            buttonPressIndex++;
          }
        }
      } else {
        levelUpMillis = millis();
        waitForNext = false;
        gameState = LEVEL_UP;
      }
      break;

    case GAME_OVER:
      gameOver();
      break;

    case LEVEL_UP:
      if (waitForNext == false) {
        levelUp();
      } else if (millis() - levelUpMillis >= 1000)
        gameState = GENERATE_SEQUENCE;
      break;
 
  }
}

