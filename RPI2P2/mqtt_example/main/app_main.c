/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#define SENSOR_NUMBER 40
#define THRESHOLD_ESTABLISHED 95

static const char *TAG = "mqtt_example_sensorization";

static esp_mqtt_client_handle_t client;
SemaphoreHandle_t xSensorListSemaphore;


// Sensor structure for ease of use
typedef struct sensor {
    char *id; // Sensor identifier
    bool is_enabled; // Sensor is (or isn't) capturing data
    int interval; // Time to capture data
} sensor;

static const char* sensors[4] = {"TEMP", "HUM", "LUX", "VIBR"};

static sensor sensor_list[SENSOR_NUMBER];

int delay_time = 10;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

// Get the last word of the topic to identify commands
char * last_word(const  char* topic){
    char* last = strrchr(topic, '/');
    if (last != NULL) {
        return last + 1;
    }
    return (char*)topic; 
}

char* all_but_last_word(const char* complete_topic, const char* last_word) {
    size_t complete_topic_len = strlen(complete_topic);
    size_t last_word_len = strlen(last_word)+1;
    char* new_topic = malloc(complete_topic_len + 1); // +1 for null terminator
    if (new_topic == NULL) {
        return NULL; // Handle memory allocation error
    }
    strncpy(new_topic, complete_topic, complete_topic_len - last_word_len);
    new_topic[complete_topic_len - last_word_len] = '\0';
    return new_topic;
}


// Function to initialize the array of sensors
static void sensor_init (sensor* list) {
    char building[] = "EDIFICIO_Pto0903";
    char* floors[] = {"0", "1", "2", "3", "4"};
    char* zones[] = {"N", "S", "E", "W"};
    char* rooms[] = {"0", "1"};
    int root_topic_enumerator = 0;
    for (int i_floor = 0; i_floor < sizeof(floors)/sizeof(floors[0]); i_floor++){
        for (int i_zone = 0; i_zone < sizeof(zones)/sizeof(zones[0]); i_zone++){
            for (int i_room = 0; i_room < sizeof(rooms)/sizeof(rooms[0]); i_room++){
                sensor localized_sensor; // Initialize the structure               
                localized_sensor.id = malloc(strlen(building) + strlen(floors[i_floor]) + strlen(zones[i_zone]) + strlen(rooms[i_room]) + 4); // +4 for slashes and null terminator
                localized_sensor.interval = 10000;
                localized_sensor.is_enabled = (bool) 1;
                // Concatenate the parts
                strcpy(localized_sensor.id, building);
                strcat(localized_sensor.id, "/");
                strcat(localized_sensor.id, floors[i_floor]);
                strcat(localized_sensor.id, "/");
                strcat(localized_sensor.id, zones[i_zone]);
                strcat(localized_sensor.id, "/");
                strcat(localized_sensor.id, rooms[i_room]);

                list[root_topic_enumerator] = localized_sensor;
                root_topic_enumerator++; 
            } 
        }
    }
}
static void data_controller(char* topic, int topic_len, char* data, int data_len){
    
    char topic_buffer[topic_len + 1]; // +1 for null terminator
    char data_buffer[data_len + 1];   // +1 for null terminator

    strncpy(topic_buffer, topic, topic_len);
    strncpy(data_buffer, data, data_len);

    topic_buffer[topic_len] = '\0';
    data_buffer[data_len] = '\0';

    char* l_w = last_word(topic_buffer);
    char* real_topic = all_but_last_word(topic_buffer, l_w);

    int i = 0;
    for (i = 0; i < SENSOR_NUMBER; i++){
        if (strcmp(sensor_list[i].id, real_topic)==0){
            break;
        }
    }
    if (i < SENSOR_NUMBER){
        if (strcmp(l_w, "interval") == 0) {
            xSemaphoreTake(xSensorListSemaphore, portMAX_DELAY); // Take the semaphore
            sensor_list[i].interval = atoi(data_buffer);
            ESP_LOGI(TAG, "Established sensor %s delay to %s", topic_buffer, data_buffer);
            xSemaphoreGive(xSensorListSemaphore); // Release the semaphore
        } else if (strcmp(l_w, "enable") == 0) {
            xSemaphoreTake(xSensorListSemaphore, portMAX_DELAY);
            sensor_list[i].is_enabled = true;
            ESP_LOGI(TAG, "Sensor %s enabled", real_topic);
            xSemaphoreGive(xSensorListSemaphore);
        } else if (strcmp(l_w, "disable") == 0) {
            xSemaphoreTake(xSensorListSemaphore, portMAX_DELAY);
            sensor_list[i].is_enabled = false;
            ESP_LOGI(TAG, "Sensor %s disabled", real_topic);
            xSemaphoreGive(xSensorListSemaphore);
        } else {
            if (atoi(data_buffer) > THRESHOLD_ESTABLISHED){
                ESP_LOGI(TAG, "ANOMALOUS VALUE %s DETECTED IN TOPIC %s", data_buffer, topic_buffer);
            }
        }
    }
    free(real_topic);
}


// Generalized task
void sensor_task(void* param) {
    int i = (int) param;
    char zzz[4];
    char full_topic[30];
    while(1) {
        if (sensor_list[i].is_enabled){
            for (int j = 0; j<sizeof(sensors)/sizeof(sensors[0]); j++){
                sprintf(full_topic, "%s/%s", sensor_list[i].id, sensors[j]);
                srand(time(NULL));
                sprintf(zzz, "%d", rand()%100);
                esp_mqtt_client_publish(client, full_topic, zzz , 0, 1, 0); 
            }
        }
        vTaskDelay(pdMS_TO_TICKS(sensor_list[i].interval));
    }
}



/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        esp_mqtt_client_subscribe(client, "EDIFICIO_Pto0903/#", 0);
        ESP_LOGI(TAG, "All events subscribed");        
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        data_controller(event->topic, event->topic_len, event->data, event->data_len);
        break;
    case MQTT_EVENT_PUBLISHED:
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .session.last_will.topic = "/disconnected",  // LWT message topic
        .session.last_will.msg = "Client offline",  // LWT message
        .session.last_will.qos = 0,  // Quality of Service for LWT message
        .session.last_will.retain = 0,  // Retain LWT message
        .session.last_will.msg_len = 0,  // Length of LWT message (0 for auto-detect)
    };

#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    sensor_init(sensor_list);

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */ 
    ESP_ERROR_CHECK(example_connect());
    mqtt_app_start();

    xSensorListSemaphore = xSemaphoreCreateMutex();
    if (xSensorListSemaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        abort(); // Or handle the error in some other way
    }

    for (int i = 0; i < SENSOR_NUMBER; i++) {
        xTaskCreate(sensor_task, sensor_list[i].id, 2048, (void*)i, 5, NULL);
    }
}