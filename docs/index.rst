.. Images
.. |mqtt_01| image:: images/mqtt_01.jpg
.. |mqtt_02| image:: images/mqtt_02.jpg

MQTT South with Payload Scripting
=================================

The *foglamp-south-mqtt-scripted* plugin uses MQTT to receive messages via an MQTT broker from sensors or other sources. It then uses an optional script, written is Python, that converts the message into a JSON document and pushes data to the FogLAMP System.

If the payload of the MQTT message is a JSON document with simple key/value pairs, e.g.

.. code-block:: json

   { "temperature" : 23.1, "humidity" : 47.2 }

Then no translation script is required. Also if the payload is a simple
numeric value the plugin will accept this and create an asset with
the data point name matching the topic on which the value was given in
the payload.

If the message format is not a simple JSON document or a single value,
or is in some other format then a Python script should be provided that
turns the message into a JSON format.

An example script, assuming the payload in the message is simply a value, might be a follows

.. code-block:: Python

   def convert(message, topic):
       return {
           'temperature' : float(message)
       }

Note that the message and topic are passed as a strings and the data we wish to
ingest into FogLAMP in this case is assumed to be a floating point value.
The example above of course is unnecessary as the plugin can consume this
data without the need of a script.

The script could return either one or two values.

The script should return the JSON document as a Python DICT in the case of a single value.

The script should return a string and a JSON document as a Python DICT in the case of two values,
the first of these values is the name of the asset to use and overrides the default asset naming defined in the plugin configuration.

First case sample:

.. code-block:: Python

    def convert(message, topic):
        return {"temperature_1": 10.2}

Second case sample:

.. code-block:: Python

    def convert(message, topic):
        return "ExternalTEMP",  {"temperature_3": 11.3}

Configuration
-------------

When adding a south service with this plugin the same flow is used as with any other south service. The configuration page for the plugin is as follows.

+-----------+
| |mqtt_01| |
+-----------+

  - **Asset Name**: The name of the asset the plugin will create for each message, unless the convert function returns an explict asset name to be used.

  - **MQTT Broker**: The IP address/hostname of the MQTT broker to use. Note FogLAMP requires an external MQTT broker is run currently and does not provide an internal broker in the current release.

  - **Username**: The username to be used if required for authentication. This should be left blank if authentication is not required.

  - **Password**: The password to use if username is to be used.

  - **Trusted Certificate**: The trusted certificate of the MQTT broker. If MQTTS communication is not required then this can be left blank.

  - **Client Certificate**: The certificate that will be used by the MQTT plugin.

  - **MQTTS Key**: The private key of the MQTT plugin. If the key is included in the PEM file of the client certificate this may be left blank.

  - **Key Password**: The password used to encrypted the private key. This may be left blank if the private key was not encrypt.

  - **Topic**: The MQTT topic to which to subscribe. The topic may include the usual MQTT wildcards; + for a single level wildcard and # for a multi-level wildcard

  - **Object Policy**: Controls how the plugin deals with nested objects within the JSON payloads it receives or the return from the script that is executed. See below for a description of the various object policy values.

  - **Time Format**: The format to both pass the timestamps into the query parameters using and also to interpret the timestamps returned in the payload.

  - **Timezone**: The timezone to use for the start and end times that are sent in the API request and also when timestamps are read from the API response. Timezone is expressed as an offset in hours and minutes from UTC for the local timezone of the API. E.g. -08:00 for PST time zones.


  - **Script**: The Python script to execute for message processing. Initially a file must be uploaded, however once uploaded the user may edit the script in the box provided. A script is optional.


Object Policy
=============

The object policy is used by the plugin to determine how it deals with nested objects within the JSON that is in the MQTT payload or the JSON that is returned from the script that is executed, if present.

+-----------+
| |mqtt_02| |
+-----------+

  - **Single reading from root level**: This is the simple behavior of the plugin, it will only take numeric and string values that are in the root of the JSON document and ignore any objects contained in the root.

  - **Single reading & collapse**: The plugin will create a single reading form the payload that will contain the string and numeric data in the root level. The plugin will also recursively traverse any child objects and add the string and numeric data from those to the reading as data points of the reading itself.

  - **Single reading & nest**: As above, the plugin will create a single reading form the payload that will contain the string and numeric data in the root level. The plugin will also recursively traverse any child objects and add the string and numeric data from those objects and add them as nested data points.

  - **Multiple readings & collapse**: The plugin will create one reading that contains any string and numeric data in the root of the JSON. It will then create one reading for each object in the root level. Each of these readings will contain the string and numeric data from those child objects along with the data found in the children of those objects. Any child data will be collapse into the base level of the readings.

  - **Multiple readings & nest**: As above, but any data in the children of the readings found below the first level, which defines the reading names, will be created as nested data points rather than collapsed.

As an example of how the policy works assume we have an MQTT payload with a message as below

.. code-block:: JSON

   {
        "name"  : "pump47",
        "motor" : {
                    "current" : 0.75,
                    "speed"   : 1496
                    },
        "flow"  : 1.72,
        "temperatures" : {
                    "bearing" : 21.5,
                    "impeller" : 16.2,
                    "motor" : {
                          "casing" : 24.6,
                          "gearbox" : 28.2
                          }
                         }
   }

If the policy is set to *Single reading from root level* then a reading would be created, with the asset name given in the configuration of the plugin, that contained two data points *name* and *flow*.

If the policy is set to *Single reading & collapse* then the reading created would now have 8 data points; *name*, *current*, *speed*, *flow*, *bearing*, *impeller*, *casing* and *gearbox*. These would all be in a reading with the asset name defined in the configuration and in a flat structure.

If the policy is set to *Single reading & nest* there would still be a single reading, with the asset name set in the configuration, which would have data points for *name*, *motor*, *flow* and *temperature*. The *motor* data point would have two child data points called *current* and *speed*, the *temperature* data point would have three child data points called *bearing*, *impeller* and *motor*. This *motor* data point would itself have two children call *casing* and *gearbox*.

If the policy is set to *Multiple readings & collapse* there would be three readings created from this payload; one that is names as per the asset name in the configuration, a *motor* reading and a *temperature* reading. The first of these readings would have data points called *name* and *flow*, the *motor* reading would have data points *current* and *speed*. The *temperatures* reading would have data points *bearing*, *impeller*, *casing* and *gearbox*.

If the policy is set to *Multiple readings & nest* there would be three readings created from this payload; one that is names as per the asset name in the configuration, a *motor* reading and a *temperature* reading. The first of these readings would have data points called *name* and *flow*, the *motor* reading would have data points *current* and *speed*. The *temperatures* reading would have data points *bearing*, *impeller* and *motor*, the *motor* data point would have two child data points *casing* and *gearbox*.



Timestamp Treatment
-------------------

The default timestamp for a reading collected via this plugin will be
the time at which the reading was taken, however it is possible for the
API that is being called to include a different timestamp.

Returning a data point called whose name is defined in the *Timestamp*
configuration option will result in the value of that data point being
used as the timestamp. This data point will not be added to the reading.
The default name of the timestamp is *timestamp*.

The timestamp data point should be a string and the timestamp should
be formatted to match the definition given in the *Time format*
configuration parameter. The format is based on the standard Linux
strptime formatting options and is discussed below in the section
discussing the :ref:ref::`time_format` selection method.


The timezone may be set by using the *Timezone* configuration parameter
to set the offset of the timezone in which the API is running.

.. _time_format:

Time Format
~~~~~~~~~~~

The format of the timestamps read in the message payload or by the script returned are defined by the *Time Format* configuration parameter and uses the standard Linux mechanism to define a time format. The following character sequences are supported.

  %%
      The % character.

  %a or %A
      The  name of the day of the week according to the current locale, in abbreviated form or the full name.

  %b or %B or %h
      The month name according to the current locale, in abbreviated form or the full name.

   %c
      The date and time representation for the current locale.

   %C
      The century number (0–99).

   %d or %e
      The day of month (1–31).

   %D
       Equivalent to %m/%d/%y.  (This is the American style date,  very  confusing  to  non- Americans, especially since %d/%m/%y is widely used in Europe.  The ISO 8601 standard format is %Y-%m-%d.)

   %H
       The hour (0–23).

   %I
       The hour on a 12-hour clock (1–12).

   %j
       The day number in the year (1–366).

   %m
        The month number (1–12).

   %M
        The minute (0–59).

   %n
        Arbitrary white space.

   %p
        The locale's equivalent of AM or PM.  (Note: there may be none.)

   %r
        The 12-hour clock time (using the locale's AM or PM).  In the POSIX locale equivalent to  %I:%M:%S  %p.   If t_fmt_ampm is empty in the LC_TIME part of the current locale, then the behavior is undefined.

   %R
        Equivalent to %H:%M.

   %S
        The second (0–60; 60 may occur for leap seconds; earlier also 61 was allowed).

   %t
        Arbitrary white space.

   %T
        Equivalent to %H:%M:%S.

   %U
        The week number with Sunday the first day of the week (0–53).  The  first  Sunday  of January is the first day of week 1.

   %w
        The ordinal number of the day of the week (0–6), with Sunday = 0.

   %W
        The  week  number  with Monday the first day of the week (0–53).  The first Monday of January is the first day of week 1.

   %x
        The date, using the locale's date format.

   %X
        The time, using the locale's time format.

   %y
        The year within century (0–99).  When a century is not otherwise specified, values in the  range  69–99  refer to years in the twentieth century (1969–1999); values in the range 00–68 refer to years in the twenty-first century (2000–2068).

   %Y
        The year, including century (for example, 1991).

