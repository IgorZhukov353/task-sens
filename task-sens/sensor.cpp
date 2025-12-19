/*
  Igor Zhukov (c)
  Created:       01-09-2025
  Last changed:  01-09-2025
*/
#define ARDUINOJSON_ENABLE_PROGMEM 1
#include <ArduinoJson.h>

#include <OneWire.h>
#include <dht.h>
#include <DallasTemperature.h>

#include "sensor.h"
#include "main.h"
#include "util.h"

enum _amount {TEMPER_MAX = 7, LED_MAX = 1, PIN_MAX = 10};
#define ALL_MAX (TEMPER_MAX+LED_MAX+PIN_MAX)

//----------------------------
TempSensor _temper[TEMPER_MAX];
LED led_array[LED_MAX];
PIN pin_array[PIN_MAX];
Sensor * sens_array[ALL_MAX];
SensorArray sa;
LED *sysledptr;

//---------------------------------------------------------------------------
void TempSensor::init(byte _id, _Type _type, byte _ppin, JsonObject root) {
  Sensor::init(_id, _type, _ppin, root);
  if (_type & _dallas) {
    t1.oneWire.begin(_ppin);
    t1.t.setOneWire(&t1.oneWire);
    t1.t.begin();
  }
}
int TempSensor::check() {
  int8_t h;
  if (type & _dallas) {
    t1.t.requestTemperaturesByIndex(0);  // Send the command to get temperatures
    value = round(t1.t.getTempCByIndex(0));
    h = 0;
  }
  else {
    t2.t.read22(pin);
    value = round(t2.t.temperature);
    h = t2.humValue = round(t2.t.humidity);
  }
  if (preValue != value) {
    app.addTempHum2Buffer(id, value, h);

    preValue = value;
    trace_begin(F("temp id="));
    trace_i(id);
    trace_s(F(" val="));
    trace_i(value);
    trace_end();
    return 1;
  }
  return 0;
}

//---------------------------------------------------------------------------
void LED::init(byte _id, _Type _type, byte _ppin, JsonObject root) {
  Sensor::init(_id, _led, _ppin, root);
  pinMode(pin, OUTPUT);
  timeout = root[F("tout")];
  if (!timeout) timeout = 1000;
  origTimeout = timeout;
  short v = root[F("sysled")];
  if (v)    sysledptr = this;
}
int LED::actionRunFunc() {
  value = !value;         // если светодиод не горит, то зажигаем, и наоборот
  digitalWrite(pin, value);  // устанавливаем состояния выхода, чтобы включить или выключить светодиод
}

//---------------------------------------------------------------------------
void PIN::init(byte _id, _Type _type, byte _ppin, JsonObject root) {
  Sensor::init(_id, _pin, _ppin, root);
  timeout = 500;  // уровень сигнала должен держаться 0,5 сек
  sysledoff = root[F("sysledoff")];
  normval = root[F("v")];
  preValue = value = normval;
  byte pullup = root[F("pup")]; // если подтяжка по питанию
  if (pullup) pinMode(pin, INPUT_PULLUP);
}
int PIN::check() {
  uint32_t currentMillis = millis();
  byte v = digitalRead(pin);
  if (v != preValue) {
    preValue = v;
    lastActivated = currentMillis;
    trace_begin(F("1.pin id="));
    trace_i(id);
    trace_s(F(" val="));
    trace_i(v);
    trace_end();
    return 0;
  }
  if (value != preValue && lastActivated > 0 && (currentMillis - lastActivated) > timeout) {
    value = preValue;
    lastActivated = 0;
    app.addSens2Buffer(id, value);

    trace_begin(F("2.pin id="));
    trace_i(id);
    trace_s(F(" val="));
    trace_i(value);
    trace_end();

    if (!sysledoff && sysledptr) {
      sysledptr->timeout = (value == normval) ? sysledptr->origTimeout : 200;
    }
    return 1;
  }
  return 0;
}

//---------------------------------------------------------------------------
void SensorArray::init(JsonDocument root) {
  timeout = root[F("tout")];
  if (!timeout) timeout = 1000L; // * 60 * 2; // 2 мин
  //trace("timeout="+ String(timeout));
}
//----------------------------
int SensorArray::actionRunFunc() { // проверка датчиков по кругу через timeout у которых в типе есть _group
  for (; groupCurPtr < len; groupCurPtr++) {
    if (sens_array[groupCurPtr]->type & _group) {
      sens_array[groupCurPtr]->check();
      break;
    }
  }
  if (++groupCurPtr >= len)
    groupCurPtr = 0;
}
//----------------------------
SensorArray::check(byte sensorMask = 0, byte actMask = 0) {
  run();
  for (int i = 0; i < len; i++) {
    if (!(sens_array[i]->type & sensorMask))
      sens_array[i]->check();
  }
}
//----------------------------
SensorArray::load(String json) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    trace_begin(F("Ошибка десериализации: "));
    trace_s(error.f_str());
    trace_end();
    return -1;
  }

  len = temper_ptr = led_ptr = pin_ptr = 0;
  JsonArray arr = doc[F("sens")];
  init(doc);

  for (int i = 0; i < arr.size(); i++) {
    JsonObject root = arr[i];
    byte notUsed = root[F("notused")];
    if (notUsed == 1)
      continue;
    short id = root[F("id")];
    short type = root[F("t")];
    short pin = root[F("p")];
    short group = root[F("g")];
    if (!id || !type || !pin) { //
      trace(F("Не определен \"id\" или \"t\" датчика!"));
      continue;
    }
    switch (type) {
      case _dht:
      case _dallas:
        sens_array[len] = &_temper[temper_ptr++];
        break;
      case _led:
        sens_array[len] = &led_array[led_ptr++];
        break;
      case _pin:
      case _pir:
        if (pin_ptr < PIN_MAX) {
          sens_array[len] = &pin_array[pin_ptr++];
        }
        else {
          trace(F("Много датчиков PIN!"));
          continue;
        }
        break;
      default: continue;
    }
    type += group * _group;
    sens_array[len++]->init(id, type, pin, root);
  }
  trace_begin(F("Sens="));
  trace_i(len);
  trace_end();
}

//----------------------------
int sensorLoad(String json) {
  //trace("TempSensorSize=" + String(sizeof(TempSensor)));
  return sa.load(json);
}
int sensorProcessing() {
  return sa.check(_group);
}
short getSensorValue(byte id) {
  for (int i = 0; i < sa.len; i++) {
    if (sens_array[i]->id == id)
      return sens_array[i]->value;
  }
  return -1;
}