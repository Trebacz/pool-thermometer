// This #include statement was automatically added by the Particle IDE.
#include <MQTT.h>
#include <PowerShield.h>

//**************************************************************************************
// Author: Gustavo Gonnet
// Contact: gusgonnet@gmail.com
// Project: https://www.hackster.io/gusgonnet/pool-temperature-monitor-5331f2
// License: Apache-2.0
//**************************************************************************************

// IO mapping
// A0 : pool_THERMISTOR

#include <math.h> // Adds log function
//#include "application.h" // Added Particle.h below 2023-03-06 instead
#include "Particle.h" // Paticle libray was application.h
#include "MQTT_Secrets.h" // File to store MQTT SECRET_MQTT_IP SECRET_MQTT_USER and SECRET_MQTT_PASS
// #include "PowerShield/PowerShield.h" // Include the Powershield library Commented out since it's above - 2023-03-06
PowerShield batteryMonitor;

// this is the thermistor used - https://www.adafruit.com/products/372

#define THERMISTORNOMINAL 10000 // resistance at 25 degrees C
#define TEMPERATURENOMINAL 25 // temp. for nominal resistance (almost always 25 C)
#define NUMSAMPLES 30 // how many samples to take and average, more takes longer but measurement is 'smoother'
#define BCOEFFICIENT 3950 // The beta coefficient of the thermistor (usually 3000-4000)
#define SERIESRESISTOR 10000 // the value of the 'other' resistor 

double _version = 1.79;
int samples[NUMSAMPLES];
int pool_THERMISTOR = A0;
char pool_temperature_str[64]; //String to store the sensor data

bool useFahrenheit = true; //by default, we'll display the temperature in degrees celsius, but if you prefer farenheit please set this to true
int rssi = 0; //For wifi signal strength
char ps_voltage[64]; //String to store voltage
char ps_soc[64]; //String to store state of charge
char mqtt_user[] = SECRET_MQTT_USER; // Replace with USER in MQTT_Secrets.h
char mqtt_password[] = SECRET_MQTT_PASS; //Replace with PASS in MQTT_Secrets.h
char mqtt_ip[] = SECRET_MQTT_IP;

// This is the maximum amount of time to wait for the cloud to be connected in
// milliseconds. This should be at least 5 minutes. If you set this limit shorter,
// on Gen 2 devices the modem may not get power cycled which may help with reconnection.
//const std::chrono::milliseconds connectMaxTime = 6min;

// This is the minimum amount of time to stay connected to the cloud. You can set this
// to zero and the device will sleep as fast as possible, however you may not get 
// firmware updates and device diagnostics won't go out all of the time. Setting this
// to 10 seconds is typically a good value to use for getting updates.
//const std::chrono::milliseconds cloudMinTime = 10s;

// How long to sleep
//const std::chrono::milliseconds sleepTime = 15min;

// Maximum time to wait for publish to complete. It normally takes 20 seconds for Particle.publish
// to succeed or time out, but if cellular needs to reconnect, it could take longer, typically
// 80 seconds. This timeout should be longer than that and is just a safety net in case something
// goes wrong.
//const std::chrono::milliseconds publishMaxTime = 3min;


// MQTT server
void callback(char* topic, byte* payload, unsigned int length);
MQTT client(mqtt_ip, 1883, callback);

// recieve message
void callback(char* topic, byte* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;

    if (!strcmp(p, "RED"))
        RGB.color(255, 0, 0);
    else if (!strcmp(p, "GREEN"))
        RGB.color(0, 255, 0);
    else if (!strcmp(p, "BLUE"))
        RGB.color(0, 0, 255);
    else
        RGB.color(255, 255, 255);
    delay(1000);  // Wait for it to settle down - delay 1.0 second MQTT
}

void setup() {

//    Particle.publish("device starting", "Version: " + String::format("%.2lf", _version), 60, PRIVATE); // Publish Code version to cloud formated to 2 decimals
    STARTUP(WiFi.selectAntenna(ANT_EXTERNAL)); // selects the u.FL antenna
    STARTUP(Time.setTime(0)); // recomended - https://community.particle.io/t/sleep-wont-wake-up/14871/4

    pinMode(pool_THERMISTOR, INPUT);
    Particle.variable("pool_temp", pool_temperature_str);
    Particle.variable("RSSI", rssi);
    Particle.variable("PS-VOLTAGE", ps_voltage);
    Particle.variable("PS-SOC", ps_soc);
    //Include the setup for power shield
    // This essentially starts the I2C bus
    batteryMonitor.begin(); // This sets up the fuel gauge
    batteryMonitor.quickStart();
    RGB.control(true); // MQTT colors of LED
    client.connect("particle-client" + String(Time.now()), mqtt_user, mqtt_password); // MQTT client connect to the server(unique id by Time.now())
    if (client.isConnected()) {
    client.publish("pool-thermometer/message","Pool MQTT Connected " + String(Time.now()));
    }
}

void loop() {
    pool_temp(); //Measure the pool temperature

    rssi = WiFi.RSSI();  // Read wifi signal strength
    Particle.publish("rssi", String(rssi), 60, PRIVATE); //Publish wifi signal to the cloud
    
    float cellVoltage = batteryMonitor.getVCell(); // Read the volatge of the LiPo Battery
    Particle.publish("ps-voltage", String::format("%.2lf", cellVoltage), 60, PRIVATE); // Publish Cell Voltage to cloud formated to 2 decimals
    
    float stateOfCharge = batteryMonitor.getSoC(); // Read the State of Charge of the LiPo
    Particle.publish("ps-soc", String::format("%.2lf", stateOfCharge), 60, PRIVATE); // Publish State of Charge to Cloud formated to 2 decimals

    // MQTT publish/subscribe
    if (client.isConnected()) {
        client.publish("pool-thermometer/pool_temp",pool_temperature_str); //Publish temperature signal to the MQTT
        client.publish("pool-thermometer/rssi", String(rssi)); //Publish wifi signal to the MQTT
        client.publish("pool-thermometer/ps-voltage", String::format("%.2lf", cellVoltage)); // Publish Cell Voltage to MQTT formated to 2 decimals
        client.publish("pool-thermometer/ps-soc", String::format("%.2lf", stateOfCharge)); // Publish State of Charge to MQTT formated to 2 decimals
        client.publish("pool-thermometer/version", String::format("%.2lf", _version)); // Publish Version to MQTT formated to 2 decimals
        client.publish("pool-thermometer/message","Pool Data Finished " + String(Time.now()));
    }

    Particle.publish("waiting for updates", "Version: " + String::format("%.2lf", _version), 60, PRIVATE); // Publish Code version to cloud formated to 2 decimals
    if (client.isConnected()) {
        client.publish("pool-thermometer/message","Pool client started waiting for updates " + String(Time.now()));
    }
    delay(60000);  // wait for 60 seconds before sleeping
    Particle.publish("device going to sleep", "Version: " + String::format("%.2lf", _version), 60, PRIVATE); // Publish Code version to cloud formated to 2 decimals
//    System.sleep(SLEEP_MODE_DEEP, 800); // Sleep for roughly 15 minutes (was 16 minutes at 900)
    SystemSleepConfiguration config; // New sleep mode as of 2023-03-05
    config.mode(SystemSleepMode::HIBERNATE)
      .duration(837s);  // Sleep for roughly 15 minutes
//      .duration(100s);  // Sleep for roughly 1 minute fpr testing
    SystemSleepResult result = System.sleep(config);
}

/*******************************************************************************
 * Function Name  : pool_temp
 * Description    : read the value of the thermistor, convert it to degrees and store it in pool_temperature_str
 * Return         : 0
 *******************************************************************************/
int pool_temp()
{
    uint8_t i;
    float average;

    // take N samples in a row, with a slight delay
    for (i=0; i< NUMSAMPLES; i++) {
        samples[i] = analogRead(pool_THERMISTOR);
        delay(10);
    }

    // average all the samples out
    average = 0;
    for (i=0; i< NUMSAMPLES; i++) {
        average += samples[i];
    }
    average /= NUMSAMPLES;

    // convert the value to resistance
    average = (4095 / average)  - 1;
    average = SERIESRESISTOR / average;


    float steinhart;
    steinhart = average / THERMISTORNOMINAL;     // (R/Ro)
    steinhart = log(steinhart);                  // ln(R/Ro)
    steinhart /= BCOEFFICIENT;                   // 1/B * ln(R/Ro)
    steinhart += 1.0 / (TEMPERATURENOMINAL + 273.15); // + (1/To)
    steinhart = 1.0 / steinhart;                 // Invert
    steinhart -= 273.15;                         // convert to C
    
    // Convert Celsius to Fahrenheit - EXPERIMENTAL, so let me know if it works please - Gustavo.
    // source: http://playground.arduino.cc/ComponentLib/Thermistor2#TheSimpleCode
    if (useFahrenheit) {
    steinhart = (steinhart * 9.0)/ 5.0 + 32.0;
    }

    char ascii[32];
    int steinhart1 = (steinhart - (int)steinhart) * 100;

    // for negative temperatures
    steinhart1 = abs(steinhart1);

    sprintf(ascii,"%0d.%d", (int)steinhart, steinhart1);
    Particle.publish("pool_temp", ascii, 60, PRIVATE);

    char tempInChar[32];
    sprintf(tempInChar,"%0d.%d", (int)steinhart, steinhart1);

    sprintf(pool_temperature_str, "%s", tempInChar); //Write temperature to string

    return 0;
}