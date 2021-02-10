#define FASTLED_INTERRUPT_RETRY_COUNT 1
#define FASTLED_ESP8266_D1_PIN_ORDER
#include <hsv2rgb.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Adafruit_NeoPixel.h>
#include "secrets.h"

/************* MQTT TOPICS (change these topics as you wish) ************************/
#define light_state_topic "rocket/led"
#define light_set_topic "rocket/led/set"

const char *on_cmd = "ON";
const char *off_cmd = "OFF";
const char *effect = "solid";
String effectString = "solid";
String oldeffectString = "solid";

/*FOR JSON*/
const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);
#define MQTT_MAX_PACKET_SIZE 512

/*********************************** FastLED Defintions ********************************/
#define NUM_LEDS 144
#define DATA_PIN D2
//#define CLOCK_PIN 5
#define CHIPSET WS2812B
#define COLOR_ORDER GRB

byte realRed = 0;
byte realGreen = 0;
byte realBlue = 0;

byte red = 255;
byte green = 255;
byte blue = 255;
byte brightness = 150;

#define PIN D2
//#define NUM_LEDS 144
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);

/******************************** GLOBALS for fade/flash *******************************/
bool stateOn = false;
bool startFade = false;
bool onbeforeflash = false;
unsigned long lastLoop = 0;
int transitionTime = 0;
int effectSpeed = 0;
bool inFade = false;
int loopCount = 0;
int stepR, stepG, stepB;
int redVal, grnVal, bluVal;

bool flash = false;
bool startFlash = false;
int flashLength = 0;
unsigned long flashStartTime = 0;
byte flashRed = red;
byte flashGreen = green;
byte flashBlue = blue;
byte flashBrightness = brightness;

/********************************** GLOBALS for EFFECTS ******************************/
//RAINBOW
uint8_t thishue = 0; // Starting hue value.
uint8_t deltahue = 10;

//CANDYCANE
CRGBPalette16 currentPalettestriped; //for Candy Cane
CRGBPalette16 gPal;                  //for fire

//NOISE
static uint16_t dist;    // A random number for our noise generator.
uint16_t scale = 30;     // Wouldn’t recommend changing this on the fly, or the animation will be really blocky.
uint8_t maxChanges = 48; // Value for blending between palettes.
CRGBPalette16 targetPalette(OceanColors_p);
CRGBPalette16 currentPalette(CRGB::Black);

//TWINKLE
#define DENSITY 80
int twinklecounter = 0;

//RIPPLE
uint8_t colour;       // Ripple colour is randomized.
int center = 0;       // Center of the current ripple.
int step = -1;        // -1 is the initializing step.
uint8_t myfade = 255; // Starting brightness.
#define maxsteps 16   // Case statement wouldn’t allow a variable.
uint8_t bgcol = 0;    // Background colour rotates.
int thisdelay = 20;   // Standard delay value.

//DOTS
uint8_t count = 0;     // Count up to 255 and then reverts to 0
uint8_t fadeval = 224; // Trail behind the LED’s. Lower => faster fade.
uint8_t bpm = 30;

//LIGHTNING
uint8_t frequency = 50; // controls the interval between strikes
uint8_t flashes = 8;    //the upper limit of flashes per strike
unsigned int dimmer = 1;
uint8_t ledstart; // Starting location of a flash
uint8_t ledlen;
int lightningcounter = 0;

//FUNKBOX
int idex = 0; //-LED INDEX (0 to NUM_LEDS-1
int TOP_INDEX = int(NUM_LEDS / 2);
int thissat = 255; //-FX LOOPS DELAY VAR
uint8_t thishuepolice = 0;
int antipodal_index(int i)
{
    int iN = i + TOP_INDEX;
    if (i >= TOP_INDEX)
    {
        iN = (i + TOP_INDEX) % NUM_LEDS;
    }
    return iN;
}
//ROCKETS
#define NUM_ROWS 3
#define NUM_COLS 48
extern const TProgmemRGBPalette16 RocketColors_p FL_PROGMEM =
{
  0xFF0000, 0xFF3300, 0xFF6600, 0xFF9900, 
  0xFFCC00, 0xFFFF00, 0xFFFF33, 0xFFFF66,
  0xFFFF99, 0xFFFFCC, 0xFFFFCC, 0xFFFFFF, 
  0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF
};


//FIRE
#define COOLING 55
#define SPARKING 120
bool gReverseDirection = false;

//BPM
uint8_t gHue = 0;

WiFiClient espClient;
PubSubClient client(espClient);
struct CRGB leds[NUM_LEDS];

/********************************** START SETUP*****************************************/
void setup()
{
    Serial.begin(115200);
    //FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    //Setup NeoPixel
    strip.begin();
    strip.show();             // Initialize all pixels to 'off'
    strip.setBrightness(150); // Set BRIGHTNESS to about 1/5 (max = 255)

    setup_wifi();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    //OTA SETUP
    ArduinoOTA.setPort(OTAport);
    // Hostname defaults to esp8266-[ChipID]
    ArduinoOTA.setHostname(SENSORNAME);

    // No authentication by default
    //ArduinoOTA.setPassword((const char *)OTApassword);

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
        {
            type = "sketch";
        }
        else
        { // U_FS
            type = "filesystem";
        }

        // NOTE: if updating FS this would be the place to unmount FS using FS.end()
        Serial.println("Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
        {
            Serial.println("Auth Failed");
        }
        else if (error == OTA_BEGIN_ERROR)
        {
            Serial.println("Begin Failed");
        }
        else if (error == OTA_CONNECT_ERROR)
        {
            Serial.println("Connect Failed");
        }
        else if (error == OTA_RECEIVE_ERROR)
        {
            Serial.println("Receive Failed");
        }
        else if (error == OTA_END_ERROR)
        {
            Serial.println("End Failed");
        }
    });
    ArduinoOTA.begin();
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

/********************************** START SETUP WIFI*****************************************/
void setup_wifi()
{

    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

/*
SAMPLE PAYLOAD:
{
"brightness": 120,
"color": {
"r": 255,
"g": 100,
"b": 100
},
"flash": 2,
"transition": 5,
"state": "ON"
}
*/

/********************************** START CALLBACK*****************************************/
void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    char message[length + 1];
    for (int i = 0; i < length; i++)
    {
        message[i] = (char)payload[i];
    }
    message[length] = '\0';
    Serial.println(message);

    if (!processJson(message))
    {
        return;
    }

    if (stateOn)
    {

        //
        realRed = map(red, 0, 255, 0, brightness);
        realGreen = map(green, 0, 255, 0, brightness);
        realBlue = map(blue, 0, 255, 0, brightness);
    }
    else
    {

        //
        realRed = 0;
        realGreen = 0;
        realBlue = 0;
    }

    Serial.println(effect);

    startFade = true;
    inFade = false; // Kill the current fade

    sendState();
}

/********************************** START PROCESS JSON*****************************************/
bool processJson(char *message)
{
    StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

    JsonObject &root = jsonBuffer.parseObject(message);

    if (!root.success())
    {
        Serial.println("parseObject() failed");
        return false;
    }

    if (root.containsKey("state"))
    {
        if (strcmp(root["state"], on_cmd) == 0)
        {
            stateOn = true;
        }
        else if (strcmp(root["state"], off_cmd) == 0)
        {
            stateOn = false;
            onbeforeflash = false;
        }
    }

    // If "flash" is included, treat RGB and brightness differently
    if (root.containsKey("flash"))
    {
        flashLength = (int)root["flash"] * 1000;

        oldeffectString = effectString;

        // Disable brightness
        // if (root.containsKey("brightness"))
        // {
        //     flashBrightness = root["brightness"];
        // }
        // else
        // {
        //     flashBrightness = brightness;
        // }

        if (root.containsKey("color"))
        {
            flashRed = root["color"]["r"];
            flashGreen = root["color"]["g"];
            flashBlue = root["color"]["b"];
        }
        else
        {
            flashRed = red;
            flashGreen = green;
            flashBlue = blue;
        }

        if (root.containsKey("effect"))
        {
            effect = root["effect"];
            effectString = effect;
            twinklecounter = 0; //manage twinklecounter
        }

        if (root.containsKey("transition"))
        {
            transitionTime = root["transition"];
        }
        else if (effectString == "solid")
        {
            transitionTime = 0;
        }

        flashRed = map(flashRed, 0, 255, 0, flashBrightness);
        flashGreen = map(flashGreen, 0, 255, 0, flashBrightness);
        flashBlue = map(flashBlue, 0, 255, 0, flashBrightness);

        flash = true;
        startFlash = true;
    }
    else
    { // Not flashing
        flash = false;

        if (stateOn)
        { //if the light is turned on and the light isn't flashing
            onbeforeflash = true;
        }

        if (root.containsKey("color"))
        {
            red = root["color"]["r"];
            green = root["color"]["g"];
            blue = root["color"]["b"];
        }

        if (root.containsKey("color_temp"))
        {
            //temp comes in as mireds, need to convert to kelvin then to RGB
            int color_temp = root["color_temp"];
            unsigned int kelvin = 1000000 / color_temp;

            temp2rgb(kelvin);
        }

        //Disable Brightness
        // if (root.containsKey("brightness"))
        // {
        //     brightness = root["brightness"];
        // }

        if (root.containsKey("effect"))
        {
            effect = root["effect"];
            effectString = effect;
            twinklecounter = 0; //manage twinklecounter
        }

        if (root.containsKey("transition"))
        {
            transitionTime = root["transition"];
        }
        else if (effectString == "solid")
        {
            transitionTime = 0;
        }
    }

    return true;
}
CRGB temp2rgb(unsigned int kelvin)
{
    int temp = kelvin / 100;
    int r, g, b;

    if (temp <= 66)
    {
        r = 255;
        g = 99.4708025861 * log(temp) - 161.1195681661;
        if (temp <= 19)
        {
            b = 0;
        }
        else
        {
            b = 138.5177312231 * log(temp - 10) - 305.0447927307;
        }
    }
    else
    {
        if (temp < 60)
        {
            r = 0;
            g = 0;
        }
        else
        {
            r = 329.698727446 * pow(temp - 60, -0.1332047592);
            g = 288.1221695283 * pow(temp - 60, -0.0755148492);
        }
        b = 255;
    }

    r = (r > 0) ? r : 0;
    g = (g > 0) ? g : 0;
    b = (b > 0) ? b : 0;

    return CRGB(r, g, b);
}
/********************************** START SEND STATE*****************************************/
void sendState()
{
    StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

    JsonObject &root = jsonBuffer.createObject();

    root["state"] = (stateOn) ? on_cmd : off_cmd;
    JsonObject &color = root.createNestedObject("color");
    color["r"] = red;
    color["g"] = green;
    color["b"] = blue;

    root["brightness"] = brightness;
    root["effect"] = effectString.c_str();

    char buffer[root.measureLength() + 1];
    root.printTo(buffer, sizeof(buffer));

    client.publish(light_state_topic, buffer, true);
}

/********************************** START RECONNECT*****************************************/
void reconnect()
{
    // Loop until we’re reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection…");
        // Attempt to connect
        if (client.connect(SENSORNAME, mqtt_username, mqtt_password))
        {
            Serial.println("connected");
            client.subscribe(light_set_topic);
            setColor(0, 0, 0);
            sendState();
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

/********************************** START Set Color*****************************************/
void setColor(int inR, int inG, int inB)
{
    for (int i = 0; i < NUM_LEDS; i++)
    {
        leds[i].red = inR;
        leds[i].green = inG;
        leds[i].blue = inB;
    }

    FastLED.show();

    Serial.println("Setting LEDs:");
    Serial.print("r: ");
    Serial.print(inR);
    Serial.print(", g: ");
    Serial.print(inG);
    Serial.print(", b: ");
    Serial.println(inB);
}

void Fire(int Cooling, int Sparking, int SpeedDelay)
{
    static byte heat[NUM_LEDS];
    int cooldown;

    // Step 1.  Cool down every cell a little
    for (int i = 0; i < NUM_LEDS; i++)
    {
        cooldown = random(0, ((Cooling * 10) / NUM_LEDS) + 2);

        if (cooldown > heat[i])
        {
            heat[i] = 0;
        }
        else
        {
            heat[i] = heat[i] - cooldown;
        }
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (int k = NUM_LEDS - 1; k >= 2; k--)
    {
        heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }

    // Step 3.  Randomly ignite new 'sparks' near the bottom
    if (random(255) < Sparking)
    {
        int y = random(7);
        heat[y] = heat[y] + random(160, 255);
    }

    // Step 4.  Convert heat to LED colors
    for (int j = 0; j < NUM_LEDS; j++)
    {
        setPixelHeatColor(j, heat[j]);
    }

    strip.show();
    delay(SpeedDelay);
}

void setPixelHeatColor(int Pixel, byte temperature)
{
    // Scale 'heat' down from 0-255 to 0-191
    byte t192 = round((temperature / 255.0) * 191);

    // calculate ramp up from
    byte heatramp = t192 & 0x3F; // 0..63
    heatramp <<= 2;              // scale up to 0..252

    // figure out which third of the spectrum we're in:
    if (t192 > 0x80)
    { // hottest
        setPixel(Pixel, 255, 255, heatramp);
    }
    else if (t192 > 0x40)
    { // middle
        setPixel(Pixel, 255, heatramp, 0);
    }
    else
    { // coolest
        setPixel(Pixel, heatramp, 0, 0);
    }
}

void setPixel(int Pixel, byte red, byte green, byte blue)
{
    strip.setPixelColor(Pixel, strip.Color(red, green, blue));
}

void setAll(byte red, byte green, byte blue)
{
    for (int i = 0; i < NUM_LEDS; i++)
    {
        setPixel(i, red, green, blue);
    }
    strip.show();
}
/********************************** START MAIN LOOP*****************************************/
void loop()
{
    // Add entropy to random number generator; we use a lot of it.
    random16_add_entropy( random8());

    if (!client.connected())
    {
        reconnect();
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        delay(1);
        Serial.print("WIFI Disconnected. Attempting reconnection.");
        setup_wifi();
        return;
    }

    client.loop();

    ArduinoOTA.handle();


    //EFFECT Rocket Take Off
    if (effectString == "rocket_take_off")
    {
        RocketTakeOff();
        showleds();
    }

     //EFFECT Rocket Flight
    if (effectString == "rocket_flight")
    {
        RocketFlight();
        showleds();
    }

    //EFFECT BPM
    if (effectString == "bpm")
    {
        uint8_t BeatsPerMinute = 62;
        CRGBPalette16 palette = PartyColors_p;
        uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
        for (int i = 0; i < NUM_LEDS; i++)
        { //9948
            leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
        }
        if (transitionTime == 0 or transitionTime == NULL)
        {
            transitionTime = 30;
        }
        showleds();
    }

    //EFFECT Candy Cane
    if (effectString == "candy cane")
    {
        static uint8_t startIndex = 0;
        startIndex = startIndex + 1; /* higher = faster motion */
        fill_palette(leds, NUM_LEDS,
                     startIndex, 16, /* higher = narrower stripes */
                     currentPalettestriped, 255, LINEARBLEND);
        if (transitionTime == 0 or transitionTime == NULL)
        {
            transitionTime = 0;
        }
        showleds();
    }

    //EFFECT CONFETTI
    if (effectString == "confetti")
    {
        fadeToBlackBy(leds, NUM_LEDS, 25);
        int pos = random16(NUM_LEDS);
        leds[pos] += CRGB(realRed + random8(64), realGreen, realBlue);
        if (transitionTime == 0 or transitionTime == NULL)
        {
            transitionTime = 30;
        }
        showleds();
    }

    //EFFECT CYCLON RAINBOW
    if (effectString == "cyclon rainbow")
    { //Single Dot Down
        static uint8_t hue = 0;
        // First slide the led in one direction
        for (int i = 0; i < NUM_LEDS; i++)
        {
            // Set the i'th led to red
            leds[i] = CHSV(hue++, 255, 255);
            // Show the leds
            showleds();
            // now that we've shown the leds, reset the i'th led to black
            // leds[i] = CRGB::Black;
            fadeall();
            // Wait a little bit before we loop around and do it again
            delay(10);
        }
        for (int i = (NUM_LEDS)-1; i >= 0; i--)
        {
            // Set the i'th led to red
            leds[i] = CHSV(hue++, 255, 255);
            // Show the leds
            showleds();
            // now that we've shown the leds, reset the i'th led to black
            // leds[i] = CRGB::Black;
            fadeall();
            // Wait a little bit before we loop around and do it again
            delay(10);
        }
    }

    //EFFECT DOTS
    if (effectString == "dots")
    {
        uint8_t inner = beatsin8(bpm, NUM_LEDS / 4, NUM_LEDS / 4 * 3);
        uint8_t outer = beatsin8(bpm, 0, NUM_LEDS - 1);
        uint8_t middle = beatsin8(bpm, NUM_LEDS / 3, NUM_LEDS / 3 * 2);
        leds[middle] = CRGB::Purple;
        leds[inner] = CRGB::Blue;
        leds[outer] = CRGB::Aqua;
        nscale8(leds, NUM_LEDS, fadeval);

        if (transitionTime == 0 or transitionTime == NULL)
        {
            transitionTime = 30;
        }
        showleds();
    }

    //EFFECT FIRE
    if (effectString == "fire")
    {
        Fire2012WithPalette();
        if (transitionTime == 0 or transitionTime == NULL)
        {
            transitionTime = 150;
        }
        showleds();
    }

    random16_add_entropy(random8());

    //EFFECT Glitter
    if (effectString == "glitter")
    {
        fadeToBlackBy(leds, NUM_LEDS, 20);
        addGlitterColor(80, realRed, realGreen, realBlue);
        if (transitionTime == 0 or transitionTime == NULL)
        {
            transitionTime = 30;
        }
        showleds();
    }

    //EFFECT JUGGLE
    if (effectString == "juggle")
    { // eight colored dots, weaving in and out of sync with each other
        fadeToBlackBy(leds, NUM_LEDS, 20);
        for (int i = 0; i < 8; i++)
        {
            leds[beatsin16(i + 7, 0, NUM_LEDS - 1)] |= CRGB(realRed, realGreen, realBlue);
        }
        if (transitionTime == 0 or transitionTime == NULL)
        {
            transitionTime = 130;
        }
        showleds();
    }

    //EFFECT LIGHTNING
    if (effectString == "lightning")
    {
        twinklecounter = twinklecounter + 1; //Resets strip if previous animation was running
        if (twinklecounter < 2)
        {
            FastLED.clear();
            FastLED.show();
        }
        ledstart = random8(NUM_LEDS);          // Determine starting location of flash
        ledlen = random8(NUM_LEDS - ledstart); // Determine length of flash (not to go beyond NUM_LEDS-1)
        for (int flashCounter = 0; flashCounter < random8(3, flashes); flashCounter++)
        {
            if (flashCounter == 0)
                dimmer = 5; // the brightness of the leader is scaled down by a factor of 5
            else
                dimmer = random8(1, 3); // return strokes are brighter than the leader
            fill_solid(leds + ledstart, ledlen, CHSV(255, 0, 255 / dimmer));
            showleds();                                           // Show a section of LED's
            delay(random8(4, 10));                                // each flash only lasts 4-10 milliseconds
            fill_solid(leds + ledstart, ledlen, CHSV(255, 0, 0)); // Clear the section of LED's
            showleds();
            if (flashCounter == 0)
                delay(130);           // longer delay until next flash after the leader
            delay(50 + random8(100)); // shorter delay between strokes
        }
        delay(random8(frequency) * 100); // delay between strikes
        if (transitionTime == 0 or transitionTime == NULL)
        {
            transitionTime = 0;
        }
        showleds();
    }

    //EFFECT POLICE ALL
    if (effectString == "police all")
    { //POLICE LIGHTS (TWO COLOR SOLID)
        idex++;
        if (idex >= NUM_LEDS)
        {
            idex = 0;
        }
        int idexR = idex;
        int idexB = antipodal_index(idexR);
        int thathue = (thishuepolice + 160) % 255;
        leds[idexR] = CHSV(thishuepolice, thissat, 255);
        leds[idexB] = CHSV(thathue, thissat, 255);
        if (transitionTime == 0 or transitionTime == NULL)
        {
            transitionTime = 30;
        }
        showleds();
    }

    //EFFECT POLICE ONE
    if (effectString == "police one")
    {
        idex++;
        if (idex >= NUM_LEDS)
        {
            idex = 0;
        }
        int idexR = idex;
        int idexB = antipodal_index(idexR);
        int thathue = (thishuepolice + 160) % 255;
        for (int i = 0; i < NUM_LEDS; i++)
        {
            if (i == idexR)
            {
                leds[i] = CHSV(thishuepolice, thissat, 255);
            }
            else if (i == idexB)
            {
                leds[i] = CHSV(thathue, thissat, 255);
            }
            else
            {
                leds[i] = CHSV(0, 0, 0);
            }
        }
        if (transitionTime == 0 or transitionTime == NULL)
        {
            transitionTime = 30;
        }
        showleds();
    }

    //EFFECT RAINBOW
    if (effectString == "rainbow")
    {
        // FastLED's built-in rainbow generator
        static uint8_t starthue = 0;
        thishue++;
        fill_rainbow(leds, NUM_LEDS, thishue, deltahue);
        if (transitionTime == 0 or transitionTime == NULL)
        {
            transitionTime = 130;
        }
        showleds();
    }

    //EFFECT RAINBOW WITH GLITTER
    if (effectString == "rainbow with glitter")
    { // FastLED's built-in rainbow generator with Glitter
        static uint8_t starthue = 0;
        thishue++;
        fill_rainbow(leds, NUM_LEDS, thishue, deltahue);
        addGlitter(80);
        if (transitionTime == 0 or transitionTime == NULL)
        {
            transitionTime = 130;
        }
        showleds();
    }

    //EFFECT SIENLON
    if (effectString == "sinelon")
    {
        fadeToBlackBy(leds, NUM_LEDS, 20);
        int pos = beatsin16(13, 0, NUM_LEDS - 1);
        leds[pos] += CRGB(realRed, realGreen, realBlue);
        if (transitionTime == 0 or transitionTime == NULL)
        {
            transitionTime = 150;
        }
        showleds();
    }

    //EFFECT TWINKLE
    if (effectString == "twinkle")
    {
        twinklecounter = twinklecounter + 1;
        if (twinklecounter < 2)
        { //Resets strip if previous animation was running
            FastLED.clear();
            FastLED.show();
        }
        const CRGB lightcolor(8, 7, 1);
        for (int i = 0; i < NUM_LEDS; i++)
        {
            if (!leds[i])
                continue; // skip black pixels
            if (leds[i].r & 1)
            {                          // is red odd?
                leds[i] -= lightcolor; // darken if red is odd
            }
            else
            {
                leds[i] += lightcolor; // brighten if red is even
            }
        }
        if (random8() < DENSITY)
        {
            int j = random16(NUM_LEDS);
            if (!leds[j])
                leds[j] = lightcolor;
        }

        if (transitionTime == 0 or transitionTime == NULL)
        {
            transitionTime = 0;
        }
        showleds();
    }

    EVERY_N_MILLISECONDS(10)
    {

        nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges); // FOR NOISE ANIMATIon
        {
            gHue++;
        }

        //EFFECT NOISE
        if (effectString == "noise")
        {
            for (int i = 0; i < NUM_LEDS; i++)
            {                                                                        // Just onE loop to fill up the LED array as all of the pixels change.
                uint8_t index = inoise8(i * scale, dist + i * scale) % 255;          // Get a value from the noise function. I'm using both x and y axis.
                leds[i] = ColorFromPalette(currentPalette, index, 255, LINEARBLEND); // With that value, look up the 8 bit colour palette value and assign it to the current LED.
            }
            dist += beatsin8(10, 1, 4); // Moving along the distance (that random number we started out with). Vary it a bit with a sine wave.
            // In some sketches, I've used millis() instead of an incremented counter. Works a treat.
            if (transitionTime == 0 or transitionTime == NULL)
            {
                transitionTime = 0;
            }
            showleds();
        }

        //EFFECT RIPPLE
        if (effectString == "ripple")
        {
            for (int i = 0; i < NUM_LEDS; i++)
                leds[i] = CHSV(bgcol++, 255, 15); // Rotate background colour.
            switch (step)
            {
            case -1: // Initialize ripple variables.
                center = random(NUM_LEDS);
                colour = random8();
                step = 0;
                break;
            case 0:
                leds[center] = CHSV(colour, 255, 255); // Display the first pixel of the ripple.
                step++;
                break;
            case maxsteps: // At the end of the ripples.
                step = -1;
                break;
            default:                                                                                 // Middle of the ripples.
                leds[(center + step + NUM_LEDS) % NUM_LEDS] += CHSV(colour, 255, myfade / step * 2); // Simple wrap from Marc Miller
                leds[(center - step + NUM_LEDS) % NUM_LEDS] += CHSV(colour, 255, myfade / step * 2);
                step++; // Next step.
                break;
            }
            if (transitionTime == 0 or transitionTime == NULL)
            {
                transitionTime = 30;
            }
            showleds();
        }
    }

    EVERY_N_SECONDS(5)
    {
        targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128, 255)), CHSV(random8(), 255, random8(128, 255)), CHSV(random8(), 192, random8(128, 255)), CHSV(random8(), 255, random8(128, 255)));
    }

    //FLASH AND FADE SUPPORT
    if (flash)
    {
        if (startFlash)
        {
            startFlash = false;
            flashStartTime = millis();
        }

        if ((millis() - flashStartTime) <= flashLength)
        {
            if ((millis() - flashStartTime) % 1000 <= 500)
            {
                setColor(flashRed, flashGreen, flashBlue);
            }
            else
            {
                setColor(0, 0, 0);
                // If you'd prefer the flashing to happen "on top of"
                // the current color, uncomment the next line.
                // setColor(realRed, realGreen, realBlue);
            }
        }
        else
        {
            flash = false;
            effectString = oldeffectString;
            if (onbeforeflash)
            { //keeps light off after flash if light was originally off
                setColor(realRed, realGreen, realBlue);
            }
            else
            {
                stateOn = false;
                setColor(0, 0, 0);
                sendState();
            }
        }
    }

    if (startFade && effectString == "solid")
    {
        // If we don't want to fade, skip it.
        if (transitionTime == 0)
        {
            setColor(realRed, realGreen, realBlue);

            redVal = realRed;
            grnVal = realGreen;
            bluVal = realBlue;

            startFade = false;
        }
        else
        {
            loopCount = 0;
            stepR = calculateStep(redVal, realRed);
            stepG = calculateStep(grnVal, realGreen);
            stepB = calculateStep(bluVal, realBlue);

            inFade = true;
        }
    }

    if (inFade)
    {
        startFade = false;
        unsigned long now = millis();
        if (now - lastLoop > transitionTime)
        {
            if (loopCount <= 1020)
            {
                lastLoop = now;

                redVal = calculateVal(stepR, redVal, loopCount);
                grnVal = calculateVal(stepG, grnVal, loopCount);
                bluVal = calculateVal(stepB, bluVal, loopCount);

                if (effectString == "solid")
                {
                    setColor(redVal, grnVal, bluVal); // Write current values to LED pins
                }
                loopCount++;
            }
            else
            {
                inFade = false;
            }
        }
    }
}

/**************************** START TRANSITION FADER ****************************************/
// From https://www.arduino.cc/en/Tutorial/ColorCrossfader
// BELOW THIS LINE IS THE MATH – YOU SHOULDN’T NEED TO CHANGE THIS FOR THE BASICS
//            The program works like this : Imagine a crossfade that moves the red LED from 0 -
//    10,
//    the green from 0 - 5, and the blue from 10 to 7, in ten steps.We’d want to count the 10 steps and increase or decrease color values in evenly stepped increments.Imagine a + indicates raising a value by 1, and a - equals lowering it.Our 10 step fade would look like : 1 2 3 4 5 6 7 8 9 10 R + +++++++++G + ++++B - --The red rises from 0 to 10 in ten steps, the green from 0 - 5 in 5 steps, and the blue falls from 10 to 7 in three steps.In the real program, the color percentages are converted to 0 - 255 values, and there are 1020 steps(255 * 4).To figure out how big a step there should be between one up - or down - tick of one of the LED values, we call calculateStep(),
//    which calculates the absolute gap between the start and end values,
//    and then divides that gap by 1020 to determine the size of the step
//        between adjustments in the value.
//            * /
int calculateStep(int prevValue, int endValue)
{
    int step = endValue - prevValue; // What’s the overall gap?
    if (step)
    {                       // If its non-zero,
        step = 1020 / step; // divide by 1020
    }

    return step;
}
/* The next function is calculateVal. When the loop value, i,
reaches the step size appropriate for one of the
colors, it increases or decreases the value of that color by 1.
(R, G, and B are each calculated separately.)
*/
int calculateVal(int step, int val, int i)
{
    if ((step) && i % step == 0)
    { // If step is non-zero and its time to change a value,
        if (step > 0)
        { // increment the value if step is positive…
            val += 1;
        }
        else if (step < 0)
        { // …or decrement it if step is negative
            val -= 1;
        }
    }

    // Defensive driving: make sure val stays in the range 0-255
    if (val > 255)
    {
        val = 255;
    }
    else if (val < 0)
    {
        val = 0;
    }

    return val;
}

/**************************** START STRIPLED PALETTE *****************************************/
void setupStripedPalette(CRGB A, CRGB AB, CRGB B, CRGB BA)
{
    currentPalettestriped = CRGBPalette16(
        A, A, A, A, A, A, A, A, B, B, B, B, B, B, B, B
        // A, A, A, A, A, A, A, A, B, B, B, B, B, B, B, B
    );
}

/********************************** START FADE************************************************/
void fadeall()
{
    for (int i = 0; i < NUM_LEDS; i++)
    {
        leds[i].nscale8(250); //for CYCLon
    }
}


uint16_t XY (uint8_t x, uint8_t y) { return (y * NUM_COLS + x);}
void RocketTakeOff()
{
    #define scalenoise 80 
    int  a = millis() * 2;
    for (byte i = 0; i < NUM_COLS; i++) {
    for (byte j = 0; j < NUM_ROWS; j++) {
        uint8_t index = qsub8 (inoise8 (i * scalenoise , j * scalenoise+ a , a /3), abs8(j - (NUM_ROWS-1)) * 255 / (NUM_ROWS+4));
        leds[XY(i,j)] = ColorFromPalette (RocketColors_p , scale8(index, 220), brightness);  
    }}
}


void RocketFlight()
{
    #define scalenoise 80 
    int  a = millis();
    for (byte i = 0; i < NUM_COLS; i++) {
        for (byte j = 0; j < NUM_ROWS; j++) {
        leds[XY(i,j)] = ColorFromPalette (HeatColors_p , qsub8 (inoise8 (i * scalenoise , j * scalenoise+ a , a /3), 
        abs8(j - (NUM_ROWS-1)) * 255 / (NUM_ROWS+4)), brightness);	
    }}
}

/********************************** START FIRE **********************************************/
void Fire2012WithPalette()
{
    int num_leds = 12;
  static byte heat[4][12];
      for (int h = 0; h < 4; h++) {
    // Step 1.  Cool down every cell a little
      for( int i = 0; i < num_leds; i++) {
        heat[h][i] = qsub8( heat[h][i],  random8(0, ((COOLING * 10) / num_leds) + 2));
      }

      // Step 2.  Heat from each cell drifts 'up' and diffuses a little
      for( int k= num_leds - 1; k >= 2; k--) {
        heat[h][k] = (heat[h][k - 1] + heat[h][k - 2] + heat[h][k - 2] ) / 3;
      }

      // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
      if( random8() < SPARKING ) {
        heat[h][0] = qadd8( heat[h][0], random8(160,255) );
      }
    }

      // Step 4.  Map from heat cells to LED colors
    for( int j = 0; j < num_leds; j++) {
        leds[(j * 4)] = HeatColor( heat[0][j]);
        leds[(j * 4) + 1] = HeatColor( heat[1][j]);
        leds[(j * 4) + 2] = HeatColor( heat[2][j]);
        leds[(j * 4) + 3] = HeatColor( heat[3][j]);
    }

    for (int j = 0; j < 4; j++) {
      leds[4+j] = leds[random(4)];
    }

    for (int j = 0; j < 4; j++) {
      leds[12+j] = leds[8+random(4)];
    }
}
/********************************** START ADD GLITTER *********************************************/
void addGlitter(fract8 chanceOfGlitter)
{
    if (random8() < chanceOfGlitter)
    {
        leds[random16(NUM_LEDS)] += CRGB::White;
    }
}

/********************************** START ADD GLITTER COLOR ****************************************/
void addGlitterColor(fract8 chanceOfGlitter, int red, int green, int blue)
{
    if (random8() < chanceOfGlitter)
    {
        leds[random16(NUM_LEDS)] += CRGB(red, green, blue);
    }
}

/********************************** START SHOW LEDS ***********************************************/
void showleds()
{

    delay(1);

    if (stateOn)
    {
        FastLED.setBrightness(brightness); //EXECUTE EFFECT COLOR
        FastLED.show();
        if (transitionTime > 0 && transitionTime < 130)
        { //Sets animation speed based on receieved value
            FastLED.delay(1000 / transitionTime);
            //delay(10*transitionTime);
        }
    }
    else if (startFade)
    {
        setColor(0, 0, 0);
        startFade = false;
    }
}