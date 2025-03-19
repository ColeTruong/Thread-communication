
import time
import paho.mqtt.client as mqtt
import socket

# Define the UDP IP and port
UDP_IP = "**************"
UDP_PORT = 12345


# Callback when a message is successfully published
def on_publish(client, userdata, mid, reason_code=None, properties=None):
    try:
        # If we're using MQTT v3, reason_code and properties will be None
        if reason_code is None or properties is None:
            print(f"Message with MID {mid} was successfully published (MQTT v3).")
        else:
            print(f"Message with MID {mid} published. Reason code: {reason_code}, Properties: {properties}")
        
        userdata.remove(mid)
    except KeyError:
        print(f"Warning: MID {mid} not found in unacknowledged set.")

# Set of unacknowledged messages
unacked_publish = set()

# MQTT Client configuration (use MQTT v3 here)
mqttc = mqtt.Client()

# Set credentials for broker
mqttc.username_pw_set("username", "password")        # Set the username and password of your MQTT broker

# Assign callback for publish
mqttc.on_publish = on_publish

# Pass user data (unacked_publish) to the client
mqttc.user_data_set(unacked_publish)

# Connect to the broker
broker = "192.168.0.2"  # Change this if necessary
port = 1883
mqttc.connect(broker, port)

# Start the loop
mqttc.loop_start()

# Function to start the UDP server
def start_udp_server(UDP_IP, UDP_PORT):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # Create UDP socket
    sock.bind((UDP_IP, UDP_PORT))  # Bind to the IP and port

    while True:
        # Receive data from UDP
        data, addr = sock.recvfrom(1024)  # buffer size is 1024 bytes
        print(f"Received message: {data.decode()} from {addr}")
        
        # Publish the received UDP data to the MQTT broker
        msg_info = mqttc.publish("test/topic", data.decode(), qos=1)
        unacked_publish.add(msg_info.mid)  # Track the message MID
        print(f"Published UDP message to MQTT with MID: {msg_info.mid}")

# Start the UDP server
start_udp_server(UDP_IP, UDP_PORT)

# Wait for all messages to be acknowledged
while len(unacked_publish):
    print("Waiting for messages to be acknowledged...")
    time.sleep(0.1)

# Disconnect
mqttc.disconnect()
mqttc.loop_stop()
print("Disconnected from broker.")
