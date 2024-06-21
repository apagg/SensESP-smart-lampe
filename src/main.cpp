// Signal K application sensESP-smart-lampe.
//
// Lampe som gir hvit sterkt lys på dagen og rødt lys på natten 
// bruker environment.mode for å finne ut om det er natt eller dag
// Kontrolerer WS2812B LED stripe

// er ADS1115 koblet til?
#define ENABLE_ADS1115
#define ENABLE_1_WIRE

#include "sensesp/sensors/analog_input.h"
#include "sensesp/sensors/digital_input.h"
#include "sensesp/sensors/sensor.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/transforms/lambda_transform.h"
#include "sensesp/transforms/press_repeater.h"
#include "sensesp_app_builder.h"
#include "sensesp/signalk/signalk_listener.h"
#include "sensesp/signalk/signalk_value_listener.h"
#include "sensesp/transforms/repeat_report.h"
#include <Adafruit_NeoPixel.h>
#include <Adafruit_ADS1X15.h>
#include "DHT.h"
#include "sensesp/transforms/linear.h"
#include "sensesp_onewire/onewire_temperature.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define PIN_WS2812B 33  // The ESP32 pin GPIO16 connected to WS2812B
#define NUM_PIXELS 60    // The number of LEDs (pixels) on WS2812B LED strip
#define DHTPIN 5   // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

Adafruit_NeoPixel ws2812b(NUM_PIXELS, PIN_WS2812B, NEO_GRB + NEO_KHZ800);
DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);




using namespace sensesp;

String lampe_navn = "sensESP-smart-lampe";
String natt_dag = "day";
long on_time = 0;

reactesp::ReactESP app;

// Define the function that will be called every time we want
// an updated temperature value from the sensor. The sensor reads degrees
// Celsius, but all temps in Signal K are in Kelvin, so add 273.15.
float read_DHT_temp_callback() { return (dht.readTemperature() + 273.15); }
float read_DHT_hum_callback() { return (dht.readHumidity()); }

#ifdef ENABLE_ADS1115
Adafruit_ADS1115 ads;  
float read_ADS_A0() { return (ads.readADC_SingleEnded(0)); }
float read_ADS_A1() { return (ads.readADC_SingleEnded(1)); }
float read_ADS_A2() { return (ads.readADC_SingleEnded(2)); }
float read_ADS_A3() { return (ads.readADC_SingleEnded(3)); }
#endif

// The setup function performs one-time application initialization.
void setup() {

// display

if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }


display.display();
 delay(2000);
 display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setRotation(2);
  display.setCursor(0, 0);
  display.println("SmartLampe");
  display.println("Valentine");
  display.display();
//display

#ifndef SERIAL_DEBUG_DISABLED
  SetupSerialDebug(115200);
#endif

  // Construct the global SensESPApp() object
  SensESPAppBuilder builder;
  sensesp_app = (&builder)
                    // Set a custom hostname for the app.
                    ->set_hostname(lampe_navn)
                    // Optionally, hard-code the WiFi and Signal K server
                    // settings. This is normally not needed.
                    //->set_wifi("My WiFi SSID", "my_wifi_password")
                    //->set_sk_server("192.168.10.3", 80)
                    ->enable_ota("OTAord")
                    ->get_app();
 
  dht.begin();

  // Start DHT
  // Read the sensor every 2 seconds
  unsigned int read_DHT_interval = 2000;

  // Create a RepeatSensor with float output that reads the temperature
  // using the function defined above.
  auto* lampe_DHT_temp =
      new RepeatSensor<float>(read_DHT_interval, read_DHT_temp_callback);

  auto* lampe_DHT_hum =
      new RepeatSensor<float>(read_DHT_interval, read_DHT_hum_callback);

  // Set the Signal K Path for the output
  String sk_DHT_temp_path = "environment.lampe.name.temperature";
  String sk_DHT_temp_conf_path = "/sensors/temp/value";
  String sk_DHT_hum_path = "environment.lampe.name.relativeHumidity";
  String sk_DHT_hum_conf_path = "/sensors/hum/value";

  // Send the temperature to the Signal K server as a Float
  lampe_DHT_temp->connect_to(new SKOutputFloat(sk_DHT_temp_path,sk_DHT_temp_conf_path));
  lampe_DHT_hum->connect_to(new SKOutputFloat(sk_DHT_hum_path,sk_DHT_hum_conf_path));
  // Slutt DHT
  //
  //
  //
  // Start PIR
  //
  // Test om det er beveglese og slå av og på lyset 

  const uint8_t kPIRInputPin = 13; // Define PIR pin
  unsigned int pir_input_interval = 250;
  
  pinMode(kPIRInputPin, INPUT_PULLDOWN);
  
  auto* pir_input = new RepeatSensor<float>(pir_input_interval,
    [kPIRInputPin]() { return digitalRead(kPIRInputPin); });

  auto rgb_light = [](bool input, float time, int s_day, int s_night) -> bool {
    //debugD("********** Digital input value changed: %d", input);

    if (input == 1) {on_time = millis() + time * 1000; };
    //debugD("********** on_time: %d", on_time);
    //debugD("********** millis(): %d", millis());

    if (on_time < millis()) {  // lyset er av
      for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {  // for each pixel
        ws2812b.setPixelColor(pixel, ws2812b.Color(0, 0, 0));
        ws2812b.show();  // update to the WS2812B Led Strip
      }
    } else {
      if (natt_dag == "night") {
        for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {  // for each pixel
          ws2812b.setPixelColor(pixel, ws2812b.Color(s_night, 0, 0));
          ws2812b.show();  // update to the WS2812B Led Strip
        }
      }
      else {
        for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {  // for each pixel
          ws2812b.setPixelColor(pixel, ws2812b.Color(s_day, s_day, s_day));
          ws2812b.show();  // update to the WS2812B Led Strip
        }
      }
    }
    return input;
  };
  
  const ParamInfo* log_lambda_param_data = new ParamInfo[4]{
      {"time", "Time"}, {"s_day", "Strenght day"}, {"s_night", "Strenght night"}};

  auto light_transform = new LambdaTransform<bool, bool, float, int, int>(
    rgb_light, 60, 255, 50, log_lambda_param_data,
      "/light/parameters");

  light_transform->set_description ("Set the time for light to be on and the strenght of the light");
  pir_input->connect_to(light_transform);
  
  
  String sk_PIR_path = "sensors.pir.name.value";
  String sk_PIR_conf_path = "/sensors/pir/value";
  pir_input->connect_to(new SKOutputBool(
      sk_PIR_path,          // Signal K path
      sk_PIR_conf_path,     // configuration path
      new SKMetadata("",    // No units for boolean values
        "PIR input value")  // Value description
      ));

  // Listen for enviroment.mode
  const char* sk_sun = "environment.mode";
  const int listen_delay = 1000;
  auto* env_sun = new StringSKListener(sk_sun, listen_delay);
  
  env_sun->connect_to(new LambdaConsumer<String>([](String sun) {
    natt_dag = sun;
    debugD(" <<<<<<<<<<<<<<<<<<Natt eller dag>>>>>>>>>>>>>>>>>: %s   ", sun);
    
  }));

  // End PIR
  //
  //
  //
  // Start ADS1X15
  #ifdef ENABLE_ADS1115
  ads.begin();

  // I2C pins 
  // SDA Pin = 21;
  // SCL Pin = 22;
    
  unsigned int read_ADS_interval = 2000;
  
  auto lampe_ADS_A0 =
      new RepeatSensor<float>(read_ADS_interval, read_ADS_A0);

  String sk_ADS_A0_path = "environment.lampe.name.A0";
  String sk_ADS_A0_conf_path = "/sensors/A0/path";
  String sk_ADS_A0_Linear_conf_path = "/sensor/A0/Linear";
  
  lampe_ADS_A0->connect_to(new Linear(0.002065,0,sk_ADS_A0_Linear_conf_path))->connect_to(
    new SKOutputFloat(sk_ADS_A0_path,sk_ADS_A0_conf_path));

  auto lampe_ADS_A1 =
      new RepeatSensor<float>(read_ADS_interval, read_ADS_A1);

  String sk_ADS_A1_path = "environment.lampe.name.A1";
  String sk_ADS_A1_conf_path = "/sensors/A1/path";
  String sk_ADS_A1_Linear_conf_path = "/sensor/A1/Linear";
  
  lampe_ADS_A1->connect_to(new Linear(0.002065,0,sk_ADS_A1_Linear_conf_path))->connect_to(
    new SKOutputFloat(sk_ADS_A1_path,sk_ADS_A1_conf_path));

  

  auto lampe_ADS_A2 =
      new RepeatSensor<float>(read_ADS_interval, read_ADS_A2);

  String sk_ADS_A2_path = "environment.lampe.name.A2";
  String sk_ADS_A2_conf_path = "/sensors/A2/path";
  String sk_ADS_A2_Linear_conf_path = "/sensor/A2/Linear";
  
  lampe_ADS_A2->connect_to(new Linear(0.002065,0,sk_ADS_A2_Linear_conf_path))->connect_to(
    new SKOutputFloat(sk_ADS_A2_path,sk_ADS_A2_conf_path));

  auto lampe_ADS_A3 =
      new RepeatSensor<float>(read_ADS_interval, read_ADS_A3);

  String sk_ADS_A3_path = "environment.lampe.name.A3";
  String sk_ADS_A3_conf_path = "/sensors/A3/path";
  String sk_ADS_A3_Linear_conf_path = "/sensor/A3/Linear";
  
  lampe_ADS_A3->connect_to(new Linear(0.002065,0,sk_ADS_A3_Linear_conf_path))->connect_to(
    new SKOutputFloat(sk_ADS_A3_path,sk_ADS_A3_conf_path));
  
  #endif
  // End ADS1X15
  //
  // Start 1-wire
  #ifdef ENABLE_1_WIRE
  uint8_t oneWirePin = 4;

  DallasTemperatureSensors* dts = new DallasTemperatureSensors(oneWirePin);

  // Define how often SensESP should read the sensor(s) in milliseconds
  uint onewire_read_delay = 500;

  // Below are temperatures sampled and sent to Signal K server
  // To find valid Signal K Paths that fits your need you look at this link:
  // https://signalk.org/specification/1.4.0/doc/vesselsBranch.html

// temp display
  auto* temp_display = new LambdaConsumer<float>([](float input) -> void {
    display.clearDisplay();
    display.setCursor(0, 0);
    //display.println("Temp: ");
    display.setTextSize(4);
    display.println(input - 273.15, 1);
    display.display();
  
  });

  

  // Measure onewire 1
  auto* onewire_temp_1 =
      new OneWireTemperature(dts, onewire_read_delay, "/onewire_1/oneWire");

  onewire_temp_1->connect_to(new Linear(1.0, 0.0, "/onewire_1/linear"))
      ->connect_to(new SKOutputFloat("environment.onewire_1.temperature",
                                     "/onewire_1/skPath"));

  onewire_temp_1->connect_to(temp_display);

  // Measure onewire 2
  auto* onewire_temp_2 =
      new OneWireTemperature(dts, onewire_read_delay, "/onewire_2/oneWire");

  onewire_temp_2->connect_to(new Linear(1.0, 0.0, "/onewire_2/linear"))
      ->connect_to(new SKOutputFloat("environment.onewire_2.temperature",
                                     "/onewire_2/skPath"));

  // Measure onewire 3
  auto* onewire_temp_3 =
      new OneWireTemperature(dts, onewire_read_delay, "/onewire_3/oneWire");

  onewire_temp_3->connect_to(new Linear(1.0, 0.0, "/onewire_3/linear"))
      ->connect_to(new SKOutputFloat("environment.onewire_3.temperature",
                                     "/onewire_3/skPath"));
  
  // Measure  onewire 4
  auto* onewire_temp_4 =
      new OneWireTemperature(dts, onewire_read_delay, "/onewire_4/oneWire");

  onewire_temp_4->connect_to(new Linear(1.0, 0.0, "/onewire_4/linear"))
      ->connect_to(new SKOutputFloat("environment.onewire_4.temperature",
                                     "/onewire_4/skPath"));

  #endif
  // End 1-wire
  //
  // Start networking, SK server connections and other SensESP internals
  sensesp_app->start();
}

void loop() { app.tick(); }
