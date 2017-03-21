/** @class M2XNanodeClient
 *  @brief Wrapper for <a href="https://m2x.att.com/developer/documentation/v2/overview">AT&T M2X API</a>
 */

/*! \mainpage Nanonode toolkit for the <a href="https://m2x.att.com/developer/documentation/v2/overview">AT&T M2X API</a>
 *
 * View the <a href="https://github.com/attm2x/m2x-nanode/blob/master/README.md">M2X Nanonode Client README</a> for usage details
 *
 * All methods in this client library require an API Key for authentication. There are multiple types of API Keys which provide granular access to your M2X resources. Please review the <a href="https://m2x.att.com/developer/documentation/v2/overview#API-Keys">API Keys documentation</a> for more details on the different types of keys available
 *
 * If an invalid API Key is utilized, you will receive the following error when calling client methods:
 *
 * ```javascript
 * >>>client.method(...)
 * Traceback (most recent call last):
 * ...
 * requests.exceptions.HTTPError: 401 Client Error: Unauthorized
 * ```
 */
#ifndef M2XNanodeClient_h
#define M2XNanodeClient_h

#include <Arduino.h>
#include <Client.h>
#include "NullPrint.h"

#define MIN(a, b) (((a) > (b))?(b):(a))

#define USER_AGENT F("User-Agent: M2X Nanode Client/2.0.2")

#define HEX(t_) ((char) (((t_) > 9) ? ((t_) - 10 + 'A') : ((t_) + '0')))
#define MAX_DOUBLE_DIGITS 7

const int E_OK = 0;
const int E_DISCONNECTED = -1;
const int E_INVALID = -2;
const int E_TIMEOUT = -3;
const int E_NOMATCH = -4;
const int E_BUFFER_TOO_SMALL = -5;

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

  /** @brief Method for <a href="https://m2x.att.com/developer/documentation/v2/device#Update-Data-Stream-Value">Update Data Stream Value</a> endpoint
   * @param device_id - ID of the Device to update the current value of specified stream
   * @param stream_name - Name of the stream to be updated
   * @param cb - Callback function for value to be pushed
   * @return (int) Response status code
   */
  int updateStreamValue(const char* device_id, const char* stream_name,
                        put_data_fill_callback cb);

  /** @brief Method for <a href="https://m2x.att.com/developer/documentation/v2/device#Post-Data-Stream-Values">Post Data Stream Values</a> endpoint
   * @param device_id - ID of the Device to post the timestamped values on an existing data stream
   * @param stream_name - Name of the data stream to post the timestamped values
   * @param value_number - Number of values to be pushed
   * @param timestamp_cb - Callback function for timestamp to be pushed
   * @param data_cb - Callback function for data to be pushed
   * @return (int) Response status code
   */
  int postStreamValues(const char* device_id, const char* stream_name, int value_number,
                       post_data_fill_callback timestamp_cb,
                       post_data_fill_callback data_cb);

  /** @brief Method for <a href="https://m2x.att.com/developer/documentation/v2/device#Post-Device-Updates--Multiple-Values-to-Multiple-Streams-">Post Device Updates (Multiple Values to Multiple Streams)</a> endpoint
   * @param device_id - ID of the Device to post values to multiple streams
   * @param stream - Number of streams
   * @param stream_cb - Callback function for stream_name to be pushed
   * @param timestamp_cb - Callback function for timestamp to be pushed
   * @param data_cb - Callback function for stream_name to be pushed
   * @return (int) Response status code
   */
  int postDeviceUpdates(const char* device_id, int stream_number,
                        post_multiple_stream_fill_callback stream_cb,
                        post_multiple_data_fill_callback timestamp_cb,
                        post_multiple_data_fill_callback data_cb);

  /** @brief Method for <a href="https://m2x.att.com/developer/documentation/v2/device#Post-Device-Update--Single-Values-to-Multiple-Streams-">Post Device Updates (Single Values to Multiple Streams)</a> endpoint
   * @param device_id - ID of the Device to post single values to multiple streams
   * @param stream_number - Number of streams
   * @param timestamp_cb - Callback function for timestamp to be pushed
   * @param stream_cb - Callback function for stream_name to be pushed
   * @param data_cb - Callback function for data to be pushed
   * @return (int) Response status code
   */
  int postDeviceUpdate(const char* device_id, int stream_number,
                       put_data_fill_callback timestamp_cb,
                       post_multiple_stream_fill_callback stream_cb,
                       post_multiple_data_fill_callback data_cb);

  /** @brief Method for <a href="https://m2x.att.com/developer/documentation/v2/device#Update-Device-Location">Update Device Location</a> endpoint
   * @param device_id - ID of the Device to update location
   * @param has_name - 0 if not updating the name, 1 if yes
   * @param has_elevation  - 0 if not updating the elevation, 1 if yes
   * @param cb - Callback function for data to be pushed
   * @return (int) Response status code
   */
  int updateLocation(const char* device_id, int has_name, int has_elevation,
                     update_location_data_fill_callback cb);

  /** @brief Method for <a href="https://m2x.att.com/developer/documentation/v2/device#Delete-Data-Stream-Values">Delete Data Stream Values</a> endpoint
   * You will need to provide from and end date/time strings in the ISO8601
   * format "yyyy-mm-ddTHH:MM:SS.SSSZ" where
   *   yyyy: the year
   *   mm: the month
   *   dd: the day
   *   HH: the hour (24 hour format)
   *   MM: the minute
   *   SS.SSS: the seconds (to the millisecond)
   * NOTE: the time is given in Zulu (GMT)
   * M2X will delete all values within the from to end date/time range.
   * The status code is 204 on success and 400 on a bad request (e.g. the
   * timestamp is not in ISO8601 format or the from timestamp is not less than
   * or equal to the end timestamp.
   * @param device_id - ID of the Device to delete values in a stream
   * @param stream_name - Name of the data stream to delete values
   * @param timestamp_cb - Callback function for timestamp to be pushed
   * @return (int) Response status code
   */
  int deleteValues(const char* device_id, const char* stream_name,
                   delete_values_timestamp_fill_callback timestamp_cb);

  /** @brief Method for <a href="https://m2x.att.com/developer/documentation/v2/commands#Device-Marks-a-Command-as-Processed">Device Marks a Command as Processed</a> endpoint
   * @param device_id - ID of the Device to marks a command as processed
   * @param command_id - ID of the Command to mark as processed
   * @param body_cb - Callback function for body to be pushed
   * @return (int) Response status code
   */
  int markCommandProcessed(const char* device_id, const char* command_id,
                           put_data_fill_callback body_cb);

  /** @brief Method for <a href="https://m2x.att.com/developer/documentation/v2/commands#Device-Marks-a-Command-as-Rejected">Device Marks a Command as Rejected</a> endpoint
   * @param device_id - ID of the Device to marks a command as rejected
   * @param command_id - ID of the Command to mark as rejected
   * @param body_cb - Callback function for body to be pushed
   * @return (int) Response status code
   */
  int markCommandRejected(const char* device_id, const char* command_id,
                          put_data_fill_callback body_cb);

  /** @brief Method for <a href="https://m2x.att.com/developer/documentation/v2/time">Time</a> endpoint.
   * Fetches current timestamp in seconds from M2X server. Since we
   * are using signed 32-bit integer as return value, this will only
   * return valid results before 03:14:07 UTC on 19 January 2038. If
   * the device is supposed to work after that, this function should
   * not be used.
   * NOTE: although returning uint32_t can give us a larger space,
   * we prefer to cope with the unix convention here.
   * @param ts - Timestamp returned(in seconds)
   * @return (int) Response status code
   */
  int getTimestampSeconds(int32_t* ts);

  /** @brief Method for <a href="https://m2x.att.com/developer/documentation/v2/time">Time</a> endpoint.
   * Notice +bufferLength+ is supposed to contain the length of the
   * buffer when calling this function. It is also the caller's
   * responsibility to ensure the buffer is big enough, otherwise
   * the library will return an error indicating the buffer is too
   * small.
   * While this is not accurate all the time, one trick here is to
   * pass in 0 as the bufferLength, in which case we will always return
   * the buffer-too-small error. However, the correct buffer length
   * can be found this way so a secound execution is most likely to work.
   * However, in most (if not all) cases, a buffer of 30 should be more than
   * enough.
   * @param buffer - Timestamp returned
   * @param bufferLength - length of the buffer when calling this function.
   * @param type - Possible values for type include: 1 - seconds, 2 - millis and any other value - ISO8601.
   * @return (int) Response status code
   */
  int getTimestamp(char* buffer, int* bufferLength, int type = 2);

  /**
   * Writes the HTTP header part for updating a stream value
   * WARNING: This function is not considered an API, it is made public
   * only to ensure callback functions can call it.
   */
  void writeHttpHeader(Print* print, int content_length);

  /**
   * Parses and returns the HTTP status code, note this function will
   * return immediately once it gets the status code
   * WARNING: This function is not considered an API, it is made public
   * only to ensure callback functions can call it.
   */
  int readStatusCode(const char* origin, int len);

  /**
   * Parses and returns the HTTP content length, note this function will
   * return immediately once it gets the content length
   * WARNING: This function is not considered an API, it is made public
   * only to ensure callback functions can call it.
   */
  int readContentLength(const char* origin, int len);

  /**
   * Parses and returns then length for the whole HTTP header section
   * WARNING: This function is not considered an API, it is made public
   * only to ensure callback functions can call it.
   */
  int skipHttpHeader(const char* origin, int len);

private:
  const char* _key;
  int _timeout_seconds;
  int _case_insensitive;
  IPAddress* _addr;
  int _port;

  /**
   * Waits for a certain string pattern in the HTTP header, and returns
   * once the pattern is found. In the pattern, you can use '*' to denote
   * any character
   */
  int waitForString(const char* origin, int len, const char* str);

  /**
   * Run network loop till one of the following conditions is met:
   * 1. A response code is obtained;
   * 2. The request has time out.
   */
  int loop();
};

#endif  /* M2XNanodeClient_h */
