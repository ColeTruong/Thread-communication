# Thread-communication

This repository contains an OpenThread UDP communication example based on the OpenThread CLI example from ESP-IDF. The device is expected to send data via UDP to IPv6 address of the Thread BR.

Requirements:
- ESP32-C6
- Micro-USB cable
- ESP-IDF v5.3.1 at least



Setup

This guide is to help people start with setting up the ESP-C6 integrated to the existing thread network.  
1. Install VSCode
2. Install ESP-IDF extension from VSCode Extension
3. From the command list within ESP-IDF, choose the Express Installation.
4. The option could be Github for download server and choose whaterver version that fits your microcontroller.
5. Connect the ESP32-C6 or ESP32-H2 with your computer via UART port or USB port (I used ESP32-C6 in this tutourial since it also implement Wi-Fi and BLuetooth 5 (LE)) with the USB cable.
6. Open ESP-IDF Extension (left conrner on the sidebar with the Extensions option)
7. Find the example ot_cli and load them to your ESP32.
8. Select Flash method: Jtag
9. Select the correct COM port
10. Set Espressif Device Target: esp32c6
11. Go to SDK Configuration Editor and we could configure the Thread Operational Dataset so that the device automatically connect to the existing Thread network
12. Search USB -> Component config -> Enable USB-Serial-JTAG Module and USB Serial/JTAG Controller for console output (furthermore you will be allowed to type the commands via the CLI from the terminal)
13. Select Build project -> Flash Device -> Monitor Device

If you have completed all the steps above, you finish the first step of setting up your ESP32-C6
After the ESP-IDF Monitor shows up, type Help and the list of commands will show up (Check the first reference for more information)

First step is to connect Thread network, in this case I already had my Thread Border Router setup so I would advise to go straight to OpenThread website about how to form a Thread network with Thread Border Router. (Second reference for more information).       

I got the networkkey, pskc and dataset from the Thread Border Router (furthere instruction in OpenThread website)

Then you go to OT CLI via the Device Monitor in ESP-IDF and start to connect ESP-C6 to Thread network:

For networkkey connection:
1. Type "dataset networkkey 731ab6a60a64a0a0b14b259b86b2be01"  (change it according to your networkkey)
2. Type "dataset commit active"
3. Type "ifconfig up"
4. Type "thread start" then it should show up like this:

> I(23058) OPENTHREAD:[N] Mle-----------: Role disabled -> detached

> Done

> I(23408) OPENTHREAD:[N] Mle-----------: Attach attempt 1, AnyPartition reattaching with Active Dataset

> I(30028) OPENTHREAD:[N] Mle-----------: Attach attempt 1 unsuccessful, will try again in 0.288 seconds

> I(30328) OPENTHREAD:[N] Mle-----------: Attach attempt 2, AnyPartition 

> I(33498) OPENTHREAD:[N] Mle-----------: Delay processing Announce - channel 21, panid 0x1444

> I(33758) OPENTHREAD:[N] Mle-----------: Processing Announce - channel 21, panid 0x1444

> I(33758) OPENTHREAD:[N] Mle-----------: Role detached -> disabled

> I(33758) OPENTHREAD:[N] Mle-----------: Role disabled -> detached

> I(34178) OPENTHREAD:[N] Mle-----------: Attach attempt 1, AnyPartition 

> I(35068) OPENTHREAD:[N] Mle-----------: RLOC16 fffe -> 2c01

> I(35068) OPENTHREAD:[N] Mle-----------: Role detached -> child

6. After a moment, type "state" check the device state. It should be the Child:

> state

> child

> Done

7. Set the state to Router by typing "state router"
8. Sucessfully connect the ESP to the Thread network



For secure commissoning: 
1. Type "pskc 54e7f18d2575014da94db09df29c5df0" (change it according to your PSKc)
2. Type "dataset panid 0x1444" (change it according to your panid)
3. The rest of the steps are the same with the networkkey connection

I got the networkkey, pskc and dataset from the Thread Border Router (furthere instruction in OpenThread website)

Next step is to send UDP packages via CLI to the UDP listener/server on my laptop.
1. Create the UDP server on your laptop
2. Find the IP addresses of the laptop (either IPv6 or IPv4 addresses)
3. Type: udp open -> udp send fdde:ad00:beef:0:bb1:ebd6:ad10:f33 1234 hello   (Thread is based on IPv6 communication but thanks to NAT64 prefix, IPv4 addresses will be automatically converted to IPv6 addresses so we could save it later for the project)
4. Then it will show up "hello" on the UDP listener on your VSCode.

** Notes **

Although the NAT64 prefix is available via the OpenThread CLI, the ESP-IDF networking stack currently supports only IPv6 for UDP communication. Consequently, it is not possible to send UDP messages directly to IPv4-only servers. Furthermore, due to the local topology of Wi-Fi networks, non-Thread devices (such as UDP servers on standard Wi-Fi) are typically unable to receive packets from Thread nodes over IPv6. This is because public Wi-Fi networks often support IPv6 communication only within the local link and do not provide proper routing or NAT64 translation for packets originating from Thread networks. As a result, while the code can successfully send UDP packets to other devices within the Thread network, it cannot deliver them to external IPv4-based servers.




   










References: 

https://github.com/openthread/openthread/blob/main/src/cli/README.md 

https://openthread.io/codelabs/esp-openthread-hardware#4

https://github.com/openthread/openthread/blob/main/src/cli/README_UDP.md

https://pypi.org/project/paho-mqtt/
