# Thread-communication

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
15. After the ESP-IDF Monitor shows up, type Help and the list of commands will show up: 









References: 
https://github.com/openthread/openthread/blob/main/src/cli/README.md
