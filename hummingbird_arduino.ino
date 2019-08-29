//BMP180
#include <Wire.h>
#include <Adafruit_BMP085.h>
//RFM96
#include <SPI.h>
#include <RH_RF95.h>
//SD card
#include <SD.h>

//Constants
#define VBATPIN A7

#define RFM95_CS 12
#define RFM95_RST 11
#define RFM95_INT 10
#define RF95_FREQ 915.0

#define SD_CS 4

#define STATUS_LED 13

Adafruit_BMP085 bmp;

RH_RF95 rf95(RFM95_CS, RFM95_INT);

struct Datapoint {
  float temperature;
  int32_t pressure;
  float battery_voltage;
  uint32_t packet_number;
  uint32_t flight_number;
} datapoint;

uint32_t packet_number = 0;

uint8_t flight_number = 0;
char filename[13];


void setup() {
  //  while (!Serial) { } //magic thing to catch all serial messages

  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);


  Serial.begin(9600);
  Serial.println("Booting........");

  if (!bmp.begin()) {
    error("No BMP sensor found");
  }

  //reset rfm95
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);
  delay(100);
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  if (!rf95.init()) {
    error("LoRa radio init failed");
  }
  if (!rf95.setFrequency(RF95_FREQ)) {
    error("setFrequency failed");
  }
  rf95.setTxPower(23, false);

  if (!SD.begin(SD_CS)) {
    error("Card failed, or not present");
  }

  findFilename(filename);
  Serial.print("writing data to ");
  Serial.println(filename);

  Serial.println("success");
}

void loop() {
  digitalWrite(STATUS_LED, HIGH);
  datapoint = {
    bmp.readTemperature(),
    bmp.readPressure(),
    getBatteryVoltage(),
    packet_number,
    flight_number
  };

  log();
  transmit();
  saveSD();

  packet_number++;
  digitalWrite(STATUS_LED, LOW);

  delay(1000);
}

void log() {
  Serial.print("Flight number = ");
  Serial.println(flight_number);

  Serial.print("Packet Number = ");
  Serial.println(packet_number);

  Serial.print("Temperature = ");
  Serial.print(datapoint.temperature);
  Serial.println(" *C");

  Serial.print("Pressure = ");
  Serial.print(datapoint.pressure);
  Serial.println(" Pa");

  Serial.print("Voltage = " );
  Serial.print(datapoint.battery_voltage);
  Serial.println(" V");
}

void transmit() {
  Serial.println("sending...");

  byte buf[sizeof(Datapoint)] = {0};

  memcpy(buf, &datapoint, sizeof(Datapoint));

  rf95.send((uint8_t *)buf, sizeof(buf));

  delay(10);
  rf95.waitPacketSent();

  Serial.println("sent");
}

float getBatteryVoltage() {
  float measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  return measuredvbat;
}

// SD Card related

void findFilename(char* filename) {
  strcpy(filename, "FLIGHT00.csv");
  for (flight_number = 0; flight_number < 100; flight_number++) {
    uint8_t offset = 6 ;
    if (flight_number < 10) {
      offset = 7;
    }
    itoa(flight_number, filename + offset * sizeof(char), 10);

    filename[8] = '.'; //clear null termination from itoa
    if (!(SD.exists(filename))) {
      break;
    }
  }
}

void saveSD() {
  File dataFile = SD.open(filename, FILE_WRITE);
  if (!dataFile) {
    error("error opening SD card file");
  }

  dataFile.print(datapoint.packet_number);
  dataFile.print(",");
  dataFile.print(datapoint.pressure);
  dataFile.print(",");
  dataFile.print(datapoint.temperature);
  dataFile.print(",");
  dataFile.print(datapoint.battery_voltage);
  dataFile.println();
  dataFile.close();
  Serial.println("done");

}

void error(char* msg) {
  digitalWrite(STATUS_LED, HIGH);
  while (1) {
    Serial.println("error");
    Serial.println(msg);
    delay(10000);
  }
}
