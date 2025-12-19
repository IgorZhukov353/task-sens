/* 
 Igor Zhukov (c)
 Created:       01-09-2025
 Last changed:  01-09-2025
*/

//---------------------------------------------------------------------------
class APP {
char WSSID[20], WPASS[10], HOST_STR[25], HOST_IP_STR[15]; 

public:
  void configRead();

  bool send2site(const String& reqStr){};
  void addEvent2Buffer(byte id, const String& msgText){};
  void addTempHum2Buffer(byte id, int8_t temp, int8_t hum){};
  void addSens2Buffer(byte id, byte val){};

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
int sensorLoad(String json);
int sensorProcessing();
