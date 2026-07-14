/*
  Igor Zhukov (c)
  Created:       01-09-2025
  Last changed:  29-12-2025
*/

//---------------------------------------------------------------------------
enum _Type {_dht = 1, _dallas = 2, _pin = 4, _analogPin = 8, _pir = 0x10, _ping = 0x20, _led = 0x40, _group = 0x80};

class Sensor {
  public:
    byte id : 6, pin : 6;
    int8_t value, preValue;
    _Type type;
    int virtual check() = 0;
    void virtual init(byte _id, _Type _type, byte _ppin, JsonObject &root) {
      id = _id;
      type = _type;
      pin = _ppin;
      preValue = -100;
    }
};
//---------------------------------------------------------------------------
class Activity {
  public:
    uint32_t timeout;
    uint32_t lastActivated;

    int virtual actionRunFunc() = 0;
    int run() {
      long currentMillis = millis();
      if (currentMillis - lastActivated > timeout) {
        lastActivated = currentMillis;
        return actionRunFunc();
      }
      return 0;
    };
};
//---------------------------------------------------------------------------
class TempSensor : public Sensor {
    union {
      struct {
        OneWire oneWire;
        DallasTemperature t;
      } t1;
      struct {
        dht t;
        int8_t humValue;
      } t2;
    };
  public:
    TempSensor() {};
    void virtual init(byte _id, _Type _type, byte _ppin, JsonObject &root);
    int virtual check();
};
//---------------------------------------------------------------------------
class LED : public Sensor, public Activity {
  public:
    uint16_t origTimeout;
    void virtual init(byte _id, _Type _type, byte _ppin, JsonObject &root);
    int virtual check(){return run();}
    int virtual actionRunFunc();
};
//---------------------------------------------------------------------------
class PIN : public Sensor {
  public:
    uint16_t timeout;
    uint32_t lastActivated;
    uint8_t normval : 1;
    byte sysledoff : 1;
    void virtual init(byte _id, _Type _type, byte _ppin, JsonObject &root);
    int virtual check();
};
//---------------------------------------------------------------------------
class ANALOGPIN : public Sensor, public Activity {
  public:
    uint16_t r1,r2;
    void virtual init(byte _id, _Type _type, byte _ppin, JsonObject &root);
    int virtual check(){return run();}
    int virtual actionRunFunc();
};
//---------------------------------------------------------------------------
class IPPING : public Sensor, public Activity {
  public:
    byte currIndex;
    void virtual init(byte _id, _Type _type, byte _ppin, JsonObject &root);
    int virtual check();
    int virtual actionRunFunc();
};
//---------------------------------------------------------------------------
class SensorArray : public Activity {
  public:
    byte len;
    byte temper_ptr, led_ptr, pin_ptr, analogpin_ptr, ping_ptr;
    byte groupCurPtr;
    //----------------------------
    void init(JsonDocument &doc);
    int virtual actionRunFunc();
    int check(byte sensorMask = 0, byte actMask = 0);
    int load(JsonDocument &doc);
};
