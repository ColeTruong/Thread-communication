/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * OpenThread Command Line Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
*/

// Standard lirbaries in ESP-IDF
#include <stdio.h>
#include <unistd.h>
#include <string.h>

// Libraries needed for OpenThread inilitation

#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "esp_openthread.h"
#include "esp_openthread_cli.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_openthread_types.h"
#include "esp_ot_config.h"
#include "esp_vfs_eventfd.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/uart_types.h"
#include "nvs_flash.h"
#include "openthread/cli.h"
#include "openthread/instance.h"
#include "openthread/logging.h"
#include "openthread/tasklet.h"

// Libraries for OpenThread

#include "esp_ot_udp_socket.h"
#include "cc.h"
#include "esp_check.h"
#include "esp_netif_net_stack.h"
#include <sys/unistd.h>
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/mld6.h"
#include "lwip/sockets.h"

// Libraries for BLE scanner

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "nvs_flash.h"


#define BLE_TAG "BLE_SCANNER"   // Define name of BLE scanner to debugging logs
#define MANUFACTURER_ID 0xFFFF  // Replace with your actual UWB manufacturer ID if different

extern UDP_CLIENT udp_client; // Access global from BLE task

#if CONFIG_OPENTHREAD_STATE_INDICATOR_ENABLE
#include "ot_led_strip.h"
#endif

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
#include "esp_ot_cli_extension.h"
#endif // CONFIG_OPENTHREAD_CLI_ESP_EXTENSION

#define TAG "ot_esp_cli"

// Function for UDP client with the message updated from BLE scanner

static UDP_CLIENT udp_client = {
    .exist = 1,
    .sock = -1,
    .local_port = 12345,
    .local_ipaddr = "::",
    .ifr = {{0}},
    .messagesend = {
        .port = 20617,                                            // Destination port address
        .ipaddr = "fd40:e3e2:5852:4d1:a433:cd2c:20c8:fb4b",       // Destination IPv6 addresss
        .message = "", 
    },
};



static esp_netif_t *init_openthread_netif(const esp_openthread_platform_config_t *config)
{
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    esp_netif_t *netif = esp_netif_new(&cfg);
    assert(netif != NULL);
    ESP_ERROR_CHECK(esp_netif_attach(netif, esp_openthread_netif_glue_init(config)));

    return netif;
}

// Function to initialise Thread network

static void ot_task_worker(void *aContext)
{
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    // Initialize the OpenThread stack
    ESP_ERROR_CHECK(esp_openthread_init(&config));

#if CONFIG_OPENTHREAD_STATE_INDICATOR_ENABLE
    ESP_ERROR_CHECK(esp_openthread_state_indicator_init(esp_openthread_get_instance()));
#endif

#if CONFIG_OPENTHREAD_LOG_LEVEL_DYNAMIC
    // The OpenThread log level directly matches ESP log level
    (void)otLoggingSetLevel(CONFIG_LOG_DEFAULT_LEVEL);
#endif
    // Initialize the OpenThread cli
#if CONFIG_OPENTHREAD_CLI
    esp_openthread_cli_init();
#endif

    esp_netif_t *openthread_netif;
    // Initialize the esp_netif bindings
    openthread_netif = init_openthread_netif(&config);
    esp_netif_set_default_netif(openthread_netif);

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
    esp_cli_custom_command_init();
#endif // CONFIG_OPENTHREAD_CLI_ESP_EXTENSION

    // Run the main loop
#if CONFIG_OPENTHREAD_CLI
    esp_openthread_cli_create_task();
#endif
#if CONFIG_OPENTHREAD_AUTO_START
    otOperationalDatasetTlvs dataset;
    otError error = otDatasetGetActiveTlvs(esp_openthread_get_instance(), &dataset);
    ESP_ERROR_CHECK(esp_openthread_auto_start((error == OT_ERROR_NONE) ? &dataset : NULL));
#endif
    esp_openthread_launch_mainloop();

    // Clean up
    esp_openthread_netif_glue_deinit();
    esp_netif_destroy(openthread_netif);

    esp_vfs_eventfd_unregister();
    vTaskDelete(NULL);
}

// Function to send a UDP message using IPv6 address over a Thread network

static void udp_client_send(UDP_CLIENT *udp_client_member)
{
    struct sockaddr_in6 dest_addr = {0};    // IPv6 destination address structure
    int len = 0;                            // Length of the sent message

    // Convert the destination IP address (string) to binary format
    inet6_aton(udp_client_member->messagesend.ipaddr, &dest_addr.sin6_addr);
    dest_addr.sin6_family = AF_INET6;    // Set address family to IPv6
    dest_addr.sin6_port = htons(udp_client_member->messagesend.port);    // Set destination port in network byte order
    // Log the destination IP and port
    ESP_LOGI(OT_EXT_CLI_TAG, "Sending to %s : %d", udp_client_member->messagesend.ipaddr,
             udp_client_member->messagesend.port);
    // Bind the socket to the specified network interface (Thread in this case)
    esp_err_t err = socket_bind_interface(udp_client_member->sock, &(udp_client_member->ifr));
    // If the binding fails, return immediately and log the issue
    ESP_RETURN_ON_FALSE(err == ESP_OK, , OT_EXT_CLI_TAG, "Stop sending message");
    // Send the message using sendto() to the destination address
    len = sendto(udp_client_member->sock, udp_client_member->messagesend.message,
                 strlen(udp_client_member->messagesend.message), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    // Check if sending failed
    if (len < 0) {
        ESP_LOGW(OT_EXT_CLI_TAG, "Fail to send message");
    }
}



static void udp_socket_client_task(void *pvParameters)
{
    UDP_CLIENT *udp_client_member = (UDP_CLIENT *)pvParameters;

    esp_err_t ret = ESP_OK;
    int err = 0;
    int sock = -1;
    int err_flag = 0;
    struct sockaddr_in6 bind_addr = {0};

    sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_IPV6);
    ESP_GOTO_ON_FALSE((sock >= 0), ESP_FAIL, exit, OT_EXT_CLI_TAG, "Unable to create socket: errno %d", errno);
    ESP_LOGI(OT_EXT_CLI_TAG, "Socket created");
    udp_client_member->sock = sock;
    err_flag = 1;

    // Restore binding the socket to the local IP/port for UDP sending source
    if (udp_client_member->local_port != -1) {
        inet6_aton(udp_client_member->local_ipaddr, &bind_addr.sin6_addr);
        bind_addr.sin6_family = AF_INET6;
        bind_addr.sin6_port = htons(udp_client_member->local_port);

        err = bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
        ESP_GOTO_ON_FALSE((err == 0), ESP_FAIL, exit, OT_EXT_CLI_TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGI(OT_EXT_CLI_TAG, "Socket bound, port %d", udp_client_member->local_port);
    }

    udp_client_member->exist = 1;
    ESP_LOGI(OT_EXT_CLI_TAG, "Successfully created");

    otInstance *instance = esp_openthread_get_instance();
    while (true) {
        otDeviceRole role = otThreadGetDeviceRole(instance);
        if (role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_ROUTER || role == OT_DEVICE_ROLE_LEADER) {
            ESP_LOGI(OT_EXT_CLI_TAG, "Device joined Thread network with role: %d", role);
            break;
        }
    ESP_LOGI(OT_EXT_CLI_TAG, "Waiting for Thread join...");
    vTaskDelay(pdMS_TO_TICKS(500));  // Check every 500ms
}


    while (true) {
        // No event wait: just continuously send UDP packets (or you can add delay here)
        udp_client_send(udp_client_member);
        vTaskDelay(pdMS_TO_TICKS(1000));  // for example, send every 1 second
    }

exit:
    if (ret != ESP_OK) {
        if (err_flag) {
            udp_client_member->sock = -1;
            shutdown(sock, 0);
            close(sock);
        }
        udp_client_member->local_port = -1;
        ESP_LOGI(OT_EXT_CLI_TAG, "Fail to create a UDP client");
    }
    vTaskDelete(NULL);
}

// BLE GAP callback function to process scan results

static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (event == ESP_GAP_BLE_SCAN_RESULT_EVT) {
        esp_ble_gap_cb_param_t *scan_result = param;
        if (scan_result->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
            uint8_t *adv_data = scan_result->scan_rst.ble_adv;
            uint8_t adv_len = scan_result->scan_rst.adv_data_len;

            for (int i = 0; i < adv_len;) {
                uint8_t field_len = adv_data[i];
                if (field_len == 0) break;
                uint8_t field_type = adv_data[i + 1];

                // Manufacturer Specific Data
                if (field_type == ESP_BLE_AD_TYPE_MANUFACTURER_SPECIFIC_TYPE && field_len > 2) {
                    uint16_t mfg_id = adv_data[i + 2] | (adv_data[i + 3] << 8);
                    if (mfg_id == MANUFACTURER_ID) {
                        // Extract string from remaining data
                        char distance_str[64] = {0};
                        memcpy(distance_str, &adv_data[i + 4], field_len - 3);
                        distance_str[field_len - 3] = '\0';  // Ensure null-termination

                        ESP_LOGI(BLE_TAG, "Received distance: %s", distance_str);

                        // Update UDP message buffer
                        strncpy(udp_client.messagesend.message, distance_str, sizeof(udp_client.messagesend.message) - 1);
                        udp_client.messagesend.message[sizeof(udp_client.messagesend.message) - 1] = '\0';
                    }
                }
                i += field_len + 1;
            }
        }
    }
}

// BLE scanner task: initialises BLE stack and starts continuous scanning

static void ble_scanner_task(void *arg)
{
    esp_err_t ret;

     // Initialise the Bluetooth controller with default configuration
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    ESP_ERROR_CHECK(ret);     // Halt if initialisation fails

    // Enable the controller in BLE-only mode
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    ESP_ERROR_CHECK(ret);        // Halt if enabling fails

    // Initialise the Bluedroid Bluetooth stack (required by ESP BLE APIs)
    ret = esp_bluedroid_init();
    ESP_ERROR_CHECK(ret);

    // Enable the Bluedroid stack
    ret = esp_bluedroid_enable();
    ESP_ERROR_CHECK(ret);

    // Register the BLE GAP event handler (our custom callback function)
    ret = esp_ble_gap_register_callback(gap_cb);
    ESP_ERROR_CHECK(ret);

    // Configure BLE scanning parameters
    esp_ble_gap_set_scan_params_t scan_params = {
        .scan_type = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0x50,
        .scan_window = 0x30,
        .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE
    };
    // Set the configured scan parameters
    ret = esp_ble_gap_set_scan_params(&scan_params);
    ESP_ERROR_CHECK(ret);

    // Start BLE scanning indefinitely (duration = 0)
    esp_ble_gap_start_scanning(0); // Continuous scanning

    // Delete this task since scanning is now handled by the registered callback
    vTaskDelete(NULL); // Let GAP callback do the work
}

void app_main(void)
{
    // Used eventfds:
    // * netif
    // * ot task queue
    // * radio driver
    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = 3,
    };

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));
    xTaskCreate(ot_task_worker, "ot_cli_main", 10240, xTaskGetCurrentTaskHandle(), 5, NULL);
    xTaskCreate(udp_socket_client_task, "udp_client", 4096, &udp_client, 3, NULL);
    xTaskCreate(ble_scanner_task, "ble_scanner", 4096, NULL, 4, NULL);
}
