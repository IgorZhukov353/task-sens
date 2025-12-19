#include <RTClib.h>

#define MAX_PIN  4
enum Comparison {_eq = 0, _notEq, _less, _greater, _lessEq, _greaterEq};
enum PinOptions {_atStart = 1, _atFinish = 2, _atBetween = 4, _startFirstStep_finishLastStep = 8, _sensorCheck = 0x10};
enum States: byte {_stopped, _firstStep, _nextStep, _lastStep};
//---------------------------------------------------------------------------
typedef struct {
  byte pin: 6; // max = 63
  byte onValue: 1;
  byte offValue: 1;
  union {
    byte options;
    struct {
      bool atStart: 1;
      bool atFinish: 1;
      bool atBetween: 1;
      bool startFirstStep_finishLastStep: 1;
      bool sensorCheck: 1;
    };
  };
} pin_info;

typedef struct {          // инфо по датчику
  byte id: 6;             // max = 63
  int8_t targetValue;      // целевое значение датчика (температура (-63 : + 63) или значене (0:1))
  int8_t curValue;
  Comparison comp: 3;     // операция сравнения
} sensor_info;
//---------------------------------------------------------------------------

class Task {
  public:
    byte id: 6;
    byte curMode: 1;            // текущий режим 0-выключено,1-включено,....
    States state: 2;

    pin_info pin[MAX_PIN];
    sensor_info sens;

    uint32_t duration;          // назначенная длительность выполненеия задания (сек)
    uint32_t startTime;         // время начала работы (сек)
    uint32_t activeWorkTime;    // активное время работы
    uint16_t activeWorkCount;   // кол-во включений за время работы
    uint32_t tmpWorkTime;       // для вычисления активного время работы

    uint32_t checkPeriod;       // периодичность активаций (сек)
    uint32_t nextActivationTime;// время следующей активации

    Task() {};
    void init(byte _id, short ppin, byte _onValue, uint32_t _duration);
    bool init(byte _id, uint32_t _duration, JsonObject & doc);
    void pinProcessing(byte mask);
    void processing();
    void putInfo(uint32_t now=0);
};
