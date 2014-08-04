#ifndef M2XNanodeClient_h
#define M2XNanodeClient_h

#include <Arduino.h>
#include <Client.h>
#include "NullPrint.h"

#define MIN(a, b) (((a) > (b))?(b):(a))

#define USER_AGENT F("User-Agent: M2X Nanode Client/0.1")

#define HEX(t_) ((char) (((t_) > 9) ? ((t_) - 10 + 'A') : ((t_) + '0')))
#define MAX_DOUBLE_DIGITS 7

const int E_OK = 0;
const int E_DISCONNECTED = -1;
const int E_INVALID = -2;
const int E_JSON_INVALID = -3;
const int E_TIMEOUT = -4;
const int E_NOMATCH = -5;

typedef void (*stream_value_read_callback)(const char* at,
                                           const char* value,
                                           int index,
                                           void* context);

typedef void (*location_read_callback)(const char* name,
                                       double latitude,
                                       double longitude,
                                       double elevation,
                                       const char* timestamp,
                                       int index,
                                       void* context);
typedef void (*put_data_fill_callback)(Print* print);
typedef void (*post_data_fill_callback)(Print* print, int index);

// Returned value is the number of values in current stream
typedef int (*post_multiple_stream_fill_callback)(Print* print, int streamIndex);
typedef void (*post_multiple_data_fill_callback)(Print* print, int valueIndex, int streamIndex);

// Values of data type:
// 1 - Latitude
// 2 - Longitude
// 3 - Name
// 4 - Elevation
const int kLocationFieldLatitude PROGMEM = 1;
const int kLocationFieldLongitude PROGMEM = 2;
const int kLocationFieldName PROGMEM = 3;
const int kLocationFieldElevation PROGMEM = 4;
typedef void (*update_location_data_fill_callback)(Print* print, int dataType);

const int kDefaultM2XPort PROGMEM = 80;

class M2XNanodeClient {
public:
  M2XNanodeClient(const char* key,
                  IPAddress* addr,
                  int timeout_seconds = 15,
                  int case_insensitive = 1,
                  int port = kDefaultM2XPort);

  // Push data stream value using PUT request, returns the HTTP status code
  int put(const char* feedId, const char* streamName,
          put_data_fill_callback cb);

  // Push multiple data stream values using POST request, returns the
  // HTTP status code
  // NOTE: timestamp is required in this function
  int post(const char* feedId, const char* streamName, int valueNumber,
           post_data_fill_callback timestamp_cb,
           post_data_fill_callback data_cb);

  // Push multiple data values to multiple streams using POST request,
  // returns HTTP status code
  // NOTE: timestamp is also required here
  int postMultiple(const char* feedId, int streamNumber,
                   post_multiple_stream_fill_callback stream_cb,
                   post_multiple_data_fill_callback timestamp_cb,
                   post_multiple_data_fill_callback data_cb);

  // Update datasource location using PUT request, returns HTTP status code.
  // Name and elevation are optional parameters in the API request. Hence
  // you can use +has_name+ and +has_elevation+ to control the presence
  // of those parameters: 1 for present, 0 for not present.
  // See the definition of +update_location_data_fill_callback+ for possible
  // value types.
  int updateLocation(const char* feedId, int has_name, int has_elevation,
                     update_location_data_fill_callback cb);

  // WARNING: The functions below this line are not considered APIs, they
  // are made public only to ensure callback functions can call them. Make
  // sure you know what you are doing before calling them.

  // Writes the HTTP header part for updating a stream value
  void writeHttpHeader(Print* print, int contentLength);

  // Parses and returns the HTTP status code, note this function will
  // return immediately once it gets the status code
  int readStatusCode(const char* origin, int len);
private:
  const char* _key;
  int _timeout_seconds;
  int _case_insensitive;
  IPAddress* _addr;
  int _port;

  // Waits for a certain string pattern in the HTTP header, and returns
  // once the pattern is found. In the pattern, you can use '*' to denote
  // any character
  int waitForString(const char* origin, int len, const char* str);

  // Run network loop till one of the following conditions is met:
  // 1. A response code is obtained;
  // 2. The request has time out.
  int loop();
};

#endif  /* M2XNanodeClient_h */
