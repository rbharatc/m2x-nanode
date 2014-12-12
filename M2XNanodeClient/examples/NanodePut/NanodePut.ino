#include <EtherCard.h>

#include "M2XNanodeClient.h"

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte Ethernet::buffer[400];

char deviceId[] = "<Device ID>"; // Device you want to post to
char streamName[] = "<Stream Name>"; // Stream you want to post to
char m2xKey[] = "<M2X Key>"; // Your M2X access key
const char website[] PROGMEM = "api-m2x.att.com";

static unsigned long timer;

byte m2xIpAddress[4];
void setup() {
  Serial.begin(9600);

  if ((!ether.begin(sizeof Ethernet::buffer, mac)) ||
      (!ether.dhcpSetup())) {
    Serial.println("Network error!");
  }

  ether.printIp(F("IP:\t"), ether.myip);
  if (ether.dnsLookup(website)) {
    ether.printIp(F("SRV:\t"), ether.hisip);
    ether.copyIp(m2xIpAddress, ether.hisip);
  }
  Serial.println();

  timer = millis();
}

static int val = 11;
void fill_data_cb(Print* print) {
  print->print(val);
}

void loop() {
  ether.packetLoop(ether.packetReceive());

  if (millis() > timer) {
    IPAddress addr(m2xIpAddress);
    M2XNanodeClient m2xClient(m2xKey, &addr);

    Serial.println("Request!");
    int response = m2xClient.updateStreamValue(deviceId, streamName, fill_data_cb);
    Serial.print("Code: ");
    Serial.println(response);

    val++;
    timer = millis() + 5000;
  }
}
