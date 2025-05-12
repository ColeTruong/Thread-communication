/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>

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
#include "openthread/cli.h"
#include "openthread/instance.h"
#include "openthread/logging.h"
#include "openthread/tasklet.h"
#include "openthread/thread.h"
#include "esp_ot_udp_socket.h"

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
#include "esp_ot_cli_extension.h"
#endif // CONFIG_OPENTHREAD_CLI_ESP_EXTENSION

#define TAG "ot_esp_cli"

esp_netif_t *g_openthread_netif = NULL;  // Global variable to store the OpenThread network interface


static esp_netif_t *init_openthread_netif(const esp_openthread_platform_config_t *config)
{
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    esp_netif_t *netif = esp_netif_new(&cfg);
    assert(netif != NULL);
    ESP_ERROR_CHECK(esp_netif_attach(netif, esp_openthread_netif_glue_init(config)));

    return netif;
}
static void ot_udp_function(void *pvParameters)
{
    // Access the global variable for the OpenThread netif
    if (g_openthread_netif == NULL) {
        ESP_LOGE(OT_EXT_CLI_TAG, "OpenThread netif is not initialized.");
        vTaskDelete(NULL);
        return;
    }
    // Now you can use g_openthread_netif for socket binding or other operations
    ESP_LOGI(OT_EXT_CLI_TAG, "Using netif for binding: %s", esp_netif_get_ifkey(g_openthread_netif));

    UDP_CLIENT udp_client = {
        .exist = 1,
        .sock = -1,
        .local_port = 12345,
        .local_ipaddr = "::",
        .ifr = {0},
        .messagesend = {
            .port = 20617,
            .ipaddr = "fd81:cc38:f9ed:2df4:bbc:b4ca:145d:3d",
            .message = "HelloFromESP32",
        },
    };

    udp_socket_client_task(&udp_client); // Passing only udp_client
    vTaskDelete(NULL); // Delete the task after completion
}


static void ot_task_worker(void *aContext)
{
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    // Initialize the OpenThread stack
    ESP_ERROR_CHECK(esp_openthread_init(&config));

#if CONFIG_OPENTHREAD_LOG_LEVEL_DYNAMIC
    // The OpenThread log level directly matches ESP log level
    (void)otLoggingSetLevel(CONFIG_LOG_DEFAULT_LEVEL);
#endif
    // Initialize the OpenThread cli
    esp_openthread_cli_init();

    esp_netif_t *openthread_netif;
    // Initialize the esp_netif bindings
    openthread_netif = init_openthread_netif(&config);

    // Save the network interface in the global variable
    g_openthread_netif = openthread_netif;

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
    esp_cli_custom_command_init();
#endif // CONFIG_OPENTHREAD_CLI_ESP_EXTENSION

    // Run the main loop
    esp_openthread_cli_create_task();
    esp_openthread_launch_mainloop();



    // Declare the OpenThread instance
    otInstance *otInstance;

    // Initialize the OpenThread instance
    otInstance = otInstanceInitSingle();

    while (otThreadGetDeviceRole(&otInstance) == OT_DEVICE_ROLE_DETACHED) {
    // The device is still detached from the network. You can log this or add a delay
    ESP_LOGI(TAG, "Waiting for Thread network join...");
    vTaskDelay(pdMS_TO_TICKS(100)); // Delay to avoid high CPU usage in the loop
    }

// Now you can create the UDP client task to send messages
    xTaskCreate(ot_udp_function, "ot_udp_task", 2048, NULL, 5, NULL);

    esp_vfs_eventfd_unregister();
    vTaskDelete(NULL);

    // Clean up
    esp_netif_destroy(openthread_netif);
    esp_openthread_netif_glue_deinit();
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

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));
    xTaskCreate(ot_task_worker, "ot_cli_main", 10240, xTaskGetCurrentTaskHandle(), 5, NULL);
}
