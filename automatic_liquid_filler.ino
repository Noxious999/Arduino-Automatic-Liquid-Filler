/*
 * AUTOMATIC LIQUID FILLING MACHINE
 * Project Akhir Matakuliah Sensor dan Tranduser
 * Universitas Negeri Malang - D4 TRSE - Semester 3 (Desember 2022)
 * Oleh: Demssi Mulia (210532516406) & Demas Hadi P. (210532516412)
 * Pembimbing: Ibu Siti Sendari
 */

/*--------------------------- Inisialisasi Keypad 4x4 ---------------------------*/
#include <Keypad.h>

const byte ROWS = 4;  //4 baris
const byte COLS = 4;  //4 kolom
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', 'S', 'D' }  //S = Simpan (#), D = Hapus Digit Terakhir
};

// Sesuaikan pin ini berdasarkan wiring aktual Anda
byte rowPins[ROWS] = { 12, 11, 10, 9 };  // Pin Arduino untuk baris keypad
byte colPins[COLS] = { 8, 7, 6, 5 };     // Pin Arduino untuk kolom keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

char key; // Variabel untuk menyimpan tombol yang ditekan

#define maksBit 4 // Maksimal digit input volume
char userInput[maksBit]; // Array karakter untuk menampung input pengguna
byte hitungData = 0; // Indeks untuk array userInput
int userInputInt = 0; // Menyimpan nilai volume target dalam integer
/*--------------------------- Akhir Keypad 4x4 ---------------------------*/

/*--------------------------- Inisialisasi LCD I2C ---------------------------*/
#include <LiquidCrystal_I2C.h>

// Alamat I2C bisa berbeda (0x27 atau 0x3F), sesuaikan jika perlu
LiquidCrystal_I2C lcd(0x27, 16, 2);  // SDA: A4, SCL:A5.
/*--------------------------- Akhir LCD I2C ---------------------------*/

/*--------------------------- Inisialisasi Sensor Water Flow ---------------------------*/
// Pin untuk sensor aliran air (biasanya di pin interrupt seperti D2 atau D3)
#define SENSOR_FLOW_PIN 2 // Ganti ke pin 2 (interrupt 0) atau 3 (interrupt 1)

// Kalibrasi: Sesuaikan angka ini berdasarkan pengujian
// Angka ini adalah jumlah pulsa per Liter per Menit (dari datasheet atau eksperimen)
// Contoh datasheet: 4.5 pulses/(L/min) -> 1 pulse = (1/4.5) L/min
// Atau hitung pulsa per mililiter
float calibrationFactor = 7.5; // Sesuaikan! Contoh: 7.5 pulses/mL atau nilai lain

volatile byte pulseCount; // Variabel untuk menghitung pulsa (volatile karena digunakan di ISR)

unsigned long totalMillisFilled = 0; // Menyimpan total mililiter yang sudah terisi
unsigned long oldTimeFlow; // Untuk menghitung flow rate (jika diperlukan)
/*--------------------------- Akhir Sensor Water Flow ---------------------------*/

/*---------------------- Inisialisasi Relay Pompa Air ------------------------*/
byte relayPompa = A1;   // Pin Arduino untuk mengontrol relay pompa
#define pompaAktif HIGH  // Ganti ke LOW jika relay Anda aktif LOW
#define pompaMati LOW    // Kebalikan dari pompaAktif
/*---------------------- Akhir Relay Pompa Air ------------------------*/

/*-------------------- Inisialisasi Relay Motor Konveyor ---------------------*/
byte relayKonveyor = A2;  // Pin Arduino untuk mengontrol relay konveyor
#define konvAktif HIGH    // Ganti ke LOW jika relay Anda aktif LOW
#define konvMati LOW      // Kebalikan dari konvAktif
/*-------------------- Akhir Relay Motor Konveyor ---------------------*/

/*--------------------------- Inisialisasi Sensor Infrared 1 (Pengisian) ---------------------------*/
int SensorIR1 = 3;      // Pin Arduino untuk sensor IR di posisi pengisian
#define adaBarang HIGH  // Ganti ke LOW jika sensor Anda aktif LOW
#define tidakAdaBarang LOW // Kebalikan dari adaBarang
/*--------------------------- Akhir Sensor Infrared 1 ---------------------------*/

/*--------------------------- Inisialisasi Sensor Infrared 2 (Ujung Konveyor) ---------------------------*/
int SensorIR2 = 4;      // Pin Arduino untuk sensor IR di ujung konveyor
// Menggunakan definisi 'adaBarang' dan 'tidakAdaBarang' yang sama
/*--------------------------- Akhir Sensor Infrared 2 ---------------------------*/

/*--------------------------- Inisialisasi EEPROM ---------------------------*/
#include <EEPROM.h>
#define address 10 // Alamat EEPROM untuk menyimpan nilai volume target
/*--------------------------- Akhir EEPROM ---------------------------*/

// --- Fungsi ISR (Interrupt Service Routine) untuk Sensor Aliran Air ---
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

// --- Fungsi Setup ---
void setup() {
  Serial.begin(9600); // Untuk debugging (opsional)

  // Konfigurasi Pin Mode
  pinMode(relayKonveyor, OUTPUT);
  pinMode(relayPompa, OUTPUT);
  pinMode(SensorIR1, INPUT);
  pinMode(SensorIR2, INPUT);
  pinMode(SENSOR_FLOW_PIN, INPUT_PULLUP); // Gunakan internal pull-up

  // Inisialisasi kondisi awal
  digitalWrite(relayKonveyor, konvMati); // Konveyor mati di awal
  digitalWrite(relayPompa, pompaMati);   // Pompa mati di awal

  // Setup Interrupt untuk Sensor Aliran Air
  pulseCount = 0;
  totalMillisFilled = 0;
  oldTimeFlow = millis();
  // Gunakan RISING edge untuk mendeteksi pulsa
  attachInterrupt(digitalPinToInterrupt(SENSOR_FLOW_PIN), pulseCounter, RISING);

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("AUTOMATIC LIQUID");
  lcd.setCursor(0, 1);
  lcd.print("FILLER MACHINE");
  delay(2000);
  lcd.clear();

  // Baca nilai volume terakhir dari EEPROM
  Baca();
  viewDefault(); // Tampilkan UI default
}

// --- Fungsi Loop Utama ---
void loop() {
  key = keypad.getKey(); // Baca input keypad

  int bacaIR1 = digitalRead(SensorIR1); // Baca sensor IR 1
  int bacaIR2 = digitalRead(SensorIR2); // Baca sensor IR 2

  // Logika Utama Berdasarkan Sensor IR
  if (bacaIR1 == adaBarang) { // Jika ada botol di posisi pengisian
    stopKonv(); // Hentikan konveyor

    // Hanya mulai proses pengisian jika belum dimulai atau sudah selesai
    // (Tambahkan logika state jika perlu untuk mencegah pengisian berulang)

    // Jika ada input keypad, proses inputnya
    if (key) {
      handleKeypadInput(key);
    }

  } else if (bacaIR2 == adaBarang) { // Jika ada botol di ujung konveyor
    stopKonv(); // Hentikan konveyor
    stopIsi();  // Pastikan pompa juga berhenti
  } else { // Jika tidak ada botol di kedua sensor
    startKonv(); // Jalankan konveyor
    stopIsi();   // Pastikan pompa mati
    // Reset tampilan jika perlu saat tidak ada botol
    if (!key) { // Hanya reset jika tidak ada input keypad aktif
       viewDefault();
       lcd.setCursor(9, 0);
       lcd.print(userInputInt); // Tampilkan nilai tersimpan
       lcd.setCursor(9,1);
       lcd.print("0   "); // Reset nilai isi saat ini
    }
  }

  delay(100); // Delay singkat untuk stabilitas
}

// --- Fungsi Pendukung ---

// Menangani Input dari Keypad
void handleKeypadInput(char keyEvent) {
  Serial.println(keyEvent); // Debug

  switch (keyEvent) {
    case 'A': // Preset 100mL
      clearInput();
      strcpy(userInput, "100");
      hitungData = 3;
      updateLcdInput();
      break;
    case 'B': // Preset 250mL
      clearInput();
      strcpy(userInput, "250");
      hitungData = 3;
      updateLcdInput();
      break;
    case 'C': // Preset 500mL
      clearInput();
      strcpy(userInput, "500");
      hitungData = 3;
      updateLcdInput();
      break;
    case 'D': // Hapus digit terakhir (Decrement)
      if (hitungData > 0) {
        hitungData--;
        userInput[hitungData] = '\0'; // Hapus karakter terakhir
        lcd.setCursor(hitungData + 9, 0);
        lcd.print(" "); // Hapus tampilan di LCD
      }
      break;
    case 'S': // Simpan '#'
      if (hitungData > 0) { // Hanya simpan jika ada input
         Simpan(); // Simpan ke EEPROM
         clearInput(); // Kosongkan input setelah disimpan
         // Mulai proses pengisian setelah menyimpan
         startPengisian();
      }
      break;
    case '*': // Tampilkan nilai tersimpan
      clearInput(); // Hapus input saat ini
      lcd.clear();
      Baca(); // Baca dari EEPROM
      lcd.setCursor(0, 0);
      lcd.print("Nilai Tersimpan:");
      lcd.setCursor(7, 1);
      lcd.print(userInputInt);
      lcd.print(" mL ");
      delay(2000);
      lcd.clear();
      viewDefault(); // Kembali ke tampilan default
      lcd.setCursor(9, 0);
      lcd.print(userInputInt); // Tampilkan lagi nilai tersimpan
      break;
    default: // Input angka 0-9
      if (hitungData < maksBit) { // Cek batas digit
        userInput[hitungData] = keyEvent;
        hitungData++;
        userInput[hitungData] = '\0'; // Pastikan null-terminated
        updateLcdInput();
      } else {
        viewMaksBit(); // Tampilkan pesan error jika digit > maksBit
      }
      break;
  }
}

// Memulai Proses Pengisian
void startPengisian() {
  lcd.clear();
  totalMillisFilled = 0; // Reset penghitung volume
  pulseCount = 0;       // Reset penghitung pulsa
  startIsi();           // Nyalakan pompa

  // Loop pengisian hingga target tercapai
  while (totalMillisFilled < userInputInt) {
     if (pulseCount > 0) {
        // Matikan interrupt sementara saat menghitung
        detachInterrupt(digitalPinToInterrupt(SENSOR_FLOW_PIN));

        // Hitung volume berdasarkan pulsa dan faktor kalibrasi
        // Hati-hati: pembagian float bisa lambat di Arduino
        // Mungkin perlu optimasi jika faktor kalibrasi adalah pulsa/mL
        // totalMillisFilled = pulseCount / calibrationFactor; // Jika calibrationFactor = pulses/mL
        // Atau:
         totalMillisFilled += (unsigned long)(pulseCount / calibrationFactor); // Jika calibrationFactor = pulses/mL, konversi ke long

        pulseCount = 0; // Reset pulse count setelah dihitung

        // Nyalakan kembali interrupt
        attachInterrupt(digitalPinToInterrupt(SENSOR_FLOW_PIN), pulseCounter, RISING);
     }

     viewIsi(totalMillisFilled); // Update tampilan LCD
     delay(50); // Delay singkat agar tidak membebani prosesor
  }

  // Setelah target tercapai
  stopIsi(); // Matikan pompa
  delay(100);
  lcd.clear();
  viewIsi(userInputInt); // Tampilkan volume final
  delay(1000);
  startKonv(); // Jalankan konveyor lagi
  delay(500); // Beri jeda sebelum kembali ke loop utama
  lcd.clear();
  viewDefault(); // Kembali ke tampilan awal
  lcd.setCursor(9, 0);
  lcd.print(userInputInt); // Tampilkan nilai tersimpan
}


// Menghapus input pengguna saat ini
int clearInput() {
  for (byte i = 0; i < maksBit; i++) {
    userInput[i] = '\0';
  }
  hitungData = 0;
  // Hapus tampilan input di LCD
  lcd.setCursor(9, 0);
  lcd.print("    "); // Spasi untuk menghapus 4 digit
  return 0;
}

// Menyimpan nilai userInput ke EEPROM
int Simpan() {
  if (hitungData == 0) return 1; // Jangan simpan jika kosong

  userInputInt = atoi(userInput); // Konversi array char ke integer

  // Simpan integer (biasanya 2 byte) ke EEPROM
  EEPROM.put(address, userInputInt);

  // Verifikasi (opsional tapi bagus untuk debugging)
  int readValue;
  EEPROM.get(address, readValue);
  if (readValue == userInputInt) {
     Serial.print("Simpan ke EEPROM berhasil: ");
     Serial.println(userInputInt);
     viewSimpan(); // Tampilkan konfirmasi di LCD
  } else {
     Serial.println("Gagal menyimpan ke EEPROM!");
     lcd.clear();
     lcd.print("Gagal Simpan!");
     delay(1000);
  }
  return 0;
}

// Membaca nilai dari EEPROM
void Baca() {
  EEPROM.get(address, userInputInt);
  // Pastikan nilai awal tidak aneh (misal EEPROM kosong/nilai -1)
  if (userInputInt < 0 || userInputInt > 9999) {
      userInputInt = 0; // Set default jika nilai tidak valid
  }
  Serial.print("Baca dari EEPROM: ");
  Serial.println(userInputInt);
}

// Menampilkan UI Default di LCD
void viewDefault() {
  lcd.setCursor(0, 0);
  lcd.print("MAX ISI:");
  lcd.setCursor(14, 0);
  lcd.print("mL");
  lcd.setCursor(0, 1);
  lcd.print("ISI:");
  lcd.setCursor(14, 1);
  lcd.print("mL");
}

// Menampilkan konfirmasi Tersimpan di LCD
void viewSimpan() {
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("TERSIMPAN!");
  lcd.setCursor(7, 1);
  lcd.print(userInputInt);
  delay(1500);
  lcd.clear();
  viewDefault(); // Kembali ke tampilan default
  lcd.setCursor(9, 0);
  lcd.print(userInputInt); // Tampilkan nilai yang baru disimpan
}

// Menampilkan pesan error jika input melebihi batas digit
void viewMaksBit() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Input Maks 4 dig"); // Pesan disingkat
  delay(1500);
  clearInput(); // Hapus input yang salah
  lcd.clear();
  viewDefault(); // Kembali ke tampilan default
  lcd.setCursor(9, 0);
  lcd.print(userInputInt); // Tampilkan nilai tersimpan
}

// Menampilkan volume target dan volume saat ini di LCD
void viewIsi(unsigned long currentMillis) {
  // Hanya update jika nilai berubah signifikan untuk mengurangi flicker
  static unsigned long lastDisplayedMillis = 99999;
  if (currentMillis != lastDisplayedMillis) {
      lcd.setCursor(9, 0);
      lcd.print(userInputInt);
      lcd.print("    "); // Hapus sisa digit lama
      lcd.setCursor(9, 1);
      lcd.print(currentMillis);
      lcd.print("    "); // Hapus sisa digit lama
      lastDisplayedMillis = currentMillis;
  }
}

// Menampilkan input pengguna saat ini di LCD
void updateLcdInput() {
    lcd.setCursor(9, 0);
    lcd.print(userInput);
    // Beri spasi untuk menghapus sisa digit jika input dihapus
    for(byte i = hitungData; i < maksBit; i++) {
        lcd.print(" ");
    }
}

// Mengaktifkan pompa
void startIsi() {
  digitalWrite(relayPompa, pompaAktif);
}

// Mematikan pompa
void stopIsi() {
  digitalWrite(relayPompa, pompaMati);
}

// Mengaktifkan konveyor
void startKonv() {
  digitalWrite(relayKonveyor, konvAktif);
}

// Mematikan konveyor
void stopKonv() {
  digitalWrite(relayKonveyor, konvMati);
}

//-------------------- Akhir Program ---------------------
