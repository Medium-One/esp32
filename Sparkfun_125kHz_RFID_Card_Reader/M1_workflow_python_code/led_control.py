'''
This workflow will turn on or off the leds.

Last Updated: June 19, 2018

Author: Medium One
'''
import MQTT


if IONode.is_trigger('in1'):
    redledlevel = IONode.get_input('in1')['event_data']['value']
    if (redledlevel == "on"):
        MQTT.publish_event_to_client('esp32', 'G1:1')
    else:
        MQTT.publish_event_to_client('esp32', 'G1:0')
    log("red led level: " + str(redledlevel))
elif IONode.is_trigger('in2'):    
    greenledlevel = IONode.get_input('in2')['event_data']['value']
    if (greenledlevel == "on"):
        MQTT.publish_event_to_client('esp32', 'G2:1')
    else:
        MQTT.publish_event_to_client('esp32', 'G2:0')
    log("green led level: " + str(greenledlevel))


