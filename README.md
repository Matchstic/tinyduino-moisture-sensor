# TinyDuino WiFi Moisture Sensor
A moisture sensor built with the TinyDuino platform, and a [SparkFun soil sensor](https://www.sparkfun.com/products/13322). 

It is designed to work alongside [homebridge-http-moisture-sensor](https://github.com/Matchstic/homebridge-http-moisture-sensor) to display readings inside the Home app on iOS and macOS.

### Hardware

The only hardware required is the ATWINC1500 TinyShield, a prototyping TinyShield, and the mentioned sensor.

Connect the sensor as follows:

- SIG -> analogue 0
- VCC -> digital 7
- GND -> GND

### Setup

Libraries required:
- [WiFi101](https://www.arduino.cc/en/Reference/WiFi101)
- [Low-Power](https://github.com/rocketscream/Low-Power)

You will need to create a file named `arduino_secrets.h` next to the `.ino` file, with the following contents:

```
#define SECRET_WIFI_SSID "<ssid>"
#define SECRET_WIFI_PASS "<wireless_password>"
#define SECRET_SERVER_ADDRESS 192,168,1,2
```

Make sure to adjust each parameter to match your environment. The `SECRET_SERVER_ADDRESS` parameter should point to a static IP where your Homebridge instance can be found.

Furthermore, `homebridge-http-notification-server` should be using the default port `8080` to accept incoming data, and the "notification ID" in use can be modified in `notify_remote()` of this sketch - it is currently `moisture-sensor`.
