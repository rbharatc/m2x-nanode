#include "M2XNanodeClient.h"

#include <jsonlite.h>
#include <EtherCard.h>

#include "StreamParseFunctions.h"
#include "LocationParseFunctions.h"

int print_encoded_string(Print* print, const char* str);
int tolower(int ch)
{
  // Arduino uses ASCII table, so we can simplify the implementation
  if ((ch >= 'A') && (ch <= 'Z')) {
    return (ch + 32);
  }
  return ch;
}

M2XNanodeClient::M2XNanodeClient(const char* key,
                                 IPAddress* addr,
                                 int timeout_seconds,
                                 int case_insensitive,
                                 int port) : _key(key),
                                             _addr(addr),
                                             _timeout_seconds(timeout_seconds),
                                             _case_insensitive(case_insensitive),
                                             _port(port) {
}

static M2XNanodeClient* s_client;
static const char* s_feed_id;
static const char* s_stream_name;
static put_data_fill_callback s_put_cb;
static int s_fd;
static int s_response_code;

static int s_number;
static post_data_fill_callback s_post_timestamp_cb;
static post_data_fill_callback s_post_data_cb;

static post_multiple_stream_fill_callback s_post_multiple_stream_cb;
static post_multiple_data_fill_callback s_post_multiple_timestamp_cb;
static post_multiple_data_fill_callback s_post_multiple_data_cb;

static int s_has_name;
static int s_has_elevation;
static update_location_data_fill_callback s_update_location_data_cb;

static uint16_t put_client_internal_datafill_cb(uint8_t fd) {
  BufferFiller bfill = EtherCard::tcpOffset();
  NullPrint null_print;

  if (fd == s_fd) {
    bfill.print(F("PUT /v1/feeds/"));
    print_encoded_string(&bfill, s_feed_id);
    bfill.print(F("/streams/"));
    print_encoded_string(&bfill, s_stream_name);
    bfill.println(F(" HTTP/1.0"));

    null_print.count = 0;
    s_put_cb(&null_print);
    // 10 for {"value": and }
    int content_length = null_print.count + 10;
    s_client->writeHttpHeader(&bfill, content_length);

    bfill.print(F("{\"value\":"));
    s_put_cb(&bfill);
    bfill.print(F("}"));
  }
  return bfill.position();
}

static void print_post_values(Print* print, int value_number,
                              post_data_fill_callback timestamp_cb,
                              post_data_fill_callback data_cb) {
  int i;
  print->print("{\"values\":[");
  for (i = 0; i < value_number; i++) {
    print->print("{\"at\":");
    timestamp_cb(print, i);
    print->print(",\"value\":");
    data_cb(print, i);
    print->print("}");
    if (i != value_number - 1) {
      print->print(",");
    }
  }
  print->print("]}");
}

static void print_post_multiple_values(Print* print, int stream_number,
                                       post_multiple_stream_fill_callback stream_cb,
                                       post_multiple_data_fill_callback timestamp_cb,
                                       post_multiple_data_fill_callback data_cb) {
  int si, vi, value_number;
  print->print("{\"values\":{");
  for (si = 0; si < value_number; si++) {
    value_number = stream_cb(print, si);
    print->print(":[");
    for (vi = 0; vi < value_number; vi++) {
      print->print("{\"at\":");
      timestamp_cb(print, vi, si);
      print->print(",\"value\":");
      data_cb(print, vi, si);
      print->print("}");
      if (vi != value_number - 1) { print->print(","); }
    }
    print->print("]");
    if (si != value_number - 1) { print->print(","); }
  }
  print->print("}}");
}

static void print_location(Print* print, int has_name, int has_elevation,
                           update_location_data_fill_callback cb) {
  print->print(F("{"));
  if (has_name) {
    print->print(F("\"name\":"));
    cb(print, kLocationFieldName);
    print->print(F(","));
  }
  if (has_elevation) {
    print->print(F("\"elevation\":"));
    cb(print, kLocationFieldElevation);
    print->print(F(","));
  }
  print->print(F("\"latitude\":"));
  cb(print, kLocationFieldLatitude);
  print->print(F(",\"longitude\":"));
  cb(print, kLocationFieldLongitude);
  print->print(F("}"));
}

static uint16_t post_client_internal_datafill_cb(uint8_t fd) {
  BufferFiller bfill = EtherCard::tcpOffset();
  NullPrint null_print;

  if (fd == s_fd) {
    bfill.print(F("POST /v1/feeds/"));
    print_encoded_string(&bfill, s_feed_id);
    bfill.print(F("/streams/"));
    print_encoded_string(&bfill, s_stream_name);
    bfill.println(F("/values HTTP/1.0"));

    null_print.count = 0;
    print_post_values(&null_print, s_number, s_post_timestamp_cb, s_post_data_cb);
    s_client->writeHttpHeader(&bfill, null_print.count);

    print_post_values(&bfill, s_number, s_post_timestamp_cb, s_post_data_cb);
  }
  return bfill.position();
}

static uint16_t post_multiple_client_internal_datafill_cb(uint8_t fd) {
  BufferFiller bfill = EtherCard::tcpOffset();
  NullPrint null_print;

  if (fd == s_fd) {
    bfill.print(F("POST /v1/feeds/"));
    print_encoded_string(&bfill, s_feed_id);
    bfill.println(F(" HTTP/1.0"));

    null_print.count = 0;
    print_post_multiple_values(&null_print, s_number, s_post_multiple_stream_cb,
                               s_post_multiple_timestamp_cb,
                               s_post_multiple_data_cb);
    s_client->writeHttpHeader(&bfill, null_print.count);

    print_post_multiple_values(&bfill, s_number, s_post_multiple_stream_cb,
                               s_post_multiple_timestamp_cb,
                               s_post_multiple_data_cb);
  }
  return bfill.position();
}

static uint16_t update_location_internal_datafill_cb(uint8_t fd) {
  BufferFiller bfill = EtherCard::tcpOffset();
  NullPrint null_print;

  if (fd == s_fd) {
    bfill.print(F("PUT /v1/feeds/"));
    print_encoded_string(&bfill, s_feed_id);
    bfill.println(F("/location HTTP/1.0"));

    null_print.count = 0;
    print_location(&null_print, s_has_name, s_has_elevation, s_update_location_data_cb);
    s_client->writeHttpHeader(&bfill, null_print.count);

    print_location(&bfill, s_has_name, s_has_elevation, s_update_location_data_cb);
  }
  return bfill.position();
}

static uint8_t client_internal_fetch_response_code_cb(uint8_t fd, uint8_t statuscode, uint16_t datapos, uint16_t len_of_data) {
  if (fd == s_fd) {
    if (statuscode == 0) {
      s_response_code = s_client->readStatusCode((char*) ether.buffer + datapos, len_of_data);
    } else {
      s_response_code = statuscode;
    }
  }
}

int M2XNanodeClient::put(const char* feed_id, const char* stream_name,
                         put_data_fill_callback cb) {
  int i;
  ether.packetLoop(ether.packetReceive());
  for (i = 0; i < 4; i++) {
    ether.hisip[i] = (*_addr)[i];
  }
  s_client = this;
  s_feed_id = feed_id;
  s_stream_name = stream_name;
  s_put_cb = cb;
  s_response_code = 0;
  s_fd = ether.clientTcpReq(client_internal_fetch_response_code_cb,
                            put_client_internal_datafill_cb,
                            _port);
  return loop();
}

int M2XNanodeClient::post(const char* feed_id, const char* stream_name, int value_number,
                          post_data_fill_callback timestamp_cb,
                          post_data_fill_callback data_cb) {
  int i;
  ether.packetLoop(ether.packetReceive());
  for (i = 0; i < 4; i++) {
    ether.hisip[i] = (*_addr)[i];
  }
  s_client = this;
  s_feed_id = feed_id;
  s_stream_name = stream_name;
  s_number = value_number;
  s_post_timestamp_cb = timestamp_cb;
  s_post_data_cb = data_cb;
  s_response_code = 0;
  s_fd = ether.clientTcpReq(client_internal_fetch_response_code_cb,
                            post_client_internal_datafill_cb,
                            _port);
  return loop();
}

int M2XNanodeClient::postMultiple(const char* feed_id, int stream_number,
                                  post_multiple_stream_fill_callback stream_cb,
                                  post_multiple_data_fill_callback timestamp_cb,
                                  post_multiple_data_fill_callback data_cb) {
  int i;
  ether.packetLoop(ether.packetReceive());
  for (i = 0; i < 4; i++) {
    ether.hisip[i] = (*_addr)[i];
  }
  s_client = this;
  s_feed_id = feed_id;
  s_number = stream_number;
  s_post_multiple_stream_cb = stream_cb;
  s_post_multiple_timestamp_cb = timestamp_cb;
  s_post_multiple_data_cb = data_cb;
  s_response_code = 0;
  s_fd = ether.clientTcpReq(client_internal_fetch_response_code_cb,
                            post_multiple_client_internal_datafill_cb,
                            _port);
  return loop();
}

int M2XNanodeClient::updateLocation(const char* feed_id, int has_name, int has_elevation,
                                    update_location_data_fill_callback cb) {
  int i;
  ether.packetLoop(ether.packetReceive());
  for (i = 0; i < 4; i++) {
    ether.hisip[i] = (*_addr)[i];
  }
  s_client = this;
  s_feed_id = feed_id;
  s_has_name = has_name;
  s_has_elevation = has_elevation;
  s_update_location_data_cb = cb;
  s_response_code = 0;
  s_fd = ether.clientTcpReq(client_internal_fetch_response_code_cb,
                            update_location_internal_datafill_cb,
                            _port);
  return loop();
}

// Encodes and prints string using Percent-encoding specified
// in RFC 1738, Section 2.2
int print_encoded_string(Print* print, const char* str) {
  int bytes = 0;
  for (int i = 0; str[i] != 0; i++) {
    if (((str[i] >= 'A') && (str[i] <= 'Z')) ||
        ((str[i] >= 'a') && (str[i] <= 'z')) ||
        ((str[i] >= '0') && (str[i] <= '9')) ||
        (str[i] == '-') || (str[i] == '_') ||
        (str[i] == '.') || (str[i] == '~')) {
      bytes += print->print(str[i]);
    } else {
      // Encode all other characters
      bytes += print->print('%');
      bytes += print->print(HEX(str[i] / 16));
      bytes += print->print(HEX(str[i] % 16));
    }
  }
  return bytes;
}

void M2XNanodeClient::writeHttpHeader(Print* print, int content_length) {
  print->println(USER_AGENT);
  print->print(F("X-M2X-KEY: "));
  print->println(_key);
  if (content_length > 0) {
    print->println(F("Content-Type: application/json"));
    print->print(F("Content-Length: "));
    print->println(content_length);
  }
  print->println();
}

int M2XNanodeClient::waitForString(const char* origin, int len, const char* str) {
  int i, j, k, cmp;
  if (str[0] == '\0') { return 0; }
  for (i = 0; i < len; i++) {
    for (j = i, k = 0; (j < len) && (str[k] != '\0'); j++, k++) {
      if (_case_insensitive) {
        cmp = tolower(origin[j]) - tolower(str[k]);
      } else {
        cmp = origin[j] - str[k];
      }
      if ((str[k] != '*') && (cmp != 0)) {
        // No match
        break;
      }
    }
    if (str[k] == '\0') {
      // Full match
      return j;
    }
  }
  return E_NOMATCH;
}

int M2XNanodeClient::readStatusCode(const char* origin, int len) {
  int responseCode = 0, i = 0;
  int ret = waitForString(origin, len, "HTTP/*.* ");
  if (ret < 0) {
    return ret;
  }

  // ret is the index to start in string origin
  for (i = 0; i < 3; i++) {
    if ((ret + i) >= len) {
      // Not enough string
      return E_DISCONNECTED;
    }
    responseCode = responseCode * 10 + (origin[ret + i] - '0');
  }
  return responseCode;
}

int M2XNanodeClient::loop() {
  int i;
  for (i = 0; i < _timeout_seconds * 10; i++) {
    ether.packetLoop(ether.packetReceive());
    if (s_response_code != 0) {
      // Request already processed
      return s_response_code;
    }
    // Here we just use an approximation, since millis() has
    // overflow problem
    delay(100);
  }
  return E_TIMEOUT;
}
