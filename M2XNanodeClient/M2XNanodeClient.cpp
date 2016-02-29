#include "M2XNanodeClient.h"

#include <EtherCard.h>

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
static const char* s_device_id;
static const char* s_stream_name;
static const char* s_command_id;
static const char* s_command_action;
static int s_timestamp_type;
static put_data_fill_callback s_put_cb;
static int s_fd;
static int s_response_code;
static char* s_response_buffer;
static int* s_response_buffer_length;

static int s_number;
static post_data_fill_callback s_post_timestamp_cb;
static post_data_fill_callback s_post_data_cb;

static post_multiple_stream_fill_callback s_post_multiple_stream_cb;
static post_multiple_data_fill_callback s_post_multiple_timestamp_cb;
static post_multiple_data_fill_callback s_post_multiple_data_cb;

static int s_has_name;
static int s_has_elevation;
static update_location_data_fill_callback s_update_location_data_cb;

static delete_values_timestamp_fill_callback s_delete_cb;

static uint16_t put_client_internal_datafill_cb(uint8_t fd) {
  BufferFiller bfill = EtherCard::tcpOffset();
  NullPrint null_print;

  if (fd == s_fd) {
    bfill.print(F("PUT /v2/devices/"));
    print_encoded_string(&bfill, s_device_id);
    bfill.print(F("/streams/"));
    print_encoded_string(&bfill, s_stream_name);
    bfill.println(F("/value HTTP/1.0"));

    null_print.count = 0;
    s_put_cb(&null_print);
    // 10 for {"value": and }
    int content_length = null_print.count + 12;
    s_client->writeHttpHeader(&bfill, content_length);

    bfill.print(F("{\"value\":\""));
    s_put_cb(&bfill);
    bfill.print(F("\"}"));
  }
  return bfill.position();
}

static void print_post_values(Print* print, int value_number,
                              post_data_fill_callback timestamp_cb,
                              post_data_fill_callback data_cb) {
  int i;
  print->print("{\"values\":[");
  for (i = 0; i < value_number; i++) {
    print->print("{\"timestamp\":");
    timestamp_cb(print, i);
    print->print(",\"value\":\"");
    data_cb(print, i);
    print->print("\"}");
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
  for (si = 0; si < stream_number; si++) {
    value_number = stream_cb(print, si);
    print->print(":[");
    for (vi = 0; vi < value_number; vi++) {
      print->print("{\"timestamp\":");
      timestamp_cb(print, vi, si);
      print->print(",\"value\":\"");
      data_cb(print, vi, si);
      print->print("\"}");
      if (vi != value_number - 1) { print->print(","); }
    }
    print->print("]");
    if (si != stream_number - 1) { print->print(","); }
  }
  print->print("}}");
}

static void print_post_multiple_values_one_device(
    Print* print, int stream_number,
    put_data_fill_callback timestamp_cb,
    post_multiple_stream_fill_callback stream_cb,
    post_multiple_data_fill_callback data_cb) {
  int si;
  print->print("{");
  if (timestamp_cb) {
    print->print("\"timestamp\":");
    timestamp_cb(print);
    print->print(",");
  }
  print->print("\"values\":{");
  for (si = 0; si < stream_number; si++) {
    stream_cb(print, si);
    print->print(":");
    data_cb(print, 0, si);
    if (si != stream_number - 1) { print->print(","); }
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

static void print_delete_values(Print* print, delete_values_timestamp_fill_callback cb) {
  print->print(F("{\"from\":"));
  cb(print, 1);
  print->print(F(",\"end\":"));
  cb(print, 2);
  print->print(F("}"));
}

static void print_command_body(Print* print, put_data_fill_callback body_cb) {
  if (body_cb) {
    body_cb(print);
  }
}

static uint16_t post_client_internal_datafill_cb(uint8_t fd) {
  BufferFiller bfill = EtherCard::tcpOffset();
  NullPrint null_print;

  if (fd == s_fd) {
    bfill.print(F("POST /v2/devices/"));
    print_encoded_string(&bfill, s_device_id);
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
    bfill.print(F("POST /v2/devices/"));
    print_encoded_string(&bfill, s_device_id);
    bfill.println(F("/updates HTTP/1.0"));

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

static uint16_t post_single_device_internal_datafill_cb(uint8_t fd) {
  BufferFiller bfill = EtherCard::tcpOffset();
  NullPrint null_print;

  if (fd == s_fd) {
    bfill.print(F("POST /v2/devices/"));
    print_encoded_string(&bfill, s_device_id);
    bfill.println(F("/update HTTP/1.0"));

    null_print.count = 0;
    print_post_multiple_values_one_device(&null_print, s_number, s_put_cb,
                                          s_post_multiple_stream_cb,
                                          s_post_multiple_data_cb);
    s_client->writeHttpHeader(&bfill, null_print.count);

    print_post_multiple_values_one_device(&bfill, s_number, s_put_cb,
                                          s_post_multiple_stream_cb,
                                          s_post_multiple_data_cb);
  }
  return bfill.position();
}

static uint16_t update_location_internal_datafill_cb(uint8_t fd) {
  BufferFiller bfill = EtherCard::tcpOffset();
  NullPrint null_print;

  if (fd == s_fd) {
    bfill.print(F("PUT /v2/devices/"));
    print_encoded_string(&bfill, s_device_id);
    bfill.println(F("/location HTTP/1.0"));

    null_print.count = 0;
    print_location(&null_print, s_has_name, s_has_elevation, s_update_location_data_cb);
    s_client->writeHttpHeader(&bfill, null_print.count);

    print_location(&bfill, s_has_name, s_has_elevation, s_update_location_data_cb);
  }
  return bfill.position();
}

static uint16_t delete_client_internal_datafill_cb(uint8_t fd) {
  BufferFiller bfill = EtherCard::tcpOffset();
  NullPrint null_print;

  if (fd == s_fd) {
    bfill.print(F("DELETE /v2/devices/"));
    print_encoded_string(&bfill, s_device_id);
    bfill.print(F("/streams/"));
    print_encoded_string(&bfill, s_stream_name);
    bfill.println(F("/values HTTP/1.0"));

    null_print.count = 0;
    print_delete_values(&null_print, s_delete_cb);
    s_client->writeHttpHeader(&bfill, null_print.count);

    print_delete_values(&bfill, s_delete_cb);
  }
  return bfill.position();
}

static uint16_t post_command_internal_datafill_cb(uint8_t fd) {
  BufferFiller bfill = EtherCard::tcpOffset();
  NullPrint null_print;

  if (fd == s_fd) {
    bfill.print(F("POST /v2/devices/"));
    print_encoded_string(&bfill, s_device_id);
    bfill.print(F("/commands/"));
    print_encoded_string(&bfill, s_command_id);
    bfill.print(F("/"));
    bfill.print(s_command_action);
    bfill.println(F(" HTTP/1.0"));

    null_print.count = 0;
    print_command_body(&null_print, s_put_cb);
    s_client->writeHttpHeader(&bfill, null_print.count);

    print_command_body(&bfill, s_put_cb);
  }
  return bfill.position();
}

static uint16_t get_timestamp_internal_datafill_cb(uint8_t fd) {
  BufferFiller bfill = EtherCard::tcpOffset();
  NullPrint null_print;

  if (fd == s_fd) {
    bfill.print(F("GET /v2/time/"));
    switch (s_timestamp_type) {
      case 1:
        bfill.print(F("seconds"));
        break;
      case 2:
        bfill.print(F("millis"));
        break;
      default:
        bfill.print(F("iso8601"));
        break;
    }
    bfill.println(F(" HTTP/1.0"));
    s_client->writeHttpHeader(&bfill, 0);
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

static int fill_buffer_with_body(const char* data, int length) {
  int content_length, offset, i;
  content_length = s_client->readContentLength(data, length);
  if (content_length > 0) {
    if (*s_response_buffer_length < content_length) {
      *s_response_buffer_length = content_length;
      return E_BUFFER_TOO_SMALL;
    }
    offset = s_client->skipHttpHeader(data, length);
    if (offset < 0) { return E_INVALID; }
    for (i = 0; i < content_length; i++) {
      s_response_buffer[i] = data[offset + i];
    }
    *s_response_buffer_length = content_length;
    return 0;
  } else {
    return E_INVALID;
  }
}

static uint8_t client_internal_fetch_code_and_body_cb(uint8_t fd, uint8_t statuscode, uint16_t datapos, uint16_t len_of_data) {
  char* origin;
  int ret;

  if (fd == s_fd) {
    if (statuscode == 0) {
      origin = (char*) ether.buffer + datapos;
      s_response_code = s_client->readStatusCode(origin, len_of_data);
      if (s_response_code == 200) {
        ret = fill_buffer_with_body(origin, len_of_data);
        if (ret < 0) {
          s_response_code = ret;
        }
      }
    } else {
      s_response_code = statuscode;
    }
  }
}

int M2XNanodeClient::updateStreamValue(const char* device_id, const char* stream_name,
                                       put_data_fill_callback cb) {
  int i;
  ether.packetLoop(ether.packetReceive());
  for (i = 0; i < 4; i++) {
    ether.hisip[i] = (*_addr)[i];
  }
  s_client = this;
  s_device_id = device_id;
  s_stream_name = stream_name;
  s_put_cb = cb;
  s_response_code = 0;
  s_fd = ether.clientTcpReq(client_internal_fetch_response_code_cb,
                            put_client_internal_datafill_cb,
                            _port);
  return loop();
}

int M2XNanodeClient::postStreamValues(const char* device_id, const char* stream_name, int value_number,
                                      post_data_fill_callback timestamp_cb,
                                      post_data_fill_callback data_cb) {
  int i;
  ether.packetLoop(ether.packetReceive());
  for (i = 0; i < 4; i++) {
    ether.hisip[i] = (*_addr)[i];
  }
  s_client = this;
  s_device_id = device_id;
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

int M2XNanodeClient::postDeviceUpdates(const char* device_id, int stream_number,
                                       post_multiple_stream_fill_callback stream_cb,
                                       post_multiple_data_fill_callback timestamp_cb,
                                       post_multiple_data_fill_callback data_cb) {
  int i;
  ether.packetLoop(ether.packetReceive());
  for (i = 0; i < 4; i++) {
    ether.hisip[i] = (*_addr)[i];
  }
  s_client = this;
  s_device_id = device_id;
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

int M2XNanodeClient::postDeviceUpdate(const char* device_id, int stream_number,
                                      put_data_fill_callback timestamp_cb,
                                      post_multiple_stream_fill_callback stream_cb,
                                      post_multiple_data_fill_callback data_cb) {
  int i;
  ether.packetLoop(ether.packetReceive());
  for (i = 0; i < 4; i++) {
    ether.hisip[i] = (*_addr)[i];
  }
  s_client = this;
  s_device_id = device_id;
  s_number = stream_number;
  s_put_cb = timestamp_cb;
  s_post_multiple_stream_cb = stream_cb;
  s_post_multiple_data_cb = data_cb;
  s_response_code = 0;
  s_fd = ether.clientTcpReq(client_internal_fetch_response_code_cb,
                            post_single_device_internal_datafill_cb,
                            _port);
  return loop();
}


int M2XNanodeClient::updateLocation(const char* device_id, int has_name, int has_elevation,
                                    update_location_data_fill_callback cb) {
  int i;
  ether.packetLoop(ether.packetReceive());
  for (i = 0; i < 4; i++) {
    ether.hisip[i] = (*_addr)[i];
  }
  s_client = this;
  s_device_id = device_id;
  s_has_name = has_name;
  s_has_elevation = has_elevation;
  s_update_location_data_cb = cb;
  s_response_code = 0;
  s_fd = ether.clientTcpReq(client_internal_fetch_response_code_cb,
                            update_location_internal_datafill_cb,
                            _port);
  return loop();
}

int M2XNanodeClient::deleteValues(const char* device_id, const char* stream_name,
                                  delete_values_timestamp_fill_callback timestamp_cb) {
  int i;
  ether.packetLoop(ether.packetReceive());
  for (i = 0; i < 4; i++) {
    ether.hisip[i] = (*_addr)[i];
  }
  s_client = this;
  s_device_id = device_id;
  s_stream_name = stream_name;
  s_delete_cb = timestamp_cb;
  s_response_code = 0;
  s_fd = ether.clientTcpReq(client_internal_fetch_response_code_cb,
                            delete_client_internal_datafill_cb,
                            _port);
  return loop();
}

int M2XNanodeClient::markCommandProcessed(const char* device_id,
                                          const char* command_id,
                                          put_data_fill_callback body_cb) {
  int i;
  ether.packetLoop(ether.packetReceive());
  for (i = 0; i < 4; i++) {
    ether.hisip[i] = (*_addr)[i];
  }
  s_client = this;
  s_device_id = device_id;
  s_command_id = command_id;
  s_command_action = "process";
  s_put_cb = body_cb;
  s_response_code = 0;
  s_fd = ether.clientTcpReq(client_internal_fetch_response_code_cb,
                            post_command_internal_datafill_cb,
                            _port);
  return loop();
}

int M2XNanodeClient::markCommandRejected(const char* device_id,
                                         const char* command_id,
                                         put_data_fill_callback body_cb) {
  int i;
  ether.packetLoop(ether.packetReceive());
  for (i = 0; i < 4; i++) {
    ether.hisip[i] = (*_addr)[i];
  }
  s_client = this;
  s_device_id = device_id;
  s_command_id = command_id;
  s_command_action = "reject";
  s_put_cb = body_cb;
  s_response_code = 0;
  s_fd = ether.clientTcpReq(client_internal_fetch_response_code_cb,
                            post_command_internal_datafill_cb,
                            _port);
  return loop();
}

int M2XNanodeClient::getTimestamp(char* buffer, int* bufferLength, int type) {
  int i;
  ether.packetLoop(ether.packetReceive());
  for (i = 0; i < 4; i++) {
    ether.hisip[i] = (*_addr)[i];
  }
  s_client = this;
  s_timestamp_type = type;
  s_response_code = 0;
  s_response_buffer = buffer;
  s_response_buffer_length = bufferLength;
  s_fd = ether.clientTcpReq(client_internal_fetch_code_and_body_cb,
                            get_timestamp_internal_datafill_cb,
                            _port);
  return loop();
}

int M2XNanodeClient::getTimestampSeconds(int32_t* ts) {
  // The maximum value of signed 64-bit integer is 0x7fffffffffffffff,
  // which is 9223372036854775807. It consists of 19 characters, so a
  // buffer of 20 is definitely enough here
  int length = 20;
  char buffer[20];
  int status = getTimestamp(buffer, &length, 1);
  if (status == 200) {
    int32_t result = 0;
    for (int i = 0; i < length; i++) {
      result = result * 10 + (buffer[i] - '0');
    }
    if (ts != NULL) { *ts = result; }
  }
  return status;
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

int M2XNanodeClient::readContentLength(const char* origin, int len) {
  int length, i, ret;
  char c;

  ret = waitForString(origin, len, "Content-Length: ");
  if (ret < 0) {
    return ret;
  }

  length = 0;
  i = 0;
  while (i + ret < len) {
    c = origin[ret + i];
    i++;

    if ((c == '\r') || (c == '\n')) {
      return (length == 0) ? (E_INVALID) : (length);
    } else {
      length = length * 10 + (c - '0');
    }
  }
  return E_INVALID;
}

int M2XNanodeClient::skipHttpHeader(const char* origin, int len) {
  return waitForString(origin, len, "\n\r\n");
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
