/*
  Igor Zhukov (c)
  Created:       01-09-2025
  Last changed:  29-12-2025
*/

#include <OneWire.h>
#include <dht.h>
#include <DallasTemperature.h>
#include <RTClib.h>

#include "main.h"
#include "util.h"
#include "sensor.h"

enum _amount {TEMPER_MAX = 7, LED_MAX = 1, PIN_MAX = 10, ANALOGPIN_MAX = 1, PING_MAX=1};
#define ALL_MAX (TEMPER_MAX+LED_MAX+PIN_MAX+ANALOGPIN_MAX)

//----------------------------
TempSensor _temper[TEMPER_MAX];
LED led_array[LED_MAX];
PIN pin_array[PIN_MAX];
ANALOGPIN analogpin_array[ANALOGPIN_MAX];
IPPING ping_array[PING_MAX];
Sensor * sens_array[ALL_MAX];
SensorArray sa;
LED *sysledptr;

//---------------------------------------------------------------------------
void TempSensor::init(byte _id, _Type _type, byte _ppin, JsonObject &root) {
  Sensor::init(_id, _type, _ppin, root);
  if (_type & _dallas) {
    t1.oneWire.begin(_ppin);
    t1.t.setOneWire(&t1.oneWire);
    t1.t.begin();
  }
}
//----------------------------
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
void LED::init(byte _id, _Type _type, byte _ppin, JsonObject &root) {
  Sensor::init(_id, _led, _ppin, root);
  pinMode(pin, OUTPUT);
  timeout = root[F("tout")];
  if (!timeout) timeout = 1000;
  origTimeout = timeout;
  short v = root[F("sysled")];
  if (v)    sysledptr = this;
}
//----------------------------
int LED::actionRunFunc() {
  value = !value;         // если светодиод не горит, то зажигаем, и наоборот
  digitalWrite(pin, value);  // устанавливаем состояния выхода, чтобы включить или выключить светодиод
}

//---------------------------------------------------------------------------
void PIN::init(byte _id, _Type _type, byte _ppin, JsonObject &root) {
  Sensor::init(_id, _pin, _ppin, root);
  timeout = 500;  // уровень сигнала должен держаться 0,5 сек
  sysledoff = root[F("sysledoff")];
  normval = root[F("v")];
  preValue = value = normval;
  byte pullup = root[F("pup")]; // если подтяжка по питанию
  if (pullup) pinMode(pin, INPUT_PULLUP);
  else pinMode(pin, INPUT);
}
//----------------------------
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
void ANALOGPIN::init(byte _id, _Type _type, byte _ppin, JsonObject &root) {
  Sensor::init(_id, _pin, _ppin, root);
  timeout = root[F("tout")];
  if (!timeout) timeout = 1000 * 60;  // 1 min
  r1 = root[F("r1")];
  r2 = root[F("r2")];
}
//----------------------------
int ANALOGPIN::actionRunFunc() {
  float vout;
  vout = (analogRead(pin) * 4.9) / 1024.0;
  if(r1 && r2)
    vout = vout / (r2 / (r1 + r2));
  if(vout < 0.09) 
    vout = 0;
  value = vout;
  return 0;
}

//---------------------------------------------------------------------------
void IPPING::init(byte _id, _Type _type, byte _ppin, JsonObject &root) {
  Sensor::init(_id, _pin, _ppin, root);
  currIndex = MAX_PING; // не активный режим
  timeout = root[F("tout")];
  type = (_Type)(type +_Type::_group);
  if (!timeout) timeout = 1000* 60 * 60; // 1 hour
}  
//----------------------------
int IPPING::check()
{
  if(currIndex >= MAX_PING){
    DateTime now = RTC.now();
    if (now.minute() <= 1){    // в начале каждого часа начать проверку
//trace("check 1");
      return run();
    }
  }
  else
    return actionRunFunc();   // в процессе проверки по одному IP за раз
}
//----------------------------
int IPPING::actionRunFunc() {
//trace_begin(F("actionRunFunc: 0-"));trace_i(currIndex);
  currIndex = (currIndex >= MAX_PING)? 0: currIndex + 1;
//trace_s(",1=");trace_i(currIndex);  
  if (!app.check_tcp_last_byte[currIndex]){ 
    currIndex = MAX_PING; // проверка закончилась
    //timeout = 1000 * 60 * 60; // 1 hour
  }
  else {  
    String cmd = F("192.168.0.");
    cmd += app.check_tcp_last_byte[currIndex];
    bool result = app.ping(cmd, 5000);
    app.check_tcp_err[currIndex] = (!result)? 1:0;
//trace_s(",2=");trace_i(currIndex);      
//trace_end();
    return 1; // продолжаем проверку
    }
//trace_s(",3=");trace_i(currIndex);      
//trace_end();
  return 0;
}

//---------------------------------------------------------------------------
void SensorArray::init(JsonDocument &doc) {
  timeout = doc[F("tout")];
  if (!timeout) timeout = 1000L; // * 60 * 2; // 2 мин
  //trace("timeout="+ String(timeout));
}
//----------------------------
int SensorArray::actionRunFunc() { // проверка датчиков по кругу через timeout у которых в типе есть _group
  for (; groupCurPtr < len; groupCurPtr++) {
    if (sens_array[groupCurPtr]->type & _group) {
      int result = sens_array[groupCurPtr]->check();
      if(!(sens_array[groupCurPtr]->type & _Type::_ping) || !result)  // если это пинговщик и у него не закончился проход по все адресам, то не меняем индекс
        groupCurPtr++;
      break;
    }
  }
  if (groupCurPtr >= len)
    groupCurPtr = 0;
}
//----------------------------
int SensorArray::check(byte sensorMask = 0, byte actMask = 0) {
  run();
  for (int i = 0; i < len; i++) {
    if (!(sens_array[i]->type & sensorMask))
      sens_array[i]->check();
  }
}
//----------------------------
int SensorArray::load(JsonDocument  &doc) {
  len = temper_ptr = led_ptr = pin_ptr = analogpin_ptr = ping_ptr = 0;
  JsonArray arr = doc[F("sens")];
  init(doc);
  //trace("5.3."+String(arr.size()));

  for (int i = 0; i < arr.size(); i++) {
    JsonObject root = arr[i];
    byte notUsed = root[F("notused")];
    if (notUsed == 1)
      continue;
    short id = root[F("id")];
    short type = root[F("t")];
    short pin = root[F("p")];
    short group = root[F("g")];
    if (!id || !type){ // || !pin) { //
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
      case _analogPin:
        sens_array[len] = &analogpin_array[analogpin_ptr++];
        break;  
      case _ping:
        sens_array[len] = &ping_array[ping_ptr++];
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
    sens_array[len++]->init(id, (_Type)type, pin, root);
  }
  trace_begin(F("Sens="));
  trace_i(len);
  trace_end();
}

//----------------------------
int sensorLoad(JsonDocument &doc) {
  //trace("2.4.MEM=" + String(checkMemoryFree()));
  return sa.load(doc);
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