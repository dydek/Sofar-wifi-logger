#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <TaskScheduler.h>

#include "utils.h"
#include "InverterMessage.h"

/**
 * #example response from sofar
 * A5:61:0:10:15:0:44:53:31:6A:66:2:1:59:44:2F:0:3B:C:0:0:D5:98:9D:5F:1:3:4E:0:2:0:0:0:0:0:0:0:0:0:0:B:F6:0:E:2:C9:0:0:0:4:0:0:0:2:0:44:13:89:9:48:0:68:9:30:0:5E:9:4C:0:66:0:0:1:67:0:0:3:34:0:3C:1:95:0:1A:0:2B:17:A5:B:F2:2:C7:0:3C:0:0:0:1:0:0:5:82:6:B9:5:61:5A:B1:E8:15:
 **/

#include "Config.h"

#define WIFI_HOSTNAME "sofar-logger"

#define PV_SERV "dane.pvmonitor.pl"
// #define TESTING

Scheduler runner;

//tasks
void fetchDataFromSofarAndSend();
void uploadDataToPVMonitor();

Task t1(15000, TASK_FOREVER, &fetchDataFromSofarAndSend, &runner, true);
Task t2(60 * 3 * 1000, TASK_FOREVER, &uploadDataToPVMonitor, &runner, true);

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

  Serial.println();
  Serial.println("WiFi connection Successful");
  Serial.print("The IP Address of ESP8266 Module is: ");
  Serial.println(WiFi.localIP()); // Print the IP address

  runner.startNow();
}

void loop()
{
  runner.execute();
}

void fetchDataFromSofarAndSend()
{
  uint8 max_tries = 15;
  Serial.println(F("Fetching data from sofar...."));
  byte buffor[220];
#ifdef TESTING
  // Serial.write(crapToSend, 36);
  // Serial.println();
  // // Serial.println(crapToSend, 36);
  // Serial.write("\xa5\x17\x00\x10\x45\x00\x00\x53\x31\x6a\x66\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x03\x00\x00\x00\x27\x05\xd0\xc2\x15", 36);
  // Serial.println();
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
    while (client.available() < 110 && max_tries > 0)
    {
      delay(100);
      Serial.print(".");
      max_tries--;
    }
    if (max_tries == 0)
    {
      Serial.println("Something wen wrong");
      // we need to quit, not possible to proceed frther
      return;
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

    client.flush();
    client.stop();
  }
  else
  {
    Serial.println("Can't connect to Sofar");
  }
#endif
}

void uploadDataToPVMonitor()
{
  if (pvClient.connect(PV_SERV, 80))
  {
    Serial.print(F("Sending data...   "));
    pvClient.print("GET http://");
    pvClient.print(PV_SERV);
    pvClient.print("/pv/get2.php?idl=");
    pvClient.print(PV_ID);
    pvClient.print("&p=");
    pvClient.print(PV_PASS);
    pvClient.print("&f1=");
    pvClient.print(sofarData.todayEnergy);
    pvClient.print("&f2=");
    pvClient.print(sofarData.PVVoltage1);
    pvClient.print("&f3=");
    pvClient.print(sofarData.PVCurrent1);
    pvClient.print("&f5=");
    pvClient.print(sofarData.PVVoltage2);
    pvClient.print("&f6=");
    pvClient.print(sofarData.PVCurrent2);
    pvClient.print("&f34=");
    pvClient.print(sofarData.ModuleTemp);
    pvClient.print(" HTTP/1.0\r\n\r\n");
    Serial.println(F("Data has been sent.."));

    String response = pvClient.readStringUntil('\r');
    Serial.println(response);
    if(response.startsWith("HTTP/1.1 200 OK")) {
      Serial.print(F("OK   "));
    } else {
      Serial.println(F("Error..."));
    }
    client.flush();
    client.stop();
  }
}