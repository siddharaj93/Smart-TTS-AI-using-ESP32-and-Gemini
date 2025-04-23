/*connections -
ESP32 Dev Module     MAX98357A I2S Amplifier
VIN                  3.3V
GND                  GND
D27                  BCLK
D26                  LRC
D25                  DIN/DOUT
GAIN                 GND

Speaker+             Amplifier +
speaker-             Amplifier -

IMPORTANT - GO TO TOOLS THEN PARTITION SCHEME AND SELECT - No OTA(2MB APP/2MB SPIFFS)(recommended)OR Huge APP(3MB No OTA/1MB SPIFFS)
Note:Use a 4 ohm 3 watt woffer/speaker
*/

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Audio.h"//Download the library from - https://github.com/schreibfaul1/ESP32-audioI2S/blob/master/src/Audio.h (hold the ctrl key and clik the link)

void setup();
void loop();
void speakAnswer(String answer);
void audio_info(const char *info);
bool isEndOfSentence(char c);

const char* ssid = "Your_WiFi_SSID";//Enter Your WiFi SSID
const char* password = "Your_WiFi_Password";//Enter Your WiFi Password
const char* gemini_api_key = "Your_Gemini_API_Key";//Get your Gemini API key at - https://aistudio.google.com/app/apikey (hold the ctrl key and clik the link)
String Question = "";

#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

Audio audio;

bool isEndOfSentence(char c) {
    return c == ' ' || c == '.' || c == '?' || c == '!' || c == ',';
}

void speakAnswer(String answer) {
    const int chunkSize = 93;
    int len = answer.length();
    int start = 0;
    String nextChunk;
    
    while (start < len) {
        String chunk;
        if (start + chunkSize >= len) {
            chunk = answer.substring(start);
            start = len;
        } else {
            int end = start + chunkSize;
            while (end > start && !isEndOfSentence(answer.charAt(end))) {
                end--;
            }
            chunk = answer.substring(start, end);
            start = end + 1;
            
            if (start < len) {
                int nextEnd = start + chunkSize;
                if (nextEnd > len) nextEnd = len;
                nextChunk = answer.substring(start, nextEnd);
            }
        }
        
        audio.connecttospeech(chunk.c_str(), "en");//It plays the Response from the Woofer/Speaker
        while(audio.isRunning()) {
            audio.loop();
        }
    }
}

void setup() {
    Serial.begin(115200);
    
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    while (!Serial);
    
    WiFi.begin(ssid, password);
    Serial.print("Connecting to ");
    Serial.println(ssid);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(85);//can be set according to your liking
    audio.setTone(0, 0, 0);
    audio.forceMono(true);
}

void loop() {
    Serial.print("Ask your Question : ");
    while (!Serial.available()) {
        audio.loop();
    }
    
    while (Serial.available()) {
        char add = Serial.read();
        Question = Question + add;
        delay(1);
    }
    
    int len = Question.length();
    Question = Question.substring(0, (len - 1));
    
    HTTPClient https;
    String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=" + String(gemini_api_key);//Note-The Model of Gemini can change with Time. The current version of Gemini model now is 2.0 flash.
    
    if (https.begin(url)) {
        https.addHeader("Content-Type", "application/json");
        https.setTimeout(30000);
        
        String payload = "{\"contents\":[{\"parts\":[{\"text\":\"" + Question + "\"}]}],\"generationConfig\":{\"temperature\":0.7,\"maxOutputTokens\":1024,\"topP\":0.8}}";
        Serial.println("Sending: " + payload);
        
        int retryCount = 0;
        int httpCode;
        
        while (retryCount < 3) {
            httpCode = https.POST(payload);
            if (httpCode == HTTP_CODE_OK) {
                break;
            }
            Serial.printf("Attempt %d failed, retrying...\n", retryCount + 1);
            delay(1000);
            retryCount++;
        }
        
        Serial.printf("HTTP Response code: %d\n", httpCode);
        
        if (httpCode == HTTP_CODE_OK) {
            String response = https.getString();
            Serial.println("Raw response: " + response);
            
            DynamicJsonDocument doc(1024);//can be set according to your liking
            DeserializationError error = deserializeJson(doc, response);
            
            if (!error) {
                String Answer = doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
                Answer.replace("*", "");
                Answer.replace("  ", " ");
                
                Serial.print("Answer: ");
                Serial.println(Answer);
                
                speakAnswer(Answer);
            } else {
                Serial.println("JSON parsing failed");
            }
        } else {
            Serial.printf("Error: %s\n", https.errorToString(httpCode).c_str());
        }
        https.end();
    }
    
    Question = "";
}

void audio_info(const char *info) {
    Serial.print("audio_info: ");
    Serial.println(info);
}
