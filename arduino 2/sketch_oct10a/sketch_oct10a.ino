#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const uint8_t ADR_LCD = 0x20;

const uint8_t PIN_LED_VERTE  = 9;
const uint8_t PIN_LED_ORANGE = 10;
const uint8_t PIN_LED_ROUGE  = 11;
const uint8_t PIN_MOTEUR     = 12;
const uint8_t PIN_BUZZER     = 7;   // Le buzzer est maintenant piloté par la Carte 2

const float SEUIL_TEMP = 40.0;
const float SEUIL_VIB  = 0.15;

enum Etat { NORMAL, VIBRATION, CRITIQUE };
LiquidCrystal_I2C lcd(ADR_LCD, 16, 2);

void appliquerSorties(Etat e) {
  digitalWrite(PIN_LED_VERTE,  e == NORMAL);
  digitalWrite(PIN_LED_ORANGE, e == VIBRATION);
  digitalWrite(PIN_LED_ROUGE,  e == CRITIQUE);
  digitalWrite(PIN_MOTEUR,     e == CRITIQUE); // Moteur actif uniquement en CRITIQUE
  
  // Le buzzer sonne dès qu'il y a une alerte (Vibration ou Critique)
  digitalWrite(PIN_BUZZER,     e != NORMAL);
}

const char* nomEtat(Etat e) {
  switch (e) {
    case NORMAL:    return "NORMAL";
    case VIBRATION: return "VIBRATION";
    default:        return "CRITIQUE";
  }
}

void setup() {
  Serial.begin(9600); // Réception UART depuis la Carte 1

  pinMode(PIN_LED_VERTE,  OUTPUT);
  pinMode(PIN_LED_ORANGE, OUTPUT);
  pinMode(PIN_LED_ROUGE,  OUTPUT);
  pinMode(PIN_MOTEUR,     OUTPUT);
  pinMode(PIN_BUZZER,     OUTPUT);
  
  appliquerSorties(NORMAL);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Supervision");
  lcd.setCursor(0, 1);
  lcd.print("En attente...");
  delay(1500);
  lcd.clear();
}

void loop() {
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    
    int indexT = data.indexOf("T:");
    int indexV = data.indexOf(";V:");
    
    if (indexT != -1 && indexV != -1) {
      float temp = data.substring(indexT + 2, indexV).toFloat();
      float vib = data.substring(indexV + 3).toFloat();

      if (temp <= -99.0 || vib < 0.0) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("ERREUR CAPTEUR");
        appliquerSorties(NORMAL);
        digitalWrite(PIN_LED_VERTE, LOW);
        digitalWrite(PIN_LED_ROUGE, HIGH);
        digitalWrite(PIN_BUZZER, HIGH); // Alerte buzzer en cas d'erreur capteur
        return;
      }

      Etat etat;
      if (vib > SEUIL_VIB && temp > SEUIL_TEMP) etat = CRITIQUE;
      else if (vib > SEUIL_VIB)                 etat = VIBRATION;
      else                                      etat = NORMAL;

      // Pilotage global des actionneurs sur la Carte 2
      appliquerSorties(etat);

      // Affichage LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("T:");
      lcd.print(temp, 1);
      lcd.print("C V:");
      lcd.print(vib, 2);
      
      lcd.setCursor(0, 1);
      lcd.print("Etat:");
      lcd.print(nomEtat(etat));
    }
  }
}