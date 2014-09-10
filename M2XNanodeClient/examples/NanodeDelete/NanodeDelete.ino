#include <EtherCard.h>

#include "M2XNanodeClient.h"

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
byte Ethernet::buffer[400];

char feedId[] = "<Feed ID>"; // Feed you want to delete values
char streamName[] = "<Stream Name>"; // Stream you want to delete values
char m2xKey[] = "<M2X Key>"; // Your M2X access key
const char website[] PROGMEM = "api-m2x.att.com";

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
}

void fill_timestamp_cb(Print* print, int type) {
  if (type == 1) {
    print->print("\"2014-07-01T00:00:00.000Z\"");
  } else {
    print->print("\"2014-07-30T00:00:00.000Z\"");
  }
}

static bool finished = false;

void loop() {
  ether.packetLoop(ether.packetReceive());

  if (!finished) {
    IPAddress addr(m2xIpAddress);
    M2XNanodeClient m2xClient(m2xKey, &addr);

    Serial.println("Request!");
    int response = m2xClient.deleteValues(feedId, streamName, fill_timestamp_cb);
    Serial.print("Code: ");
    Serial.println(response);

    finished = true;
  }
}
