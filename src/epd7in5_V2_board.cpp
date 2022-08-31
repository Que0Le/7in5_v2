/**
 *  @filename   :   epd7in5-demo.ino
 *  @brief      :   7.5inch e-paper display demo
 *  @author     :   Yehui from Waveshare
 *
 *  Copyright (C) Waveshare     July 10 2017
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <SPI.h>
#include "epd7in5_V2.h"
#include "imagedata.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <base64.h>
#include <Arduino.h>
#include <string.h>

#define USE_SERIAL Serial
// the actual bitmap header is 62Bytes length. However, the python backend serves extra 6Bytes for some reasons,
// so we have to work around by adding 6 positions in the buffer. Note that serving static file will not add this 6 bytes!
#define BMP_HEADER_LEN 68
#define R 480 // 480 row
#define C 100 // 800 div 8 = 100 bytes to encode 800 pixel in a row

#define BUTTON_PIN_BITMASK 0x7000 // GPIOs 14 and 13 12
#define uS_TO_S_FACTOR 1000000    /* Conversion factor for micro seconds to seconds */

RTC_DATA_ATTR int TIME_TO_SLEEP = 30; /* Time ESP32 will go to sleep (in seconds) */
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int wake_reason = -1;

WiFiMulti wifiMulti;
Epd epd;

// Create a new image cache
int BUFF_SIZE;
unsigned char *buff_48k;
const char *wifi_ssid = "";
const char *wifi_password = "";

const String authUsername = "server_user_name";
const String authPassword = "password";

const String TOWER_ADDR = "http://192.168.1.16:8000";


int getStaticBMP(String auth)
{
    HTTPClient http;
    // http.addHeader("Authorization", "Basic " + auth);
    http.begin(TOWER_ADDR + "/api/project1_server/next_bitmap");
    //    http.begin(TOWER_ADDR + "/static/ausserhalb_1bit.bmp");
    // http.begin("192.168.1.12", 80, "/test.html");

    USE_SERIAL.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();
    if (httpCode > 0)
    {
        // HTTP header has been send and Server response header has been handled
        USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {

            // get lenght of document (is -1 when Server sends no Content-Length header)
            int len = http.getSize();
            

            // create buffer for read
            uint8_t buff[128] = {0};
            // memset(buff_48k, 0, sizeof(buff_48k));  // => not work
            for (int i = 0; i < BUFF_SIZE; i++)
            {
                buff_48k[i] = 0;
            }
            // get tcp stream
            WiFiClient *stream = http.getStreamPtr();

            // read all data from server
            int pos = 0;
            while (http.connected() && (len > 0 || len == -1))
            {
                // get available data size
                size_t size = stream->available();

                if (size)
                {
                    // read up to 128 byte
                    int c = stream->readBytes(buff_48k + pos, ((size > BUFF_SIZE) ? BUFF_SIZE : size));
                    pos += c;
                    // write it to Serial
                    // USE_SERIAL.write(buff_48k, c);

                    if (len > 0)
                    {
                        len -= c;
                    }
                }
                delay(1);
            }
            USE_SERIAL.print("pos: ");
            USE_SERIAL.println(pos);
            /* Flip the image*/
            for (int r = 0; r < int(R / 2); r++)
            {
                for (int col = 0; col < C; col++)
                {
                    unsigned char temp = buff_48k[r * C + col + BMP_HEADER_LEN];
                    buff_48k[r * C + col + BMP_HEADER_LEN] = buff_48k[(R - r - 1) * C + col + BMP_HEADER_LEN];
                    buff_48k[(R - r - 1) * C + col + BMP_HEADER_LEN] = temp;
                }
            }
            // epd.Clear();
            // Serial.println("Clearing display ...");
            delay(1000);
            epd.DisplayFrame(buff_48k + BMP_HEADER_LEN);
            Serial.println("Displayed. ");
            // epd.Sleep();

            /*for (int i=BMP_HEADER_LEN;i<true_size; i++) {
                buff_48k[i] = gImage_7in5_V2[i-BMP_HEADER_LEN];
            }*/

            /* Init eink */
            // DEV_Module_Init();
            // printf("e-Paper Init and Clear...\r\n");
            // EPD_7IN5_V2_Init();
            // //            EPD_7IN5_V2_Clear();
            // DEV_Delay_ms(500);
            // USE_SERIAL.println("Displaying on eink ... ");
            // //            Paint_Clear(WHITE);
            // EPD_7IN5_V2_Display(buff_48k + BMP_HEADER_LEN);
            // DEV_Delay_ms(2000);

            // USE_SERIAL.println();
            // USE_SERIAL.print("[HTTP] connection closed or file end.\n");
            return 0;
        }
    }
    return -1;
}

void setup()
{
    Serial.begin(115200);
    wifiMulti.addAP(wifi_ssid, wifi_password);
    // while (wifiMulti.run() != WL_CONNECTED)
    // {
    //     delay(1000);
    //     Serial.println("Connecting to WiFi..");
    // }
    // Serial.println("Connected to WiFi");

    Serial.print("e-Paper init \r\n ");
    if (epd.Init() != 0) {
        Serial.print("e-Paper init failed\r\n ");
        return;
    }
    epd.Clear();
    Serial.println("cleared ...");

    BUFF_SIZE = 48000 + BMP_HEADER_LEN; //((EPD_7IN5_V2_WIDTH % 8 == 0) ? (EPD_7IN5_V2_WIDTH / 8 ) : (EPD_7IN5_V2_WIDTH / 8 + 1)) * EPD_7IN5_V2_HEIGHT;
    // BUFF_SIZE = 48000;
    if ((buff_48k = (unsigned char *)malloc(BUFF_SIZE)) == NULL)
    {
        printf("Failed to apply for buff_48k memory...\r\n");
    }
    // for (int i=0; i<BUFF_SIZE; i++) {
    //     if (i%2==0)
    //         buff_48k[i] = 0xff;
    //     else
    //         buff_48k[i] = 0x00;
    // }

    // epd.DisplayFrame(buff_48k);
    // epd.Sleep();
    // epd.Displaypart(IMAGE_DATA,250, 200,240,103);
    // if (getStaticBMP("auth")!=0) {
    //   Serial.println("Failed get and print bmp\n");
    // }
}

int count = 0;
void loop()
{
    count += 1;

    // wifiMulti.addAP(wifi_ssid, wifi_password);
    while (wifiMulti.run() != WL_CONNECTED)
    {
        delay(1000);
        Serial.println("Connecting to WiFi..");
    }
    Serial.println("Connected to WiFi");
    // memset(buff_48k, 0, BUFF_SIZE);

    if (getStaticBMP("auth")!=0) {
      Serial.println("Failed get and print bmp\n");
    }

    // Serial.print("start drawing ... ");
    // Serial.println(count);
    // // epd.Reset();
    // for (int i=0; i<BUFF_SIZE; i++) {
    //     if (i%5==0)
    //         buff_48k[i] = count%2 == 0 ? 0xff : 0x00;
    //     else
    //         buff_48k[i] = count%2 == 0 ? 0x00 : 0xff;
    // }

    // epd.Clear();
    // delay(1000);
    // epd.DisplayFrame(buff_48k);
    // epd.Sleep();
    delay(5000);
}

