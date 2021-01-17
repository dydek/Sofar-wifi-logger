#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <TaskScheduler.h>
#include <ezTime.h>

#include "utils.h"
#include "InverterMessage.h"

/**
 * #example response from sofar
 * A5:61:0:10:15:0:44:53:31:6A:66:2:1:59:44:2F:0:3B:C:0:0:D5:98:9D:5F:1:3:4E:0:2:0:0:0:0:0:0:0:0:0:0:B:F6:0:E:2:C9:0:0:0:4:0:0:0:2:0:44:13:89:9:48:0:68:9:30:0:5E:9:4C:0:66:0:0:1:67:0:0:3:34:0:3C:1:95:0:1A:0:2B:17:A5:B:F2:2:C7:0:3C:0:0:0:1:0:0:5:82:6:B9:5:61:5A:B1:E8:15:
 **/

#include "Config.h"

#define WIFI_HOSTNAME "sofar-logger"
#define MAX_SOFAR_TRIES 150

#define PV_SERV F("dane.pvmonitor.pl")

Scheduler runner;
Timezone pvTZ;

#if MEASURE_CONSUMPTION

uint32 impulses;

ICACHE_RAM_ATTR void measureConsumption()
{
  // for many devices we need to filter the noise, usually it's around 50 to 100ms,
  // which gives us max around 45kwh per hour max accurate counting
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // 150 noise filtering
  if (interrupt_time - last_interrupt_time > 150)
  {
    impulses++;
    // probably won't happen - even when using smaller variable (with 4000kw per month should be ok for around ~89 years)
    if (impulses > UINT32_MAX)
      impulses = 0;
  }
  last_interrupt_time = interrupt_time;
}

float getConsumption()
{
  return impulses / float(MEASURE_CONSUMPTION_IMPULSES_PER_KWH);
}

#endif

//tasks
void fetchDataFromSofarAndSend();
void uploadDataToPVMonitor();

Task t1(15000, TASK_FOREVER, &fetchDataFromSofarAndSend, &runner, true);
Task t2(30 * 1000, TASK_FOREVER, &uploadDataToPVMonitor, &runner, true);

WiFiClient client;
WiFiClient pvClient;

struct SofarData
{
  float totalEnergy;
  float todayEnergy;
  float PVVoltage1;
  float PVVoltage2;
  float PVCurrent1;
  float PVCurrent2;
  float PVPower1;
  float PVPower2;
  float ModuleTemp;
  float ACVoltage1;
  float ACCurrent1;
  float ACVoltage2;
  float ACCurrent2;
  float ACVoltage3;
  float ACCurrent3;
} sofarData;

void setup()
{
  Serial.begin(115200);

  WiFi.hostname(WIFI_HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print("*");
  }

  waitForSync();

  // set location for the PL timezone 
  pvTZ.setLocation(F("Europe/Warsaw"));

  Serial.println(pvTZ.dateTime("Y-m-d\\TH:i:s"));

  Serial.println();
  Serial.println("WiFi connection Successful");
  Serial.print("The IP Address of ESP8266 Module is: ");
  Serial.println(WiFi.localIP()); // Print the IP address

  if (!MDNS.begin(MDNS_NAME))
  { // Start the mDNS responder for *.local
    Serial.println("Error setting up MDNS responder!");
  }

#if MEASURE_CONSUMPTION
  pinMode(MEASURE_CONSUMPTION_PIN, INPUT_PULLUP);
  attachInterrupt(
      digitalPinToInterrupt(MEASURE_CONSUMPTION_PIN),
      measureConsumption,
      FALLING);
#endif
  runner.startNow();
}

void loop()
{
  runner.execute();
}

void fetchDataFromSofarAndSend()
{
  uint8 tries = 0;
  Serial.print(F("Fetching data from sofar...."));
  byte buffor[220];
#ifdef TESTING
  InverterMessage inverterMessage(exampleResponseBytes, 220);
  Serial.print("Total energy: ");
  Serial.println(inverterMessage.getTotalEnergy());
  Serial.print("Today energy: ");
  Serial.println(inverterMessage.getTodayEnergy());
  Serial.print("PV1 Voltage: ");
  Serial.println(inverterMessage.getPVVoltage(1));
  Serial.print("PV2 Voltage: ");
  Serial.println(inverterMessage.getPVVoltage(2));
  Serial.print("PV1 Current: ");
  Serial.println(inverterMessage.getPVCurrent(1));
  Serial.print("PV2 Current: ");
  Serial.println(inverterMessage.getPVCurrent(2));
  Serial.print("PV1 Power: ");
  Serial.println(inverterMessage.getPVPower(1));
  Serial.print("PV2 Power: ");
  Serial.println(inverterMessage.getPVPower(2));
  Serial.print("Module temp: ");

  Serial.println(inverterMessage.getModuleTemp());
#else
  if (client.connect(SOFAR_IP, 8899))
  {
    client.write(SOFAR_REQUEST, 36);
    // the correct message from the inverter is 110 long
    while (client.available() < 110 && tries < MAX_SOFAR_TRIES)
    {
      delay(10);
      tries++;
    }
    if (tries == MAX_SOFAR_TRIES)
    {
      Serial.println(F("Something went wrong, will try next time"));
      // we need to quit, not possible to proceed frther
      return;
    }
    else
    {
      // TODO change it
      Serial.printf(" (took %d ms)\n", tries * 10);
    }

    delay(2);
    client.readBytes(buffor, client.available());
    InverterMessage inverterMessage(buffor, 220);

    sofarData.totalEnergy = inverterMessage.getTotalEnergy();
    sofarData.todayEnergy = inverterMessage.getTodayEnergy();
    sofarData.PVVoltage1 = inverterMessage.getPVVoltage(1);
    sofarData.PVVoltage2 = inverterMessage.getPVVoltage(2);
    sofarData.PVCurrent1 = inverterMessage.getPVCurrent(1);
    sofarData.PVCurrent2 = inverterMessage.getPVCurrent(2);
    sofarData.PVPower1 = inverterMessage.getPVPower(1);
    sofarData.PVPower2 = inverterMessage.getPVPower(2);
    sofarData.ModuleTemp = inverterMessage.getModuleTemp();

    // AC Voltages
    sofarData.ACVoltage1 = inverterMessage.getACVoltage(1);
    sofarData.ACCurrent1 = inverterMessage.getACCurrent(1);

    sofarData.ACVoltage2 = inverterMessage.getACVoltage(2);
    sofarData.ACCurrent2 = inverterMessage.getACCurrent(2);

    sofarData.ACVoltage3 = inverterMessage.getACVoltage(3);
    sofarData.ACCurrent3 = inverterMessage.getACCurrent(3);

    client.flush();
    client.stop();
  }
  else
  {
    Serial.println(F("Can't connect to Sofar"));
  }
#endif
}

void uploadDataToPVMonitor()
{
  if (pvClient.connect(PV_SERV, 80))
  {
    Serial.print(F("Sending data...   "));

    pvClient.print(F("GET http://"));
    pvClient.print(PV_SERV);
    pvClient.print(F("/pv/get2.php?idl="));
    pvClient.print(PV_ID);
    pvClient.print(F("&p="));
    pvClient.print(PV_PASS);
    pvClient.print(F("&tm="));
    pvClient.print(pvTZ.dateTime("Y-m-d\\TH:i:s"));
    pvClient.print(F("&f1="));
    pvClient.print(sofarData.todayEnergy);
    pvClient.print(F("&f2="));
    pvClient.print(sofarData.PVVoltage1);
    pvClient.print(F("&f3="));
    pvClient.print(sofarData.PVCurrent1);
    pvClient.print(F("&f5="));
    pvClient.print(sofarData.PVVoltage2);
    pvClient.print(F("&f6="));
    pvClient.print(sofarData.PVCurrent2);
    pvClient.print(F("&f34="));
    pvClient.print(sofarData.ModuleTemp);
    // phase 1
    pvClient.print(F("&f320="));
    pvClient.print(sofarData.ACVoltage1);
    pvClient.print(F("&f420="));
    pvClient.print(sofarData.ACCurrent1);
    // phase 2
    pvClient.print(F("&f322="));
    pvClient.print(sofarData.ACVoltage2);
    pvClient.print(F("&f422="));
    pvClient.print(sofarData.ACCurrent2);
    // phase 3
    pvClient.print(F("&f324="));
    pvClient.print(sofarData.ACVoltage3);
    pvClient.print(F("&f424="));
    pvClient.print(sofarData.ACCurrent3);
#if MEASURE_CONSUMPTION
    pvClient.printf("&f4=%.4f", getConsumption());
#endif
    pvClient.print(F(" HTTP/1.0\r\n\r\n"));
    Serial.println(F("Data has been sent.."));

    String response = pvClient.readStringUntil('\r');
    Serial.println(response);
    if (response.startsWith(F("HTTP/1.1 200 OK")))
    {
      Serial.print(F("OK   "));
    }
    else
    {
      Serial.println(F("Error..."));
    }
    client.flush();
    client.stop();
  }
}

// void updateNTPClient()
// {
//   timeClient.update();
// }