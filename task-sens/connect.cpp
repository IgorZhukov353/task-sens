/*
  Igor Zhukov (c)
  Created:       01-09-2025
  Last changed:  14-07-2026
*/
#include<Arduino.h>
#include <RTClib.h>
#include <avr/wdt.h>

#include "util.h"
#include "main.h"

extern HardwareSerial Serial1;
#define ESP_Serial Serial1 // для МЕГИ

//---------------------------------------------------------------------------
void APP::setup()
{
  startTime = RTC.now().secondstime();  
  strcpy_P(WSSID,PSTR("Keenetic-7832"));
  strcpy_P(WPASS,PSTR("tgbvgy789"));
  strcpy_P(HOST_STR,PSTR("igorzhukov353.h1n.ru"));
  strcpy_P(HOST_IP_STR,PSTR("81.90.182.128"));
  check_tcp_last_byte[0] = 9;
  check_tcp_last_byte[1] = 22;
  check_tcp_last_byte[2] = 29;
  check_tcp_last_byte[3] = 31;

  //trace(WSSID + String(":") + WPASS);
}

//------------------------------------------------------------------------
bool APP::espSerialSetup() {
  bool r;
  //esp_power_switch(true);
  //delay(200);
  
  ESP_Serial.begin(115200); // default baud rate for ESP8266
  delay(100);
  r = espSendCommand(F("AT"), _STATE::OK , 5000 );
  r = espSendCommand(F("ATE0"), _STATE::OK , 5000 );
  r = espSendCommand( F("AT+GMR"), _STATE::OK, 10000 );
  r = espSendCommand( F("AT+CWMODE=1"), _STATE::OK, 5000 );
  {
    String str = F("AT+CWJAP=\""); 
    str += WSSID;
    str += F("\",\"");
    str += WPASS;
    str += F("\"");
    r = espSendCommand(str, _STATE::OK, 20000);  
  }
  if(!r){
    routerConnectErrorCounter++; 
    } else {
    routerConnectErrorCounter = 0;
    r = espSendCommand( F("AT+CIFSR"), _STATE::OK, 5000 );
  }
  return r;
}
  
//---------------------------------------------------------------------------
bool  APP::ping(const String &host, short timeout = 5000, byte _act=0) {
  String str;
  switch(_act){
  case 0:  
    str = F("AT+PING=\"");
    str += host;
    str += F("\"");
    break;
  case 1:
    str = F("AT+CIPSTART=\"TCP\",\"");
    str += host;
    str += F("\",80");
    break;
  case 2:
    str = F("AT+CIPCLOSE");
    break;
  }
  return espSendCommand(str, _STATE::OK , timeout);
}
//------------------------------------------------------------------------
bool APP::checkInitialized() {
  if(!wifi_initialized){
    wifi_initialized = espSerialSetup();
  }
  return wifi_initialized;
}
//---------------------------------------------------------------------------
bool APP::check_Wait_Internet()
{
  if (!checkInitialized())
    return 0;
  bool res = false;
  trace(F("check_Wait_Internet ..."));
  unsigned long tstart, tnow, timeout = 1000L * 60 * 2; // izh 28-10-2018 таймаут 2 мин или до появления пинга
  tnow = tstart = millis();
  while (tnow < tstart + timeout)
  {
    // delay(10000);
    wdt_delay(10000);               // 6-02-2025 !watchdog!
    res = ping(HOST_IP_STR, 15000); // попытка пингануть свой сервер
    if (res)
    {
      break;
    }
    res = ping(F("ya.ru"), 5000); // попытка пингануть яндекс
    if (res)
    { // 6-01-2026 отключен мобильный интернет по БПЛА-опасности, перегружать нет смысла, нужно ждать когда будет доступен HOST_IP_STR
      yandexOnly = true;
      return 0;
    }
    tnow = millis();
  }
  return res;
}
//------------------------------------------------------------------------
void APP::addInfo2Buffer(const char *str) {
  short currBufLen = strlen(buffer);
  short strLen = strlen(str);

  if (strLen > sizeof(buffer)) {
    buffOverCounter++;
    return;
  }
  if (!currBufLen) {
    strcpy(buffer, str);
    return;
  }
  if (currBufLen + strLen + 1 > sizeof(buffer)) {
    buffOverCounter++;
    strcpy(buffer, str);
    return;
  }
  //trace("1 currBufLen="+String(strlen(buffer)) +" buffer=" + String(buffer));
  strcat(buffer, ",");
  strcat(buffer, str);
  //trace("2 currBufLen="+String(strlen(buffer)) +" buffer=" + String(buffer));
}

//------------------------------------------------------------------------
void APP::addEvent2Buffer(byte id, const String &msgText) {
  char *pfmt = (msgText[0] == '{') ? PSTR("{\"type\":\"E\",\"id\":%d,\"text\":%s,\"date\":\"%s\"}") : PSTR("{\"type\":\"E\",\"id\":%d,\"text\":\"%s\",\"date\":\"%s\"}");
  short len = strlen_P(pfmt) + 1;
  char fmt[len];
  strcpy_P(fmt, pfmt);
  len += 2 + msgText.length() + 20;
  char loc_buf[len];
  sprintf(loc_buf, fmt, id, msgText.c_str(), getCurrentDate(0).c_str());
  addInfo2Buffer(loc_buf);
}
//------------------------------------------------------------------------
void APP::addTempHum2Buffer(byte id, int8_t temp, int8_t hum) {
  char *pfmt = PSTR("{\"type\":\"T\",\"id\":%d,\"temp\":%d,\"hum\":%d,\"date\":\"%s\"}");
  short len = strlen_P(pfmt) + 1;
  char fmt[len];
  strcpy_P(fmt, pfmt);
  len += 2 + 4 + 4 + 20;
  char loc_buf[len];
  sprintf(loc_buf, fmt, id, temp, hum, getCurrentDate(0).c_str());
  addInfo2Buffer(loc_buf);
}
//------------------------------------------------------------------------
void APP::addSens2Buffer(byte id, byte val) {
  char *pfmt = PSTR("{\"type\":\"S\",\"id\":%d,\"v\":%d,\"date\":\"%s\"}");
  short len = strlen_P(pfmt) + 1;
  char fmt[len];
  strcpy_P(fmt, pfmt);
  len += 2 + 1 + 20;
  char loc_buf[len];
  sprintf(loc_buf, fmt, id, val, getCurrentDate(0).c_str());
  addInfo2Buffer(loc_buf);
}

//------------------------------------------------------------------------
bool APP::sendBuffer2Site() {
  if (strlen(buffer) == 0)
    return false;
  if (_send2site(F("upd/send_info.php"), buffer))
    buffer[0] = 0;
  return true;
}
//------------------------------------------------------------------------
void APP::closeConnect() {
  if(wifi_initialized){
    espSendCommand(F("AT+CWQAP"), _STATE::OK, 15000 );
    //delay(1000);
    wdt_delay(1000); // 6-02-2025 !watchdog!
    espSendCommand(F("AT+RST"), _STATE::OK , 20000 );
    //delay(1000); 
    wdt_delay(1000); // 6-02-2025 !watchdog!
    wifi_initialized = false;
    esp_power_switch(false);
  }  
}

//------------------------------------------------------------------------
bool APP::sendError_check() {
  {
    trace_begin(F("MEM="));
    trace_i(checkMemoryFree());
    trace_s(F(" MaxMsgLen="));
    trace_i(maxSendedMSG);
    trace_s(F(" LastSendErr="));
    trace_i((byte)lastErrorTypeId);
    trace_s(F(" SErr="));
    trace_i(sendErrorCounter);
    trace_s(F(" RCErr="));
    trace_i(routerConnectErrorCounter);
    trace_s(F(" HttpErr="));
    trace_i(httpFailCounter);
    trace_s(F(" BufOvrErr="));
    trace_i(buffOverCounter);
    trace_s(F(" TOutErr="));
    trace_i(timeoutCounter);
    trace_s(F(" ConnErr="));
    trace_i(connectFailCounter);
    trace_s(F(" YandexOnly="));
    trace_i(yandexOnly);
    trace_end();
  }
  
  bool res;
  if(sendErrorCounter > 3)
    res = false;
  else  
    res = 1; //ping(F("192.168.8.1"), 5000); // попытка пингануть модем    
    
  if(!res){
    res = ping(HOST_IP_STR, 15000); // попытка пингануть свой сервер
    if(res){ // все наладилось
        res = ping(HOST_IP_STR, 15000, 1); // попытка соединиться со своим сервером через TCP
        if(res){ // все действительно наладилось
          ping(HOST_IP_STR, 15000, 2);  // отсоединиться
          sendErrorCounter = 0;
          yandexOnly = false;
          return 0;
        }
    }
  else {
    res = ping(F("ya.ru"), 5000); // попытка пингануть яндекс
    if(res){ // 6-01-2026 возможно отключен мобильный интернет по БПЛА-опасности (доступен только яндекс), перегружать нет смысла, нужно ждать когда будет доступен HOST_IP_STR
      yandexOnly = true;  
      return 0;
    }
  }

    res = ping(F("192.168.0.1"), 5000); // попытка пингануть роутер
    if (!res && lastErrorTypeId == _ErrorType::TIMEOUT && lastRouterReboot > lastWIFISended) {  // роутер не отвечает на ping, последняя ошибка была по таймауту и последнее успешное отправление было до перезагрузки роутера -> перегрузить МЕГУ
      wdt_enable(WDTO_8S);                                                                     // Для тестов не рекомендуется устанавливать значение менее 8 сек
      delay(10000); // перезагрузка
      return 0; // сюда уже не попадем
    }
    if(millis() - lastRouterReboot > (60000 * 60) ){ // если роутер отвечает на ping, то проблема с доступом в Инет (модем, кончились деньги и т.п.), инче проблема в роутере -> перегрузить роутер (но не чаще чем в 1 час)
      closeConnect(); // izh 22-05-2020 отключить от WIFI
      lastRouterReboot = millis();
      routerRebootCount++;
      sendErrorCounter_ForAll = 0;
      httpFailCounter = 0;
      timeoutCounter = 0;
      buffOverCounter = 0;
      connectFailCounter = 0;
      yandexOnly = false;
      return 1;
      //return 0;
      }
    }
  return 0;
}

