#include <EtherCard.h>

#include "M2XNanodeClient.h"

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
// A larger buffer is needed, since one HTTP request here contains much more
// bytes than the previous 2 examples
byte Ethernet::buffer[700];

char feedId[] = "<Feed ID>"; // Feed you want to post to
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

int fill_stream_cb(Print* print, int streamIndex) {
  if (streamIndex == 1) {
    print->print("\"humidity\"");
    return 2;
  } else {
    print->print("\"temperature\"");
    return 3;
  }
}

static int seconds = 10;
void fill_timestamp_cb(Print* print, int valueIndex, int streamIndex) {
  print->print("\"2014-07-30T");
  print->print(19 + streamIndex);
  print->print(":");
  print->print(15 + valueIndex);
  print->print(":");
  print->print(seconds);
  print->print(".624Z\"");
}

static int val = 11;
void fill_data_cb(Print* print, int valueIndex, int streamIndex) {
  print->print(val * (valueIndex + 1) + streamIndex);
}

void loop() {
  ether.packetLoop(ether.packetReceive());

  if (millis() > timer) {
    IPAddress addr(m2xIpAddress);
    M2XNanodeClient m2xClient(m2xKey, &addr);

    Serial.println("Request!");
    int response = m2xClient.postMultiple(feedId, 2, fill_stream_cb, fill_timestamp_cb, fill_data_cb);
    Serial.print("Code: ");
    Serial.println(response);

    val++;
    seconds++;
    if (seconds >= 60) seconds = 10;
    timer = millis() + 5000;
  }
}
