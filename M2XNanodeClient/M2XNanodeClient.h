#ifndef M2XNanodeClient_h
#define M2XNanodeClient_h

#include <Arduino.h>
#include <Client.h>
#include "NullPrint.h"

#define MIN(a, b) (((a) > (b))?(b):(a))

#define USER_AGENT F("User-Agent: M2X Nanode Client/2.0.0")

#define HEX(t_) ((char) (((t_) > 9) ? ((t_) - 10 + 'A') : ((t_) + '0')))
#define MAX_DOUBLE_DIGITS 7

const int E_OK = 0;
const int E_DISCONNECTED = -1;
const int E_INVALID = -2;
const int E_TIMEOUT = -3;
const int E_NOMATCH = -4;

typedef void (*put_data_fill_callback)(Print* print);
typedef void (*post_data_fill_callback)(Print* print, int index);

// Returned value is the number of values in current stream
typedef int (*post_multiple_stream_fill_callback)(Print* print, int stream_index);
typedef void (*post_multiple_data_fill_callback)(Print* print, int valueIndex, int stream_index);

// Values of data type:
// 1 - Latitude
// 2 - Longitude
// 3 - Name
// 4 - Elevation
const int kLocationFieldLatitude PROGMEM = 1;
const int kLocationFieldLongitude PROGMEM = 2;
const int kLocationFieldName PROGMEM = 3;
const int kLocationFieldElevation PROGMEM = 4;
typedef void (*update_location_data_fill_callback)(Print* print, int data_type);

// Values of timestamp type:
// 1 - Start
// 2 - End
const int kDeleteTimestampStart PROGMEM = 1;
const int kDeleteTimestampEnd PROGMEM = 2;
typedef void (*delete_values_timestamp_fill_callback)(Print* print, int type);

const int kDefaultM2XPort PROGMEM = 80;

class M2XNanodeClient {
public:
  M2XNanodeClient(const char* key,
                  IPAddress* addr,
                  int timeout_seconds = 15,
                  int case_insensitive = 1,
                  int port = kDefaultM2XPort);

  // Push data stream value using PUT request, returns the HTTP status code
  int updateStreamValue(const char* device_id, const char* stream_name,
                        put_data_fill_callback cb);

  // Push multiple data stream values using POST request, returns the
  // HTTP status code
  // NOTE: timestamp is required in this function
  int postStreamValues(const char* device_id, const char* stream_name, int value_number,
                       post_data_fill_callback timestamp_cb,
                       post_data_fill_callback data_cb);

  // Push multiple data values to multiple streams using POST request,
  // returns HTTP status code
  // NOTE: timestamp is also required here
  int postDeviceUpdates(const char* device_id, int stream_number,
                        post_multiple_stream_fill_callback stream_cb,
                        post_multiple_data_fill_callback timestamp_cb,
                        post_multiple_data_fill_callback data_cb);

  // Update datasource location using PUT request, returns HTTP status code.
  // Name and elevation are optional parameters in the API request. Hence
  // you can use +has_name+ and +has_elevation+ to control the presence
  // of those parameters: 1 for present, 0 for not present.
  // See the definition of +update_location_data_fill_callback+ for possible
  // value types.
  int updateLocation(const char* device_id, int has_name, int has_elevation,
                     update_location_data_fill_callback cb);

  // Delete values from a data stream
  // You will need to provide from and end date/time strings in the ISO8601
  // format "yyyy-mm-ddTHH:MM:SS.SSSZ" where
  //   yyyy: the year
  //   mm: the month
  //   dd: the day
  //   HH: the hour (24 hour format)
  //   MM: the minute
  //   SS.SSS: the seconds (to the millisecond)
  // NOTE: the time is given in Zulu (GMT)
  // M2X will delete all values within the from to end date/time range.
  // The status code is 204 on success and 400 on a bad request (e.g. the
  // timestamp is not in ISO8601 format or the from timestamp is not less than
  // or equal to the end timestamp.
  int deleteValues(const char* device_id, const char* stream_name,
                   delete_values_timestamp_fill_callback timestamp_cb);

  // WARNING: The functions below this line are not considered APIs, they
  // are made public only to ensure callback functions can call them. Make
  // sure you know what you are doing before calling them.

  // Writes the HTTP header part for updating a stream value
  void writeHttpHeader(Print* print, int content_length);

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
