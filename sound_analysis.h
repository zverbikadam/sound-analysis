#include "esphome.h"

class SoundAnalysis : public Component, public CustomMQTTDevice {
 public:
  void setup() override {
    // This will be called once to set up the component
    // think of it as the setup() call in Arduino
    pinMode(2, OUTPUT);

    subscribe("the/topic", &MyCustomComponent::on_message);
  }
  void on_message(const std::string &payload) {
    if (payload == "ON") {
      digitalWrite(2, HIGH);
      publish("the/other/topic", "Hello World!");
    } else {
      digitalWrite(2, LOW);
      publish("the/other/topic", "Goodby World!");
    }
  }
};