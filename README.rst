south-mqtt-scripted
===================

A south plugin that uses MQTT to receive messages via an MQTT broker from sensor. An optional script may be given, written is Python, that converts the message into a JSON document.

If the payload of the MQTT is a JSON document with simple key/value pairs, e.g.

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

.. code-block:: Python

   def convert(message):
       return {
           'temperature' : float(message)
       }

Note that the message is passed as a string and the data we wish to
ingest into FogLAMP in this case is assumed to be a floating point value.
