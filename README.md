# Read an AC Power Meter using IR Pulses

Many AC power meters (e.g., residential power meeter on a house) have an infrared (IR) LED that blinks every watt-hour.

This Arduino code calculates power from the flashing LED and publishes the power/energy information as an MQTT message.

This code runs on an ESP8266/NodeMCU.  For example,

https://www.amazon.com/gp/product/B01IK9GEQG/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&th=1

# Usage

A `secrets.h` file is required in the same folder as the .ino file (supplied by user, not in repo).  This file must contain the SSID and password of the WiFi network used:

```
#define SECRETS_WIFI_SSID           "your-wifi-ssid"
#define SECRETS_WIFI_PASSWORD       "your-wifi-password"
```
