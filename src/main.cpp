// Signal K application sensESP-smart-lampe.
//
// Lampe som gir hvit sterkt lys på dagen og rødt lys på natten 
// bruker environment.mode for å finne ut om det er natt eller dag
// Kontrolerer WS2812B LED stripe

#include "sensesp/sensors/analog_input.h"
#include "sensesp/sensors/digital_input.h"
#include "sensesp/sensors/sensor.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp_app_builder.h"
#include "sensesp/signalk/signalk_listener.h"
#include "sensesp/signalk/signalk_value_listener.h"
#include <Adafruit_NeoPixel.h>
#include "DHT.h"

#define PIN_WS2812B 33  // The ESP32 pin GPIO16 connected to WS2812B
#define NUM_PIXELS 24    // The number of LEDs (pixels) on WS2812B LED strip
#define DHTPIN 5   // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define PIRPIN 13 // PIR PIN


Adafruit_NeoPixel ws2812b(NUM_PIXELS, PIN_WS2812B, NEO_GRB + NEO_KHZ800);
DHT dht(DHTPIN, DHTTYPE);

using namespace sensesp;

String lampe_navn = "salong";
String lampe_navn_fult = "sensESP-smart-lampe-" + lampe_navn;
String natt_dag = "day";

reactesp::ReactESP app;

// Define the function that will be called every time we want
// an updated temperature value from the sensor. The sensor reads degrees
// Celsius, but all temps in Signal K are in Kelvin, so add 273.15.
float read_DHT_temp_callback() { return (dht.readTemperature() + 273.15); }
float read_DHT_hum_callback() { return (dht.readHumidity()); }

// The setup function performs one-time application initialization.
void setup() {


#ifndef SERIAL_DEBUG_DISABLED
  SetupSerialDebug(115200);
#endif

  // Construct the global SensESPApp() object
  SensESPAppBuilder builder;
  sensesp_app = (&builder)
                    // Set a custom hostname for the app.
                    ->set_hostname(lampe_navn_fult)
                    // Optionally, hard-code the WiFi and Signal K server
                    // settings. This is normally not needed.
                    //->set_wifi("My WiFi SSID", "my_wifi_password")
                    //->set_sk_server("192.168.10.3", 80)
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
  String sk_DHT_temp_path = "environment.lampe." + lampe_navn + ".temperature";
  String sk_DHT_hum_path = "environment.lampe." + lampe_navn + ".relativeHumidity";

  // Send the temperature to the Signal K server as a Float
  lampe_DHT_temp->connect_to(new SKOutputFloat(sk_DHT_temp_path));
  lampe_DHT_hum->connect_to(new SKOutputFloat(sk_DHT_hum_path));
  // Slutt DHT

  
  // Test om det er beveglese og slå av og på lyset 
  const uint8_t kPIRInputPin = 13; // Define PIR pin
  unsigned int pir_input_interval = 250;
  String sk_PIR_path = "sensors.pir." + lampe_navn + ".value";
  String sk_PIR_conf_path = "/sensors/pir/" + lampe_navn + "/value";
  
  pinMode(kPIRInputPin, INPUT_PULLDOWN);
  
  auto* pir_input = new RepeatSensor<bool>(
      pir_input_interval,
      [kPIRInputPin]() { return digitalRead(kPIRInputPin); });

  pir_input->connect_to(new LambdaConsumer<bool>([](bool input) {
    //debugD("********** Digital input value changed: %d", input);
     
    if (input == 0) {  // lyset er av
      
      for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {  // for each pixel
        ws2812b.setPixelColor(pixel, ws2812b.Color(0, 0, 0));
        ws2812b.show();  // update to the WS2812B Led Strip
      }
    } else {
      
      if (natt_dag == "night") {
        for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {  // for each pixel
          ws2812b.setPixelColor(pixel, ws2812b.Color(50, 0, 0));
          ws2812b.show();  // update to the WS2812B Led Strip
        }
      }

      else {
        for (int pixel = 0; pixel < NUM_PIXELS; pixel++) {  // for each pixel
          ws2812b.setPixelColor(pixel, ws2812b.Color(255, 255, 255));
          ws2812b.show();  // update to the WS2812B Led Strip
        }
      }
    }
  }));
  
  // Connect PIR to Signal K output.
  pir_input->connect_to(new SKOutputBool(
      sk_PIR_path,          // Signal K path
      sk_PIR_conf_path,         // configuration path
      new SKMetadata("",                       // No units for boolean values
                     "PIR input value")  // Value description
      ));

  // Listen for enviroment.mode
  const char* sk_sun = "environment.mode";
  const int listen_delay = 1000;
  auto* env_sun = new StringSKListener(sk_sun, listen_delay);
  
  env_sun->connect_to(new LambdaConsumer<String>([](String sun) {
    natt_dag = sun;
    Serial.printf(" Natt eller dag: %s   ", sun);
    Serial.println();
  }));

  // Start networking, SK server connections and other SensESP internals
  sensesp_app->start();
}

void loop() { app.tick(); }
