/*
  Igor Zhukov (c)
  Created:       01-09-2025
  Last changed:  19-12-2025
*/

#include <SD.h>
#include <RTClib.h>
#include "util.h"

//---------------------------------------------------------------------------
#define MAIN 1
#include "main.h"

#define CS_PIN 53
short mem;

//---------------------------------------------------------------------------
void APP::configRead()
{
  int result;
  JsonDocument doc;
  {
    String strbuf;
    for (int i = 0; i < 3; i++)
    {
      result = SD.begin(CS_PIN);
      if (result)
        break;
      delay(10);
    }
    trace((result) ? F("SD init-OK!") : F("SD init-failed!"));
    if (result)
    {
      File textFile = SD.open(F("conf.txt"));
      if (textFile)
      {
        char c;
        while (textFile.available())
        {
          c = textFile.read();
          strbuf += c;
        }
        textFile.close();
      }
      else
      {
        trace(F("Error opening conf.txt!"));
      }
    }

    strbuf = F("{\"wifi\":{\"sid\":\"Keenetic-7832\",\"pass\":\"tgbvgy789\",\"host\":\"igorzhukov353.h1n.ru\",\"ip\":\"81.90.182.128\"},\"ping\":[9,21,22,31],\"sens\":[{\"id\":1,\"t\":1,\"p\":2,\"g\":1,\"n\":\"t дом\"},{\"id\":2,\"t\":1,\"p\":3,\"g\":1,\"n\":\"t подпол\"},{\"id\":3,\"t\":1,\"p\":4,\"g\":1,\"n\":\"t ул\"},{\"id\":4,\"t\":2,\"p\":12,\"g\":1,\"n\":\"t дрен кол\"},{\"id\":5,\"t\":2,\"p\":32,\"g\":1,\"n\":\"t септ\"},{\"id\":6,\"t\":2,\"p\":34,\"g\":1,\"n\":\"t 2эт\"},{\"id\":7,\"t\":2,\"g\":1,\"p\":36,\"n\":\"t гар\"},{\"id\":8,\"t\":16,\"p\":6,\"v\":0,\"n\":\"pir1 дв1\"},{\"id\":9,\"t\":16,\"p\":5,\"v\":0,\"n\":\"pir2 гар\"},{\"id\":10,\"t\":4,\"p\":7,\"v\":1,\"sysledoff\":0,\"pup\":1,\"n\":\"дв1\"},{\"id\":11,\"t\":4,\"p\":8,\"v\":1,\"sysledoff\":0,\"pup\":1,\"n\":\"дв2\"},{\"id\":12,\"t\":4,\"p\":33,\"v\":0,\"sysledoff\":1,\"n\":\"~220V\"},{\"id\":13,\"t\":16,\"p\":9,\"v\":0,\"n\":\"pir2 кух\",\"notused\":1},{\"id\":14,\"t\":4,\"p\":28,\"v\":0,\"sysledoff\":1,\"n\":\"ур дрен кол\"},{\"id\":15,\"t\":16,\"p\":35,\"v\":0,\"n\":\"pir4 2эт\"},{\"id\":16,\"t\":64,\"p\":13,\"tout\":1000,\"sysled\":1,\"n\":\"sysled\"},{\"id\":17,\"t\":4,\"p\":42,\"v\":1,\"sysledoff\":1,\"pup\":1,\"n\":\"ур бочк полив\"},{\"id\":18,\"t\":32,\"n\":\"ping\"}]}");
    DeserializationError error = deserializeJson(doc, strbuf);
    if (error)
    {
      trace_begin(F("Ошибка десериализации: "));
      trace_s(error.f_str());
      trace_end();
      return;
    }
  }
  JsonArray arr = doc[F("ping")];
  int maxind = (arr.size() > MAX_PING)? MAX_PING: arr.size();
  for (int i = 0; i < maxind; i++) {
    check_tcp_last_byte[i] = doc[F("ping")][i];
    trace_begin(F("ping ip: "));
    trace_i(check_tcp_last_byte[i]);
    trace_end();
  }
  sensorLoad(doc);
}

//---------------------------------------------------------------------------
void setup()
{
  mem = checkMemoryFree();
  RTC.begin();

  trace("1.MEM=" + String(mem) + " " + String(__DATE__) + " " + String(__TIME__));

  pinMode(48, INPUT_PULLUP);
  pinMode(49, INPUT_PULLUP);

  app.setup();
  app.configRead();

  mem = checkMemoryFree();
  trace("2.MEM=" + String(mem));
}

void loop()
{
  if (digitalRead(48) == LOW)
  {
    delay(500);
    taskInit(F("[\
  {\"id\":6,\"w\":0,\"d\":10,\"p\":2,\"pin\":[{\"p\":25,\"v\":[1,0],\"s\":1,\"f\":1},{\"p\":40,\"v\":[1,0],\"s\":1,\"f\":1},{\"p\":41,\"v\":[1,0],\"s\":1},{\"p\":41,\"v\":[0,0],\"f\":1}]}\
  ]"));
  }
  if (digitalRead(49) == LOW)
  {
    delay(500);
    taskInit("[{\"id\":6,\"w\":1}]");
  }
  taskProcessing();
  sensorProcessing();
  short m = checkMemoryFree();
  if (mem != m)
  {
    mem = m;
    trace("3.MEM=" + String(mem));
  }
}
