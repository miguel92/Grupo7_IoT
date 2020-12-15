
/*
 * Universidad de Málaga, Máster en Ingeniería Informática
 * IoT - Grupo 7 
 * 
*/
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"
#include <ArduinoJson.h>
#include <ESP8266httpUpdate.h>
#include "config.h"
#include "Button2.h";
#define BUTTON_PIN  0
// Update these with values suitable for your network.
ADC_MODE(ADC_VCC);

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
unsigned long tiempoEsperaActu = 600000;
unsigned long lastActu = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;
DHTesp dht;
//int boton_flash=0;       // GPIO0 = boton flash
int estado_polling=HIGH; // por defecto HIGH (PULLUP). Cuando se pulsa se pone a LOW
int estado_int=HIGH;     // por defecto HIGH (PULLUP). Cuando se pulsa se pone a LOW
volatile unsigned long ahora=0;
volatile unsigned long ultima_int = 0;
volatile boolean pulsado = false;
unsigned int ESPID = ESP.getChipId();

void progreso_OTA(int,int);
void final_OTA();
void inicio_OTA();
void error_OTA(int);

Button2 button = Button2(BUTTON_PIN);

struct registro_datos { // Estructura de datos que recoge todas las lecturas necesarias para el topic de Datos
  float temperatura;
  float humedad;
  float vcc;
  int led;
  unsigned long uptime;
  String ssid;
  String ip;
  long rssi;
} datos;

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
   char *mensaje=(char* )malloc(length+1); // reservo memoria para copia del mensaje
  strncpy(mensaje,(char*)payload,length); // copio el mensaje en cadena de caracteres

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // compruebo que es el topic adecuado
  if(strcmp(topic,"infind/GRUPO7/led/cmd")==0)
  {
    StaticJsonDocument<512> root; // el tamaño tiene que ser adecuado para el mensaje
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(root, mensaje);

    // Compruebo si no hubo error
    if (error) {
      Serial.print("Error deserializeJson() failed: ");
      Serial.println(error.c_str());
    }
    else
    if(root.containsKey("level"))  // comprobar si existe el campo/clave que estamos buscando
    {
     int valor = root["level"];
     Serial.print("Mensaje OK, level = ");
     Serial.println(valor);
     
     analogWrite(BUILTIN_LED,((100-valor)*1023/100)); // Se manda el valor analogico al LED PWM para variar su intensidad. Como el valor 0 es maxima intensidad y 1023 es apagado, hacemos la conversion para que vaya de 0 a 100(de menos a mas intensidad)
     client.publish("infind/GRUPO7/led/status",serializa_JSON_LED(valor).c_str()); //Se serializa la informacion y se envia por el topic de status
    }
    else
    {
      Serial.print("Error : ");
      Serial.println("\"level\" key not found in JSON");
    }
  } // if topic

  snprintf(msg, 128, "infind/GRUPO7/ESP%d/FOTA", ESPID);
  
  if(strcmp(topic,msg)==0){
      actualizacionOTA();
    }
  else
  {
    Serial.println("Error: Topic desconocido");
  }

  free(mensaje); // libero memoria
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(ESP.getChipId()); // ID en base a la ID del propio CHIP del ESP, en lugar de aleatoria

    char* willTopic = "infind/GRUPO7/conexion";
    unsigned int QoS = 2;
    boolean willRetain = true;
    char* willMessage = "Offline";
    boolean cleanSession = false;
    char* online = "Online";
    
    // Attempt to connect
    if (client.connect(clientId.c_str(), "","", willTopic, QoS,willRetain, willMessage, cleanSession)) { // Aqui se configura el mensaje de ultimas voluntades, cuando la maquina se cierre de manera abrupta el broker tendra retenido este mensaje
      client.publish(willTopic, online, true);
      Serial.println("connected");
      client.subscribe("infind/GRUPO7/led/cmd");
      
      snprintf(msg, 128, "infind/GRUPO7/ESP%d/FOTA", ESPID); // Suscripción a actualizaciones
      client.subscribe(msg);
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

String serializa_JSON_Datos (struct registro_datos datos)
{
  StaticJsonDocument<300> jsonRoot;
  String jsonString;
 
  jsonRoot["Uptime"]= datos.uptime;
  jsonRoot["Voltaje"]= datos.vcc;
  JsonObject DHT11=jsonRoot.createNestedObject("DHT11");
  DHT11["temp"] = datos.temperatura;
  DHT11["hum"] = datos.humedad;
  jsonRoot["LED"]= datos.led;
  JsonObject Wifi=jsonRoot.createNestedObject("Wifi");
  Wifi["SSID"] = datos.ssid;
  Wifi["IP"] = datos.ip;
  Wifi["RSSI"] = datos.rssi;
  
  
  serializeJson(jsonRoot,jsonString);
  return jsonString;
}

String serializa_JSON_LED(int led){
  StaticJsonDocument<300> jsonRoot;
  String jsonString;
  jsonRoot["level"]= led;

  serializeJson(jsonRoot,jsonString);
  return jsonString;
  }


String publicarDatos(){ // Esta funcion recoge todas las lecturas y los datos necesarios de conexion, los serializa y devuelve un String
  datos.humedad = dht.getHumidity();
  datos.temperatura = dht.getTemperature();
  datos.vcc = ESP.getVcc();
  datos.uptime = millis();
  datos.led = analogRead(BUILTIN_LED);
  datos.ssid = WiFi.SSID();
  datos.ip = WiFi.localIP().toString();
  datos.rssi = WiFi.RSSI();

  String json = serializa_JSON_Datos(datos);


  return json;
}

void final_OTA()
{
  Serial.println("Fin OTA. Reiniciando...");
}

void inicio_OTA()
{
  Serial.println("Nuevo Firmware encontrado. Actualizando...");
}

void error_OTA(int e)
{
  char cadena[64];
  snprintf(cadena,64,"ERROR: %d",e);
  Serial.println(cadena);
}

void progreso_OTA(int x, int todo)
{
  char cadena[256];
  int progress=(int)((x*100)/todo);
  if(progress%10==0)
  {
    snprintf(cadena,256,"Progreso: %d%% - %dK de %dK",progress,x/1024,todo/1024);
    Serial.println(cadena);
  }
}

void actualizacionOTA(){
    Serial.println( "---------------------------" );  
  Serial.println( "Comprobando actualización:" );
  Serial.print(HTTP_OTA_ADDRESS);Serial.print(":");Serial.print(HTTP_OTA_PORT);Serial.println(HTTP_OTA_PATH);
  Serial.println( "--------------------------" );  
  ESPhttpUpdate.setLedPin(16,LOW);
  ESPhttpUpdate.onStart(inicio_OTA);
  ESPhttpUpdate.onError(error_OTA);
  ESPhttpUpdate.onProgress(progreso_OTA);
  ESPhttpUpdate.onEnd(final_OTA);
  switch(ESPhttpUpdate.update(HTTP_OTA_ADDRESS, HTTP_OTA_PORT, HTTP_OTA_PATH, HTTP_OTA_VERSION)) {
    case HTTP_UPDATE_FAILED:
      Serial.printf(" HTTP update failed: Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(F(" El dispositivo ya está actualizado"));
      break;
    case HTTP_UPDATE_OK:
      Serial.println(F(" OK"));
      break;
    }
  }
  
void longpress(Button2& btn) {
    unsigned int time = btn.wasPressedFor();
    
    if (time > 5000) {
        Serial.print("Pulsación larga de 5 segundos: Se va a lanzar la actualización ");
        actualizacionOTA();
        
    }
}
void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  dht.setup(5, DHTesp::DHT11);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  Serial.print("ID del CHIP ESP: ");
  Serial.println(ESPID);
  actualizacionOTA();
  
  //Se configura el boton flash con interrupcion para permitir actualizar cuando se pulse al menos 5 seg
  button.setLongClickHandler(longpress);

}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  button.loop();
  
  unsigned long now = millis();

  if (now - lastMsg > 30000){ // Se envian los datos cada 30 segundos
      lastMsg = now;
      String datosJSON = publicarDatos();
      Serial.println(datosJSON.c_str());
      client.publish("infind/GRUPO7/datos", datosJSON.c_str());
    }
  
  if(now - lastActu > tiempoEsperaActu){
    lastActu = now;
    actualizacionOTA();
  }
}
