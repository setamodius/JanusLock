#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <U8g2lib.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <PubSubClient.h>

#define OLED_SCL 6
#define OLED_SDA 5

#define LED_PIN 8
#define RESET_BUTTON_PIN 9
#define RESET_HOLD_MS 5000

#define DOOR1_PIN 4
#define DOOR2_PIN 7
#define PULSE_MS 500

#define CONFIG_FILE "/config.json"

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(
    U8G2_R0,
    U8X8_PIN_NONE,
    OLED_SCL,
    OLED_SDA
);

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

struct AppConfig
{
    char mqttHost[64] = "";
    char mqttPort[8] = "1883";
    char mqttUser[32] = "";
    char mqttPass[32] = "";
    char topicPrefix[40] = "keremhome";
};

AppConfig config;
bool shouldSaveConfig = false;

bool mqttOk = false;
String door1State = "READY";
String door2State = "READY";

char deviceStatusTopic[96];
char door1CommandTopic[96];
char door1StatusTopic[96];
char door2CommandTopic[96];
char door2StatusTopic[96];

void buildTopics()
{
    snprintf(deviceStatusTopic, sizeof(deviceStatusTopic), "%s/device/status", config.topicPrefix);

    snprintf(door1CommandTopic, sizeof(door1CommandTopic), "%s/door1/command", config.topicPrefix);
    snprintf(door1StatusTopic,  sizeof(door1StatusTopic),  "%s/door1/status",  config.topicPrefix);

    snprintf(door2CommandTopic, sizeof(door2CommandTopic), "%s/door2/command", config.topicPrefix);
    snprintf(door2StatusTopic,  sizeof(door2StatusTopic),  "%s/door2/status",  config.topicPrefix);
}

void show3(const char* a, const char* b = "", const char* c = "")
{
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(0, 10, a);
    u8g2.drawStr(0, 23, b);
    u8g2.drawStr(0, 36, c);
    u8g2.sendBuffer();
}

void showDashboard()
{
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x8_tf);

    u8g2.drawStr(0, 8, mqttOk ? "JANUS MQTT OK" : "JANUS OFFLINE");

    String d1 = "D1: " + door1State;
    String d2 = "D2: " + door2State;

    u8g2.drawStr(0, 22, d1.c_str());
    u8g2.drawStr(0, 36, d2.c_str());

    u8g2.sendBuffer();
}

void saveConfigCallback()
{
    shouldSaveConfig = true;
}

bool loadConfig()
{
    buildTopics();

    if (!LittleFS.exists(CONFIG_FILE))
        return false;

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file)
        return false;

   JsonDocument  doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
        return false;

    strlcpy(config.mqttHost, doc["mqttHost"] | "", sizeof(config.mqttHost));
    strlcpy(config.mqttPort, doc["mqttPort"] | "1883", sizeof(config.mqttPort));
    strlcpy(config.mqttUser, doc["mqttUser"] | "", sizeof(config.mqttUser));
    strlcpy(config.mqttPass, doc["mqttPass"] | "", sizeof(config.mqttPass));
    strlcpy(config.topicPrefix, doc["topicPrefix"] | "keremhome", sizeof(config.topicPrefix));

    buildTopics();
    return true;
}

bool saveConfig()
{
    JsonDocument doc;

    doc["mqttHost"] = config.mqttHost;
    doc["mqttPort"] = config.mqttPort;
    doc["mqttUser"] = config.mqttUser;
    doc["mqttPass"] = config.mqttPass;
    doc["topicPrefix"] = config.topicPrefix;

    File file = LittleFS.open(CONFIG_FILE, "w");
    if (!file)
        return false;

    serializeJsonPretty(doc, file);
    file.close();

    return true;
}

void factoryReset()
{
    show3("Factory", "Resetting...", "");

    WiFiManager wm;
    wm.resetSettings();

    LittleFS.remove(CONFIG_FILE);

    delay(1000);
    ESP.restart();
}

void checkResetButton()
{
    static unsigned long pressStart = 0;
    static bool shown = false;

    bool pressed = digitalRead(RESET_BUTTON_PIN) == LOW;

    if (pressed)
    {
        if (pressStart == 0)
            pressStart = millis();

        unsigned long held = millis() - pressStart;

        if (!shown && held > 1000)
        {
            show3("Hold BOOT", "Factory reset", "");
            shown = true;
        }

        if (held > RESET_HOLD_MS)
            factoryReset();
    }
    else
    {
        pressStart = 0;
        shown = false;
    }
}

bool isTriggerCommand(String msg)
{
    msg.trim();
    msg.toLowerCase();

    return msg == "open" ||
           msg == "press" ||
           msg == "trigger" ||
           msg == "1";
}

void publishDoorReady(uint8_t door)
{
    if (door == 1)
    {
        door1State = "READY";
        mqtt.publish(door1StatusTopic, "ready", true);
    }
    else
    {
        door2State = "READY";
        mqtt.publish(door2StatusTopic, "ready", true);
    }

    showDashboard();
}

void triggerDoor(uint8_t door)
{
    int pin;
    const char* statusTopic;

    if (door == 1)
    {
        pin = DOOR1_PIN;
        statusTopic = door1StatusTopic;
        door1State = "TRIG";
    }
    else
    {
        pin = DOOR2_PIN;
        statusTopic = door2StatusTopic;
        door2State = "TRIG";
    }

    showDashboard();
    mqtt.publish(statusTopic, "triggered", true);

    digitalWrite(pin, HIGH);
    digitalWrite(LED_PIN, HIGH);

    delay(PULSE_MS);

    digitalWrite(pin, LOW);
    digitalWrite(LED_PIN, LOW);

    publishDoorReady(door);
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
    String msg;

    for (unsigned int i = 0; i < length; i++)
        msg += (char)payload[i];

    Serial.print("MQTT [");
    Serial.print(topic);
    Serial.print("] ");
    Serial.println(msg);

    if (!isTriggerCommand(msg))
        return;

    if (String(topic) == door1CommandTopic)
        triggerDoor(1);
    else if (String(topic) == door2CommandTopic)
        triggerDoor(2);
}

void connectMqtt()
{
    if (strlen(config.mqttHost) == 0)
    {
        mqttOk = false;
        show3("MQTT Host", "empty", "");
        return;
    }

    mqtt.setServer(config.mqttHost, atoi(config.mqttPort));
    mqtt.setCallback(mqttCallback);

    while (!mqtt.connected())
    {
        checkResetButton();

        mqttOk = false;
        showDashboard();

        bool ok;

        if (strlen(config.mqttUser) > 0)
        {
            ok = mqtt.connect(
                "janus-garage-c3",
                config.mqttUser,
                config.mqttPass,
                deviceStatusTopic,
                1,
                true,
                "offline"
            );
        }
        else
        {
            ok = mqtt.connect(
                "janus-garage-c3",
                deviceStatusTopic,
                1,
                true,
                "offline"
            );
        }

        if (ok)
        {
            mqttOk = true;

            mqtt.subscribe(door1CommandTopic);
            mqtt.subscribe(door2CommandTopic);

            mqtt.publish(deviceStatusTopic, "online", true);
            mqtt.publish(door1StatusTopic, "ready", true);
            mqtt.publish(door2StatusTopic, "ready", true);

            door1State = "READY";
            door2State = "READY";

            showDashboard();

            Serial.println("MQTT connected");
            Serial.print("Device status: ");
            Serial.println(deviceStatusTopic);
            Serial.print("Door1 command: ");
            Serial.println(door1CommandTopic);
            Serial.print("Door2 command: ");
            Serial.println(door2CommandTopic);
        }
        else
        {
            show3("MQTT failed", "Retrying...", "");
            Serial.print("MQTT failed rc=");
            Serial.println(mqtt.state());
            delay(3000);
        }
    }
}

void setupWifiAndConfig()
{
    loadConfig();

    WiFiManager wm;
    wm.setSaveConfigCallback(saveConfigCallback);

    WiFiManagerParameter p_mqttHost(
        "mqttHost",
        "MQTT Host",
        config.mqttHost,
        sizeof(config.mqttHost)
    );

    WiFiManagerParameter p_mqttPort(
        "mqttPort",
        "MQTT Port",
        config.mqttPort,
        sizeof(config.mqttPort)
    );

    WiFiManagerParameter p_mqttUser(
        "mqttUser",
        "MQTT User",
        config.mqttUser,
        sizeof(config.mqttUser)
    );

    WiFiManagerParameter p_mqttPass(
        "mqttPass",
        "MQTT Password",
        config.mqttPass,
        sizeof(config.mqttPass),
        "type='password'"
    );

    WiFiManagerParameter p_topicPrefix(
        "topicPrefix",
        "Topic Prefix",
        config.topicPrefix,
        sizeof(config.topicPrefix)
    );

    wm.addParameter(&p_mqttHost);
    wm.addParameter(&p_mqttPort);
    wm.addParameter(&p_mqttUser);
    wm.addParameter(&p_mqttPass);
    wm.addParameter(&p_topicPrefix);

    wm.setAPCallback([](WiFiManager* wm)
    {
        show3("Connect WiFi:", "Janus-Setup", "192.168.4.1");
    });

    show3("WiFi", "Connecting...", "");

    bool connected = wm.autoConnect("Janus-Setup");

    if (!connected)
    {
        show3("WiFi failed", "Restarting", "");
        delay(2000);
        ESP.restart();
    }

    strlcpy(config.mqttHost, p_mqttHost.getValue(), sizeof(config.mqttHost));
    strlcpy(config.mqttPort, p_mqttPort.getValue(), sizeof(config.mqttPort));
    strlcpy(config.mqttUser, p_mqttUser.getValue(), sizeof(config.mqttUser));
    strlcpy(config.mqttPass, p_mqttPass.getValue(), sizeof(config.mqttPass));
    strlcpy(config.topicPrefix, p_topicPrefix.getValue(), sizeof(config.topicPrefix));

    buildTopics();

    if (shouldSaveConfig)
        saveConfig();

    show3("WiFi OK", WiFi.localIP().toString().c_str(), "Config saved");
}

void setup()
{
    Serial.begin(115200);
    delay(1500);

    pinMode(LED_PIN, OUTPUT);
    pinMode(DOOR1_PIN, OUTPUT);
    pinMode(DOOR2_PIN, OUTPUT);
    pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

    digitalWrite(LED_PIN, LOW);
    digitalWrite(DOOR1_PIN, LOW);
    digitalWrite(DOOR2_PIN, LOW);

    u8g2.begin();
    show3("JANUS", "Starting...", "");

    LittleFS.begin(true);

    setupWifiAndConfig();

    connectMqtt();
}

void loop()
{
    checkResetButton();

    if (WiFi.status() == WL_CONNECTED)
    {
        if (!mqtt.connected())
        {
            mqttOk = false;
            showDashboard();
            connectMqtt();
        }

        mqtt.loop();
    }
    else
    {
        mqttOk = false;
        show3("WiFi lost", "Reconnecting", "");
    }

    delay(10);
}