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
#include "cour20.h"
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
char msg[MQTT_BUFFER_SIZE];

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
uint32_t big_interval = 30000; // 30sec
uint32_t small_interval = 300;
uint32_t big_interval_timestamp = 0;
uint32_t small_interval_timestamp = 0;
int vref = 1100;
char buf[128];
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

uint32_t pressed_cnt = 0;
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

void mqtt_rec(char *topic, byte *payload, unsigned int length) {
    Serial.printf("Message arrived in topic: %s\n", topic);
    Serial.printf("Message (%d):", length);
    Serial.write(payload, length);
    Serial.println();
    Serial.println("-----------------------");
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(mqtt_pub_topic, "Reconnected");
      // ... and resubscribe
      client.subscribe(mqtt_sub_topic);
    } else {
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

void draw_layout()
{
    epd_poweron();
    epd_clear();

    // memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    // epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    delay(10);

    epd_draw_hline(0, 144, EPD_WIDTH, 0, framebuffer);
    epd_draw_vline(320, 0, EPD_HEIGHT, 0, framebuffer);
    epd_draw_vline(640, 144, EPD_HEIGHT - 144, 0, framebuffer);
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
    client.publish(mqtt_pub_topic, "Hi, I'm ESP32 :)");
    client.subscribe(mqtt_sub_topic);

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
    if (Wire.endTransmission() == 0) {
        touchAddress = 0x14;
    }
    Wire.beginTransmission(0x5D);
    if (Wire.endTransmission() == 0) {
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
}

/////////////////// LOOP //////////////////////////
void loop()
{
    // MQTT
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    // Big Interval
    if (millis() > big_interval_timestamp)
    {
        // Every 10 seconds
        big_interval_timestamp = millis() + big_interval;

        draw_layout();
        draw_rtc_time();
        draw_rtc_date();
        // draw_touch_buttons();

        Serial.println("big loop");
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

    epd_poweron();

    int32_t cursor_x = 200;
    int32_t cursor_y = 450;

    Rect_t area = {
        .x = 200,
        .y = 410,
        .width = 400,
        .height = 50,
    };
    epd_clear_area(area);

    snprintf(buf, 128, "➸ X:%d Y:%d", x, y);
    Serial.println(buf);

    bool pressButton = false;
    for (int i = 0; i < sizeof(touchPoint) / sizeof(touchPoint[0]); ++i)
    {
        if ((x > touchPoint[i].x && x < (touchPoint[i].x + touchPoint[i].w)) && (y > touchPoint[i].y && y < (touchPoint[i].y + touchPoint[i].h)))
        {
            snprintf(buf, 128, "➸ Pressed Button: %c\n", 65 + touchPoint[i].buttonID);
            Serial.println(buf);
            writeln((GFXfont *)&FiraSans, buf, &cursor_x, &cursor_y, NULL);
            pressButton = true;

            if (touchPoint[i].buttonID == 4)
            {
                deep_sleep("");
            }
            if (touchPoint[i].buttonID == 5)
            {
                deep_clean();
            }
        }
    }
    if (!pressButton)
    {
        writeln((GFXfont *)&FiraSans, buf, &cursor_x, &cursor_y, NULL);
    }
    epd_poweroff_all();

    delay(2);
}

