/* 
 Igor Zhukov (c)
 Created:       01-09-2025
 Last changed:  14-07-2026
*/
#define ARDUINOJSON_ENABLE_PROGMEM 1
#include <ArduinoJson.h>

//---------------------------------------------------------------------------
// enum class _STATE {OK = 0, ERR = 1, HTTP = 2, HTTP_OK = 3, CLOSED = 4};
// enum class _ErrorType {NONE=0,HTTP_FAIL=1,TIMEOUT=2,OTHER=3,CONNECT=4,BUFFOVER=5};
enum class _STATE {OK = 0, ERR = 1, HTTP = 2, HTTP_OK = 3, CLOSED = 4, ALREADY = 5};
enum class _ErrorType {NONE=0,HTTP_FAIL=1,BUFFOVER=2,ALREADY_CONNECT=3,YANDEX_ONLY=4,TIMEOUT=20,OTHER=21,CONNECT=22};

#define MAX_PING 10

class APP {
char WSSID[20], WPASS[10], HOST_STR[25], HOST_IP_STR[16]; 

public:
  long startTime;
  byte check_tcp_last_byte[MAX_PING]; // устройства для пингования
  byte check_tcp_err[MAX_PING];       // признак ошибки пингования устройств

  char buffer[1500];
  byte wifi_initialized:1;
  unsigned long lastWIFISended;
  unsigned short sendErrorCounter;
  unsigned short httpFailCounter;
  unsigned short buffOverCounter;
  unsigned short timeoutCounter;
  unsigned short connectFailCounter;
  _ErrorType lastErrorTypeId;
  
  short routerConnectErrorCounter;
  int routerRebootCount;                  // счетчик перезагрузок роутера
  unsigned long lastRouterReboot;         // время последней перезагрузки роутера

  int sendErrorCounter_ForAll;
  unsigned long sendCounter_ForAll;
  unsigned long bytesSended;
  short maxSendedMSG;
  bool yandexOnly:1;

  void setup();
  void configRead();

  bool espSerialSetup();
  bool espSendCommand(const String& cmd, const _STATE goodResponse, const unsigned long timeout, const char *postBuf=NULL, const String &cmd2="");

  bool _send2site(const String &reqStr, const char *postBuf);
  bool send2site(const String& reqStr){return _send2site(reqStr, NULL);};
  bool sendBuffer2Site();

  void addInfo2Buffer(const char *str);
  void addEvent2Buffer(byte id, const String& msgText);
  void addTempHum2Buffer(byte id, int8_t temp, int8_t hum);
  void addSens2Buffer(byte id, byte val);

  int responseProcessing(const String& response){};

  bool checkInitialized();
  bool check_Wait_Internet();
  bool ping(const String &host, short timeout=5000, byte _act=0);

  void closeConnect();
  bool sendError_check();
  
  void esp_power_switch(bool p){};
};


#ifdef MAIN
APP app;

// TempSensor temper[TEMPER_MAX];
// LED led[LED_MAX];
// PIN p[PIN_MAX];
// Sensor * s[ALL_MAX];
// SensorArray sa;
// LED *sysledptr;

#else
extern APP app;

// extern TempSensor temper[TEMPER_MAX];
// extern LED led[LED_MAX];
// extern PIN p[PIN_MAX];
// extern Sensor * s[ALL_MAX];
// extern SensorArray sa;
// extern LED *sysledptr;

#endif

//---------------------------------------------------------------------------
void taskInit(String json);
void taskProcessing();
//---------------------------------------------------------------------------
//int sensorLoad(String json);
int sensorLoad(JsonDocument & doc);
int sensorProcessing();
