#include "main.h"

// Variables used to store data values read from sensors
uint32_t microphoneData[MICROPHONE_SAMPLE_RATE / DATA_SEND_RATE + DATA_BUFFER_SIZE];
uint32_t lightData[LIGHT_SAMPLE_RATE / DATA_SEND_RATE + DATA_BUFFER_SIZE];
uint32_t temperatureData[TEMP_HUMIDITY_SAMPLE_RATE / DATA_SEND_RATE + DATA_BUFFER_SIZE];
uint32_t humidityData[TEMP_HUMIDITY_SAMPLE_RATE / DATA_SEND_RATE + DATA_BUFFER_SIZE];
si7021_env envData;

// Variables used to store the size of valid data in the sensor data arrays
size_t microphoneDataSize = 0;
size_t lightDataSize = 0;
size_t temperatureDataSize = 0;
size_t humidityDataSize = 0;

// Used to read from Si7021 sensor
SI7021 sensor;

// Variable used to hold data from protobuf encode
uint8_t outputBuffer[512];
size_t message_length;
bool message_status;
pb_ostream_t outputStream;
SensorData outputData;

// Variables used to hold data for CSPRNG seeding
uint8_t microphone_entropy[MICROPHONE_ENTROPY_SIZE];
uint8_t microphone_entropy_fullness;

// Function prototypes for task callback functions
void readMicCallback();
void readLightCallback();
void readTempHumidityCallback();
void sendDataPacketCallback();
void sendClientServiceMessageCallback();
void listenForUDPPacketCallback();
void NTPSyncCallback();
void oneTimePasswordGenerationCallback();

// function prototype for ntpUnixTime
uint64_t ntpUnixTime(WiFiUDP &udp);

// Task objects used for task scheduling
Task readMic(MS_PER_SECOND / MICROPHONE_SAMPLE_RATE, TASK_FOREVER, &readMicCallback);
Task readLight(MS_PER_SECOND / LIGHT_SAMPLE_RATE, TASK_FOREVER, &readLightCallback);
Task readTempHumidity(MS_PER_SECOND / TEMP_HUMIDITY_SAMPLE_RATE, TASK_FOREVER, &readTempHumidityCallback);
Task sendDataPacket(MS_PER_SECOND / DATA_SEND_RATE, TASK_FOREVER, &sendDataPacketCallback);
Task sendClientServiceMessage(MS_PER_SECOND * ADVERTISEMENT_RATE, TASK_FOREVER, &sendClientServiceMessageCallback);
Task NTPSync(MS_PER_SECOND * NTP_SYNC_TIMEOUT, TASK_FOREVER, &NTPSyncCallback);
Task oneTimePasswordGeneration(OTP_ENTROPY_WAIT_TIME, TASK_FOREVER, &oneTimePasswordGenerationCallback);

// Set up a scheduler to schedule all of these tasks
Scheduler taskRunner;

// An IPAddress to hold the computed broadcast IP
IPAddress broadcastIP;

// An IPAddress to hold the server that we will send our UDP packets to
IPAddress serverIP(0, 0, 0, 0);

// A UDP instance
WiFiUDP udp;

// Set up a secure server instance
WiFiServerSecure webServer(443);

// Create a variable to hold the Unix time during setup, as well as a variable to hold the current millis when
// we got this time. This way we can compute current NTP time by comparison
uint64_t NTPSyncTime;
uint64_t NTPSyncMillis;

// Set up the server magic string in a constant string variable
const char serverMagicString[] = SERVER_SERVICE_MESSAGE;

void setup() {
  // Set up serial
  Serial.begin(38400);

  // Set mux pins to output mode
  pinMode(MUX_A_PIN, OUTPUT);
  pinMode(MUX_B_PIN, OUTPUT);

  // Start the I2C connection with the si7021
  sensor.begin(SDA_PIN, SCL_PIN);

  // Connect to the wifi
  Serial.print("Connecting to:");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Wait until the wifi is connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("LocalIP: ");
  Serial.println(WiFi.localIP());

  // Compute the broadcast IP
  broadcastIP = ~WiFi.subnetMask() | WiFi.gatewayIP();

  // Start up the UDP service
  udp.begin(UDP_PORT);

  // Set server key
  webServer.setServerKeyAndCert_P(rsakey, sizeof(rsakey), x509, sizeof(x509));

  webServer.begin();

  // Add all the tasks to the runner and enable them
  taskRunner.addTask(readMic);
  taskRunner.addTask(readLight);
  taskRunner.addTask(readTempHumidity);
  taskRunner.addTask(sendDataPacket);
  taskRunner.addTask(sendClientServiceMessage);
  taskRunner.addTask(NTPSync);
  taskRunner.addTask(oneTimePasswordGeneration);
  NTPSync.enable();
  readMic.enable();
  readLight.enable();
  readTempHumidity.enable();
  sendDataPacket.enable();
  sendClientServiceMessage.enable();
  // DEBUG
  oneTimePasswordGeneration.enable();
  // Note that oneTimePasswordGeneration starts off as disabled

  // Set the initial microphone entropy fullness to 0
  microphone_entropy_fullness = 0;
}

// Helper function that configures the input select lines of the MUX so that the mic is feeding data to A0
void select_microphone() {
  digitalWrite(MUX_A_PIN, HIGH);
  digitalWrite(MUX_B_PIN, LOW);
}

// Helper function that configures the input select lines of the MUX so that the light sensor is feeding data to A0
void select_light() {
  digitalWrite(MUX_A_PIN, LOW);
  digitalWrite(MUX_B_PIN, HIGH);
}

void readMicCallback() {
  select_microphone();
  uint32_t microphone_read = analogRead(ANALOG_PIN);
  microphoneData[microphoneDataSize] = microphone_read;
  microphoneDataSize = microphoneDataSize + 1;

  // Put this data into the microphone entropy array if it's not full
  if (microphone_entropy_fullness != MICROPHONE_ENTROPY_SIZE) {
    addDataToMicrophoneEntropy(microphone_read);
  }
}

void addDataToMicrophoneEntropy(uint32_t data) {
  // Lo-fi solution, fancy ones didn't work :(
  uint8_t extracted_byte;
  for (int i = 0; i < 4; i++) {
    extracted_byte = (data >> (8 * i)) & 0xff;
    microphone_entropy[microphone_entropy_fullness] = extracted_byte;
    microphone_entropy_fullness += 1;
  };
}

void readLightCallback() {
  select_light();
  lightData[lightDataSize] = analogRead(ANALOG_PIN);
  lightDataSize = lightDataSize + 1;
}

void readTempHumidityCallback() {
  envData = sensor.getHumidityAndTemperature();
  temperatureData[temperatureDataSize] = envData.celsiusHundredths;
  humidityData[humidityDataSize] = envData.humidityBasisPoints;
  temperatureDataSize = temperatureDataSize + 1;
  humidityDataSize = humidityDataSize + 1;
}

// Send the special UDP packaet advertising this device as a light pet client device
void sendClientServiceMessageCallback() {
  udp.beginPacket(broadcastIP, UDP_PORT);
  udp.write(CLIENT_SERVICE_MESSAGE);
  udp.endPacket();
}

void NTPSyncCallback() {
  // Get the time from an NTP server and set NTP sync values appropriately
  static WiFiUDP timeUDP;
  NTPSyncTime = ntpUnixTime(timeUDP);
  NTPSyncMillis = (uint64_t) millis();
}

void sendDataPacketCallback() {
  // Zero our sensor data object
  outputData = SensorData_init_zero;
  // Set some fields in the output data
  // Compute the current NTP time with currentmillis - syncmillis + synctime * 1000 (since synctime is in seconds)
  // FIXME: This will have issues if millis overflows back to 0. This should probably trigger an NTP sync. Set this
  // up once we have a ntp sync task
  outputData.timestamp = ((uint64_t) millis()) - NTPSyncMillis + (NTPSyncTime * 1000);
  outputData.has_timestamp = true;

  outputData.temperatureSampleRate = TEMP_HUMIDITY_SAMPLE_RATE;
  outputData.has_temperatureSampleRate = true;

  outputData.humiditySampleRate = TEMP_HUMIDITY_SAMPLE_RATE;
  outputData.has_humiditySampleRate = true;

  outputData.audioSampleRate = MICROPHONE_SAMPLE_RATE;
  outputData.has_audioSampleRate = true;

  outputData.lightSampleRate = LIGHT_SAMPLE_RATE;
  outputData.has_lightSampleRate = true;

  outputData.chipID = ESP.getChipId();
  outputData.has_chipID = true;

  // Setup the callback functions for the variable length packed data segments
  outputData.temperatureData.funcs.encode = &encodePackedArray;
  outputData.humidityData.funcs.encode = &encodePackedArray;
  outputData.audioData.funcs.encode = &encodePackedArray;
  outputData.lightData.funcs.encode = &encodePackedArray;

  // Setup the arguments for these functions
  ArrayWithSize arg[4];
  arg[0].array = (uint32_t**)&temperatureData;
  arg[0].arraySize = temperatureDataSize;
  outputData.temperatureData.arg = &(arg[0]);

  arg[1].array = (uint32_t**)&humidityData;
  arg[1].arraySize = humidityDataSize;
  outputData.humidityData.arg = &(arg[1]);

  arg[2].array = (uint32_t**)&lightData;
  arg[2].arraySize = lightDataSize;
  outputData.lightData.arg = &(arg[2]);
 
  arg[3].array = (uint32_t**)&microphoneData;
  arg[3].arraySize = microphoneDataSize;
  outputData.audioData.arg = &(arg[3]);

  // Setup the output stream for our protobuf encode to output to our outputBuffer
  outputStream = pb_ostream_from_buffer(outputBuffer, sizeof(outputBuffer));

  // Encode the data to the stream
  message_status = pb_encode(&outputStream, SensorData_fields, &outputData);
  message_length = outputStream.bytes_written;

  // Depending on the status, write a different thing to the serial monitor
  if (!message_status) {
    Serial.println("Encoding failed");
    Serial.println(PB_GET_ERROR(&outputStream));
  }
  else {
    // If we have a real server, not just the default 0.0.0.0 IP
    if (serverIP != IPAddress(0, 0, 0, 0)) {
      // Send the encoded data from the output buffer
      udp.beginPacket(serverIP, UDP_PORT);
      udp.write(outputBuffer, message_length);
      udp.endPacket();
    }

    if (DEBUG_MODE) {
      // Print the data to the serial
      for (int i = 0; i < message_length; i++) {
        Serial.print(outputBuffer[i]);
        Serial.print(" ");
      }
      Serial.print("\n");
    }
  }

  // Reset the values that we use to track the size of our data arrays to zero
  temperatureDataSize = 0;
  humidityDataSize = 0;
  lightDataSize = 0;
  microphoneDataSize = 0;
}

void oneTimePasswordGenerationCallback() {
  if (microphone_entropy_fullness == MICROPHONE_ENTROPY_SIZE) {
    // NOTE: This code is almost directly from the Spritz-Cipher examples

    spritz_ctx hash_ctx; /* For the hash */
    spritz_ctx rng_ctx; /* For the random bytes generator */

    uint8_t digest[32]; /* Hash result, 256-bit */
    uint8_t buf[32];

    uint8_t i;
    uint16_t j;
    uint16_t LOOP_ROUNDS = 1024; /* 32 KB: (1024 * 32) / sizeof(buf) */

    /* Make a 256-bit hash of the entropy in "buf" using one function */
    spritz_hash(buf, (uint8_t)(sizeof(buf)), microphone_entropy, (uint8_t)(sizeof(microphone_entropy)));
    /* Initialize/Seed the RNG with the hash of entropy */
    spritz_setup(&rng_ctx, buf, (uint8_t)(sizeof(buf)));

    /* The data will be generated in small chunks,
    * So we can not use "one function API" */
    spritz_hash_setup(&hash_ctx);

    for (j = 0; j < LOOP_ROUNDS; j++) {
      /* Fill buf with Spritz random bytes generator output */
      for (i = 0; i < (uint8_t)(sizeof(buf)); i++) {
        buf[i] = spritz_random8(&rng_ctx);
      }
      /* Add buf data to hash_ctx */
      spritz_hash_update(&hash_ctx, buf, (uint16_t)(sizeof(buf)));
    }

    /* Output the final hash */
    spritz_hash_final(&hash_ctx, digest, (uint8_t)(sizeof(digest)));

    Serial.println("One time password:");

    /* Print the hash in HEX */
    for (i = 0; i < (uint8_t)(sizeof(digest)); i++) {
      if (digest[i] < 0x10) { /* To print "0F", not "F" */
        Serial.write('0');
      }
      Serial.print(digest[i], HEX);
    }
    Serial.println();

    /* wipe "hash_ctx" data by replacing it with zeros (0x00) */
    spritz_state_memzero(&hash_ctx);

    /* wipe "digest" data by replacing it with zeros (0x00) */
    spritz_memzero(digest, (uint16_t)(sizeof(digest)));

    /* Keys, RNG seed & buffer should be wiped in realworld.
    * In this example we should not wipe "entropy"
    * cause it is embedded in the code */
    spritz_state_memzero(&rng_ctx);
    spritz_memzero(buf, (uint16_t)(sizeof(buf)));
    spritz_memzero(microphone_entropy, (uint16_t)(sizeof(microphone_entropy)));
    // Generate OTP and print, based on entroy
    // Disable this task 
    oneTimePasswordGeneration.disable();
  }
  // This task will run periodically until it has enough entropy
}

bool encodePackedArray(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
  // First, set up a substream that we will use to figure out the length of our data
  pb_ostream_t dummySubstream = PB_OSTREAM_SIZING;
  size_t dataLength;

  ArrayWithSize *localArg = (ArrayWithSize*) *arg;

  // Write all of the data from the temperature data array to the dummy substream, then check the bytes written
  size_t arraySize = localArg->arraySize;
  uint32_t **localValues = localArg->array;
  for (int i = 0; i < arraySize; i++) {
    pb_encode_varint(&dummySubstream, (uint64_t) localValues[i]);
  }

  // Get the bytes written
  dataLength = dummySubstream.bytes_written;

  // Now we can actually do the encoding on the real stream
  // First encode the tag and field type. Since this is a packed array, we use the PB_WT_STRING type to denote that
  // it is length delimited
  if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
    return false;
  }

  // Next encode the data length we just found
  if (!pb_encode_varint(stream, (uint64_t) dataLength)) {
    return false;
  }

  // Finally encode the data as varints
  for (int i = 0; i < arraySize; i++) {
    if (! pb_encode_varint(stream, (uint64_t) localValues[i])) {
      return false;
    }
  }

  return true;
}

void loop() {
  taskRunner.execute();
  WiFiClientSecure client = webServer.available();
  if (!client) {
    return;
  }

  unsigned long timeout = millis() + 3000;
  while(!client.available() && millis() < timeout) {
    delay(1);
  }
  if (millis() > timeout) {
    Serial.println("timeout");
    client.flush();
    client.stop();
    return;
  }

  // Read the first line of the request
  String req = client.readStringUntil('\r');
  Serial.println(req);
  client.flush();
  
  client.flush();

  // Prepare the response
  String s = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello world from arduino!\n";

  // Send the response to the client
  client.print(s);
  delay(1);
  Serial.println("Client disonnected");

  // The client will actually be disconnected 
  // when the function returns and 'client' object is detroyed
}
