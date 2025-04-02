/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 #pragma once

 #include <stdint.h>
 #include <openthread/error.h>
 #include "lwip/sockets.h"
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 // Define constants for event bits
 #define UDP_CLIENT_SEND_BIT BIT0
 #define UDP_CLIENT_CLOSE_BIT BIT1
 #define UDP_SERVER_BIND_BIT BIT0
 #define UDP_SERVER_SEND_BIT BIT1
 #define UDP_SERVER_CLOSE_BIT BIT2

 /**
 * @brief User command "mcast" process.
 *
 */
otError esp_ot_process_mcast_group(void *aContext, uint8_t aArgsLength, char *aArgs[]);

otError esp_ot_process_udp_client(void *aContext, uint8_t aArgsLength, char *aArgs[]);
 
 // Structure for sending a message in UDP communication
 typedef struct send_message {
     int port;                  // Port number to send the message to
     char ipaddr[128];          // IPv6 address of the recipient
     char message[128];         // Message to send
 } SEND_MESSAGE;
 
 // Structure representing a UDP client

 typedef struct udp_client {
    int exist;
    int sock;
    int local_port;
    char local_ipaddr[128];
    char dest_ipaddr[64];    // Destination IP address (IPv6)
    uint16_t dest_port;     // Destination port number
    struct ifreq ifr;
    SEND_MESSAGE messagesend;
} UDP_CLIENT;
 
 // Structure representing a UDP server
 typedef struct udp_server {
     int exist;                 // Whether the server is active
     int sock;                  // Socket descriptor
     int local_port;            // Local port the server is bound to
     char local_ipaddr[128];    // Local IP address (IPv6)
     struct ifreq ifr;          // Interface request struct
     SEND_MESSAGE messagesend; // Send message information
 } UDP_SERVER;
 
 // Function declarations for handling UDP client operations
 
 /**
  * @brief User command "udpsockclient" process.
  *
  * This function processes UDP client commands such as opening a client,
  * sending messages, and closing the client.
  *
  * @param aContext      The context (unused in this case).
  * @param aArgsLength   The number of arguments passed to the command.
  * @param aArgs         The arguments for the command.
  *
  * @return              OT_ERROR_NONE if the command is processed successfully.
  */
 otError esp_ot_process_udp_client(void *aContext, uint8_t aArgsLength, char *aArgs[]);
 
 /**
  * @brief User command "udpsockserver" process.
  *
  * This function processes UDP server commands such as opening a server,
  * sending messages, and closing the server.
  *
  * @param aContext      The context (unused in this case).
  * @param aArgsLength   The number of arguments passed to the command.
  * @param aArgs         The arguments for the command.
  *
  * @return              OT_ERROR_NONE if the command is processed successfully.
  */
 otError esp_ot_process_udp_server(void *aContext, uint8_t aArgsLength, char *aArgs[]);
 
 /**
  * @brief Get the Interface name struct.
  *
  * This function retrieves the interface struct associated with a given interface name.
  *
  * @param name_input    The name of the Interface.
  * @param ifr           The interface name struct to be populated.
  *
  * @return              ESP_OK if the interface is retrieved successfully.
  *                      ESP_FAIL on failure.
  */
 esp_err_t socket_get_netif_impl_name(char *name_input, struct ifreq *ifr);
 
 /**
  * @brief Bind the socket to an interface.
  *
  * This function binds the socket to the specified interface.
  *
  * @param sock          The socket to bind.
  * @param ifr           The interface name struct.
  *
  * @return              ESP_OK on successful binding.
  *                      ESP_FAIL on failure.
  */
 esp_err_t socket_bind_interface(int sock, struct ifreq *ifr);
 
 // Function prototypes for internal tasks (used within esp_ot_udp_socket.c)
 
 /**
  * @brief Receives UDP messages in the client task.
  *
  * This task waits for and receives UDP messages, then processes them.
  *
  * @param pvParameters  Pointer to the UDP_CLIENT struct.
  */
 void udp_client_receive_task(void *pvParameters);
 
 /**
  * @brief Sends a message to the specified UDP address and port.
  *
  * This function sends a message to the provided IP address and port.
  *
  * @param udp_client_member   The UDP client to send the message from.
  */
 void udp_client_send(UDP_CLIENT *udp_client_member);
 
 /**
  * @brief Deletes a UDP client by shutting down the socket and releasing resources.
  *
  * @param udp_client_member   The UDP client to delete.
  */
 void udp_client_delete(UDP_CLIENT *udp_client_member);
 
 /**
  * @brief The main task for handling the UDP client lifecycle.
  *
  * This task creates a UDP client, binds the socket if necessary, and manages its lifecycle.
  *
  * @param pvParameters  Pointer to the UDP_CLIENT struct.
  */
 void udp_socket_client_task(void *pvParameters);
 
 #ifdef __cplusplus
 }
 #endif
    // End of header guard 
