.. Images
.. |mqtt_01| image:: images/mqtt_01.jpg

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
turns the message into a JSON format. The script should return the JSON
document as a Python DICT.

An example script, assuming the payload in the message is simply a value, might be a follows

.. code-block:: json

   def convert(message):
       return {
           'temperature' : float(message)
       }

Note that the message is passed as a string and the data we wish to
ingest into FogLAMP in this case is assumed to be a floating point value.
The example above of course is unnecessary as the plugin can consume this
data without the need of a script.

Configuration
-------------

When adding a south service with this plugin the same flow is used as with any other south service. The configuration page for the plugin is as follows.

+-----------+
| |mqtt_01| |
+-----------+

  - **Asset Name**: The name of the asset the plugin will create for each message.

  - **MQTT Broker**: The IP address/hostname of the MQTT broker to use. Note FogLAMP requires an external MQTT broker is run currently and does not provide an internal broker in the current release.

  - **Topic**: The MQTT topic to which to subscribe. The topic may include the usual MQTT wildcards; + for a single level wildcard and # for a multi-level wildcard

  - **Script**: The Python script to execute for message processing. Initially a file must be uploaded, however once uploaded the user may edit the script in the box provided. A script is optional.
