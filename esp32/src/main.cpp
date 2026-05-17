#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>

#include "esp_ota_ops.h"
#include "esp_partition.h"
#include <WiFiManager.h>
#include <WiFiClientSecure.h>

WiFiManager wm;

#define BOOT_PIN 0
#define LED_PIN 2

String manifestURL =
    "https://esp32-ai-system.onrender.com/device_config";

String baseURL =
    "https://esp32-ai-system.onrender.com/";

String firmwareURL =
    "https://esp32-ai-system.onrender.com/uploads/firmware.bin";

String CURRENT_VERSION = "0.0.0";

void jumpToRuntime()
{
    Serial.println("Starting Runtime Firmware");

    const esp_partition_t* partition =
        esp_partition_find_first(
            ESP_PARTITION_TYPE_APP,
            ESP_PARTITION_SUBTYPE_APP_OTA_1,
            NULL);

    if (partition == NULL)
    {
        Serial.println("Runtime partition not found");

        return;
    }

    esp_err_t result =
        esp_ota_set_boot_partition(partition);

    if (result == ESP_OK)
    {
        Serial.println("Boot partition switched");

        delay(1000);

        ESP.restart();
    }
    else
    {
        Serial.println("Partition switch failed");
    }
}

void connectWiFi()
{
    Serial.println();
    Serial.println("Starting WiFi Manager");

    bool result =
        wm.autoConnect(
            "ESP32_MANAGER",
            "sh9ez29a"
        );

    if(!result)
    {
        Serial.println("WiFi Failed");

        ESP.restart();
    }

    Serial.println("WiFi Connected!");
    Serial.print("ESP IP: ");
    Serial.println(WiFi.localIP());
}

bool performOTA(String firmwareURL)
{
    WiFiClient client;

    HTTPClient http;

    Serial.println();
    Serial.println("Starting OTA");

    Serial.println(firmwareURL);

    http.begin(client, firmwareURL);

    int httpCode = http.GET();

    Serial.print("HTTP Code: ");
    Serial.println(httpCode);

    if (httpCode != HTTP_CODE_OK)
    {
        Serial.println("Firmware download failed");

        http.end();

        return false;
    }

    int contentLength = http.getSize();

    Serial.print("Firmware Size: ");
    Serial.println(contentLength);

    const esp_partition_t* partition =
        esp_partition_find_first(
            ESP_PARTITION_TYPE_APP,
            ESP_PARTITION_SUBTYPE_APP_OTA_1,
            NULL);

    if (partition == NULL)
    {
        Serial.println("OTA partition not found");

        http.end();

        return false;
    }

    if (!Update.begin(partition->size))
    {
        Serial.println("Not enough OTA space");

        http.end();

        return false;
    }

    WiFiClient* stream = http.getStreamPtr();

    size_t written =
        Update.writeStream(*stream);

    if (written != contentLength)
    {
        Serial.println("OTA write failed");

        http.end();

        return false;
    }

    if (!Update.end())
    {
        Serial.println(Update.errorString());

        http.end();

        return false;
    }

    Serial.println();
    Serial.println("================================");
    Serial.println("NEW FIRMWARE INSTALLED");
    Serial.println("PRESS RESET TO START");
    Serial.println("================================");

    esp_ota_set_boot_partition(partition);

    http.end();

    while(true)
    {
        digitalWrite(LED_PIN, HIGH);
        delay(200);

        digitalWrite(LED_PIN, LOW);
        delay(200);
    }

    return true;
}

void sendStatus()
{
    HTTPClient statusHttp;

    WiFiClientSecure statusClient;

    statusClient.setInsecure();

    String statusURL =
    "https://esp32-ai-system.onrender.com/esp32_status?name=ESP32_WROVER";

    statusHttp.begin(statusClient, statusURL);

    int statusCode =
        statusHttp.GET();

    Serial.print("Status HTTP Code: ");
    Serial.println(statusCode);

    statusHttp.end();
}

void recoveryMode()
{
    Serial.println();
    Serial.println("RECOVERY MODE");

    connectWiFi();

    HTTPClient http;

    WiFiClientSecure client;

    client.setInsecure();

    Serial.println();
    Serial.println("Requesting manifest");

    http.begin(client, manifestURL);

    int httpCode = http.GET();

    Serial.print("Manifest HTTP Code: ");
    Serial.println(httpCode);

    if (httpCode != 200)
    {
        Serial.println("Manifest request failed");

        http.end();

        return;
    }

    String payload = http.getString();

    Serial.println();
    Serial.println("Manifest:");

    Serial.println(payload);

    DynamicJsonDocument doc(512);

    DeserializationError error =
        deserializeJson(doc, payload);

    if (error)
    {
        Serial.println("JSON Parse Failed");

        http.end();

        return;
    }

    String latestVersion =
        doc["version"];

    String firmwarePath =
        doc["firmware"];

    String firmwareURL =
        baseURL + firmwarePath;

    Serial.println();
    Serial.print("Current Version: ");
    Serial.println(CURRENT_VERSION);

    Serial.print("Latest Version: ");
    Serial.println(latestVersion);

    Serial.println("WAITING FOR OTA COMMAND");

    http.end();

    while(true)
    {
        sendStatus();

        delay(10000);

        HTTPClient cmdHttp;

        WiFiClientSecure cmdClient;

        cmdClient.setInsecure();

        String commandURL =
        "https://esp32-ai-system.onrender.com/device_command";

        cmdHttp.setTimeout(10000);
        cmdHttp.begin(cmdClient, commandURL);

        int cmdCode =
            cmdHttp.GET();

        Serial.print("Command HTTP Code: ");
        Serial.println(cmdCode);

        if(cmdCode == 200)
        {
            String cmdPayload =
                cmdHttp.getString();

            Serial.println(cmdPayload);

            DynamicJsonDocument cmdDoc(512);

            deserializeJson(cmdDoc, cmdPayload);

            bool update =
                cmdDoc["update"];

            if(update)
            {
                String fw =
                    cmdDoc["firmware"];

                String otaURL =
                    baseURL + fw;

                Serial.println("OTA COMMAND RECEIVED");

                performOTA(otaURL);
            }
        }

        cmdHttp.end();
    }
}

void blinkRGB()
{
    pinMode(LED_PIN, OUTPUT);

    while(true)
    {
        digitalWrite(LED_PIN, HIGH);
        delay(300);

        digitalWrite(LED_PIN, LOW);
        delay(300);
    }
}

unsigned long pressStart = 0;

void checkRecoveryButton()
{
    if(digitalRead(BOOT_PIN) == LOW)
    {
        if(pressStart == 0)
        {
            pressStart = millis();
        }

        if(millis() - pressStart > 3000)
        {
            Serial.println("ENTERING RECOVERY");

            recoveryMode();
        }
    }
    else
    {
        pressStart = 0;
    }
}

void setup()
{
    Serial.begin(115200);

    pinMode(BOOT_PIN, INPUT_PULLUP);

    delay(1000);

    const esp_partition_t* running =
        esp_ota_get_running_partition();

    Serial.println();
    Serial.println("CURRENT PARTITION:");

    Serial.println(running->label);

    bool bootPressed =
        digitalRead(BOOT_PIN) == LOW;

    // =========================
    // FACTORY MANAGER
    // =========================

    if(
        running->subtype ==
        ESP_PARTITION_SUBTYPE_APP_FACTORY
    )
    {
        Serial.println();
        Serial.println("FACTORY RECOVERY MANAGER");

        if(bootPressed)
        {
            Serial.println();
            Serial.println("ENTERING RECOVERY MODE");

            recoveryMode();
        }
        else
        {
            Serial.println();
            Serial.println("NO OTA FIRMWARE");

            blinkRGB();
        }
    }

    // =========================
    // OTA RUNTIME
    // =========================

    else
    {
        Serial.println();
        Serial.println("RUNTIME FIRMWARE ACTIVE");

        if(bootPressed)
        {
            Serial.println();
            Serial.println("RETURNING TO FACTORY");

            const esp_partition_t* factory =
                esp_partition_find_first(
                    ESP_PARTITION_TYPE_APP,
                    ESP_PARTITION_SUBTYPE_APP_FACTORY,
                    NULL);

            if(factory != NULL)
            {
                esp_ota_set_boot_partition(factory);

                delay(1000);

                ESP.restart();
            }
        }
    }
}

void loop()
{
    checkRecoveryButton();
}