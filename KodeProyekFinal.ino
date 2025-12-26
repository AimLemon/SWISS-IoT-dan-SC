/*************************************************************
 PROYEK: Sistem Pemilah Sampah (PIN SERVO 1 -> GPIO 16)
 HARDWARE: ESP32 + 5x HC-SR04 + 4x Servo + L298N (Tanpa ENA)
 *************************************************************/

// ==========================================
// 1. KONFIGURASI BLYNK & WIFI
// ==========================================
#define BLYNK_TEMPLATE_ID "TMPL6PsOdclVO"      // Ganti dengan ID Anda
#define BLYNK_TEMPLATE_NAME "ESP HCSR04 SERVO MOTOR DC" // Ganti dengan Nama Anda
#define BLYNK_AUTH_TOKEN "OGKd_wU3-rhB8hPl16-Vc1Z7Y1UnJTRf"    // Ganti dengan Token Anda

#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <time.h>
#include <ESP32Servo.h>
#include <BlynkSimpleEsp32.h>


#define WIFI_SSID "POCO M4 Pro"
#define WIFI_PASSWORD "Yuk1n427"

// ==========================================
// 2. KONFIGURASI FIREBASE
// ==========================================
#define API_KEY "AIzaSyCAlIs1IOTTqbs9zDWTaSb6UTP-0QVIptI"
#define DATABASE_URL "https://swiss-cc4fe-default-rtdb.firebaseio.com/"

// ==========================================
// 3. KONFIGURASI PIN
// ==========================================

// --- SENSOR ULTRASONIK ---
struct TempatSampah {
  int id;
  int trigPin;
  int echoPin;
  bool aktif;
  bool statusTerakhir; 
};

TempatSampah bins[5] = {
  {1, 19, 21, true, false},
  {2, 22, 23, true, false},
  {3, 32, 33, true, false},
  {4, 25, 26, true, false},
  {5, 13, 18, false, false}, 
};
float BATAS_PENUH = 7.5; 

// --- SERVO ---
Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;

// UPDATE: Menggunakan GPIO 16 untuk Servo 1
const int PIN_SERVO_1 = 16; // Arah 135 (PIN BARU)
const int PIN_SERVO_2 = 14; // Arah 135
const int PIN_SERVO_3 = 27; // Arah 45
const int PIN_SERVO_4 = 5;  // Arah 45

const int POSISI_AWAL = 90;

// --- MOTOR DC (L298N) ---
// ENA di-jumper fisik (selalu 5V/Full Speed)
const int IN1_PIN = 2;
const int IN2_PIN = 4;

// --- FIREBASE & WAKTU ---
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
bool signupOK = false;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 8 * 3600; 
const int   daylightOffset_sec = 0; 

// ==========================================
// FUNGSI BANTUAN
// ==========================================

float getJarak(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long duration = pulseIn(echo, HIGH);
  return duration * 0.034 / 2;
}

// FUNGSI PENGENDALI MOTOR (Mode Free-Running Stop / Coast)
void kontrolKonveyor(bool jalan) {
  if (jalan) {
    // Motor Bergerak (Maju)
    digitalWrite(IN1_PIN, HIGH);
    digitalWrite(IN2_PIN, LOW);
    Serial.println("Konveyor JALAN (IN1:H, IN2:L)");
  } else {
    // Motor Berhenti: FREE-RUNNING STOP (Memutus Daya)
    digitalWrite(IN1_PIN, LOW); 
    digitalWrite(IN2_PIN, LOW);
    Serial.println("Konveyor STOP (IN1:L, IN2:L) -> Power Cut");
  }
}

void gerakkanServo(Servo &s, int targetSudut) {
  Serial.print("Servo bergerak ke ");
  Serial.println(targetSudut);
  s.write(targetSudut);
  delay(6000); // Blocking delay 6 detik
  s.write(POSISI_AWAL);
  Serial.println("Servo kembali ke 90");
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  // 1. Setup Sensor
  for (int i = 0; i < 5; i++) {
    if (bins[i].aktif) {
      pinMode(bins[i].trigPin, OUTPUT);
      pinMode(bins[i].echoPin, INPUT);
    }
  }

  // 2. Setup Motor DC
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  
  // Default awal: Motor Jalan
  kontrolKonveyor(true); 
  Serial.println("Motor Konveyor Dinyalakan.");

  // 3. Setup Servo
  // Pastikan attach ke PIN_SERVO_1 yang baru (16)
  servo1.setPeriodHertz(50); servo1.attach(PIN_SERVO_1); servo1.write(POSISI_AWAL);
  servo2.setPeriodHertz(50); servo2.attach(PIN_SERVO_2); servo2.write(POSISI_AWAL);
  servo3.setPeriodHertz(50); servo3.attach(PIN_SERVO_3); servo3.write(POSISI_AWAL);
  servo4.setPeriodHertz(50); servo4.attach(PIN_SERVO_4); servo4.write(POSISI_AWAL);

  // 4. Koneksi
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // 5. Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase OK!");
    signupOK = true;
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

// ==========================================
// BLYNK CALLBACKS
// ==========================================
BLYNK_WRITE(V1) { if (param.asInt() == 1) gerakkanServo(servo1, 145); }
BLYNK_WRITE(V2) { if (param.asInt() == 1) gerakkanServo(servo2, 145); }
BLYNK_WRITE(V3) { if (param.asInt() == 1) gerakkanServo(servo3, 45); }
BLYNK_WRITE(V4) { if (param.asInt() == 1) gerakkanServo(servo4, 45); }

// ==========================================
// LOOP UTAMA
// ==========================================
void loop() {
  Blynk.run(); 

  if (Firebase.ready() && signupOK) {
    
    int jumlahTongKosong = 0; // Reset penghitung setiap loop

    for (int i = 0; i < 5; i++) {
      if (bins[i].aktif) {
        
        float jarak = getJarak(bins[i].trigPin, bins[i].echoPin);
        
        // --- LOGIKA STATUS ---
        bool statusSekarang = false; // Default KOSONG (False)
        
        // Cek Penuh/Kosong
        if (jarak > 0 && jarak < 400) { 
          if (jarak > 2.0 && jarak < BATAS_PENUH) {
            statusSekarang = true; // PENUH (True)
          }
        }

        // Hitung Tong Kosong
        if (statusSekarang == false) {
          jumlahTongKosong++;
        }

        // --- KIRIM KE FIREBASE JIKA BERUBAH ---
        if (statusSekarang != bins[i].statusTerakhir) {
          String basePath = "/tempat_sampah/bin_" + String(bins[i].id);
          
          if (Firebase.RTDB.setBool(&fbdo, basePath + "/status", statusSekarang)) {
            if (statusSekarang == true) { 
              time_t now; struct tm timeinfo; time(&now); localtime_r(&now, &timeinfo);
              char timeString[20]; strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
              Firebase.RTDB.setString(&fbdo, basePath + "/id_sampah", timeString);
            } else { 
              Firebase.RTDB.setString(&fbdo, basePath + "/id_sampah", "0"); 
            }
            bins[i].statusTerakhir = statusSekarang;
          }
        }
      }
    }

    // ==========================================
    // LOGIKA PENGHENTIAN MOTOR KONVEYOR
    // ==========================================
    // Jika hanya tersisa 1 tong kosong, motor berhenti.
    
    if (jumlahTongKosong == 1) {
       // Matikan Motor (IN1=LOW, IN2=LOW)
       kontrolKonveyor(false);
    } else {
       // Nyalakan Motor (IN1=HIGH, IN2=LOW)
       kontrolKonveyor(true);
    }

  }
  
  delay(500); 
}