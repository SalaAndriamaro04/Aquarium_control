#include <OneWire.h>
#include <DallasTemperature.h>

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ESP_Mail_Client.h>

#include "time.h"
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define API_KEY "AIzaSyB2OoT9iDo5D_jsezMQedViOSsGXYNMnIM"
#define DATABASE_URL "https://envoidb-424a8-default-rtdb.europe-west1.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0; 
int count = 0; 
bool signupOK = false;

const char* ssid = "DESKTOP-QE692P6 9742";
const char* password = "N36{259a";

const int oneWireBus = 15;     
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

int numberOfDevices;
DeviceAddress tempDeviceAddress;

#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "salajaona04@gmail.com"
#define AUTHOR_PASSWORD "ghes knbv qwvg ycfr"
#define RECIPIENT_EMAIL "fayolmanah@gmail.com"

SMTPSession smtp;
void smtpCallback(SMTP_Status status);

bool emailSent[2] = {false,false};  // Drapeau pour indiquer si un email a été envoyé

float valeur_min = 0.0;
float valeur_max = 0.0;

String parentPath;
int timestamp;
FirebaseJson json;
const char* ntpServer = "pool.ntp.org";
// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

void setup(){
    Serial.begin(115200);
    delay(1000);
 configTime(0, 0, ntpServer);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("\nConnecting");

    while(WiFi.status() != WL_CONNECTED){
        Serial.print(".");
        delay(100);
    }

    Serial.println("\nConnected to the WiFi network");
    Serial.print("Local ESP32 IP: ");
    Serial.println(WiFi.localIP());

    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    if (Firebase.signUp(&config, &auth, "", "")){
      Serial.println("ok"); 
      signupOK = true; 
    } else {
      Serial.printf("%s\n", config.signer.signupError.message.c_str()); 
    }

    config.token_status_callback = tokenStatusCallback; 
    Firebase.begin(&config, &auth); 
    Firebase.reconnectWiFi(true);

    sensors.begin();
    numberOfDevices = sensors.getDeviceCount();
  
    Serial.print("Locating devices...");
    Serial.print("Found ");
    Serial.print(numberOfDevices, DEC);
    Serial.println(" devices.");

}

void loop(){
  sensors.requestTemperatures();
 //Get current timestamp
            timestamp = getTime();
            Serial.print ("time: ");
            Serial.println (timestamp);
  for(int i = 0; i < numberOfDevices; i++) {
      if(sensors.getAddress(tempDeviceAddress, i)){
          Serial.print("Temperature for device: ");
          Serial.println(i, DEC);
          
          float tempC = sensors.getTempC(tempDeviceAddress);
          
          if (Firebase.ready() && signupOK){
            String chemin = "aquarium/temper_eau_capt" + String(i);
            parentPath = "aquarium/" + String(timestamp);
            json.set(chemin.c_str(), String(tempC));
              if (Firebase.RTDB.setFloat(&fbdo, chemin.c_str(), tempC)){
                  Serial.println("PASSED");
                  Serial.println("PATH: " + fbdo.dataPath());
                  Serial.println("TYPE: " + fbdo.dataType());
              } else {
                  Serial.println("FAILED");
                  Serial.println("REASON: " + fbdo.errorReason());
              }

              if (Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json)) {
                Serial.println("PASSED");
                Serial.println("PATH: " + fbdo.dataPath());
                Serial.println("TYPE: " + fbdo.dataType());
                } else {
                Serial.println("FAILED");
                Serial.println("REASON: " + fbdo.errorReason());
                }
              Serial.println("Temp C: ");
              Serial.print(tempC);

              if (Firebase.RTDB.getFloat(&fbdo, "aquarium/valeur_min")){
                  valeur_min = fbdo.floatData();
              }
              Serial.println("valeur minimum: ");
              Serial.print(valeur_min);

              if (Firebase.RTDB.getFloat(&fbdo, "aquarium/valeur_max")){
                  valeur_max = fbdo.floatData();
              }
              Serial.println("valeur maximum: ");
              Serial.print(valeur_max);
              if(tempC < valeur_min || tempC > valeur_max){
                if(!emailSent[i]){
                    MailClient.networkReconnect(true);
                    smtp.debug(1);
                    smtp.callback(smtpCallback);

                    Session_Config config;
                    config.server.host_name = SMTP_HOST;
                    config.server.port = SMTP_PORT;
                    config.login.email = AUTHOR_EMAIL;
                    config.login.password = AUTHOR_PASSWORD;
                    config.login.user_domain = "";

                    config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
                    config.time.gmt_offset = 3;
                    config.time.day_light_offset = 0;
                    SMTP_Message message;
                    message.sender.name = F("Aquarium");
                    message.sender.email = AUTHOR_EMAIL;
                    message.subject = F("Alerte Temperature Aquarium");
                    message.addRecipient(F("Aquarium"), RECIPIENT_EMAIL);

                    String textMsg = "Bonjour! - Envoyer par ESP.  ";
                    textMsg += "Température actuelle: " + String(tempC) + " °C\n";
                    message.text.content = textMsg.c_str();
                    message.text.charSet = "us-ascii";
                    message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

                    message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_high;
                    message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

                    if (!smtp.connect(&config)){
                        ESP_MAIL_PRINTF("Connection error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
                        return;
                    }
                    
                    if (smtp.isLoggedIn()){
                        if (smtp.isAuthenticated())
                        Serial.println("\nSuccessfully logged in.");
                        else
                        Serial.println("\nConnected with no Auth.");
                    }
                    /* Start sending Email and close the session */
                    if (!MailClient.sendMail(&smtp, &message))
                        ESP_MAIL_PRINTF("Error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
                    }
                    emailSent[i] = true;
                }
                else{
                    emailSent[i] = false;
                }
                 
              }
          }
      }
  sendDataPrevMillis = millis();
  delay(3000);
}

void smtpCallback(SMTP_Status status){
    Serial.println(status.info());

    if (status.success()){
        Serial.println("----------------");
        ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
        ESP_MAIL_PRINTF("Message sent failed: %d\n", status.failedCount());
        Serial.println("----------------\n");

        for (size_t i = 0; i < smtp.sendingResult.size(); i++){
            SMTP_Result result = smtp.sendingResult.getItem(i);
            ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
            ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
            ESP_MAIL_PRINTF("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
            ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
            ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
        }
        Serial.println("----------------\n");
        smtp.sendingResult.clear();
    }
}