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

#include <stdio.h>
#include <unistd.h>
#include <string.h>

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

// Added header files for to implement udp task
#include "my_esp_ot_udp_socket.h"
#include "cc.h"
#include "esp_check.h"
#include "esp_netif_net_stack.h"
#include <sys/unistd.h>
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/mld6.h"
#include "lwip/sockets.h"
#include <stdlib.h>



#if CONFIG_OPENTHREAD_STATE_INDICATOR_ENABLE
#include "ot_led_strip.h"
#endif

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
#include "esp_ot_cli_extension.h"
#endif // CONFIG_OPENTHREAD_CLI_ESP_EXTENSION

#define TAG "ot_esp_cli"

// Define the parameter for udp_client
static UDP_CLIENT udp_client = {
    .exist = 1,
    .sock = -1,
    .local_port = 12345,
    .local_ipaddr = "::",
    .ifr = {{0}},
    .messagesend = {
        .port = 20617,
        .ipaddr = "fd40:e3e2:5852:4d1:a433:cd2c:20c8:fb4b",
        .simulated_data = 0,  // Simulated data to send
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

static void udp_client_send(UDP_CLIENT *udp_client_member)
{
    struct sockaddr_in6 dest_addr = {0};
    int len = 0;
    char simulated_data_str[32];  // buffer to hold temperature as string

    udp_client_member->messagesend.simulated_data = rand() % 50;    // generate random values

    sniprintf(simulated_data_str, sizeof(simulated_data_str), "UWB =%d", udp_client_member->messagesend.simulated_data);        // format the data as a string 

    inet6_aton(udp_client_member->messagesend.ipaddr, &dest_addr.sin6_addr);
    dest_addr.sin6_family = AF_INET6;
    dest_addr.sin6_port = htons(udp_client_member->messagesend.port);
    

    ESP_LOGI(OT_EXT_CLI_TAG, "Sending to %s : %d", udp_client_member->messagesend.ipaddr,
             udp_client_member->messagesend.port);
    esp_err_t err = socket_bind_interface(udp_client_member->sock, &(udp_client_member->ifr));
    ESP_RETURN_ON_FALSE(err == ESP_OK, , OT_EXT_CLI_TAG, "Stop sending message");

    // send the string of data as UDP payload
    len = sendto(udp_client_member->sock, simulated_data_str,
                 strlen(simulated_data_str), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
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
    xTaskCreate(udp_socket_client_task, "udp_client", 4096, &udp_client, 4, NULL);

}
