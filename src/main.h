#include <Arduino.h>

#include <pb_common.h>
#include <pb.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "sensorData.pb.h"
#include "configuration.h"

#include <TaskScheduler.h>
#include <Wire.h>
#include <SI7021.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// The following file should #define two values, WIFI_SSID and WIFI_PASSWORD, the ssid and pass for the network you
// want to connect to
#include "wifi.credentials.h"

// Relevant pin definitions. MUX pins are as per datasheet
#define MUX_A_PIN D3
#define MUX_B_PIN D4
#define SCL_PIN D1
#define SDA_PIN D2
#define ANALOG_PIN A0

// Just to avoid magic numbers, define the ms in a second
#define MS_PER_SECOND 1000

// Make a typdef to define an array with associated size. This will be used to encode packed arrays in protobufs
typedef struct _ArrayWithSize {
  uint32_t **array;
  size_t arraySize;
} ArrayWithSize;

bool encodePackedArray(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);