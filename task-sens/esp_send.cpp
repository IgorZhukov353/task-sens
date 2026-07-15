/*
  Igor Zhukov (c)
  Created:       01-09-2025
  Last changed:  14-07-2026
*/
#include <Arduino.h>
#include <RTClib.h>
#include <avr/wdt.h>

#include "util.h"
#include "main.h"

extern HardwareSerial Serial1;
#define ESP_Serial Serial1 // для МЕГИ

#define OTHER_HOST F("cloud-api.yandex.net")
#define OTHER_HOST_TOKEN F("y0__xCUvKUJGJSePSCz26qaFjDmy9b-B4i8Bo9O74ZhdYVkUc5SMnESwVgv")
#define MY_AUTH ("Basic aWdvcmp1a292MzUzOlJEQ3RndjE5Ng==")

//------------------------------------------------------------------------
#define BUF_SIZE 10
#define STATE_STR_MAX 6
#define RESPONSE_LEN_MAX 2048
static const char *state_str[STATE_STR_MAX] = {"OK", "ERROR", "HTTP/1.1", "200 OK", "CLOSED", "ALREADY"};
static const byte state_str_len[STATE_STR_MAX] = {2, 5, 8, 6, 6, 7};

bool APP::espSendCommand(const String &cmd, const _STATE goodResponse, const unsigned long timeout, const char *postBuf, const String &cmd2)
{
  {
    trace_begin(F("espSendCommand(\""));
    trace_s(cmd);
    if (postBuf)
    {
      trace_c(postBuf);
      trace_s(cmd2);
    }
    trace_s(F("\","));
    trace_i((byte)goodResponse);
    trace_s(F(","));
    trace_l(timeout);
    trace_s(F(")"));
    trace_end();
  }
  // return true;

  short msglen = cmd.length();
  if (postBuf)
  {
    msglen += strlen(postBuf);
    msglen += cmd2.length();
    ESP_Serial.print(cmd);
    ESP_Serial.print(postBuf);
    ESP_Serial.println(cmd2);
  }
  else
    ESP_Serial.println(cmd);
  ESP_Serial.flush();
  // delay(100);
  if (maxSendedMSG < msglen)
    maxSendedMSG = msglen;

  unsigned long tnow, tstart;
  bool result;
  String response;
  short responseLenMax = (goodResponse == _STATE::CLOSED) ? RESPONSE_LEN_MAX : 200;
  char c;
  char cbuffer[BUF_SIZE]; //= {'*','*','*','*','*','*','*','*','*','*'};
  bool state_str_on[STATE_STR_MAX] = {0, 0, 0, 0, 0, 0};
  bool recived = false;
  bool bufOverFlag = false;
  short currResponseLen = 0;
  tnow = tstart = millis();
  response.reserve(responseLenMax);

  while (tnow <= tstart + timeout)
  {
    c = ESP_Serial.read();
    if (c > 0)
    {
      currResponseLen++;
      if (currResponseLen < responseLenMax)
      {
        response += c; // String(c);
        if (!state_str_on[(byte)_STATE::ERR] && !state_str_on[(byte)_STATE::CLOSED])
        {
          memmove(cbuffer, cbuffer + 1, sizeof(cbuffer) - 1);
          cbuffer[sizeof(cbuffer) - 1] = c;
          for (byte i = 0; i < STATE_STR_MAX; i++)
          {
            if ((i == (byte)_STATE::HTTP_OK || i == (byte)_STATE::CLOSED) && !state_str_on[(byte)_STATE::HTTP])
              continue;
            if (!memcmp(cbuffer + sizeof(cbuffer) - 1 - state_str_len[i], state_str[i], state_str_len[i]))
            {
              state_str_on[i] = true;
              if (i == (byte)goodResponse || i == (byte)_STATE::ERR)
                recived = true;
            }
          }
        }
      }
      else
      {
        bufOverFlag = true;
      }
    }
    if (recived)
      break;
    tnow = millis();

    wdt_reset(); // 6-02-2025 !watchdog!
  }

  while (ESP_Serial.available())
  {
    c = ESP_Serial.read();
    currResponseLen++;
    response += c;
    if (currResponseLen < responseLenMax)
      response += c;
    else
    {
      bufOverFlag = true;
    }
  }
  {
    if (bufOverFlag)
      buffOverCounter++;

    trace_begin(F("espSendCommand:"));
    if (recived)
    {
      if (state_str_on[(byte)_STATE::ALREADY])
      {
        lastErrorTypeId = _ErrorType::ALREADY_CONNECT;
        result = false;
      }
      else
      {
        if (state_str_on[(byte)_STATE::HTTP] && !state_str_on[(byte)_STATE::HTTP_OK])
        {
          httpFailCounter++;
          lastErrorTypeId = _ErrorType::HTTP_FAIL;
          result = false;
        }
        else
        {
          result = (state_str_on[(byte)_STATE::ERR] || bufOverFlag) ? false : true;
          lastErrorTypeId = (result) ? _ErrorType::NONE : (bufOverFlag) ? _ErrorType::BUFFOVER
                                                                        : _ErrorType::OTHER;
        }
      }
      trace_s((result) ? F("SUCCESS") : F("ERROR"));
    }
    else
    {
      result = false;
      trace_s(F("ERROR - Timeout"));
      timeoutCounter++;
      lastErrorTypeId = _ErrorType::TIMEOUT;
    }
    trace_s(F(" - Response time: "));
    trace_l(millis() - tstart);
    trace_s(F("ms. Len: "));
    trace_i(currResponseLen);
    trace_s(F("\n\rRESPONSE:"));
    trace_s(response);
    trace_s(F("\n\r---END RESPONSE---"));
    trace_end();
  }
  if (result)
    responseProcessing(response);

  checkMemoryFree();
  return result;
}
//------------------------------------------------------------------------
bool APP::_send2site(const String &reqStr, const char *postBuf)
{
  return __send2site(_MYHOME_HOST, (postBuf)?F("POST"):F("GET"), nullptr, reqStr, (postBuf)?MY_AUTH:nullptr);
}
//------------------------------------------------------------------------
// выполнить команду на удаленном сервере
bool APP::__send2site(const byte opt, const String &metod, const String &host, const String &reqStr, const String &authStr, const char *postBuf = nullptr)
{
  if (!checkInitialized())
  {
    return false;
  }
  bool r;
  String cmd1;
  cmd1 = F("AT+CIPSTART=\"");
  cmd1 += (opt & _SSL) ? F("SSL") : F("TCP");
  cmd1 += F("\",\"");
  cmd1 += (opt & _MYHOME_HOST) ? this->HOST_IP_STR : host;
  cmd1 += F("\"");
  if (opt & _SSL)
  {
    r = espSendCommand(F("AT+CIPSSLSIZE=4096"), _STATE::OK, 5000);
    cmd1 += F(",443,1200");
  }
  else
    cmd1 += F(",80");
  r = espSendCommand(cmd1, _STATE::OK, 15000); // установить соединение с хостом
  if (lastErrorTypeId == _ErrorType::ALREADY_CONNECT)
  {
    r = true;
  }

  if (r)
  {
    short postBufLen = (postBuf) ? strlen(postBuf) : 0; //

    String request;
    short requestLength;
    short maxlen = reqStr.length() + postBufLen + 200;
    if (maxlen > 1024)
      maxlen = 1024;
    request.reserve(maxlen);

    request = metod;
    request += F(" ");
    request += reqStr;
    request += F(" HTTP/1.1\r\n");
    request += F("Host: ");
    request += (opt & _MYHOME_HOST) ? HOST_STR : host;
    request += F("\r\n");
    request += F("User-Agent: ESP8266\r\n");
    // request += F("Connection: keep-alive\r\n");
    request += F("Connection: close\r\n");
    // request += F("Accept: application/json\r\n");
    request += F("Cache-Control: no-cache\r\n");
    if (authStr.length() > 0)
    {
      request += F("Authorization: ");
      request += authStr;
      request += F("\r\n");
    }

    if (postBufLen)
    {
      // request += F("Content-Type: application/json\r\n");
      request += F("Content-Length: ");
      request += postBufLen;
      request += F("\r\n");
      request += F("\r\n"); // перед postBuf
    }

    {
      requestLength = request.length() + postBufLen + 2; // add 2 because \r\n will be appended by Serial.println().
      String cmd2 = F("AT+CIPSEND=");
      cmd2 += requestLength;
      r = espSendCommand(cmd2, _STATE::OK, 5000); // подготовить отсылку запроса - длина запроса
      bytesSended += requestLength;
    }
    r = espSendCommand(request, _STATE::CLOSED, 5000, postBuf); // отослать запрос и получить ответ

    espSendCommand(F("AT+CIPCLOSE"), _STATE::OK, 5000);
  }
  return r;
}
//------------------------------------------------------------------------
int APP::responseProcessing(const String &response)
{
}
//------------------------------------------------------------------------
void getHostUrl(const String & s, String * host, String * url)
{
int i1 = s.indexOf(F("uploader"));
  if (i1 >= 0) {
    int i2 = s.indexOf(F(":443"), i1);
    if (i2 >= 0) {
      *host = s.substring(i1, i2);
    }
    i2 = s.indexOf(F("/upload-target"), i1);
    if (i2 >= 0) {
      if (host->length() == 0) {
        *host = s.substring(i1, i2);
      }
      i1 = s.indexOf(F("\""), i2);
      if (i1 >= 0) {
        *url = s.substring(i2, i1);
      }
    }

    // Serial.println(*host);
    // Serial.println(*url);
  }
}
