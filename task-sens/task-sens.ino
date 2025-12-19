/*
  Igor Zhukov (c)
  Created:       01-09-2025
  Last changed:  17-09-2025
*/
#include <SD.h>
#include "util.h"

//---------------------------------------------------------------------------
#define MAIN 1
#include "main.h"

#define CS_PIN 53

//---------------------------------------------------------------------------
void APP::configRead() {
  int result;
  for (int i = 0; i < 3; i++) {
    result = SD.begin(CS_PIN);
    if (result)
      break;
    delay(10);
  }
  trace((result) ? F("SD init-OK!") : F("SD init-failed!"));
  if (!result) {
    return;
  }
  File textFile = SD.open(F("conf.txt"));
  if (textFile) {
    String strbuf;
    char c;
    while (textFile.available()) {
      c = textFile.read();
      //strbuf += String(c);
      strbuf += c;
    }
    textFile.close();
    //trace("config:" + strbuf);
    sensorLoad(strbuf);
  } else {
    trace(F("Error opening conf.txt!"));
  }
}

//---------------------------------------------------------------------------
short mem;
void setup() {
  mem = checkMemoryFree();
  RTC.begin();
  //RTC.adjust(DateTime(__DATE__, __TIME__));
  trace("1.MEM=" + String(mem) + " " + String(__DATE__) + " " + String(__TIME__));

  Serial.begin(115200);  // инициализируем порт
  pinMode(48, INPUT_PULLUP);
  pinMode(49, INPUT_PULLUP);

  app.configRead();
  mem = checkMemoryFree();
  trace("2.MEM=" + String(mem));
}

void loop() {
  if (digitalRead(48) == LOW) {
    delay(500);
    taskInit(F("[\
  {\"id\":6,\"w\":0,\"d\":10,\"p\":2,\"pin\":[{\"p\":25,\"v\":[1,0],\"s\":1,\"f\":1},{\"p\":40,\"v\":[1,0],\"s\":1,\"f\":1},{\"p\":41,\"v\":[1,0],\"s\":1},{\"p\":41,\"v\":[0,0],\"f\":1}]}\
  ]"));
  }
  if (digitalRead(49) == LOW) {
    delay(500);
    taskInit("[{\"id\":6,\"w\":1}]");
  }
  taskProcessing();
  sensorProcessing();
  short m = checkMemoryFree();
  if ( mem != m) {
    mem = m;
    trace("3.MEM=" + String(mem));
  }
}
