/* pms5003.ino
**
** An esp8266 reading from pms5003
** https://cdn-shop.adafruit.com/product-files/3686/plantower-pms5003-manual_v2-3.pdf
**
** web server endpoints:
**	curl esp8266-pms5003
**	curl esp8266-pms5003/stats
**	prometheus esp8266-pms5003/metrics
** mDNS
** Over the air updates
**
** Copyright (c) 2020 Atanu Ghosh
** MIT license
*/
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>

// Set in config.ino
extern const char* SSID;
extern const char* PASSWORD;
const char* NODE_NAME = "esp8266-pms5003";

const int DEFAULT_SERIAL_SPEED = 9600; // The rate required by device

// NodeMCU 1.0 (ESP-12E Module) has two led's
// 2  (blue) LED_BUILTIN
// 16 (red)
const int led = LED_BUILTIN;
const int OFF = 1;
const int ON = 0;

bool const DEBUG = false;

ESP8266WebServer server(80);

struct Stats {
        unsigned long read;       // Number of packets that have been read
        unsigned long bad_length; // Packet read length failed
        unsigned long bad_packet;
        unsigned long bad_ch1;
        unsigned long bad_ch2;
        unsigned long bad_packet_length;
        unsigned long bad_checksum;

        void format(char* buf)
        {
                sprintf(buf,
                        "read %d\r\n"
                        "bad length: %d\n"
                        "bad packet: %d\n"
                        "\t bad ch1: %d\n"
                        "\t bad ch2: %d\n"
                        "\t bad packet length: %d\n"
                        "\t bad checksum: %d\n"
                        "time since boot: %dms\n",
                        read,
                        bad_length,
                        bad_packet,
                        bad_ch1,
                        bad_ch2,
                        bad_packet_length,
                        bad_checksum,
                        millis());
        }
} stats;

struct PMV {
        static const int LENGTH = 32;

        static const char START_CH_1 = 0x42;
        static const char START_CH_2 = 0x4d;
        const int INPACKET_LENGTH = 28; // The length carried in the packet

        const int PM_1d0_OFFSET = 4;
        const int PM_2d5_OFFSET = 6;
        const int PM_10d0_OFFSET = 8;

        int _pm_1d0 = -1;  // PM 1.0
        int _pm_2d5 = -1;  // PM 2.5
        int _pm_10d0 = -1; // PM 10

        static unsigned int twobytes(unsigned char* offset)
        {
                return (offset[0] << 8) + offset[1];
        }

        bool validate(unsigned char* pmbuf, int buflen)
        {
                if (pmbuf[0] != START_CH_1) {
                        Serial.println("Bad start character 1");
                        stats.bad_ch1++;
                        return false;
                }
                if (pmbuf[1] != START_CH_2) {
                        Serial.println("Bad start character 2");
                        stats.bad_ch2++;
                        return false;
                }
                if (twobytes(&pmbuf[2]) != INPACKET_LENGTH) {
                        Serial.println("Bad in packet length");
                        stats.bad_packet_length++;
                        return false;
                }

                return checksum(pmbuf, buflen);
        }

        bool checksum(unsigned char* pmbuf, int buflen)
        {
                int sum = 0;
                int datalen = buflen - 2;

                for (int i = 0; i < datalen; i++) {
                        sum += pmbuf[i];
                }

                if (sum == twobytes(&pmbuf[buflen - 2])) {
                        return true;
                }

                Serial.println("Checksum failed");
                stats.bad_checksum++;

                return false;
        }

        bool parse(unsigned char* pmbuf, int buflen)
        {
                if (!validate(pmbuf, buflen))
                        return false;

                _pm_1d0 = twobytes(&pmbuf[PM_1d0_OFFSET]);
                _pm_2d5 = twobytes(&pmbuf[PM_2d5_OFFSET]);
                _pm_10d0 = twobytes(&pmbuf[PM_10d0_OFFSET]);

                return true;
        }

        void format(char* outbuf)
        {
                sprintf(outbuf,
                        "PM1.0: %d ug/m3\r\n"
                        "PM2.5: %d ug/m3\r\n"
                        "PM10: %d ug/m3\r\n",
                        _pm_1d0,
                        _pm_2d5,
                        _pm_10d0);
        }

        void prometheus(char* outbuf)
        {
                sprintf(outbuf,
                        "# HELP pm_1_0 particles less than 1.0 microns\n"
                        "# TYPE pm_1_0 gauge\n"
                        "pm_1_0 %d\n"
                        "# HELP pm_2_5 particles less than 2.5 microns\n"
                        "# TYPE pm_2_5 gauge\n"
                        "pm_2_5 %d\n"
                        "# HELP pm_10_0 particles less than 10.0 microns\n"
                        "# TYPE pm_10_0 gauge\n"
                        "pm_10_0 %d\n",
                        _pm_1d0,
                        _pm_2d5,
                        _pm_10d0);
        }
} pmv;

void
handleRoot()
{
        digitalWrite(led, ON);
        char buf[1024];

        pmv.format(&buf[0]);
        server.send(200, "text/plain", buf);
        digitalWrite(led, OFF);
}

// Respond to prometheus probes
void
handleMetrics()
{
        char buf[1024];

        pmv.prometheus(&buf[0]);
        server.send(200, "text/plain", buf);
}

void
handleStats()
{
        char buf[1024];

        stats.format(&buf[0]);
        server.send(200, "text/plain", buf);
}

void
handleNotFound()
{
        digitalWrite(led, ON);
        String message = "File Not Found\n\n";
        message += "URI: ";
        message += server.uri();
        message += "\nMethod: ";
        message += (server.method() == HTTP_GET) ? "GET" : "POST";
        message += "\nArguments: ";
        message += server.args();
        message += "\n";
        for (uint8_t i = 0; i < server.args(); i++) {
                message +=
                  " " + server.argName(i) + ": " + server.arg(i) + "\n";
        }
        server.send(404, "text/plain", message);
        digitalWrite(led, OFF);
}

void
setup(void)
{
        Serial.begin(DEFAULT_SERIAL_SPEED);
        // Initialize the LED_BUILTIN pin as an output
        pinMode(led, OUTPUT);
        // Connect to WiFi network
        WiFi.mode(WIFI_STA);
        WiFi.hostname(NODE_NAME);
        WiFi.begin(SSID, PASSWORD);
        Serial.println("");

        // Wait for connection
        while (WiFi.status() != WL_CONNECTED) {
                delay(500);
                Serial.print(".");
        }
        Serial.println("");
        Serial.print("Connected to ");
        Serial.println(SSID);
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        if (!MDNS.begin(NODE_NAME)) {
                Serial.println("Error setting up MDNS responder!");
                for (;;) {
                        delay(1000);
                }
        }
        Serial.print("mDNS responder started (");
        Serial.print(NODE_NAME);
        Serial.println(")");

        server.on("/", handleRoot);
        server.on("/metrics", handleMetrics);
        server.on("/stats", handleStats);
        server.onNotFound(handleNotFound);
        server.begin();
        Serial.println("HTTP server started");

        // Add service to MDNS-SD
        MDNS.addService("http", "tcp", 80);

        // OTA - From BasicOTA.ino

        // Port defaults to 8266
        // ArduinoOTA.setPort(8266);

        // Hostname defaults to esp8266-[ChipID]
        // ArduinoOTA.setHostname("myesp8266");

        // No authentication by default
        // ArduinoOTA.setPassword("admin");

        // Password can be set with it's md5 value as well
        // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
        // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

        ArduinoOTA.onStart([]() {
                String type;
                if (ArduinoOTA.getCommand() == U_FLASH) {
                        type = "sketch";
                }
                else { // U_FS
                        type = "filesystem";
                }

                // NOTE: if updating FS this would be the place to unmount FS
                // using FS.end()
                Serial.println("Start updating " + type);
        });
        ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
                Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        });
        ArduinoOTA.onError([](ota_error_t error) {
                Serial.printf("Error[%u]: ", error);
                if (error == OTA_AUTH_ERROR) {
                        Serial.println("Auth Failed");
                }
                else if (error == OTA_BEGIN_ERROR) {
                        Serial.println("Begin Failed");
                }
                else if (error == OTA_CONNECT_ERROR) {
                        Serial.println("Connect Failed");
                }
                else if (error == OTA_RECEIVE_ERROR) {
                        Serial.println("Receive Failed");
                }
                else if (error == OTA_END_ERROR) {
                        Serial.println("End Failed");
                }
        });
        ArduinoOTA.begin();
        Serial.println("OTA updater started");
}

void
loop(void)
{
        static unsigned long start_time_us = 0;
        unsigned long current_time_us = 0;
        static bool on = true;

        if (start_time_us == 0) {
                start_time_us = micros();
        }
        else {
                current_time_us = micros();
                unsigned long diff = current_time_us - start_time_us;
                if (diff > 1000000) {
                        if (on) {
                                digitalWrite(led, OFF);
                                on = false;
                        }
                        else {
                                digitalWrite(led, ON);
                                on = true;
                        }
                        // Serial.print("Time in micro seconds: ");
                        // Serial.println(current_time_us);
                        start_time_us = 0;
                }
        }

        MDNS.update();
        server.handleClient();
        ArduinoOTA.handle();

        // Read from the PMS device
        pms();
}

void
pms()
{
        unsigned char pmsbuf[PMV::LENGTH];
        int ret;

        if (Serial.find(PMV::START_CH_1)) {
                stats.read++;
                pmsbuf[0] = 0x42; // Needed for checksum
                ret = Serial.readBytes(&pmsbuf[1], PMV::LENGTH - 1);
                if (ret != (PMV::LENGTH - 1)) {
                        Serial.println("Bad length");
                        stats.bad_length++;
                        return;
                }
                if (!pmv.parse(pmsbuf, ret + 1)) {
                        Serial.println("Bad value");
                        stats.bad_packet++;
                        return;
                }
        }

        // Debugging output every second
        static unsigned long start_time_ms = millis();
        if (millis() - start_time_ms >= 1000) {
                start_time_ms = millis();
                char pbuf[255];
                pmv.format(&pbuf[0]);
                Serial.println(pbuf);
                if (DEBUG)
                        hexdump(&pbuf[0], &pmsbuf[0], ret);
                Serial.println(pbuf);
        }
}

void
hexdump(char* pbuf, unsigned char* pmsbuf, int length)
{
        pbuf[0] = '\0';
        for (int i = 0; i < length; i++) {
                char sbuf[1024];
                sprintf(&sbuf[0], "%02x ", pmsbuf[i]);
                strcat(&pbuf[0], &sbuf[0]);
                if ((i % 16) == 0 && (i != 0)) {
                        strcat(&pbuf[0], "\r\n");
                }
        }
        strcat(&pbuf[0], "\r\n");
}

/* Local Variables:  */
/* mode: c           */
/* c-basic-offset: 8 */
/* End:              */
