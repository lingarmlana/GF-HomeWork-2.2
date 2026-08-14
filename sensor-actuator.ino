#include <Wire.h> // Обмін данними з дисплеєм
#include <Adafruit_GFX.h> // Малювання на дисплеї
#include <Adafruit_SSD1306.h> // робота з OLED-дисплеями, на базі Adafruit_GFX.h

#include "FS.h" // команди для файлової системи
#include "LittleFS.h" // файлова система

#define SCREEN_WIDTH 128 // Ширина екрана в px
#define SCREEN_HEIGHT 64 // Висота екрана в px

#define I2C_SDA 21 // Display Serial Data (SDA)
#define I2C_SCL 22 // Display Serial Clock (SCL) -тактова частота

// Обʼєкт дисплея
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Sensor Pins
const int activeBuzzerPin = 15;   // D15 - акивний буззер
const int passiveBuzzerPin = 18;  // D18 - пасивний буззер (мелодія)
const int buttonPin = 23;         // D23 - кнопка (INPUT_PULLUP by default)
const int obstaclePin = 27;       // D27 - датчик наближення / перешкод
const int redLedPin = 25;         // D25 - червоний діод
const int greenLedPin = 26;       // D26 - зелений діод
const int vibroPin = 33;          // D33 - вібромотор

// Ноти в герцах для теми Super Mario Bros
#define NOTE_E5  659
#define NOTE_C5  523
#define NOTE_G4  392
#define NOTE_E4  330
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_AS4 466
#define NOTE_F4  349
#define NOTE_D5  587

const int anthemNotes[] = {
  NOTE_E5, NOTE_E5, 0, NOTE_E5, 0, NOTE_C5, NOTE_E5, 0,
  NOTE_G4, 0, NOTE_G4, 
  NOTE_C5, 0, NOTE_G4, 0, NOTE_E4, 0,
  NOTE_A4, NOTE_B4, NOTE_AS4, NOTE_A4, 
  NOTE_G4, NOTE_E5, NOTE_G4, NOTE_A4, 0, NOTE_F4, NOTE_G4, 0, NOTE_E5, 0, NOTE_C5, NOTE_D5, NOTE_B4
};

const int anthemDurations[] = {
  150, 150, 150, 150, 150, 150, 150, 150,
  300, 150, 300,
  300, 150, 150, 150, 300, 150,
  200, 200, 150, 300,
  150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150, 150
};

const int anthemLength = sizeof(anthemNotes) / sizeof(anthemNotes[0]);

// Button and Music
bool isPlaying = false;
bool lastButtonState = HIGH;
unsigned long previousButtonMillis = 0;
const long buttonInterval = 50; // Антидребезг

// Таймер для датчика наближення / перешкод
unsigned long previousObstacleMillis = 0;
const long obstacleInterval = 100;

void setup() {
  pinMode(activeBuzzerPin, OUTPUT);
  pinMode(passiveBuzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(obstaclePin, INPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(vibroPin, OUTPUT);

  Serial.begin(115200); // Стандартний Boud Rate для передачі даних по UART
  // UART (Universal Asynchronous Receiver-Transmitter) - апаратний вузол в мікроконтролері та протокол передачі даних між пристроями по двох проводах TX & RX.
  // UART не потребує окремий дріт тактування, бо задає швидкість обміну даними 

  // Шина для пердачі даних на дисплей
  Wire.begin(I2C_SDA, I2C_SCL);

  // Ініціалізація дисплея за адресою I2C 0x3C
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // налаштування живлення, адреса
    Serial.println(F("Display Failed"));
    for(;;);
  }

  display.clearDisplay();
  display.setTextSize(1);              
  display.setTextColor(SSD1306_WHITE); 
  
  display.setCursor(0, 0);          
  display.println(F("Hello World!"));
  display.display(); // Рендерінг
}

// --- Method 1: Button listener, music toggler, display updater ---
void updateButtonAndDisplay(unsigned long currentMillis) {
  if (currentMillis - previousButtonMillis >= buttonInterval) {
    previousButtonMillis = currentMillis;

    int currentButtonState = digitalRead(buttonPin);

    if (currentButtonState != lastButtonState) {
      lastButtonState = currentButtonState;
      if (currentButtonState == LOW) {
        isPlaying = !isPlaying;
      }
    }

    // Оновлюємо лише другий рядок екрана, не чіпаючи решту
    // Затираэмо попередній текст, заливаючи чорним прямокутник
    // 0, 16 - координати верхнього лівого кута
    // 128 - ширина екрану по горизонталі
    // 8 - висота рядка в px
    display.fillRect(0, 16, 128, 8, SSD1306_BLACK);
    display.setCursor(0, 16);

    if (currentButtonState == LOW) {
      display.print(F("Button pressed"));  
    } else {
      display.print(F("Press the Button"));   
    }
    
    display.display();
  }
}

// --- Method 2: Non-blocking music playback with pause-on-click functionality ---
void playMusicNonBlocking(unsigned long currentMillis) {
  static int noteIndex = 0;
  static unsigned long previousNoteMillis = 0;
  static bool notePlayingState = false;
  static int currentDuration = 0;

  if (!isPlaying) {
    // Якщо музика на паузі — глушимо баззер, але індекс noteIndex НЕ скидаємо!
    noTone(passiveBuzzerPin);
    notePlayingState = false;
    return;                  
  }

  // Якщо флаг play активний, продовжуємо грати мелодію
  if (!notePlayingState) {
    if (noteIndex >= anthemLength) {
      noteIndex = 0; // loop music
    }

    // Додаємо паузи між нотами рівні 30% часу довжини кожної ноти
    currentDuration = anthemDurations[noteIndex];
    unsigned long stepDuration = currentDuration * 1.3;

    if (anthemNotes[noteIndex] > 0) {
      tone(passiveBuzzerPin, anthemNotes[noteIndex]);
    } else {
      noTone(passiveBuzzerPin);
    }

    previousNoteMillis = currentMillis;
    notePlayingState = true;
  } else {
    // Чекаємо завершення звучання поточної ноти
    unsigned long stepDuration = currentDuration * 1.3;
    if (currentMillis - previousNoteMillis >= stepDuration) {
      noTone(passiveBuzzerPin);
      notePlayingState = false;
      noteIndex++; // Переходимо до наступної ноти тільки після завершення попередньої
    }
  }
}

// --- Method 3: Handle obstacle sensor, switch LEDs, and control the vibration motor ---
void updateObstacleAndIndicators(unsigned long currentMillis) {
  if (currentMillis - previousObstacleMillis >= obstacleInterval) {
    previousObstacleMillis = currentMillis;
    int obstacleState = digitalRead(obstaclePin);

    if (obstacleState == LOW) {
      digitalWrite(redLedPin, HIGH);
      digitalWrite(greenLedPin, LOW);
      digitalWrite(vibroPin, HIGH);  
    } else {
      digitalWrite(redLedPin, LOW);
      digitalWrite(greenLedPin, HIGH);
      digitalWrite(vibroPin, LOW);
    }
  }
}

// loop() - безперервно по колу (polling на максимальній швидкості)
void loop() {
  unsigned long currentMillis = millis(); // Єдине джерело часу для неблокуючого коду. millis() - Час від запуску або ребуту плати

  updateButtonAndDisplay(currentMillis);
  playMusicNonBlocking(currentMillis);
  updateObstacleAndIndicators(currentMillis);
}