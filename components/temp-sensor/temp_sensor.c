#include <stdio.h>
#include <math.h>
#include <string.h>

#include "stdatomic.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "unistd.h"

#include "owb.h"
#include "owb_rmt.h"
#include "ds18b20.h"

#include "./temp_sensor.h"


#define MAX_DEVICES 10
#define DS18B20_RESOLUTION 12
#define STACK_SIZE 5048


static const char* TAG = "temp-sensor";


typedef struct {
  owb_rmt_driver_info driver_info;
  OneWireBus * bus;
  uint8_t num_devices;
  DS18B20_Info * devices[10];
} Sensors;


static double exp_weighted_moving_avg(double prev_value, double value, double alpha) {
  return ((1.0-alpha) * prev_value) + (alpha * value);
}


static int find_devices(OneWireBus *owb, OneWireBus_ROMCode device_rom_codes[]) {
  ESP_LOGI(TAG, "finding devices");

  int num_devices = 0;

  OneWireBus_SearchState search_state = {0};
  bool found = false;

  owb_search_first(owb, &search_state, &found);

  while (found) {
    char rom_code_s[17];
    owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));
    ESP_LOGI(TAG, " %d : %s", num_devices, rom_code_s);

    device_rom_codes[num_devices] = search_state.rom_code;
    ++num_devices;
    owb_search_next(owb, &search_state, &found);
  }

  if (num_devices == 1) {
    // For a single device only:
    OneWireBus_ROMCode rom_code;
    owb_status status = owb_read_rom(owb, &rom_code);

    if (status == OWB_STATUS_OK) {
      char rom_code_s[OWB_ROM_CODE_STRING_LENGTH];
      owb_string_from_rom_code(rom_code, rom_code_s, sizeof(rom_code_s));
      ESP_LOGI(TAG, "Single device %s present", rom_code_s);
    } else {
      ESP_LOGE(TAG, "An error occurred reading device ROM code: %d", status);
    }
  }

  ESP_LOGI(TAG, "found %d devices", num_devices);
  return num_devices;
}


static void sensors_init(Sensors * sensors, uint8_t gpio) {
  ESP_LOGI(TAG, "setting up OneWire on GPIO %d", gpio);

  // Create a 1-Wire bus, using the RMT timeslot driver
  sensors->bus = owb_rmt_initialize(
    &(sensors->driver_info), gpio,
    RMT_CHANNEL_1, RMT_CHANNEL_0
  );
  owb_use_crc(sensors->bus, true);  // enable CRC check for ROM code

  OneWireBus_ROMCode device_rom_codes[MAX_DEVICES] = {0};

  int num_devices = find_devices(sensors->bus, device_rom_codes);

  for (int i = 0; i < num_devices; ++i) {
    DS18B20_Info * ds18b20_info = ds18b20_malloc();
    sensors->devices[i] = ds18b20_info;

    if (num_devices == 1) {
      ESP_LOGI(TAG, "Single device optimisations enabled");
      ds18b20_init_solo(ds18b20_info, sensors->bus);
    } else {
      ds18b20_init(ds18b20_info, sensors->bus, device_rom_codes[i]);
    }
    ds18b20_use_crc(ds18b20_info, true);
    ds18b20_set_resolution(ds18b20_info, DS18B20_RESOLUTION);
  }

  ESP_LOGI(TAG, "sensors initialized");
}


static float read_temperature(DS18B20_Info * device) {
  float reading = 0;

  ds18b20_convert_all(device->bus);
  ds18b20_wait_for_conversion(device);
  DS18B20_ERROR err = ds18b20_read_temp(device, &reading);

  if (err == DS18B20_OK) {
    return reading;
  }

  ESP_LOGE(TAG, "error reading temp %u", err);
  return 100;
}


static void atomic_store_float(atomic_uint_fast32_t* dest, float value) {
  const uint32_t store_value = *(uint32_t *)&value;
  atomic_store(dest, store_value);
}


static float atomic_load_float(atomic_uint_fast32_t* src) {
  const uint32_t value = atomic_load(src);
  return *(float*)&value;
}


static void temp_task(void * arg) {
  temp_sensor_t * temp_sensor = (temp_sensor_t *) arg;

  Sensors sensors;
  sensors_init(&sensors, temp_sensor->gpio_num);

  double avg_temp = 18.0;
  double alpha = 0.2;


  while(true) {
    TickType_t start_time = xTaskGetTickCount();

    const float temp = read_temperature(sensors.devices[0]);

    avg_temp = exp_weighted_moving_avg(avg_temp, temp, alpha);

    atomic_store_float(&(temp_sensor->curr_temp), avg_temp);

    const TickType_t end_time = xTaskGetTickCount();
    const float duration = (end_time-start_time) * portTICK_PERIOD_MS;

    ESP_LOGI(TAG, "read temp %0.2f (avg %0.2f) in %f ms", temp, avg_temp, duration);

    vTaskDelayUntil(&start_time, temp_sensor->read_interval / portTICK_PERIOD_MS);
  }
}


double get_temperature(temp_sensor_t * temp_sensor) {
  return atomic_load_float(&(temp_sensor->curr_temp));
}


temp_sensor_t * start_temp_sensors(gpio_num_t gpio_num, uint64_t read_interval) {
  ESP_LOGI(TAG, "starting temp-sensors task ...");

  temp_sensor_t * temp_sensor = malloc(sizeof(temp_sensor_t));
  *temp_sensor = (temp_sensor_t) {
    .gpio_num = gpio_num,
    .read_interval = read_interval
  };
  xTaskCreate(temp_task, "temp-sensor", STACK_SIZE, temp_sensor, ESP_TASK_MAIN_PRIO, &temp_sensor->task_handle);

  ESP_LOGI(TAG, "started temp-sensors task");
  return temp_sensor;
}


void stop_temp_sensors(temp_sensor_t * temp_sensor) {
  ESP_LOGI(TAG, "stopping temp-sensors ...");

  // TODO: free up
  // ds18b20_free(&devices[i]);
  // owb_uninitialize(owb);

  vTaskDelete(temp_sensor->task_handle);
  ESP_LOGI(TAG, "stopped temp-sensors");
}


