# Nanode M2X API Client #

The AT&T M2X API provides all the needed operations to connect your device to AT&T's [M2X](http://m2x.att.com) service. This client provides an easy to use interface for [Nanode](http://www.nanode.eu) microcontroller based devices. This client library supports writing data from a Nanode devices to M2X but does not support reading data back out from the M2X service.

## Getting Started ##

1. Signup for an [M2X Account](https://m2x.att.com/signup).
2. Obtain your _Master Key_ from the Master Keys tab of your [Account Settings](https://m2x.att.com/account) screen.
3. Create your first [Data Source Blueprint](https://m2x.att.com/blueprints) and copy its _Device ID_.
4. Review the [M2X API Documentation](https://m2x.att.com/developer/documentation/overview).

Please consult the [M2X glossary](https://m2x.att.com/developer/documentation/glossary) if you have questions about any M2X specific terms.


## How to Use the Library ##

1. Clone [ethercard](https://github.com/xxuejie/ethercard) library:

    $ git clone https://github.com/xxuejie/ethercard

2. Open the Arduino IDE, click `Sketch`->`Import Library...`->`Add Library...`, navigate and select the ethercard repo folder, click `OK` to import the libray into Arduino.

3. Clone [m2x-nanode](https://github.com/attm2x/m2x-nanode) library:

    $ git clone https://github.com/attm2x/m2x-nanode

4. Use the process outlined in Step #2 above to import `M2XNanodeClient` in this repo. Note that while in Step #2, we are selecting the repo folder `ethercard`, in this step we are selecting a sub-directory of the repo.

5. Open one of the Nanode examples in `File`->`Examples`->`M2XNanodeClient`.

6. Change `Device ID`, `Stream Name` and `M2X Key` fields to contain correct values (from your M2X account).

7. Set the board type to Arduino UNO via `Tools` -> `Board Type` -> `Arduino UNO`.

8. Now you can run the examples!

**NOTE**: For simplicity, all of provided examples use fixed values as inputs. If you want to use values obtained from real sensors as inputs, please refer to [this tutorial](http://shop.wickeddevice.com/2013/10/16/nanode-gatewayremote-sensing-tutorial-1/) for the value gathering functionality.

## Client API ##

Due to the memory limitations of Nanode boards, most of the APIs provided work in a callback-based approach:

1. You define one or more function(s) to provide the data needed.

2. You pass the function(s) as parameter(s) to API calls.

3. The library will invoke provided function(s) as callbacks to fill in necessary data.

For more details on how this works, please refer to the examples.

### UpdateStreamValue API ###

The UpdateStreamValue API is the simplest approach to pushing data into M2X. The API interface is as follows:

```
typedef void (*put_data_fill_callback)(Print* print);
int updateStreamValue(const char* device_id, const char* stream_name,
                      put_data_fill_callback cb);
```

A sample callback function will look like the following:

```
static int val = 11;
void fill_data_cb(Print* print) {
  print->print(val);
}
```

If you want to pass strings as values, you need to include double-quotes:

```
void fill_data_cb(Print* print) {
  print->print("\"11\"");
}
```

### PostStreamValues API ###

PostStreamValues API has the following differences from UpdateStreamValue API:

1. You can push multiple values in one request;
2. For each value, you also need to provide an ISO8601-formatted timestamp.

The calling interface for this API is as follows:

```
typedef void (*post_data_fill_callback)(Print* print, int index);
int postStreamValues(const char* device_id, const char* stream_name, int value_number,
                     post_data_fill_callback timestamp_cb,
                     post_data_fill_callback data_cb);
```

Notice here that we use two callback functions: one for timestamp, and one for data. For each callback function, the client library will invoke it `value_number` times, each time for a different value to be pushed.

Notice for timestamp and string-typed values, double quotes are needed:

```
static int seconds = 10;
void fill_timestamp_cb(Print* print, int index) {
  print->print("\"2014-07-09T19:");
  print->print(15 + index);
  print->print(":");
  print->print(seconds);
  print->print(".624Z\"");
}
```

### PostDeviceUpdates API ###

PostDeviceUpdates API has one more advantage over the PostStreamValues API: it allows pushing multiple values to multiple streams in one request. As a result, the calling interface for this API is more complex:

```
typedef int (*post_multiple_stream_fill_callback)(Print* print, int stream_index);
typedef void (*post_multiple_data_fill_callback)(Print* print, int value_index, int stream_index);
int postDeviceUpdates(const char* device_id, int stream_number,
                      post_multiple_stream_fill_callback stream_cb,
                      post_multiple_data_fill_callback timestamp_cb,
                      post_multiple_data_fill_callback data_cb);
```

First, `stream_number` denotes how many streams we are pushing to. For each stream, `stream_cb` will be called with the corresponding index, this callback function has 2 effects:

1. It prints the stream name into `print`(double quotes needed);
2. It returns the number of values we want to push for this stream.

Suppose during the `i`th time we call `stream_cb`, `j` is returned as a result. Then we will call `timestamp_cb` callback function `j` times, with `i` as `stream_index`, and `0` through `j-1` as `value_index`, each time, we are getting the timestamp for the value to push.

We are also calling `data_cb` the same number of times with the same input parameter to get the value to push.

Notice that for all callback functions, if string values are printed, double quotes are needed.

### Update Location API ###

The update Location API can be used to update the location of a datasource. The calling interface is as follows:

```
// Values of data type:
// 1 - Latitude
// 2 - Longitude
// 3 - Name
// 4 - Elevation
typedef void (*update_location_data_fill_callback)(Print* print, int data_type);
int updateLocation(const char* device_id, int has_name, int has_elevation,
                   update_location_data_fill_callback cb);
```

In this API request, `name` and `elevation` are optional values. `has_name` and `has_elevation` thus determine if `name` and `elevation` are present respectively: 1 for present, 0 for absent.

Depending on the values `has_name` and `has_elevation`, the callback function will be called 2 - 4 times. Each time, a different `data_type` value is used, so we can know which value is requested right now.

Like all the requests outlined above, if string-typed values are printed, double quotes are needed as well.

### Delete Values API ###

Delete Values API can be used to delete values within a specific time range. The calling API is as follows:

```
// Values for timestamp type:
// 1 - Start Time
// 2 - End Time
typedef void (*delete_values_timestamp_fill_callback)(Print* print, int type);
int deleteValues(const char* device_id, const char* stream_name,
                 delete_values_timestamp_fill_callback timestamp_cb);

```

Following is an example of the callback function:

```
void fill_timestamp_cb(Print* print, int type) {
  if (type == 1) {
    print->print("\"2014-07-01T00:00:00.000Z\"");
  } else {
    print->print("\"2014-07-30T00:00:00.000Z\"");
  }
}
```

## Known Issues ##

* In our tests with Nanode based devices, we found that there is a small chance that an API request may timeout. This occurs inside the ethercard library: our internal callback functions are not called at all. We suspect that this may be related to the way TCP/IP is implemented in the library, or our way of using the library (we might accidently set the wrong parameter for some option).

* Unlike the Arduino client library, fetching values is not supported in the Nanode library. This is because [ethercard](https://github.com/xxuejie/ethercard) does not support packet streaming: when a packet arrives, ethercard will read all the data into memory before handling over control to our code. Considering the fact that a Nanode only has 2kb memory, and that a simple M2X List Value API response contains 4k-5k data, we can never process this request on Nanode successfully.

## License

The Nanode M2X API Client is available under the MIT license. See the [LICENSE](LICENSE) file for more information.


