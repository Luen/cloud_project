/**
 * CloudCloud
 * Shawn Hymel @ SparkFun Electronics
 * September 17, 2015
 *
 * Use Blynk app to set up some buttons and LEDs. See the pin
 * definitions below.
 */

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include "States.h"

// Debug to Blynk Serial
#if DEBUG
#define BLYNK_PRINT Serial
#endif

// Pin definitions
#define LED_PIN 5
#define WEATHER_BTN_PIN V0
#define WEATHER_LED_PIN V1
#define RGB_BTN_PIN V2
#define RGB_LED_PIN V3
#define DISCO_BTN_PIN V4
#define DISCO_LED_PIN V5
#define DEMO_BTN_PIN V6
#define DEMO_LED_PIN V7
#define ZERGBA_PIN V8
#define LCD_PIN V9

// WiFi parameters
const char WIFI_SSID[] = "";
const char WIFI_PSK[] = "";
const char BLYNK_AUTH[] = "";

// Location data (Werther, GERMANY)
// const float LATITUDE = 52.084565;
// const float LONGITUDE = 8.438282;
// Location data (Townsville, QLD)
const float LATITUDE = -19.307333;
const float LONGITUDE = 146.771162;

// Open Weather API key
const char OPENWEATHER[] = "";

// Remote site information
const char http_site[] = "api.openweathermap.org";
const int http_port = 80;

// Other constants
const byte SERIAL_SOF = 0xA5;
const byte SERIAL_EOF = 0x5A;
const int OUT_BUF_MAX = 7; // bytes
const byte COMM_RGB = 1;
const byte COMM_DISCO = 2;
const byte COMM_BLUE_SKY = 3;
const byte COMM_NIGHT = 4;
const byte COMM_OVERCAST = 5;
const byte COMM_GOLDEN = 6;
const byte COMM_SNOW = 7;
const byte COMM_LIGHTNING = 8;

// Other constants
const int TIMEOUT = 1000;   // ms
const int BUFFER_SIZE = 22; // bytes
const int ICON_SIZE = 4;    // bytes
const int DT_SIZE = 12;     // bytes

// Global variables
WidgetLED weather_led(WEATHER_LED_PIN);
WidgetLED rgb_led(RGB_LED_PIN);
WidgetLED disco_led(DISCO_LED_PIN);
WidgetLED demo_led(DEMO_LED_PIN);
WidgetLCD lcd(LCD_PIN);
CloudState cloud_state;
WiFiClient client;
WeatherType current_weather;
unsigned long led_time;
bool force_update;
uint8_t color_r;
uint8_t color_g;
uint8_t color_b;
byte out_msg[OUT_BUF_MAX];

/****************************************************************
 * Weather Functions
 ***************************************************************/
// Shift buffer to the left by 1 and append new char at end
void pushOnBuffer(char c, char *buf, uint8_t len)
{
  for (uint8_t i = 0; i < len - 1; i++)
  {
    buf[i] = buf[i + 1];
  }
  buf[len - 1] = c;
}

// Perform an HTTP GET request to openweathermap.org
WeatherType getWeather()
{

  unsigned long led_time = millis();
  unsigned long dt = 0;
  unsigned long sunrise = 0;
  unsigned long sunset = 0;
  char buf[BUFFER_SIZE];
  char icon[ICON_SIZE];
  char dt_str[DT_SIZE];
  char c;
  char *dummy;
  bool golden = false;
  ParserState parser_state = PARSER_START;
  WeatherType condition = WEATHER_UNKNOWN;

  // Serial.println("if ( client.available() ) {");
  //  Flush read buffer
  if (client.available())
  {
    client.read();
    // Serial.println("client.read(); }");
  }

  // Set string buffers to null
  memset(icon, 0, ICON_SIZE);
  memset(dt_str, 0, DT_SIZE);

  // Attempt to make a connection to the remote server
  if (!client.connect(http_site, http_port))
  {
    return WEATHER_ERROR_1;
  }

  // Construct HTTP GET request
  String cmd = "GET /data/2.5/weather?lat=";
  cmd += String(LATITUDE);
  cmd += "&lon=";
  cmd += String(LONGITUDE);
  cmd += "&APPID=";
  cmd += String(OPENWEATHER);
  cmd += " HTTP/1.1\nHost: ";
  cmd += http_site;
  cmd += "\nConnection: close\n\n";

  // Send out GET request to site
  client.print(cmd);
  // Serial.println("Construct HTTP GET request");
  // Serial.println(cmd);

  // Parse data
  // Serial.println("(millis() - led_time) < TIMEOUT");
  // Serial.print("(");
  // Serial.print(millis());
  // Serial.print(" - ");
  // Serial.print(led_time);
  // Serial.print(") < ");
  // Serial.println(TIMEOUT);

  while ((millis() - led_time) < TIMEOUT)
  {

    // Serial.println("if ( client.available() ) {");
    //  Get web page data and push to window buffer
    if (client.available())
    {
      c = client.read();
      pushOnBuffer(c, buf, BUFFER_SIZE);
      led_time = millis();
      // Serial.println("client.available()M");
    }

    // Depending on the state of the parser, look for string
    switch (parser_state)
    {
    case PARSER_START:
      if (memcmp(buf, "\"weather\":", 10) == 0)
      {
        parser_state = PARSER_WEATHER;
      }
      // Serial.println("PARSER_START");
      break;

    case PARSER_WEATHER:
      if (memcmp(buf, "\"icon\":\"", 8) == 0)
      {
        parser_state = PARSER_DT;
        memcpy(icon, buf + 8, 3);
      }
      // Serial.println("PARSER_WEATHER");
      break;

    case PARSER_DT:
      if (memcmp(buf, "\"dt\":", 5) == 0)
      {
        parser_state = PARSER_SUNRISE;
        memcpy(dt_str, buf + 5, 10); // This assumes 10 digits!
        dt = strtol(dt_str, &dummy, 10);
      }
      // Serial.println("PARSER_DT");
      break;

    case PARSER_SUNRISE:
      if (memcmp(buf, "\"sunrise\":", 10) == 0)
      {
        parser_state = PARSER_SUNSET;
        memcpy(dt_str, buf + 10, 10); // Assumes 10 digits!
        sunrise = strtol(dt_str, &dummy, 10);
      }
      // Serial.println("PARSER_SUNRISE");
      break;

    case PARSER_SUNSET:
      if (memcmp(buf, "\"sunset\":", 9) == 0)
      {
        parser_state = PARSER_SUNSET;
        memcpy(dt_str, buf + 9, 10); // Assumes 10 digits!
        sunset = strtol(dt_str, &dummy, 10);
      }
      // Serial.println("PARSER_SUNSET");
      break;

    default:
      break;
    }
  }

  // Make sure we have a legitimate icon and led_times
  if ((strcmp(icon, "") == 0) || (dt == 0) ||
      (sunrise == 0) || (sunset == 0))
  {
    // Serial.print("icon: ");
    // Serial.println(icon);
    // Serial.print("dt: ");
    // Serial.println(dt);
    // Serial.print("sunrise: ");
    // Serial.println(sunrise);
    // Serial.print("sunset: ");
    // Serial.println(sunset);

    return WEATHER_ERROR_2;
  }

  // See if we are in the golden hour
  if (((dt - sunrise) >= 0) && ((dt - sunrise) <= 1800))
  {
    golden = true;
  }
  if (((sunset - dt) >= 0) && ((sunset - dt) <= 1800))
  {
    golden = true;
  }

  // Lookup what to display based on icon
  // http://openweathermap.org/weather-conditions
  if ((strcmp(icon, "01d") == 0) ||
      (strcmp(icon, "02d") == 0))
  {
    if (golden)
    {
      return WEATHER_GOLDEN;
    }
    else
    {
      return WEATHER_BLUE_SKY;
    }
  }
  else if ((strcmp(icon, "01n") == 0) ||
           (strcmp(icon, "02n") == 0))
  {
    if (golden)
    {
      return WEATHER_GOLDEN;
    }
    else
    {
      return WEATHER_NIGHT;
    }
  }
  else if ((strcmp(icon, "03d") == 0) ||
           (strcmp(icon, "03n") == 0) ||
           (strcmp(icon, "04d") == 0) ||
           (strcmp(icon, "04n") == 0) ||
           (strcmp(icon, "09d") == 0) ||
           (strcmp(icon, "09n") == 0) ||
           (strcmp(icon, "10d") == 0) ||
           (strcmp(icon, "10n") == 0))
  {
    return WEATHER_OVERCAST;
  }
  else if ((strcmp(icon, "11d") == 0) ||
           (strcmp(icon, "11n") == 0))
  {
    return WEATHER_LIGHTNING;
  }
  else if ((strcmp(icon, "13d") == 0) ||
           (strcmp(icon, "13n") == 0))
  {
    return WEATHER_SNOW;
  }
  else if ((strcmp(icon, "50d") == 0) ||
           (strcmp(icon, "50n") == 0))
  {
    return WEATHER_OVERCAST;
  }
  else
  {
    return WEATHER_UNKNOWN;
  }
}

/****************************************************************
 * Transmitter
 ***************************************************************/
void transmitMessage(byte msg[], uint8_t len)
{

  int i;
  byte cs;
  byte *out_buf;
  uint8_t buf_size;

  // If message is greater than max size, only xmit max bytes
  if (len > OUT_BUF_MAX)
  {
    len = OUT_BUF_MAX;
  }

  // Full buffer is message + SOF, EOF bytes
  buf_size = len + 2;

  // Create the output buffer with BEGIN, SOF, CS, and EOF
  out_buf = (byte *)malloc(buf_size * sizeof(byte));
  out_buf[0] = SERIAL_SOF;
  memcpy(out_buf + 1, msg, len);
  out_buf[buf_size - 1] = SERIAL_EOF;

  // Transmit buffer
  for (i = 0; i < buf_size; i++)
  {
    Serial.write(out_buf[i]);
  }

  // Free some memory
  free(out_buf);
}

// Every 30 seconds, pull weather data and update LEDs
void doWeather()
{

  // Only update every 30 seconds
  if (((millis() - led_time) >= 30000) || force_update)
  {
    force_update = false;
    led_time = millis();
    lcd.clear();
    lcd.print(0, 0, "WEATHER");

    // Serial.println("getWeather()");
    //  Get weather data
    current_weather = getWeather();

    // Display weather on clouds
    switch (current_weather)
    {

    // Cannot get weather data
    case WEATHER_ERROR:
      lcd.print(0, 1, "ERROR");
      break;

    // Attempt to make a connection to the remote server failed
    case WEATHER_ERROR_1:
      lcd.print(0, 1, "CANNOT CONNECT");
      break;

      // Not a legitimate icon and led_times
    case WEATHER_ERROR_2:
      lcd.print(0, 1, "ERROR ICON + LED_TIMES");
      break;

    // Clear skies (day)
    case WEATHER_BLUE_SKY:
      lcd.print(0, 1, "BLUE SKY");
      out_msg[0] = COMM_BLUE_SKY;
      break;

    // Clear skies (night)
    case WEATHER_NIGHT:
      lcd.print(0, 1, "NIGHT");
      out_msg[0] = COMM_NIGHT;
      break;

    // Lots of clouds
    case WEATHER_OVERCAST:
      lcd.print(0, 1, "OVERCAST");
      out_msg[0] = COMM_OVERCAST;
      break;

    // Dawn or dusk
    case WEATHER_GOLDEN:
      lcd.print(0, 1, "GOLDEN");
      out_msg[0] = COMM_GOLDEN;
      break;

    // It's snowing!
    case WEATHER_SNOW:
      lcd.print(0, 1, "SNOW");
      out_msg[0] = COMM_SNOW;
      break;

    // Storm
    case WEATHER_LIGHTNING:
      lcd.print(0, 1, "STORM");
      out_msg[0] = COMM_LIGHTNING;
      break;

    // Unknown weather type
    case WEATHER_UNKNOWN:
    default:
      lcd.print(0, 1, "UNKNOWN");
      break;
    }
    transmitMessage(out_msg, 4);
  }
}

// Set app LEDs
void setLEDs()
{

  // Turn them all off
  weather_led.off();
  rgb_led.off();
  disco_led.off();
  demo_led.off();
  lcd.clear();

  // Only turn on the one we want
  switch (cloud_state)
  {
  case CLOUD_WEATHER:
    weather_led.on();
    lcd.print(0, 0, "WEATHER");
    break;
  case CLOUD_RGB:
    rgb_led.on();
    lcd.print(0, 0, "RGB");
    break;
  case CLOUD_DISCO:
    disco_led.on();
    lcd.print(0, 0, "DISCO");
    break;
  case CLOUD_DEMO:
    demo_led.on();
    lcd.print(0, 0, "DEMO MODE");
    break;
  default:
    break;
  }
}

/****************************************************************
 * Setup
 ***************************************************************/
void setup()
{

  uint8_t led = 0;

  // Set up status LED
  pinMode(LED_PIN, OUTPUT);

  // Set up debug serial console
  Serial.begin(9600);

  // Start Blynk and wait for connection
  Blynk.begin(BLYNK_AUTH, WIFI_SSID, WIFI_PSK);
  while (Blynk.connect() == false)
  {
    led ^= 0x01;
    digitalWrite(LED_PIN, led);
    delay(200);
  }
  digitalWrite(LED_PIN, HIGH);

  // Define initial state
  cloud_state = CLOUD_WEATHER;
  setLEDs();
  led_time = millis();
  force_update == false;
  color_r = 0;
  color_g = 0;
  color_b = 0;
  lcd.clear();
}

/****************************************************************
 * Send Modes to Pro Mini
 ***************************************************************/

// Send RGB code with red, green, blue values
void sendRGB()
{
  out_msg[0] = COMM_RGB;
  out_msg[1] = color_r;
  out_msg[2] = color_g;
  out_msg[3] = color_b;
  transmitMessage(out_msg, 4);
}

// Send Disco code
void sendDisco()
{
  out_msg[0] = COMM_DISCO;
  transmitMessage(out_msg, 4);
}

/****************************************************************
 * Loop
 ***************************************************************/
void loop()
{
  Blynk.run();

  // Perform LED functions
  switch (cloud_state)
  {

  // Reflect weather data in clouds
  case CLOUD_WEATHER:
    doWeather();
    break;

  case CLOUD_RGB:
  case CLOUD_DISCO:
  default:
    break;
  }
}

/****************************************************************
 * Blynk Callbacks
 ***************************************************************/

// Callback when weather button is pushed
BLYNK_WRITE(WEATHER_BTN_PIN)
{
  if ((param.asInt() == 1) && (cloud_state != CLOUD_WEATHER))
  {
    cloud_state = CLOUD_WEATHER;
    setLEDs();
    force_update = true;
  }
}

// Callback when RGB button is pushed
BLYNK_WRITE(RGB_BTN_PIN)
{
  if ((param.asInt() == 1) && (cloud_state != CLOUD_RGB))
  {
    cloud_state = CLOUD_RGB;
    setLEDs();
    sendRGB();
  }
}

// Callback when Disco button is pushed
BLYNK_WRITE(DISCO_BTN_PIN)
{
  if ((param.asInt() == 1) && (cloud_state != CLOUD_DISCO))
  {
    cloud_state = CLOUD_DISCO;
    setLEDs();
    sendDisco();
  }
}

// Callback when demo mode button is pushed
BLYNK_WRITE(DEMO_BTN_PIN)
{
  memset(out_msg, 0, 3);
  if ((param.asInt() == 1))
  {
    lcd.clear();
    switch (current_weather)
    {
    case WEATHER_BLUE_SKY:
      current_weather = WEATHER_NIGHT;
      lcd.print(0, 0, "NIGHT");
      out_msg[0] = COMM_NIGHT;
      break;

    case WEATHER_NIGHT:
      current_weather = WEATHER_OVERCAST;
      lcd.print(0, 0, "OVERCAST");
      out_msg[0] = COMM_OVERCAST;
      break;

    case WEATHER_OVERCAST:
      current_weather = WEATHER_GOLDEN;
      lcd.print(0, 0, "GOLDEN");
      out_msg[0] = COMM_GOLDEN;
      break;

    case WEATHER_GOLDEN:
      current_weather = WEATHER_SNOW;
      lcd.print(0, 0, "SNOW");
      out_msg[0] = COMM_SNOW;
      break;

    case WEATHER_SNOW:
      current_weather = WEATHER_LIGHTNING;
      lcd.print(0, 0, "STORM");
      out_msg[0] = COMM_LIGHTNING;
      break;

    case WEATHER_LIGHTNING:
    default:
      current_weather = WEATHER_BLUE_SKY;
      lcd.print(0, 0, "BLUE SKY");
      out_msg[0] = COMM_BLUE_SKY;
      break;
    }
    transmitMessage(out_msg, 3);
  }
}

// Callback when the ZeRGBa changes
BLYNK_WRITE(ZERGBA_PIN)
{
  color_r = param[0].asInt();
  color_g = param[1].asInt();
  color_b = param[2].asInt();
  if (cloud_state == CLOUD_RGB)
  {
    sendRGB();
  }
}
