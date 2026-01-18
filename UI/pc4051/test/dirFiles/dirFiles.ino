#include "SPIFFS.h"

void listSPIFFS() {
  Serial.println("===== SPIFFS info =====");
  Serial.printf("Total: %u bytes\n", SPIFFS.totalBytes());
  Serial.printf("Used : %u bytes\n", SPIFFS.usedBytes());
  Serial.println("=======================");

  Serial.println("===== SPIFFS file list =====");
  File root = SPIFFS.open("/");
  File file = root.openNextFile();

  if (!file) {
    Serial.println("No files found in SPIFFS.");
  }

  while (file) {
    Serial.print("FILE: ");
    Serial.print(file.name());
    Serial.print("  SIZE: ");
    Serial.print(file.size());
    Serial.println(" bytes");
    file = root.openNextFile();
  }
  Serial.println("============================");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!SPIFFS.begin(false)) {
    Serial.println("SPIFFS mount failed");
    while (1);
  }

  Serial.println("SPIFFS mounted OK");
  listSPIFFS();
}

void loop() {}
