/**
 * @copyright Copyright (c) 2024  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2024-04-05
 * @note      Arduino Setting
 *            Tools ->
 *                  Board:"ESP32S3 Dev Module"
 *                  USB CDC On Boot:"Enable"
 *                  USB DFU On Boot:"Disable"
 *                  Flash Size : "16MB(128Mb)"
 *                  Flash Mode"QIO 80MHz
 *                  Partition Scheme:"16M Flash(3M APP/9.9MB FATFS)"
 *                  PSRAM:"OPI PSRAM"
 *                  Upload Mode:"UART0/Hardware CDC"
 *                  USB Mode:"Hardware CDC and JTAG"
 *
 */

#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
#endif

#include <Arduino.h>
#include "epd_driver.h"
#include "firasans.h"
#include "courb36.h"
#include "courb28.h"
#include "courb20.h"
#include "cour28.h"
#include "cour24.h"
#include "cour20.h"
#include "courb16.h"
#include "cour16.h"
#include "esp_adc_cal.h"
#include <FS.h>
#include <SPI.h>
#include <SD.h>
#include "Button2.h" //Arduino IDE -> Library manager -> Install Button2
#include <Wire.h>
#include <TouchDrvGT911.hpp> //Arduino IDE -> Library manager -> Install SensorLib v0.19
#include <SensorPCF8563.hpp>
#include <WiFi.h>
#include <esp_sntp.h>
#include "utilities.h"
#include "pillow.h"
#include <PubSubClient.h>
#include "secrets.h"
#include <ArduinoJson.h>
#include "icons.h"

/////////////////// WIFI //////////////////////////
// #ifndef WIFI_SSID
// #define WIFI_SSID ""
// #endif

// #ifndef WIFI_PASSWORD
// #define WIFI_PASSWORD ""
// #endif

/////////////////// MQTT //////////////////////////
// const char *mqtt_server = "";
// const int mqtt_port = 1883;
// const char *mqtt_username = "";
// const char *mqtt_password = "";
// const char *mqtt_pub_topic = "";
// const char *mqtt_sub_topic = "";

unsigned long lastMsg = 0;
#define MQTT_BUFFER_SIZE (16384)
#define MISC_MSG_SIZE (1024)
char msg[MISC_MSG_SIZE];

/////////////////// TIME //////////////////////////
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
const char *time_zone = "CST-1"; // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)

/////////////////// DEVICES //////////////////////////
Button2 btn(BUTTON_1);
SensorPCF8563 rtc;
TouchDrvGT911 touch;
WiFiClient espClient;
PubSubClient client(espClient);

/////////////////// VARS //////////////////////////
uint8_t *framebuffer = NULL;
bool touchOnline = false;
// uint32_t big_interval = 30000; // 30sec
uint32_t big_interval = 60000; // 1min
// uint32_t big_interval = 300000; // 5min
uint32_t small_interval = 300;
uint32_t big_interval_timestamp = 0;
uint32_t small_interval_timestamp = 0;
int vref = 1100;
char buf[128];
uint32_t pressed_cnt = 0;
uint16_t touch_registered = 0;
int16_t last_x = 0;
int16_t last_y = 0;

struct _point
{
    uint8_t buttonID;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} touchPoint[] = {
    {0, 10, 10, 80, 80},
    {1, EPD_WIDTH - 80, 10, 80, 80},
    {2, 10, EPD_HEIGHT - 80, 80, 80},
    {3, EPD_WIDTH - 80, EPD_HEIGHT - 80, 80, 80},
    {4, EPD_WIDTH / 2 - 60, EPD_HEIGHT - 80, 120, 80},
    {5, EPD_WIDTH / 2 - 60, 10, 120, 80}};

const uint8_t *weather_icons[] = {
    // Unkown
    w0_data,
    // Day
    w1_data, w2_data, w3_data, w4_data, w5_data, w6_data, w7_data, w8_data, w9_data, w10_data, w11_data, w12_data, w13_data, w14_data, w15_data, w16_data,
    // Night
    w101_data, w102_data, w103_data, w104_data, w105_data, w106_data, w107_data, w108_data, w109_data, w110_data, w111_data, w112_data, w113_data, w114_data, w115_data, w116_data};

#define MAX_WEATHER_DATA 3
struct weather_data
{
    char date[32];
    float temp;
    float precip;
    uint8_t icon_idx;
} weather[MAX_WEATHER_DATA];

#define MAX_DB_DATA 5
struct db_data
{
    char scheduledDeparture[32];
    char destination[128];
    char train[12];
    int delayDeparture;
    int isCancelled;
} db[MAX_DB_DATA];

#define MAX_CAL_DATA 5
struct cal_data
{
    char date[32];
    char time[32];
    char title[128];
} cal[MAX_CAL_DATA];

/////////////////// DEVICE FUNCTIONS //////////////////////////
void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
}

void timeavailable(struct timeval *t)
{
    Serial.println("[WiFi]: Got time adjustment from NTP!");
    rtc.hwClockWrite();
}

void buttonPressed(Button2 &b)
{
    Serial.println("Button1 Pressed!");
    int32_t cursor_x = 200;
    int32_t cursor_y = 450;

    Rect_t area = {
        .x = 200,
        .y = 410,
        .width = 400,
        .height = 50,
    };

    epd_poweron();

    epd_clear_area(area);
    snprintf(buf, 128, "➸ Pressed : btn VN:%u", pressed_cnt++);
    writeln((GFXfont *)&FiraSans, buf, &cursor_x, &cursor_y, NULL);
    epd_poweroff_all();
}

void mqtt_rec(char *topic, byte *payload, unsigned int length)
{
    // Serial.printf("Message arrived in topic: %s\n", topic);
    // Serial.printf("Message (%d):", length);
    // Serial.write(payload, length);
    // Serial.println();
    // Serial.println("-----------------------");

    // Parse the JSON message
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        deep_sleep("JSON parse error");
    }

    // Extract the values
    JsonArray entries = doc["entries"];
    for (JsonVariant entry : entries)
    {
        if (entry["type"] == "weather" && entry["status"] == "ok")
        {
            JsonArray data = entry["data"];
            int i = 0;
            for (JsonVariant item : data)
            {
                if (i >= MAX_WEATHER_DATA)
                {
                    break;
                }
                strncpy(weather[i].date, item["date"], sizeof(weather[i].date));
                weather[i].temp = item["temp"];
                weather[i].precip = item["precip"];
                weather[i].icon_idx = item["icon-idx"];
                i++;
            }
        }

        if (entry["type"] == "db" && entry["status"] == "ok")
        {
            JsonArray data = entry["data"];
            int i = 0;
            for (JsonVariant item : data)
            {
                if (i >= MAX_DB_DATA)
                {
                    break;
                }
                strncpy(db[i].scheduledDeparture, item["scheduledDeparture"], sizeof(db[i].scheduledDeparture));
                strncpy(db[i].destination, item["destination"], sizeof(db[i].destination));
                strncpy(db[i].train, item["train"], sizeof(db[i].train));
                db[i].delayDeparture = item["delayDeparture"];
                db[i].isCancelled = item["isCancelled"];
                i++;
            }
        }

        if (entry["type"] == "calendar" && entry["status"] == "ok")
        {
            JsonArray data = entry["data"];
            int i = 0;
            for (JsonVariant item : data)
            {
                if (i >= MAX_CAL_DATA)
                {
                    break;
                }
                strncpy(cal[i].date, item["date"], sizeof(cal[i].date));
                // strncpy(cal[i].time, item["time"], sizeof(cal[i].time));
                strncpy(cal[i].title, item["title"], sizeof(cal[i].title));
                i++;
            }
        }
    }

    Serial.println("Weather data:");
    for (int i = 0; i < MAX_WEATHER_DATA; i++)
    {
        Serial.printf("Date: %s, Temp: %.1f, Precip: %.1f, Icon: %d\n",
                      weather[i].date, weather[i].temp, weather[i].precip, weather[i].icon_idx);
    }

    Serial.println("DB data:");
    for (int i = 0; i < MAX_DB_DATA; i++)
    {
        Serial.printf("Scheduled Departure: %s, Destination: %s, Train: %s, Delay: %d, Cancelled: %d\n",
                      db[i].scheduledDeparture, db[i].destination, db[i].train, db[i].delayDeparture, db[i].isCancelled);
    }

    Serial.println("Calendar data:");
    for (int i = 0; i < MAX_CAL_DATA; i++)
    {
        Serial.printf("Date: %s, Title: %s\n",
                      cal[i].date, cal[i].title);
    }

    draw_layout();
    draw_rtc_time();
    draw_rtc_date();
    draw_weather();
    draw_db();
    draw_cal();
}

void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientId = "ESP8266Client-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect
        if (client.connect(clientId.c_str()))
        {
            Serial.println("connected");
            // Once connected, publish an announcement...
            client.publish(mqtt_pub_topic, "Reconnected");
            // ... and resubscribe
            client.subscribe(mqtt_sub_topic);
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

/////////////////// DISPLAY FUNCTIONS //////////////////////////
void deep_sleep(const char *msg)
{
    Serial.println("Sleep !!!!!!");
    Serial.printf("Msg: %s\n", msg);

    epd_poweron();

    epd_clear();
    int32_t cursor_x = 0;
    int32_t cursor_y = 0;
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    delay(1000);
    Rect_t area = {
        .x = 224,
        .y = 14,
        .width = pillow_width,
        .height = pillow_height,
    };
    epd_draw_grayscale_image(area, (uint8_t *)pillow_data);
    epd_draw_image(area, (uint8_t *)pillow_data, BLACK_ON_WHITE);

    if (strlen(msg) != 0)
    {
        cursor_x = 20;
        cursor_y = 60;
        writeln((GFXfont *)&FiraSans, msg, &cursor_x, &cursor_y, NULL);
    }

    epd_poweroff_all();

    WiFi.disconnect(true);
    touch.sleep();
    delay(100);
    Wire.end();
    Serial.end();

    // BOOT(STR_IO0) Button wakeup
    esp_sleep_enable_ext1_wakeup(_BV(0), ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
}

void deep_clean()
{
    Serial.println("Cleaning...");
    int32_t i = 0;

    Rect_t area = epd_full_screen();
    epd_poweron();
    delay(10);
    epd_clear();
    for (i = 0; i < 20; i++)
    {
        epd_push_pixels(area, 50, 0);
        delay(100);
    }
    epd_clear();
    for (i = 0; i < 40; i++)
    {
        epd_push_pixels(area, 50, 1);
        delay(100);
    }
    epd_clear();
    epd_poweroff_all();
    Serial.println("Cleaning OK");
}

/////////////////// DRAW FUNCTIONS //////////////////////////
void draw_touch_buttons()
{
    epd_poweron();

    // Draw buttons
    FontProperties props = {
        .fg_color = 15,
        .bg_color = 0,
        .fallback_glyph = 0,
        .flags = 0};

    // A
    int32_t x = 18;
    int32_t y = 50;
    epd_fill_rect(10, 10, 80, 80, 0x0000, framebuffer);
    write_mode((GFXfont *)&FiraSans, "A", &x, &y, framebuffer, WHITE_ON_BLACK, &props);

    // B
    x = EPD_WIDTH - 72;
    y = 50;
    epd_fill_rect(EPD_WIDTH - 80, 10, 80, 80, 0x0000, framebuffer);
    write_mode((GFXfont *)&FiraSans, "B", &x, &y, framebuffer, WHITE_ON_BLACK, &props);

    // C
    x = 18;
    y = EPD_HEIGHT - 30;
    epd_fill_rect(10, EPD_HEIGHT - 80, 80, 80, 0x0000, framebuffer);
    write_mode((GFXfont *)&FiraSans, "C", &x, &y, framebuffer, WHITE_ON_BLACK, &props);

    // D
    x = EPD_WIDTH - 72;
    y = EPD_HEIGHT - 30;
    epd_fill_rect(EPD_WIDTH - 80, EPD_HEIGHT - 80, 80, 80, 0x0000, framebuffer);
    write_mode((GFXfont *)&FiraSans, "D", &x, &y, framebuffer, WHITE_ON_BLACK, &props);

    // Sleep / E
    x = EPD_WIDTH / 2 - 55;
    y = EPD_HEIGHT - 30;
    epd_draw_rect(EPD_WIDTH / 2 - 60, EPD_HEIGHT - 80, 120, 75, 0x0000, framebuffer);
    write_mode((GFXfont *)&FiraSans, "Sleep", &x, &y, framebuffer, WHITE_ON_BLACK, NULL);

    // Clean / F
    x = EPD_WIDTH / 2 - 55;
    y = 50;
    epd_draw_rect(EPD_WIDTH / 2 - 60, 10, 120, 75, 0x0000, framebuffer);
    write_mode((GFXfont *)&FiraSans, "Clean", &x, &y, framebuffer, WHITE_ON_BLACK, NULL);

    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff_all();
}

void draw_rtc_time()
{
    epd_poweron();

    struct tm timeinfo;
    rtc.getDateTime(&timeinfo);

    int32_t cursor_x = 45;
    int32_t cursor_y = 70;
    strftime(buf, 64, "%H:%M", &timeinfo);
    writeln((GFXfont *)&CourB36, buf, &cursor_x, &cursor_y, NULL);

    epd_poweroff_all();
}

void draw_rtc_date()
{
    epd_poweron();

    struct tm timeinfo;
    rtc.getDateTime(&timeinfo);

    int32_t cursor_x = 50;
    int32_t cursor_y = 120;
    // strftime(buf, 64, "%b %d %Y", &timeinfo);
    strftime(buf, 64, "%a %d.%m", &timeinfo);
    writeln((GFXfont *)&Cour20, buf, &cursor_x, &cursor_y, NULL);

    epd_poweroff_all();
}

void draw_weather()
{
    epd_poweron();

    Rect_t area = {
        .x = 400,
        .y = 80,
        .width = 64,
        .height = 64,
    };
    int32_t cursor_x;
    int32_t cursor_y;
    int32_t icon_idx;

    // Loop over the weather data
    for (int i = 0; i < MAX_WEATHER_DATA; i++)
    {
        int temp_int = (int)round(weather[i].temp);
        snprintf(buf, 128, "%d°C",temp_int); 
        int temp_length = strlen(buf) - 3;

        cursor_x = 400 - 5*temp_length  + i*205;
        cursor_y = 70;
        area.x = 400 + i * 200;
        writeln((GFXfont *)&CourB28, buf, &cursor_x, &cursor_y, NULL);

        icon_idx = weather[i].icon_idx;
        if (icon_idx > 16) { icon_idx = icon_idx - 100 + 16; }
        epd_draw_grayscale_image(area, (uint8_t *)weather_icons[icon_idx]);
        epd_draw_image(area, (uint8_t *)weather_icons[icon_idx], BLACK_ON_WHITE);
    } 
    epd_poweroff_all();
}

void draw_db()
{
    epd_poweron();

    int32_t cursor_x;
    int32_t cursor_y;

    // Loop over the db data
    // Three rows, Cour24
    // for (int i = 0; i < MAX_DB_DATA; i++)
    // {
    //     cursor_x = 30;
    //     cursor_y = 200 + i*130;

    //     snprintf(buf, 128, "%s", db[i].train);
    //     writeln((GFXfont *)&CourB28, buf, &cursor_x, &cursor_y, NULL);
        
    //     cursor_x = 160;
    //     snprintf(buf, 128, "%s", db[i].scheduledDeparture);
    //     writeln((GFXfont *)&Cour28, buf, &cursor_x, &cursor_y, NULL);

        
    //     cursor_x = 390;
    //     if (db[i].delayDeparture > 0)
    //     {
    //         if (db[i].delayDeparture > 9) { cursor_x -= 20; }
    //         snprintf(buf, 128, "+%d", db[i].delayDeparture); 
    //         writeln((GFXfont *)&CourB28, buf, &cursor_x, &cursor_y, NULL);
    //     }
    //     else if (db[i].isCancelled)
    //     {
    //         snprintf(buf, 128, "X!");
    //         writeln((GFXfont *)&CourB28, buf, &cursor_x, &cursor_y, NULL);
    //     }
    //     else
    //     {
    //         snprintf(buf, 128, "OK");
    //         writeln((GFXfont *)&Cour28, buf, &cursor_x, &cursor_y, NULL);
    //     }

    //     cursor_x = 30;
    //     cursor_y += 50;
    //     // If the destination is too long, cut it off
    //     snprintf(buf, 128, "%s", db[i].destination);
    //     buf[12] = '\0';
    //     writeln((GFXfont *)&Cour24, buf, &cursor_x, &cursor_y, NULL);
    // } 

    // 6 rows, Cour20
    for (int i = 0; i < MAX_DB_DATA; i++)
    {
        cursor_x = 20;
        cursor_y = 200 + i*70;

        snprintf(buf, 128, "%s", db[i].scheduledDeparture);
        writeln((GFXfont *)&Cour20, buf, &cursor_x, &cursor_y, NULL);
        
        cursor_x = 170;
        if (db[i].isCancelled)
        {
            snprintf(buf, 128, "--");
            writeln((GFXfont *)&CourB20, buf, &cursor_x, &cursor_y, NULL);
        }
        else if (db[i].delayDeparture > 0)
        {
            if (db[i].delayDeparture > 9) { cursor_x -= 20; }
            snprintf(buf, 128, "+%d", db[i].delayDeparture); 
            writeln((GFXfont *)&CourB20, buf, &cursor_x, &cursor_y, NULL);
        }
        else
        {
            snprintf(buf, 128, "+0");
            writeln((GFXfont *)&Cour20, buf, &cursor_x, &cursor_y, NULL);
        }

        cursor_x = 230;
        // If the destination is too long, cut it off
        snprintf(buf, 128, "%s", db[i].destination);
        buf[9] = '\0';
        writeln((GFXfont *)&Cour20, buf, &cursor_x, &cursor_y, NULL);

        if (db[i].isCancelled)
        {
            cursor_x = 20;
            snprintf(buf, 128, "------------------");
            writeln((GFXfont *)&Cour20, buf, &cursor_x, &cursor_y, NULL);
        }
    } 
    epd_poweroff_all();
}

void draw_cal()
{
    epd_poweron();

    int32_t cursor_x;
    int32_t cursor_y;

    for (int i = 0; i < MAX_CAL_DATA; i++)
    {
        cursor_x = 500;
        cursor_y = 200 + i*125;

        String isoDate = cal[i].date;

        snprintf(buf, 128, "%s.%s %s:%s", 
            isoDate.substring(8, 10), //Day
            isoDate.substring(5, 7), //Month
            isoDate.substring(11, 13), //Hour
            isoDate.substring(14, 16) //Minute
            
        );
        // snprintf(buf, 128, "%s", cal[i].date);
        writeln((GFXfont *)&Cour20, buf, &cursor_x, &cursor_y, NULL);
        
        cursor_x = 500;
        cursor_y += 50;
        // If the title is too long, cut it off
        snprintf(buf, 128, "%s", cal[i].title);
        buf[18] = '\0';
        writeln((GFXfont *)&CourB20, buf, &cursor_x, &cursor_y, NULL);
    } 
    // epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff_all();
}

void draw_layout()
{
    epd_poweron();
    epd_clear();

    // memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    // epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    delay(10);

    epd_draw_hline(0, 144, EPD_WIDTH, 0, framebuffer);

    // Three column layout
    // epd_draw_vline(320, 0, EPD_HEIGHT, 0, framebuffer);
    // epd_draw_vline(640, 144, EPD_HEIGHT - 144, 0, framebuffer);

    // Two column layout
    epd_draw_vline(320, 0, 144, 0, framebuffer);
    epd_draw_vline(EPD_WIDTH/2, 144, EPD_HEIGHT - 144, 0, framebuffer);
    
    // Debug Weather
    // epd_draw_vline(533, 0, 144, 0, framebuffer);
    // epd_draw_vline(747, 0, 144, 0, framebuffer);
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff_all();
}

/////////////////// SETUP //////////////////////////
void setup()
{
    Serial.begin(115200);

    // WIFI
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.println("Connecting to WiFi..");
    }

    // MQTT
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(mqtt_rec);
    client.setBufferSize(MQTT_BUFFER_SIZE);

    while (!client.connected())
    {
        String client_id = "esp32-client-";
        client_id += String(WiFi.macAddress());
        Serial.printf("The client %s connects to the MQTT broker\n", client_id.c_str());
        if (client.connect(client_id.c_str(), mqtt_username, mqtt_password))
        {
            Serial.println("MQTT broker connected");
        }
        else
        {
            Serial.print("failed with state ");
            Serial.print(client.state());
            deep_sleep("MQTT probe failed!!!");
        }
    }
    client.subscribe(mqtt_sub_topic);

    // Send out first request
    sniprintf(msg, MISC_MSG_SIZE, "{\"type\":\"request\",\"data\":[{\"name\":\"weather\"},{\"name\":\"db\",\"count\":%d},{\"name\":\"calendar\",\"count\":%d}]}", MAX_DB_DATA, MAX_CAL_DATA);
    client.publish(mqtt_pub_topic, msg);
    
    // Time
    sntp_set_time_sync_notification_cb(timeavailable);
    configTzTime(time_zone, ntpServer1, ntpServer2);

    // Framebuffer
    framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!framebuffer)
    {
        Serial.println("alloc memory failed !!!");
        while (1)
            ;
    }
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    // Initialize
    epd_init();
    deep_clean();

    // Touch
    pinMode(TOUCH_INT, OUTPUT);
    digitalWrite(TOUCH_INT, HIGH);

    // RTC
    Wire.begin(BOARD_SDA, BOARD_SCL);
    Wire.beginTransmission(PCF8563_SLAVE_ADDRESS);
    if (!Wire.endTransmission() == 0)
    {
        deep_sleep("RTC probe failed!!!");
    }
    rtc.begin(Wire, PCF8563_SLAVE_ADDRESS, BOARD_SDA, BOARD_SCL);

    // Touch
    uint8_t touchAddress = 0x00;
    Wire.beginTransmission(0x14);
    if (Wire.endTransmission() == 0)
    {
        touchAddress = 0x14;
    }
    Wire.beginTransmission(0x5D);
    if (Wire.endTransmission() == 0)
    {
        touchAddress = 0x5D;
    }
    if (touchAddress == 0x00)
    {
        deep_sleep("[SETUP] Touch probe failed (0)!!!");
    }

    touch.setPins(-1, TOUCH_INT);
    if (!touch.begin(Wire, touchAddress, BOARD_SDA, BOARD_SCL))
    {
        deep_sleep("[SETUP] Touch probe failed (1)!!!");
    }
    touch.setMaxCoordinates(EPD_WIDTH, EPD_HEIGHT);
    touch.setSwapXY(true);
    touch.setMirrorXY(false, true);
    touchOnline = true;

    if (!touchOnline)
    {
        deep_sleep("[SETUP] Touch probe failed (2)!!!");
    }

    // Set the button callback function
    btn.setPressedHandler(buttonPressed);

    client.loop();
}

/////////////////// LOOP //////////////////////////
void loop()
{
    // MQTT
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();

    // Big Interval
    if (millis() > big_interval_timestamp || touch_registered)
    {
        // Every big_interval
        big_interval_timestamp = millis() + big_interval;

        // Reset touch
        touch_registered = 0;

        // client.publish(mqtt_pub_topic, "{\"type\":\"request\",\"data\":[\"weather\",\"db\"]}");
        sniprintf(msg, MISC_MSG_SIZE, "{\"type\":\"request\",\"data\":[{\"name\":\"weather\"},{\"name\":\"db\",\"count\":%d},{\"name\":\"calendar\",\"count\":%d}]}", MAX_DB_DATA, MAX_CAL_DATA);
        client.publish(mqtt_pub_topic, msg);


        // draw_layout();
        // draw_rtc_time();
        // draw_rtc_date();
        // draw_weather();
        // draw_db();
        // draw_cal();
        // draw_touch_buttons();

        // Serial.println("big loop");
        // delay(1000);
    }

    // Button
    btn.loop();

    // Touch
    int16_t x, y;
    if (!digitalRead(TOUCH_INT))
    {
        return;
    }
    uint8_t touched = touch.getPoint(&x, &y);
    // Filter duplicate touch
    if ((x == last_x) && (y == last_y))
    {
        return;
    }

    if (!touched)
    {
        return;
    }
    last_x = x;
    last_y = y;
    touch_registered = 1;

    // epd_poweron();

    // int32_t cursor_x = 200;
    // int32_t cursor_y = 450;

    // Rect_t area = {
    //     .x = 200,
    //     .y = 410,
    //     .width = 400,
    //     .height = 50,
    // };
    // epd_clear_area(area);

    // snprintf(buf, 128, "➸ X:%d Y:%d", x, y);
    // Serial.println(buf);

    // bool pressButton = false;
    // for (int i = 0; i < sizeof(touchPoint) / sizeof(touchPoint[0]); ++i)
    // {
    //     if ((x > touchPoint[i].x && x < (touchPoint[i].x + touchPoint[i].w)) && (y > touchPoint[i].y && y < (touchPoint[i].y + touchPoint[i].h)))
    //     {
    //         snprintf(buf, 128, "➸ Pressed Button: %c\n", 65 + touchPoint[i].buttonID);
    //         Serial.println(buf);
    //         // writeln((GFXfont *)&FiraSans, buf, &cursor_x, &cursor_y, NULL);
    //         pressButton = true;

    //         if (touchPoint[i].buttonID == 4)
    //         {
    //             deep_sleep("");
    //         }
    //         if (touchPoint[i].buttonID == 5)
    //         {
    //             deep_clean();
    //         }
    //     }
    // }
    // if (!pressButton)
    // {
    //     // writeln((GFXfont *)&FiraSans, buf, &cursor_x, &cursor_y, NULL);
    // }
    epd_poweroff_all();

    delay(2);
}
