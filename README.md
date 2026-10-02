# Huskylens-Homeassistant

Connecting HuskyLens to Home Assistant using an ESP32 and MQTT (Arduino/ESP32 core).

![husky](https://github.com/user-attachments/assets/b70faeda-7f0b-4f48-b796-f696424fce6b)

Overview
--------
This project reads recognition data from a DFRobot HuskyLens connected to an ESP32, and forwards the results to Home Assistant via an MQTT broker. It is intended as a lightweight bridge so you can use HuskyLens face, object, or tag recognition as sensors/inputs in Home Assistant automations.

Features
--------
- Uses HuskyLens library over I2C/Serial to receive recognition events
- Connects ESP32 to an MQTT broker and publishes events/state
- Designed to be easily integrated into Home Assistant via MQTT discovery or manual configuration

Prerequisites
-------------
- ESP32 development board (flashed with Arduino core or PlatformIO)
- DFRobot HuskyLens
- MQTT broker (e.g., Mosquitto, or Home Assistant built-in MQTT)
- Home Assistant instance
- Arduino IDE or PlatformIO with ESP32 board support

Wiring (example)
-----------------
Note: adapt pins to your board and preferred interface (I2C or UART).
- HuskyLens VCC -> 3.3V (check your HuskyLens version)
- HuskyLens GND -> GND
- HuskyLens SDA -> ESP32 SDA pin (or GPIO21)
- HuskyLens SCL -> ESP32 SCL pin (or GPIO22)
- If using serial: HuskyLens TX -> ESP32 RX, HuskyLens RX -> ESP32 TX

Configuration
-------------
1. Configure MQTT broker settings in the sketch (broker address, port, username/password if required).
2. Set your Wi-Fi SSID and password in the sketch.
3. Optionally adjust topic names and payload formats to match your Home Assistant setup.

Example MQTT topics
--------------------
- huskylens/<device>/status — online/offline
- huskylens/<device>/recognition — payload with recognized id/label
- huskylens/<device>/raw — raw data (optional)

Example Home Assistant (manual MQTT sensor)
------------------------------------------
```yaml
# Example: sensor configuration for a recognized face/label
sensor:
  - platform: mqtt
    name: "HuskyLens Last Seen"
    state_topic: "huskylens/esp32/recognition"
    value_template: "{{ value_json.label }}"
    json_attributes_topic: "huskylens/esp32/recognition"
```

Usage
-----
1. Open the sketch in Arduino IDE / PlatformIO, update Wi‑Fi and MQTT credentials.
2. Upload to your ESP32 board.
3. Watch the serial monitor for boot and MQTT connection logs.
4. When HuskyLens recognizes an object/face, the ESP32 will publish the recognition event to the configured MQTT topic.
5. Create Home Assistant sensors/automations from the published topics.

Notes and troubleshooting
-------------------------
- Ensure the HuskyLens uses the same logic level as the ESP32 (3.3V). Using 5V without a level shifter can damage the HuskyLens or ESP32.
- If you do not see MQTT messages, verify the broker is reachable from the ESP32 and credentials are correct.
- Use the serial monitor to inspect debug logs.

Contributing
------------
Contributions, bug reports and feature requests are welcome — please open an issue or a pull request.

License
-------
This project is provided under the MIT License. See LICENSE for details.

Acknowledgements
-----------------
- DFRobot HuskyLens
- Home Assistant community
