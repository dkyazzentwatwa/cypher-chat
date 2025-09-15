#include "Config.h"
#ifdef ROLE_SERVER
#include "Server.h"
#else
#include "Client.h"
#endif

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);
unsigned long lastButtonPressMillis = 0;
const int DEBOUNCE_DELAY = 200;
const int BUTTON_PINS[] = {32, 33, 25, 26};
const int NUM_BUTTONS = sizeof(BUTTON_PINS) / sizeof(int);
const char* BUTTON_LABELS[] = {"ACK", "ENROUTE", "NEED HELP", "ALL GOOD"};

void beep() {
#if BUZZER_PIN != -1
  digitalWrite(BUZZER_PIN, HIGH);
  delay(20);
  digitalWrite(BUZZER_PIN, LOW);
#endif
}

void oledPrint(const char* line1, const char* line2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(line1);
  if (line2 && strlen(line2) > 0) {
    display.setCursor(0, 10);
    display.println(line2);
  }
  display.display();
}

void setup() {
  Serial.begin(115200);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;) ;
  }
  display.clearDisplay();
  display.display();

  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  }

#if BUZZER_PIN != -1
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#endif

#ifdef ROLE_SERVER
  setupServer();
#else
  setupClient();
#endif
}

void loop() {
#ifdef ROLE_SERVER
  loopServer();
#else
  clientLoop();
#endif
  delay(10);
}
