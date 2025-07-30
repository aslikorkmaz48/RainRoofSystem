#include <Servo.h>

// Pinler
const int yagmurSensorPin = A0;
const int servoPin = 9;
const int durumLedPin = 13;    // LED pin (dahili LED olabilir)

// Servo pozisyonları
const int servoAcikPozisyon = 0;
const int servoKapaliPozisyon = 90;

// Kalibrasyon değerleri (başlangıçta belirsiz)
int kuruDeger = -1;
int yagmurluDeger = -1;

// Kalibrasyon zamanları (ms)
const unsigned long kalibrasyonSuresi = 10000;  // 10 saniye kalibrasyon
unsigned long kalibrasyonBaslangicZamani;

// Filtreleme için kayan ortalama
const int filtreBoyutu = 10;
int sensorOkumaDizisi[filtreBoyutu];
int filtreIndeksi = 0;

// Durumlar
bool catiKapali = false;
bool kalibrasyonTamamlandi = false;
bool yagmurVar = false;

// Zamanlayıcılar
unsigned long yagmurDurduZamani = 0;
const unsigned long gecikmeSure = 10000;  // 10 saniye yağmur durduktan sonra açma gecikmesi

Servo catiServo;

void setup() {
  Serial.begin(9600);
  catiServo.attach(servoPin);
  pinMode(durumLedPin, OUTPUT);
  digitalWrite(durumLedPin, LOW);

  Serial.println("=== YAĞMUR SENSÖRLÜ ÇATI SİSTEMİ ===");
  Serial.println("Kalibrasyon başlıyor, lütfen 10 saniye boyunca kuru ortamda bekleyin...");

  kalibrasyonBaslangicZamani = millis();

  // Filtre dizisini sıfırla
  for (int i=0; i<filtreBoyutu; i++) {
    sensorOkumaDizisi[i] = 0;
  }

  // Çatı başlangıçta açık
  catiAc();
}

void loop() {
  // Kalibrasyon süreci
  if (!kalibrasyonTamamlandi) {
    kalibrasyonYap();
    return;  // Kalibrasyon bitene kadar döngüyü burada durdur
  }

  // Manuel komutlar kontrolü
  if (Serial.available() > 0) {
    char komut = Serial.read();
    if (komut == 'o' || komut == 'O') {
      Serial.println("Manuel: Çatı açılıyor.");
      catiAc();
    }
    else if (komut == 'k' || komut == 'K') {
      Serial.println("Manuel: Çatı kapanıyor.");
      catiKapat();
    }
  }

  // Sensör değerini oku ve filtrele
  int sensordenOkunan = analogRead(yagmurSensorPin);
  sensorOkumaDizisi[filtreIndeksi] = sensordenOkunan;
  filtreIndeksi = (filtreIndeksi + 1) % filtreBoyutu;

  long toplam = 0;
  for (int i=0; i<filtreBoyutu; i++) toplam += sensorOkumaDizisi[i];
  int ortalamaDeger = toplam / filtreBoyutu;

  Serial.print("Filtreli sensör değeri: ");
  Serial.println(ortalamaDeger);

  // Yağmur kontrolü - eşikleri kalibrasyona göre ayarla
  int yagmurEsiği = (kuruDeger + yagmurluDeger) / 2;

  if (ortalamaDeger > yagmurEsiği) {
    if (!yagmurVar) {
      Serial.println("Yağmur algılandı! Çatı kapanacak.");
      catiKapat();
      yagmurVar = true;
    }
    // Yağmur devam ediyor, zamanlayıcı sıfırla
    yagmurDurduZamani = 0;
  }
  else {
    if (yagmurVar) {
      // Yağmur durdu, zamanlayıcı başlat
      if (yagmurDurduZamani == 0) {
        yagmurDurduZamani = millis();
        Serial.println("Yağmur durdu, çatı açılması için gecikme başladı.");
      }
      else {
        if (millis() - yagmurDurduZamani >= gecikmeSure) {
          Serial.println("Gecikme süresi doldu, çatı açılıyor.");
          catiAc();
          yagmurVar = false;
          yagmurDurduZamani = 0;
        }
      }
    }
  }

  delay(500);  // Okuma hızı
}

// Kalibrasyon fonksiyonu
void kalibrasyonYap() {
  unsigned long gecenSure = millis() - kalibrasyonBaslangicZamani;

  // Kalibrasyonun ilk yarısı kuru ortam, ikinci yarısı yağmurlu ortam için
  if (gecenSure < kalibrasyonSuresi / 2) {
    // Kuru ortam kalibrasyonu
    int sensordeKuru = analogRead(yagmurSensorPin);
    kuruDeger = (kuruDeger == -1) ? sensordeKuru : (kuruDeger + sensordeKuru) / 2;
    Serial.print("Kuru ortam kalibrasyonu: ");
    Serial.println(kuruDeger);
  }
  else if (gecenSure < kalibrasyonSuresi) {
    // Yağmurlu ortam kalibrasyonu (kullanıcıdan yağmur damlaları eklemesini bekliyoruz)
    int sensordeYagmur = analogRead(yagmurSensorPin);
    yagmurluDeger = (yagmurluDeger == -1) ? sensordeYagmur : (yagmurluDeger + sensordeYagmur) / 2;
    Serial.print("Yağmurlu ortam kalibrasyonu: ");
    Serial.println(yagmurluDeger);
  }
  else {
    kalibrasyonTamamlandi = true;
    Serial.println("Kalibrasyon tamamlandı.");
    Serial.print("Kuru ortam değeri: "); Serial.println(kuruDeger);
    Serial.print("Yağmurlu ortam değeri: "); Serial.println(yagmurluDeger);
    Serial.println("Sistem aktif, manuel komut için 'o' (aç), 'k' (kapat) yazabilirsiniz.");
  }
}

// Çatıyı açan fonksiyon, servo yumuşak hareket
void catiAc() {
  if (catiKapali) {
    for (int pos = servoKapaliPozisyon; pos >= servoAcikPozisyon; pos -= 1) {
      catiServo.write(pos);
      delay(15);
    }
    catiKapali = false;
    digitalWrite(durumLedPin, LOW);
    Serial.println("Çatı açık konuma getirildi.");
  }
}

// Çatıyı kapatan fonksiyon, servo yumuşak hareket
void catiKapat() {
  if (!catiKapali) {
    for (int pos = servoAcikPozisyon; pos <= servoKapaliPozisyon; pos += 1) {
      catiServo.write(pos);
      delay(15);
    }
    catiKapali = true;
    digitalWrite(durumLedPin, HIGH);
    Serial.println("Çatı kapalı konuma getirildi.");
  }
}
