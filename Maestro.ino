#include "Wire.h"
#include <ArduinoJson.hpp>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#if defined(ESP32)
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
  #define DEVICE "ESP32"
  #elif defined(ESP8266)
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "ESP8266"
  #endif

  #include <InfluxDbClient.h>
  #include <InfluxDbCloud.h>
  
  // WiFi AP SSID
  #define WIFI_SSID "RedLSP"
  // WiFi password
  #define WIFI_PASSWORD "LSPCOVISA"

  const char *ssid = "RedLSP";
  const char *password = "LSPCOVISA";

// InfluxDB v2 server url, e.g. https://eu-central-1-1.aws.cloud2.influxdata.com (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "http://192.168.100.44:8086/"
// InfluxDB v2 server or cloud API token (Use: InfluxDB UI -> Data -> API Tokens -> <select token>)
#define INFLUXDB_TOKEN "G9SxjpwRSZ5f8AQpEFMbx--FPniLJOIjqttTT75s_G6kjw9a72us_NxIZm97vEyXu3JzURVDiBVMwxJYsPqD7g=="
// InfluxDB v2 organization id (Use: InfluxDB UI -> User -> About -> Common Ids )
#define INFLUXDB_ORG "laboratorio3"
// InfluxDB v2 bucket name (Use: InfluxDB UI ->  Data -> Buckets)
#define INFLUXDB_BUCKET "laboratorio3"

// Time zone info
#define TZ_INFO "UTC-3"
  
// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
  
// Declare Data point
Point sensor("wifi_status");
Point sensorReadings("measurements");

const byte I2C_SLAVE_ADDR = 0x8;

String response;
StaticJsonDocument<300> doc;


void setup()
{
  Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  
    Serial.print("Connecting to wifi");
    while (wifiMulti.run() != WL_CONNECTED) {
      Serial.print(".");
      delay(100);
    }
    Serial.println();

        // Add tags to the data point
    sensor.addTag("device", DEVICE);
    sensor.addTag("SSID", WiFi.SSID());
  
    // Accurate time is necessary for certificate validation and writing in batches
    // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
    // Syncing progress and the time will be printed to Serial.
    timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  
  
    // Check server connection
    if (client.validateConnection()) {
      Serial.print("Connected to InfluxDB: ");
      Serial.println(client.getServerUrl());
    } else {
      Serial.print("InfluxDB connection failed: ");
      Serial.println(client.getLastErrorMessage());
    }

  Wire.begin(D2, D1); //SDA=D2, SCL=D1
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("");

  Serial.println("Conexión establecida");
}

void loop()
{
   // Clear fields for reusing the point. Tags will remain the same as set above.
  sensor.clearFields();
  
  askSlave();
  if(response != "") DeserializeResponse();
  delay(2000);
  obtener_datos_grupo_1();
  obtener_datos_grupo_2();
  obtener_datos_grupo_4();
   // Check WiFi connection and reconnect if needed
    if (wifiMulti.run() != WL_CONNECTED) {
      Serial.println("Wifi connection lost");
    }
  
    // Write point
    if (!client.writePoint(sensor)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());
    }
  
  // Clear fields for next usage. Tags remain the same.
  sensorReadings.clearFields();

    Serial.println("Waiting 1 second");
    delay(1000);

}

const char ASK_FOR_LENGTH = 'L';
const char ASK_FOR_DATA = 'D';

void askSlave()
{
  response = "";

  unsigned int responseLenght = askForLength();
  if (responseLenght == 0) return;

  askForData(responseLenght);
  delay(2000);
}

unsigned int askForLength()
{
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write(ASK_FOR_LENGTH);
  Wire.endTransmission();

  Wire.requestFrom(I2C_SLAVE_ADDR, 1);
  unsigned int responseLenght = Wire.read();
  return responseLenght;
}

void askForData(unsigned int responseLenght)
{
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write(ASK_FOR_DATA);
  Wire.endTransmission();

  for (int requestIndex = 0; requestIndex <= (responseLenght / 32); requestIndex++)
  {
    Wire.requestFrom(I2C_SLAVE_ADDR, requestIndex < (responseLenght / 32) ? 32 : responseLenght % 32);
    while (Wire.available())
    {
      response += (char)Wire.read();
    }
  }
}

const char* text;
int id;
bool ventilador;
float temperatura;
void DeserializeResponse()
{
  DeserializationError error = deserializeJson(doc, response);
    if (error) { return; }
 
text = doc["text"];
   // id = doc["id"];
    ventilador = doc["ventilador"];
    temperatura = doc["temperatura"];
   sensor.addField("ventilador", ventilador);
   sensor.addField("temperatura", temperatura);


    Serial.println(text);
  //  Serial.println(id);
  Serial.print("Ventilador: ");
    Serial.println(ventilador);
     Serial.print("Temperatura: ");
    Serial.println(temperatura);
    Serial.println("Fin lectura I2C");
}

void obtener_datos_grupo_1(){
  WiFiClient client;  // Crea un objeto WiFiClient para usar con HTTPClient
  HTTPClient http;
  http.begin(client, "http://192.168.100.40:80/"); // Reemplaza "ip_del_servidor" con la IP del ESP8266 servidor
  int httpCode = http.GET();

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      // Parsea el JSON recibido
      StaticJsonDocument<200> jsonDoc;
      deserializeJson(jsonDoc, payload);
      // Obtén los valores del JSON
      int inclinacion = jsonDoc["Inclinacion"];
      float  motorEncendido = jsonDoc["Motor"];
      String  potenciometro = jsonDoc["Ubi_Potenciometro"];

      sensor.addField("Inclinacion", inclinacion);
      sensor.addField("MotorInclinacion", motorEncendido);
      sensor.addField("Ubi_Potenciometro", potenciometro);
      // Imprime los valores
      Serial.println("ESCLAVO HTTP");
      Serial.print("Inclinación: ");
      Serial.println(inclinacion);
      Serial.print("Motor: ");
      Serial.println(motorEncendido ? "Encendido" : "Apagado");
      Serial.print("Ubi_Potenciometro: ");
      Serial.println(potenciometro);
        Serial.println("Fin lectura HTTP");
    }
  } else {
    Serial.println("Error en la solicitud HTTP");
  }

  http.end();
 
}

void obtener_datos_grupo_2(){
  WiFiClient client;  // Crea un objeto WiFiClient para usar con HTTPClient
  HTTPClient http;
  http.begin(client, "http://192.168.100.46/"); // Reemplaza "ip_del_servidor" con la IP de la Raspberry Servidor
  int httpCode = http.GET();

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();

      // Parsear el JSON recibido
      StaticJsonDocument<200> jsonDoc;
      deserializeJson(jsonDoc, payload);

      // Obtener los valores del JSON
      String estado_maquina = jsonDoc["estado_maquina"];
      float velocidad_maquina = jsonDoc["velocidad_maquina"];
      String sensor_proximidad = jsonDoc["sensor_prox"];

      sensor.addField("estado_maquina", estado_maquina);
      sensor.addField("velocidad_maquina", velocidad_maquina);
      sensor.addField("sensor_proximidad", sensor_proximidad);

      // Imprimir los valores
      Serial.println("Esclavo Raspberry-2 via HTTP");
      Serial.print("Estado de la Maquina: ");
      Serial.println(estado_maquina);
      Serial.print("Velocidad de la Maquina: ");
      Serial.println(velocidad_maquina);
      Serial.print("Sensor de Proximidad: ");
      Serial.println(sensor_proximidad);
      Serial.println("Fin Lectura Esclavo Raspberry-2 via HTTP");
    }
  } else {
    Serial.println("Error en la solicitud HTTP del Esclavo Raspberry-2");
  }

  http.end();
}

void obtener_datos_grupo_4() {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, "http://192.168.100.42:5000/");

  int httpCode = http.GET();

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();

      // Parse the JSON received
      StaticJsonDocument<200> jsonDoc;
      DeserializationError error = deserializeJson(jsonDoc, payload);

      if (error) {
        Serial.print("JSON parsing error: ");
        Serial.println(error.c_str());
        return;
      }

      // Extract values from the JSON
      int humedad = jsonDoc["humedad"];
      String sensorValue = jsonDoc["sensor"];

      // Add fields to the JSON document
      sensor.addField("humedad", humedad);
      sensor.addField("sensor", sensorValue);


      // Print the values
      Serial.println("Esclavo Raspberry-4 via HTTP");
      Serial.print("Humedad: ");
      Serial.println(humedad);
      Serial.print("Sensor: ");
      Serial.println(sensorValue);
      Serial.println("Fin Lectura Esclavo Raspberry-4 via HTTP");

      // Now you can use the sensorReadings JSON document as needed
      // For example, you can send it to InfluxDB or perform other actions.
    }
  } else {
    Serial.println("Error in HTTP request for Esclavo Raspberry-4");
  }

  http.end();
}
