#include <esphome.h>
#include <../header/wavelib.h>

class DWTSensor : public Component, public CustomMQTTDevice, public BinarySensor {
public:

// binary sensor with states 0 -> not recognized; 1 -> recognized
BinarySensor *dwt_sensor = new BinarySensor();

// For components that should be initialized after a data connection (API/MQTT) is connected
float get_setup_priority() const override { return esphome::setup_priority::AFTER_CONNECTION; }

void setup() override {}

void loop() override {}

};