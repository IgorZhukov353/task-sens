//#define ARDUINOJSON_ENABLE_PROGMEM 1
//#include <ArduinoJson.h>
#include <RTClib.h>

#include "util.h"
#include "main.h"
#include "task.h"

//---------------------------------------------------------------------------
#define MAX_PIN  4
#define MAX_TASK 2
//---------------------------------------------------------------------------
uint16_t getSensorValue(byte id);

//--------------------------------------------------------------------------------------------------------
void Task::init(byte _id, short ppin, byte _onValue, uint32_t _duration)
{
  DateTime now = RTC.now();
  id = _id;
  startTime = now.secondstime();
  duration = _duration;
  checkPeriod = 1;
  pin[0].pin = ppin;
  pin[0].onValue  = _onValue;
  pin[0].offValue = !_onValue;
  pin[0].startFirstStep_finishLastStep = 1;
  nextActivationTime = startTime;
  tmpWorkTime = startTime;
  //  trace_begin(F("Задача \""));  trace_i(id);  trace_s(F("\" стартовала."));  trace_end();
  //  putInfo();

}
//--------------------------------------------------------------------------------------------------------
bool Task::init(byte _id, uint32_t _duration, JsonObject & doc) {
  id = _id;
  duration = _duration;
  checkPeriod = doc[F("p")];
  if (!checkPeriod)
    checkPeriod = 1;
  sens.id = doc[F("sen")][F("id")]; // 6
  sens.targetValue = doc[F("sen")][F("tv")]; // 1
  sens.comp = doc[F("sen")][F("c")]; // 1

  auto arr = doc[F("pin")].as<JsonArray>();
  for (int i = 0; i < arr.size(); i++ ) {
    JsonObject pin_item = arr[i];
    pin[i].pin = pin_item[F("p")]; // 7, 8, 8
    pin[i].onValue  = pin_item[F("v")][0]; // 1, 1, 1
    pin[i].offValue = pin_item[F("v")][1]; // 0, 0, 0
    pin[i].atStart = pin_item[F("s")]; // 0, 1, 1
    pin[i].atFinish = pin_item[F("f")]; // 0, 0, 0
    pin[i].atBetween = pin_item[F("b")]; // 0, 0, 0
    pin[i].startFirstStep_finishLastStep = pin_item[F("fl")]; // 1, 0, 0
    pin[i].sensorCheck = pin_item[F("c")]; // 0, 0, 0

    trace_begin(F("init:pin="));
    trace_i(pin[i].pin);
    trace_s(F(";onVal="));
    trace_i(pin[i].onValue);
    trace_s(F(";offVal="));
    trace_i(pin[i].offValue);
    trace_s(F(";s="));
    trace_i(pin[i].atStart);
    trace_s(F(";f="));
    trace_i(pin[i].atFinish);
    trace_s(F(";b="));
    trace_i(pin[i].atBetween);
    trace_s(F(";fl="));
    trace_i(pin[i].startFirstStep_finishLastStep);
    trace_end();
  }
  DateTime now = RTC.now();
  startTime = now.secondstime();
  nextActivationTime = startTime;
  tmpWorkTime = startTime;

  //  trace_begin(F("Задача \""));  trace_i(id);  trace_s(F("\" стартовала."));  trace_end();
  //  putInfo();
  return true;
}
//--------------------------------------------------------------------------------------------------------
void Task::pinProcessing(byte mask)
{
  for (int i = 0; i < MAX_PIN; i++) {
    if (pin[i].pin == 0)
      break;
    if (pin[i].options & mask) {
      pinMode(pin[i].pin, OUTPUT);
      byte val = (curMode) ? pin[i].onValue : pin[i].offValue;
      //trace("pin=" + String(pin[i].pin) + " val=" + String(val));
      digitalWrite(pin[i].pin, val);
    }
  }
}
//--------------------------------------------------------------------------------------------------------
void Task::processing()
{
  uint32_t now = RTC.now().secondstime();
  States preState = state;
  byte preCurMode = curMode;

  if ((startTime + duration) <= now) {  // закончено время работы задания
    state = States::_stopped;
  }
  else if ((startTime + checkPeriod) >= now) {
    if (state != States::_firstStep) {
      state = States::_firstStep;
      trace_begin(F("Задача \""));  trace_i(id);  trace_s(F("\" стартовала."));  trace_end();
      putInfo(now);
    }
  }
  else if ((startTime + duration - checkPeriod) <= now) {
    if (state != States::_lastStep) {
      state = States::_lastStep;
    }
  }
  else {
    state = States::_nextStep;
  }

  //trace("0 preState=" + String(preState) + " state=" + String(state) + " now=" + String(now) + " mode=" + String(startTime + duration));
  if (preState != state) {
    PinOptions s;
    if (preState == States::_nextStep || preState == States::_firstStep || preState == States::_lastStep) {
      curMode = false;
      s = (PinOptions)((preState == States::_firstStep) ? PinOptions::_atStart :
          (preState == States::_lastStep) ? PinOptions::_atFinish | PinOptions::_startFirstStep_finishLastStep | PinOptions::_atBetween :
          PinOptions::_atBetween);
      pinProcessing(s);
      //trace("1.1 preState=" + String(preState) + " state=" + String(state) + " now=" + String(now) + " mode=" + String(curMode));
    }
    if (state == States::_firstStep || state == States::_lastStep) {
      curMode = true;
      s = (PinOptions)((state == States::_firstStep) ? PinOptions::_atStart | PinOptions::_startFirstStep_finishLastStep : PinOptions::_atFinish);
      pinProcessing(s);
      //trace("1.2 preState=" + String(preState) + " state=" + String(state) + " now=" + String(now) + " mode=" + String(curMode));
    }
    else if (state == States::_stopped) {
      trace_begin(F("Задача \""));         trace_i(id);          trace_s(F("\" финишировала."));          trace_end();
      putInfo(now);
      memset(this, 0, sizeof(Task));
    }
  }
  if (state != States::_stopped && now >= nextActivationTime) {
    if (sens.id) {
      bool result;
      sens.curValue = getSensorValue(sens.id);
      switch (sens.comp) {
        case Comparison::_eq :
          result = (sens.curValue == sens.targetValue) ? 1 : 0;
          break;
        case Comparison::_notEq :
          result = (sens.curValue != sens.targetValue) ? 1 : 0;
          break;
        case Comparison::_less :
          result = (sens.curValue < sens.targetValue) ? 1 : 0;
          break;
        case Comparison::_greater :
          result = (sens.curValue > sens.targetValue) ? 1 : 0;
          break;
        case Comparison::_lessEq :
          result = (sens.curValue <= sens.targetValue) ? 1 : 0;
          break;
        case Comparison::_greaterEq :
          result = (sens.curValue >= sens.targetValue) ? 1 : 0;
          break;
      }
      curMode = result;
      //trace("5. val=" + String(sens.curValue) + " mode=" + String(curMode));
    }
    else
      curMode = !curMode;

    if (preCurMode != curMode) {
      if (curMode) {
        activeWorkCount++;
        tmpWorkTime = now;
      } else {
        activeWorkTime += now - tmpWorkTime;
      }
    }
    pinProcessing(PinOptions::_atBetween);
    //trace("2. state=" + String(state) + " now=" + String(now) + " mode=" + String(curMode) + " id=" + String(sens.comp));
    nextActivationTime = now + checkPeriod;
  }
}
//-------------------------------------------------------------------------------------------------------------------------
void Task::putInfo(uint32_t now=0)
{
  if(!now)
    now = RTC.now().secondstime();
  uint32_t tmp;
  byte w = (state != States::_stopped) ? 1 : 0;
  String str;
  str.reserve(100);
  str  = F("{\"id\":\"");
  str += id;
  if (activeWorkCount) {
    str += F(",\"cnt\":");
    str += (activeWorkCount);
  }
  if (w) {
    str += F("\",\"l\":");
    str += (startTime + duration) - now;

    str += F(",\"w\":");
    str += w;
  }
  if (curMode) {
    str += F(",\"a\":");
    str += curMode;
  }
  tmp = (activeWorkTime + (curMode ? (now - tmpWorkTime) : 0));
  if (tmp) {
    str += F(",\"actt\":");
    str += tmp;
  }
  tmp = (now - startTime);
  if (tmp) {
    str += F(",\"ont\":");
    str += tmp;
  }
  str += F("}");
  trace(str);
  app.addEvent2Buffer(8, str);
}

//--------------------------------------------------------------------------------------------------------
#define PUT_CMD_INFO_TIMEOUT 60000
Task taskPool[MAX_TASK];
uint32_t putInfoLastTime; // для отслеживания периодичности выдачи информации по работающим задачам (раз в минуту)

class Task *getFreeTask(short taskId = 0)
{
    for (int i = 0; i < MAX_TASK; i++) {
      if (taskId == 0) {
        if (!taskPool[i].id)
          return &taskPool[i];
      }
      else {
        if (taskPool[i].id == taskId)
          return &taskPool[i];
      }
    }
    return nullptr;
}

//--------------------------------------------------------------------------------------------------------
void taskProcessing()
{
  byte putInfoFlag = 0;

  for (int i = 0; i < MAX_TASK; i++) {
    if (taskPool[i].id) {
      if (putInfoFlag == 0)
        putInfoFlag = (millis() - putInfoLastTime > PUT_CMD_INFO_TIMEOUT) ? 1 : 2;
      taskPool[i].processing();
      if (putInfoFlag == 1)
        taskPool[i].putInfo();
    }
  }
  if (putInfoFlag == 1)
    putInfoLastTime = millis();
}

//--------------------------------------------------------------------------------------------------------
void taskInit(String json) {
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    trace_begin(F("Ошибка десериализации: "));
    trace_s(error.f_str());
    trace_end();
    return;
  }
  for (int i = 0; i < doc.size(); i++) {
    JsonObject root = doc[i];
    byte id = root[F("id")];
    bool online = root["w"];
    uint32_t duration = root[F("d")];
    if (!id || (!online && !duration)) { //
      trace(F("Не указан \"id\" или \"duration\" задачи!"));
      return;
    }

    Task *t = getFreeTask(id);  // поиск структуры команды
    if (online) {               // запрос на завершение команды
      if (!t) {                 // команда не в работе (структура команды не найдена)
        trace_begin(F("Останов: Задача \""));
        trace_i(id);
        trace_s(F("\" не активна."));
        trace_end();
        //esp.addEvent2Buffer(ERR, "Структура команды \"" + String(name) + "\" не найдена!");
        //esp.addEvent2Buffer(8, "{\"id\":" + String(id) + ",\"online\":0}");
      }
      else {
        uint32_t now = RTC.now().secondstime();
        t->duration = now - t->startTime + t->checkPeriod;
      }
      continue;
    }
    else {      // запрос на запуск команды
      if (t) {  // команда уже в работе (структура команды найдена)
        trace_begin(F("Старт: Задача \""));
        trace_i(id);
        trace_s(F("\" уже активна."));
        trace_end();
        //esp.addEvent2Buffer(ERR, "Команда \"" + String(name) + "\" уже в работе!");
        continue;
      }
    }
    t = getFreeTask(0);  // получить свободную структутру
    if (!t) {
      trace_begin(F("Нет места в очереди задач для \""));
      trace_i(id);
      trace_s(F("\"."));
      trace_end();
      //esp.addEvent2Buffer(ERR, "Нет свободной структуры для команды :" +  String(name) + "!");
      break;
    }
    t->init(id, duration, root);
  }
}

