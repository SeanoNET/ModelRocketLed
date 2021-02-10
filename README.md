# ModelRocketLed
A nodemcu powering a WS2812B 144 LED strip to create lighting effects under a model rocket powered by Home Assistant

## Parts

- [ESP8266 Development Board](https://www.altronics.com.au/p/z6381-wifi-development-board/)
- [1M RGB LED Strip - WS2812B](https://core-electronics.com.au/5m-rgb-led-strip-ws2812-144-per-meter-black-strip-weatherproof.html)
- [Electrolytic Decoupling Capacitors - 1000uF/25V](https://core-electronics.com.au/electrolytic-decoupling-capacitors-1000uf-25v.html)
- [Resistor 330 Ohm 1/4 Watt](https://core-electronics.com.au/resistor-330-ohm-1-4-watt-pth-20-pack-thick-leads.html)
- [74AHCT125 - Quad Level-Shifter (3V to 5V) - 74AHCT125](https://core-electronics.com.au/74ahct125-quad-level-shifter-3v-to-5v-74ahct125.html) - Wasn't used but is [recommended](https://learn.adafruit.com/neopixel-levelshifter)

## Wiring Diagram 

![](/docs/wiring-diagram.png)

## Getting Started
Create a `secrets.h` file in the same directory as `rocket_led.ino` and define the following secrets.

```c
/************ WIFI and MQTT Information (CHANGE THESE FOR YOUR SETUP) *************/
#define STASSID "" // Wifi SSID
#define STAPSK "" // Wifi Password
const char *ssid = STASSID;
const char *password = STAPSK;
#define mqtt_server ""
#define mqtt_username ""
#define mqtt_password ""
const int mqtt_port = 1883;

/**************************** FOR OTA **************************************************/
#define SENSORNAME "led"    //change this to whatever you want to call your device
#define OTApassword "password" //the password you will need to enter to upload remotely via the ArduinoIDE
int OTAport = 8266;
```

## Home Assistant

The LED effects and power is controlled by [Home Assistant](https://www.home-assistant.io/).

### Light MQTT configuration
```yaml

light:
  - platform: mqtt
    schema: json
    name: model_rocket_led
    state_topic: "rocket/led"
    command_topic: "rocket/led/set"
    rgb: true
    effect: true
    effect_list:
      [
        rocket_take_off,
        rocket_flight,
        bpm,
        candy cane,
        confetti,
        cyclon rainbow,
        dots,
        fire,
        glitter,
        juggle,
        lightning,
        noise,
        police all,
        police one,
        rainbow,
        rainbow with glitter,
        ripple,
        sinelon,
        solid,
        twinkle,
      ]
    optimistic: false
    qos: 0

input_number:
  rocket_animation_speed:
    name: Rocket Animation Speed
    initial: 150
    min: 1
    max: 150
    step: 10
```

![](/docs/home-assistant.gif)

### Automation for animation speed
For changing the speed of the light effect.

```yaml
- alias: "Rocket Animation Speed"
  initial_state: True
  hide_entity: False
  trigger:
    - platform: state
      entity_id: input_number.rocket_animation_speed
  action:
    - service: mqtt.publish
      data_template:
        topic: "rocket/led/set"
        payload: '{"transition":{{ trigger.to_state.state | int }}}'
```

Based on [ESP-MQTT-JSON-Digital-LEDs](https://github.com/bruhautomation/ESP-MQTT-JSON-Digital-LEDs)