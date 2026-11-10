#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <RTClib.h>

// RC522 pins
#define SS_PIN   5
#define RST_PIN  4
#define SCK_PIN  18
#define MOSI_PIN 17
#define MISO_PIN 16

// OLED I2C pins
#define OLED_SDA 9
#define OLED_SCL 8
#define OLED_ADDR 0x3C

// DHT22 pin
#define DHT_PIN 7
#define DHT_TYPE DHT22

// Servo pin
#define SERVO_PIN 6

// Authorized RFID UID
const String AUTHORIZED_UID = "EB 92 19 4B";

// Servo positions
const int SERVO_INITIAL_ANGLE = 0;
const int SERVO_ACTIVE_ANGLE  = 90;

// Objects
MFRC522 rfid(SS_PIN, RST_PIN);
Adafruit_SH1106G display(128, 64, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);
Servo servoMotor;

// DS3231 RTC
RTC_DS3231 rtc;

// Timing
unsigned long lastDHTRead = 0;
unsigned long cardDisplayTime = 0;

const unsigned long DHT_INTERVAL = 2000;
const unsigned long CARD_DISPLAY_DURATION = 3000;

// States
bool showingCard = false;
bool servoActivated = false;
bool rtcOK = false;

// Sensor values
float temperature = 0;
float humidity = 0;
bool dhtOK = false;


// --------------------------------
// OLED SENSOR DATA
// --------------------------------
void showSensorData() {

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("ENVIRONMENT MONITOR");

  display.drawLine(0, 12, 127, 12, SH110X_WHITE);

  // Temperature
  display.setCursor(0, 20);
  display.print("Temp: ");

  if (dhtOK) {
    display.print(temperature, 1);
    display.println(" C");
  } else {
    display.println("Error");
  }

  // Humidity
  display.setCursor(0, 32);
  display.print("Humidity: ");

  if (dhtOK) {
    display.print(humidity, 1);
    display.println(" %");
  } else {
    display.println("--");
  }

  // RTC
  if (rtcOK) {

    DateTime now = rtc.now();

    display.setCursor(0, 44);

    if (now.day() < 10) {
      display.print("0");
    }

    display.print(now.day());
    display.print("/");

    if (now.month() < 10) {
      display.print("0");
    }

    display.print(now.month());
    display.print("/");

    display.print(now.year());

    display.setCursor(75, 44);

    if (now.hour() < 10) {
      display.print("0");
    }

    display.print(now.hour());
    display.print(":");

    if (now.minute() < 10) {
      display.print("0");
    }

    display.print(now.minute());
  }

  display.setCursor(0, 56);
  display.println("Tap RFID card");

  display.display();
}


// --------------------------------
// OLED RFID CARD SCREEN
// --------------------------------
void showCardUID(String uid, bool authorized) {

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  display.setTextSize(1);

  display.setCursor(0, 5);
  display.println("RFID CARD DETECTED");

  display.drawLine(0, 18, 127, 18, SH110X_WHITE);

  display.setCursor(0, 27);
  display.print("UID: ");
  display.println(uid);

  display.setCursor(0, 45);

  if (authorized) {

    if (servoActivated) {
      display.println("Servo: OPEN");
    } else {
      display.println("Servo: CLOSED");
    }

  } else {

    display.println("Access denied");
  }

  display.display();
}


// --------------------------------
// READ RFID UID
// --------------------------------
String readCardUID() {

  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++) {

    if (i > 0) {
      uid += " ";
    }

    if (rfid.uid.uidByte[i] < 0x10) {
      uid += "0";
    }

    String hexByte =
      String(rfid.uid.uidByte[i], HEX);

    hexByte.toUpperCase();

    uid += hexByte;
  }

  return uid;
}


// --------------------------------
// SETUP
// --------------------------------
void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("==============================");
  Serial.println("MEDICINE CONTAINER SYSTEM");
  Serial.println("==============================");


  // --------------------------------
  // I2C
  // --------------------------------

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );


  // --------------------------------
  // OLED
  // --------------------------------

  if (!display.begin(
        OLED_ADDR,
        true)) {

    Serial.println(
      "OLED not detected!"
    );

    while (true) {
      delay(100);
    }
  }


  display.clearDisplay();

  display.setTextColor(
    SH110X_WHITE
  );

  display.setCursor(0, 10);

  display.println(
    "Starting system..."
  );

  display.display();


  // --------------------------------
  // DHT22
  // --------------------------------

  dht.begin();


  // --------------------------------
  // SERVO
  // --------------------------------

  ESP32PWM::allocateTimer(0);

  servoMotor.setPeriodHertz(50);

  servoMotor.attach(
    SERVO_PIN,
    500,
    2400
  );

  servoMotor.write(
    SERVO_INITIAL_ANGLE
  );

  servoActivated = false;


  // --------------------------------
  // DS3231 RTC
  // --------------------------------

  Serial.println(
    "Initializing DS3231..."
  );

  if (!rtc.begin()) {

    Serial.println(
      "DS3231 not detected!"
    );

    rtcOK = false;

  } else {

    rtcOK = true;

    Serial.println(
      "DS3231 detected!"
    );


    // Check whether RTC lost power
    if (rtc.lostPower()) {

      Serial.println(
        "RTC lost power!"
      );

      Serial.println(
        "Setting RTC to compile time..."
      );

      // IMPORTANT:
      // This sets the RTC to the computer's
      // compile time.
      rtc.adjust(
        DateTime(
          F(__DATE__),
          F(__TIME__)
        )
      );
    }

    DateTime now = rtc.now();

    Serial.print("RTC Date: ");

    Serial.print(now.day());
    Serial.print("/");
    Serial.print(now.month());
    Serial.print("/");
    Serial.println(now.year());

    Serial.print("RTC Time: ");

    Serial.print(now.hour());
    Serial.print(":");
    Serial.print(now.minute());
    Serial.print(":");
    Serial.println(now.second());
  }


  // --------------------------------
  // RFID
  // --------------------------------

  SPI.begin(
    SCK_PIN,
    MISO_PIN,
    MOSI_PIN,
    SS_PIN
  );

  rfid.PCD_Init();

  delay(100);


  byte version =
    rfid.PCD_ReadRegister(
      MFRC522::VersionReg
    );


  Serial.print(
    "RC522 Version: 0x"
  );

  Serial.println(
    version,
    HEX
  );


  if (
    version == 0x00 ||
    version == 0xFF
  ) {

    display.clearDisplay();

    display.setCursor(0, 10);

    display.println(
      "RC522 ERROR"
    );

    display.println(
      "Check wiring"
    );

    display.display();

    Serial.println(
      "Check RC522 wiring!"
    );

    while (true) {
      delay(100);
    }
  }


  Serial.println(
    "System ready!"
  );

  Serial.println(
    "Tap your RFID card..."
  );

  Serial.print(
    "Authorized UID: "
  );

  Serial.println(
    AUTHORIZED_UID
  );


  // --------------------------------
  // INITIAL DHT READING
  // --------------------------------

  temperature =
    dht.readTemperature();

  humidity =
    dht.readHumidity();

  dhtOK =
    !isnan(temperature) &&
    !isnan(humidity);

  lastDHTRead =
    millis();


  if (dhtOK) {

    Serial.print(
      "Temperature: "
    );

    Serial.print(
      temperature
    );

    Serial.println(" C");

    Serial.print(
      "Humidity: "
    );

    Serial.print(
      humidity
    );

    Serial.println(" %");

  } else {

    Serial.println(
      "DHT22 reading failed!"
    );
  }


  showSensorData();
}


// --------------------------------
// MAIN LOOP
// --------------------------------
void loop() {

  unsigned long now =
    millis();


  // --------------------------------
  // DHT22
  // --------------------------------

  if (
    now - lastDHTRead >=
    DHT_INTERVAL
  ) {

    lastDHTRead = now;

    float t =
      dht.readTemperature();

    float h =
      dht.readHumidity();


    if (
      !isnan(t) &&
      !isnan(h)
    ) {

      temperature = t;

      humidity = h;

      dhtOK = true;


      Serial.print(
        "Temperature: "
      );

      Serial.print(
        temperature,
        1
      );

      Serial.print(
        " C | Humidity: "
      );

      Serial.print(
        humidity,
        1
      );

      Serial.println(" %");

    } else {

      Serial.println(
        "DHT22 reading failed!"
      );
    }


    if (!showingCard) {

      showSensorData();
    }
  }


  // --------------------------------
  // RFID
  // --------------------------------

  if (
    rfid.PICC_IsNewCardPresent() &&
    rfid.PICC_ReadCardSerial()
  ) {

    String uid =
      readCardUID();


    Serial.print(
      "Card UID: "
    );

    Serial.println(
      uid
    );


    bool authorized =
      (uid == AUTHORIZED_UID);


    if (authorized) {

      if (!servoActivated) {

        servoMotor.write(
          SERVO_ACTIVE_ANGLE
        );

        servoActivated = true;

        Serial.println(
          "Authorized! Servo moved to 90 degrees."
        );

      } else {

        servoMotor.write(
          SERVO_INITIAL_ANGLE
        );

        servoActivated = false;

        Serial.println(
          "Authorized! Servo returned to 0 degrees."
        );
      }

    } else {

      Serial.println(
        "Unauthorized card. Servo unchanged."
      );
    }


    showCardUID(
      uid,
      authorized
    );


    showingCard = true;

    cardDisplayTime =
      millis();


    rfid.PICC_HaltA();

    rfid.PCD_StopCrypto1();
  }


  // --------------------------------
  // RETURN TO SENSOR SCREEN
  // --------------------------------

  if (
    showingCard &&
    millis() - cardDisplayTime >=
    CARD_DISPLAY_DURATION
  ) {

    showingCard = false;

    showSensorData();
  }


  delay(20);
}