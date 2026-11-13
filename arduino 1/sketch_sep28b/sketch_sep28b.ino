#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <math.h>

// Mettre 0 pour desactiver la SD (test, ou si la RAM sature)
#define USE_SD 1

const uint8_t ADR_DS1621 = 0x48;
const uint8_t ADR_MPU    = 0x68;
const uint8_t PIN_SD_CS  = 4;

bool dsOK = false;
bool mpuOK = false;
bool sdOK = false;

// ---------- I2C ----------
bool i2cDisponible(uint8_t adresse) {
  Wire.beginTransmission(adresse);
  return Wire.endTransmission() == 0;
}

// ---------- DS1621 ----------
bool initDS1621() {
  if (!i2cDisponible(ADR_DS1621)) return false;

  Wire.beginTransmission(ADR_DS1621);
  Wire.write(0xAC);          // registre config
  Wire.write(0x00);          // mode continu
  if (Wire.endTransmission() != 0) return false;

  Wire.beginTransmission(ADR_DS1621);
  Wire.write(0xEE);          // start conversion
  if (Wire.endTransmission() != 0) return false;

  delay(800);                // 1ere conversion
  return true;
}

float lireTemperature() {
  Wire.beginTransmission(ADR_DS1621);
  Wire.write(0xAA);          // lire temperature
  if (Wire.endTransmission(false) != 0) return -999.0;   // repeated start
  if (Wire.requestFrom(ADR_DS1621, (uint8_t)2) != 2) return -999.0;

  int8_t msb = (int8_t)Wire.read();
  uint8_t lsb = Wire.read();

  float t = msb;             // partie entiere (complement a 2)
  if (lsb & 0x80) t += 0.5;  // +0.5 aussi pour les negatifs
  return t;
}

// ---------- MPU6050 ----------
bool initMPU() {
  if (!i2cDisponible(ADR_MPU)) return false;
  Wire.beginTransmission(ADR_MPU);
  Wire.write(0x6B);          // PWR_MGMT_1
  Wire.write(0x00);          // sortie de veille
  return Wire.endTransmission() == 0;
}

bool lireAccel(float &ax, float &ay, float &az) {
  Wire.beginTransmission(ADR_MPU);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(ADR_MPU, (uint8_t)6) != 6) return false;

  int16_t rx = (Wire.read() << 8) | Wire.read();
  int16_t ry = (Wire.read() << 8) | Wire.read();
  int16_t rz = (Wire.read() << 8) | Wire.read();

  ax = rx / 16384.0;
  ay = ry / 16384.0;
  az = rz / 16384.0;
  return true;
}

float lireVibration() {
  const uint8_t N = 15;
  float normes[N];
  float somme = 0;

  for (uint8_t i = 0; i < N; i++) {
    float ax, ay, az;
    if (!lireAccel(ax, ay, az)) return -1.0;
    normes[i] = sqrt(ax * ax + ay * ay + az * az);
    somme += normes[i];
    delay(5);
  }

  float moyenne = somme / N;
  float ecart = 0;
  for (uint8_t i = 0; i < N; i++) ecart += fabs(normes[i] - moyenne);
  return ecart / N;
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(9600);
  while (!Serial) { }
  delay(1500);
  Serial.println(F("\nSYSTEME DE SURVEILLANCE"));

  Wire.begin();
  Wire.setClock(100000);
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(25000, true);   // evite le blocage du bus
#endif

  Serial.println(F("[I2C] Test DS1621..."));
  dsOK = initDS1621();
  Serial.println(dsOK ? F("DS1621 OK") : F("DS1621 ABSENT"));

  Serial.println(F("[I2C] Test MPU6050..."));
  mpuOK = initMPU();
  Serial.println(mpuOK ? F("MPU6050 OK") : F("MPU6050 ABSENT"));

#if USE_SD
  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);
  Serial.println(F("[SD] Initialisation..."));
  sdOK = SD.begin(PIN_SD_CS);
  Serial.println(sdOK ? F("SD OK") : F("SD indisponible"));
#else
  Serial.println(F("[SD] desactivee"));
#endif

  Serial.println(F("Debut de la boucle."));
}

// ---------- LOOP ----------
void loop() {
  Serial.println(F("\n--- MESURES ---"));

  float temp = dsOK ? lireTemperature() : -999.0;
  float vib  = mpuOK ? lireVibration() : -1.0;

  Serial.print(F("Temperature : "));
  if (temp == -999.0) Serial.println(F("ERREUR"));
  else { Serial.print(temp, 1); Serial.println(F(" C")); }

  Serial.print(F("Vibration : "));
  if (vib < 0) Serial.println(F("ERREUR"));
  else Serial.println(vib, 4);

#if USE_SD
  if (sdOK) {
    File f = SD.open("log.txt", FILE_WRITE);
    if (f) {
      f.print(F("T="));
      if (temp == -999.0) f.print(F("ERROR")); else f.print(temp, 1);
      f.print(F("; V="));
      if (vib < 0) f.println(F("ERROR")); else f.println(vib, 4);
      f.close();
      Serial.println(F("[SD] Enregistrement OK"));
    } else {
      Serial.println(F("[SD] Erreur fichier"));
    }
  }
#endif

  delay(1000);
}
