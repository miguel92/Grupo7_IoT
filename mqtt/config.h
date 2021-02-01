// config.h
// datos para actualización   >>>> SUSTITUIR IP <<<<<

#define HTTP_OTA_ADDRESS      F("192.168.1.142")       // Address of OTA update server
#define HTTP_OTA_PATH         F("/esp8266-ota/update") // Path to update firmware
#define HTTP_OTA_PORT         1880                     // Port of update server
#define HTTP_OTA_VERSION   String(__FILE__).substring(String(__FILE__).lastIndexOf('\\')+1)+".nodemcu"

//Descomentar este bloque para actualizaciones en local y comentar el otro
#define OTA_URL ""
String OTAfingerprint("");

/*
#define OTA_URL "https://iot.ac.uma.es:1880/esp8266-ota/update"// Address of OTA update server
String OTAfingerprint("5D 56 09 5C 5F 7B A4 3F 01 B7 22 31 D3 A7 DA A3 6E 10 2E 60"); // sustituir valor
*/

const char* ssid = "";
const char* password = "";
const char* mqtt_server = "iot.ac.uma.es";
const char* mqtt_user = "infind";
const char* mqtt_pass = "zancudo";
