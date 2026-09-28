#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>


// ── Pin Definitions ──────────────────────────────────────────────
#define BTN_DOT        4
#define BTN_DASH       5
#define BTN_END_LETTER 6
#define BTN_BACK       7
#define BTN_SPACE      8
#define BTN_SEND       9

#define BUZZER_PIN     10
#define IR_TX_PIN      3
#define IR_RX_PIN      11

// ── Tone Frequencies ─────────────────────────────────────────────
#define TONE_DOT         1000
#define TONE_DASH         700
#define TONE_END_LETTER  1400
#define TONE_BACK         500
#define TONE_SEND        1800
#define TONE_SPACE       1200
#define TONE_ERROR        250
#define TONE_RECEIVE     1500
#define TONE_DURATION      80

// ── Modes ────────────────────────────────────────────────────────
#define MODE_RECEIVE 0
#define MODE_SEND    1
int currentMode = MODE_RECEIVE;

// ── LCD ──────────────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ── Morse Table ──────────────────────────────────────────────────
struct MorseEntry {
  char letter;
  const char* code;
};

const MorseEntry morseTable[] = {
  {'A', ".-"},   {'B', "-..."}, {'C', "-.-."}, {'D', "-.."},
  {'E', "."},    {'F', "..-."}, {'G', "--."},  {'H', "...."},
  {'I', ".."},   {'J', ".---"}, {'K', "-.-"},  {'L', ".-.."},
  {'M', "--"},   {'N', "-."},   {'O', "---"},  {'P', ".--."},
  {'Q', "--.-"}, {'R', ".-."},  {'S', "..."},  {'T', "-"},
  {'U', "..-"},  {'V', "...-"}, {'W', ".--"},  {'X', "-..-"},
  {'Y', "-.--"}, {'Z', "--.."},
  {'0', "-----"},{'1', ".----"},{'2', "..---"},{'3', "...--"},
  {'4', "....-"},{'5', "....."},{'6', "-...."},{'7', "--..."},
  {'8', "---.."},{'9', "----."}
};
const int MORSE_TABLE_SIZE = sizeof(morseTable) / sizeof(MorseEntry);

// ── State ────────────────────────────────────────────────────────
String currentMorse    = "";
String composedMessage = "";
String receivedMessage = "";

// chat history
const int CHAT_HISTORY_SIZE = 6;
String chatLog[CHAT_HISTORY_SIZE];

// ── Buttons ──────────────────────────────────────────────────────
const int buttonPins[6] = {
  BTN_DOT, BTN_DASH, BTN_END_LETTER, BTN_BACK, BTN_SPACE, BTN_SEND
};

bool lastStableState[6] = {LOW, LOW, LOW, LOW, LOW, LOW};
unsigned long lastChangeTime[6] = {0, 0, 0, 0, 0, 0};
bool pressHandled[6] = {false, false, false, false, false, false};

const unsigned long DEBOUNCE_MS = 35;

// ── Audio Helpers ────────────────────────────────────────────────
void playToneSimple(int freq, int duration) {
  tone(BUZZER_PIN, freq, duration);
}

void playError() {
  tone(BUZZER_PIN, TONE_ERROR, 120);
}

void playReceiveChime() {
  tone(BUZZER_PIN, TONE_RECEIVE, 100);
}

// ── Morse Decode ─────────────────────────────────────────────────
char decodeMorse(const String& code) {
  for (int i = 0; i < MORSE_TABLE_SIZE; i++) {
    if (code == morseTable[i].code) {
      return morseTable[i].letter;
    }
  }
  return '\0';
}

// ── Chat Log ─────────────────────────────────────────────────────
void addToLog(const String& msg) {
  for (int i = CHAT_HISTORY_SIZE - 1; i > 0; i--) {
    chatLog[i] = chatLog[i - 1];
  }
  chatLog[0] = msg;
}

// ── LCD Update ───────────────────────────────────────────────────
void updateLCD() {
  lcd.clear();

  if (currentMode == MODE_SEND) {
    lcd.setCursor(0, 0);
    String row0 = "TX:";
    row0 += (currentMorse.length() == 0) ? "---" : currentMorse;
    if (row0.length() > 16) row0 = row0.substring(row0.length() - 16);
    lcd.print(row0);

    lcd.setCursor(0, 1);
    String row1 = composedMessage;
    if (row1.length() > 16) row1 = row1.substring(row1.length() - 16);
    lcd.print(row1);
  } else {
    lcd.setCursor(0, 0);
    String row0 = (chatLog[0].length() == 0) ? "Listening..." : chatLog[0];
    if (row0.length() > 16) row0 = row0.substring(row0.length() - 16);
    lcd.print(row0);

    lcd.setCursor(0, 1);
    String row1 = (chatLog[1].length() == 0) ? "" : chatLog[1];
    if (row1.length() > 16) row1 = row1.substring(row1.length() - 16);
    lcd.print(row1);
  }
}

// ── IR Send ──────────────────────────────────────────────────────
void sendMessageIR(const String& msg) {
  const uint16_t address = 0x00;

  for (int i = 0; i < msg.length(); i++) {
    uint8_t command = (uint8_t)msg[i];
    IrSender.sendNEC(address, command, 0);
    delay(120);
  }

  // newline as end marker
  IrSender.sendNEC(address, '\n', 0);
  delay(150);

  // re-arm receiver after sending
  IrReceiver.begin(IR_RX_PIN, DISABLE_LED_FEEDBACK);
}

// ── IR Receive ───────────────────────────────────────────────────
void checkReceive() {
  if (currentMode != MODE_RECEIVE) return;

  if (IrReceiver.decode()) {
    uint8_t cmd = IrReceiver.decodedIRData.command;
    IrReceiver.resume();

    if (cmd == '\n') {
      if (receivedMessage.length() > 0) {
        playReceiveChime();
        addToLog("<<" + receivedMessage);
        receivedMessage = "";
        currentMode = MODE_RECEIVE;
        updateLCD();
      }
      return;
    }

    if (cmd >= 32 && cmd <= 126) {
      receivedMessage += (char)cmd;
      updateLCD();
    }
  }
}

// ── Button Action Handler ────────────────────────────────────────
void handleButton(int index) {
  if (currentMode == MODE_RECEIVE) {
    currentMode = MODE_SEND;
    currentMorse = "";
    composedMessage = "";
    updateLCD();
  }

  switch (index) {
    case 0: // DOT
      currentMorse += ".";
      playToneSimple(TONE_DOT, TONE_DURATION);
      updateLCD();
      break;

    case 1: // DASH
      currentMorse += "-";
      playToneSimple(TONE_DASH, TONE_DURATION);
      updateLCD();
      break;

    case 2: // END LETTER
      if (currentMorse.length() == 0) {
        playError();
        break;
      } else {
        char decoded = decodeMorse(currentMorse);
        if (decoded == '\0') {
          playError();
          currentMorse = "";
          updateLCD();
        } else {
          composedMessage += decoded;
          currentMorse = "";
          playToneSimple(TONE_END_LETTER, TONE_DURATION);
          updateLCD();
        }
      }
      break;

    case 3: // BACKSPACE
      playToneSimple(TONE_BACK, TONE_DURATION);
      if (currentMorse.length() > 0) {
        currentMorse.remove(currentMorse.length() - 1);
      } else if (composedMessage.length() > 0) {
        composedMessage.remove(composedMessage.length() - 1);
      }
      updateLCD();
      break;

    case 4: // SPACE
      if (currentMorse.length() > 0) {
        char decoded = decodeMorse(currentMorse);
        if (decoded != '\0') {
          composedMessage += decoded;
        }
        currentMorse = "";
      }
      composedMessage += ' ';
      playToneSimple(TONE_SPACE, TONE_DURATION);
      updateLCD();
      break;

    case 5: // SEND
      if (composedMessage.length() == 0) {
        playError();
        break;
      }

      playToneSimple(TONE_SEND, TONE_DURATION * 2);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(">> SENDING:");
      lcd.setCursor(0, 1);
      {
        String display = composedMessage;
        if (display.length() > 16) display = display.substring(display.length() - 16);
        lcd.print(display);
      }

      sendMessageIR(composedMessage);
      addToLog(">>" + composedMessage);

      composedMessage = "";
      currentMorse = "";
      currentMode = MODE_RECEIVE;
      updateLCD();
      break;
  }
}

// ── Button Scan ──────────────────────────────────────────────────
void checkButtons() {
  for (int i = 0; i < 6; i++) {
    bool reading = digitalRead(buttonPins[i]);

    if (reading != lastStableState[i]) {
      lastChangeTime[i] = millis();
      lastStableState[i] = reading;
    }

    if ((millis() - lastChangeTime[i]) > DEBOUNCE_MS) {
      if (reading == HIGH) {
        if (!pressHandled[i]) {
          handleButton(i);
          pressHandled[i] = true;
        }
      } else {
        pressHandled[i] = false;
      }
    }
  }
}

// ── Setup ────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  for (int i = 0; i < 6; i++) {
    pinMode(buttonPins[i], INPUT); 
  }

  pinMode(BUZZER_PIN, OUTPUT);

  for (int i = 0; i < CHAT_HISTORY_SIZE; i++) {
    chatLog[i] = "";
  }

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Morse Comm HW");
  lcd.setCursor(0, 1);
  lcd.print("Listening...");

  IrSender.begin(IR_TX_PIN);
  IrReceiver.begin(IR_RX_PIN, DISABLE_LED_FEEDBACK);

  delay(1000);
  updateLCD();
}

// ── Loop ─────────────────────────────────────────────────────────
void loop() {
  checkButtons();
  checkReceive();
}