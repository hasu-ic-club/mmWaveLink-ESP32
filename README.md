# TI mmWaveLink Library running on ESP32 (RISC-V architecture) CPUs

## Introduce

TI's mmWaveLink library provides a comprehensive set of APIs for configuring and controlling the functional blocks of TI's xWR series mmWave radar devices through SPI on embedded platforms. Officially developed and maintained by Texas Instruments, it serves as the standard software interface for interacting with these radar devices.

Although mmWaveLink can be ported to ESP32 platforms by adapting its platform-dependent components to the ESP-IDF environment, including FreeRTOS, SPI, and GPIO drivers, the process involves several hardware-specific details and non-trivial integration challenges.

To simplify this process, we developed this ESP-IDF component to handle the platform-specific adaptations and integration. It allows developers to use TI's official mmWaveLink APIs directly on ESP32 chips, making it easier to configure and control TI's xWR series radar devices.

## Start to Work
### Prerequirements
- ESP32 Development Environment (ESP-IDF), version >= 6.0.2

### Hardware Configurations
You can connect ESP32 to xWR series radars by SPI and GPIO. 

For most ESP32s, the SPI interface can be assigned to ANY pin on the chip. You can connect `SCK`, `MOSI`, `MISO` and `CS#` to the SPI interface of xWR radar. Also, `RESET#` and `HOST_INT` pin are required. These two signals can be controlled by GPIO. Note you should make `HOST_INT` as input (w/ edge interrupt support) and `RESET#` as output (push-pull or open-drain).

### Integrate to Your Project

To integrate this component into your ESP32 project. You can simply clone this repository into `components` sub-folder in your ESP32's project root.

After this, you can add a `REQUIRES` to `idf_component_register` in the main application's `CMakeLists.txt`.

```cmake
idf_component_register(SRCS ...
                    INCLUDE_DIRS ...
                    REQUIRES
                    mmwavelink-esp32
                    esp_driver_gpio
                    esp_driver_spi)

```

We also add `esp_driver_gpio` and `esp_driver_spi` as requirement since you mostly need them.

Then, you can include following headers and start to use the APIs.

```c
#include <mmwavelink.h>
#include <mmwavelink_esp.h>
#include <mmwl_esp_freertos.h>
```

### Implement Interface Functions
You need to implement following functions to let mmWaveLink library work and connect to your real hardware.

```c
static rlInt32_t xwr_print(const rlInt8_t* format, ...);
static uint8_t get_xwr_irq_pin_lvl(uint8_t deviceId);
static void enable_host_irq(uint8_t deviceId);
static void enable_device(mmwl_esp_context_t* context, uint8_t deviceId);
static void disable_device(mmwl_esp_context_t* context, uint8_t deviceId);
static int32_t write_data(mmwl_esp_context_t* context, uint8_t deviceId, uint8_t* buffer, uint16_t length);
static int32_t read_data(mmwl_esp_context_t* context, uint8_t deviceId, uint8_t* buffer, uint16_t length);

static void pwup_done_cb(uint8_t deviceId, uint8_t* payload, uint16_t length);
static void rf_pwup_done_cb(uint8_t deviceId, uint8_t* payload, uint16_t length);
static void init_calib_done_cb(uint8_t deviceId, uint8_t* payload, uint16_t length);
static void frame_trig_rdy_cb(uint8_t deviceId, uint8_t* payload, uint16_t length);
```

These seems a huge set of function but don't worry, most of them are simple.

- `xwr_print` implement the debug printer, here is an example. *Note: The mmWaveLink library may call `xwr_print` in interrupt context. But the ESP32 do not support serial print in interrupt context, so we need to ignore these callings.*
```c
static rlInt32_t xwr_print(const rlInt8_t* format, ...)
{
    if (xPortInIsrContext())
    {
        return 0;
    }

    va_list args;
    va_start(args, format);

    esp_log_writev(
        ESP_LOG_INFO,
        RADAR_TAG,
        (const char *)format,
        args
    );

    va_end(args);
    return 0;
}
```

- `get_xwr_irq_pin_lvl` is a simple function to return the level of `HOST_INT` pin. `1` for high and `0` for low.
- `enable_host_irq` is to enable the corresponding `HOST_INT` pin's interrupt. You can write `gpio_intr_enable` in it.
- `enable_device` and `disable_device` is used to release and assert the `RESET#` to device. You can control the GPIO in it, `disable_device` should assert the RESET (0) and `enable_device` should release it (1). *Note: It is recommended to wait 10ms after release the reset and wait 20us after assert the reset.*
- `write_data` and `read_data` is the transfer function on SPI interface. TI's SPI timing is very serious. We must following the manaual provided by TI official or the SPI transaction may not be recognized. TI specifies that SPI transfers must be performed in 16-bit (2-byte) units, meaning the total transfer length must be an even number of bytes. Data must be transmitted in big-endian order (MSB first). Additionally, `CS#` may only be deasserted at 16-bit boundaries. Here is an example to implement `write_data` and `read_data` by manually control the `CS#` by GPIO.
```c
static __always_inline esp_err_t xwr_spi_transfer16(const uint8_t* tx, uint8_t* rx)
{
    spi_transaction_t trans = {0};
    trans.length = 16;

    if (tx != nullptr)
    {
        trans.flags |= SPI_TRANS_USE_TXDATA;
        trans.tx_data[0] = tx[1];
        trans.tx_data[1] = tx[0];
    }

    if (rx != nullptr)
    {
        trans.flags |= SPI_TRANS_USE_RXDATA;
    }

    gpio_set_level(CONFIG_XWR_SPI_CSN_GPIO, 0);
    esp_rom_delay_us(XWR_SPI_CS_DELAY_US);
    const esp_err_t ret = spi_device_polling_transmit(xwr_spi, &trans);
    gpio_set_level(CONFIG_XWR_SPI_CSN_GPIO, 1);

    if (ret != ESP_OK) return ret;

    if (rx != NULL)
    {
        rx[0] = trans.rx_data[1];
        rx[1] = trans.rx_data[0];
    }

    return ESP_OK;
}

static int32_t write_data(mmwl_esp_context_t* context, uint8_t deviceId, uint8_t* buffer, uint16_t length)
{
    if (deviceId != 0 || buffer == nullptr || (length & 1U)) return -1;

    if (xSemaphoreTake(xwr_spi_mutex, portMAX_DELAY) != pdTRUE)
    {
        return -1;
    }

    esp_err_t ret = ESP_OK;

    esp_rom_delay_us(50);

    for (uint16_t bid = 0; bid < length; bid += 2)
    {
        ret = xwr_spi_transfer16(&buffer[bid], nullptr);

        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "xwr_spi_transfer16 failed: %x", ret);
            break;
        }
    }

    xSemaphoreGive(xwr_spi_mutex);
    return (ret == ESP_OK) ? (int32_t)length : -1;
}

static int32_t read_data(mmwl_esp_context_t* context, uint8_t deviceId, uint8_t* buffer, uint16_t length)
{
    if (deviceId != 0 || buffer == nullptr || (length & 1U)) return -1;

    if (xSemaphoreTake(xwr_spi_mutex, portMAX_DELAY) != pdTRUE)
    {
        return -1;
    }

    const uint8_t dummy[2] = {0xFF, 0xFF};
    esp_err_t ret = ESP_OK;

    for (uint16_t bid = 0; bid < length; bid += 2) {
        ret = xwr_spi_transfer16(dummy, &buffer[bid]);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI read failed at offset %u: %s",
                     bid, esp_err_to_name(ret));
            break;
        }
    }

    xSemaphoreGive(xwr_spi_mutex);
    return (ret == ESP_OK) ? (int32_t)length : -1;
}
```

- `pwup_done_cb`, `rf_pwup_done_cb`, `init_calib_done_cb` and `frame_trig_rdy_cb` are callback functions calling when asynchronous events. You can give semaphore or notify in these callbacks to let application to process forward.

### Register the Functions
After you implemented the functions, you should register them to the library. Here is an example to do this. *Note: `mmwl_handle` MUST be registered by the user exactly ONCE and MUST
remain a SINGLETON. The library supports only one `mmwl_handle` instance;
registering additional handles is not supported.*

```c
static mmwl_esp_context_t mmwl_handle;

static void init_mmwl_handle()
{
    // Initialize the worker spawn task for mmwavelink driver
    mmwl_esp_worker_spawn_task_init();

    // Create mmwavelink handle
    mmwl_handle.ack_timeout_ms = 100;                 // Set 100 ms timeout
    mmwl_handle.default_crc_type = RL_CRC_TYPE_32BIT; // Use CRC-32
    mmwl_handle.dbg_level = RL_DBG_LEVEL_WARNING;  // Debug level: Warning
    mmwl_handle.dbg_print_cb = xwr_print;
    mmwl_handle.dev_type = RL_AR_DEVICETYPE_22XX; // Radar type: xWR22xx

    //  Hardware callbacks
    mmwl_handle.hw_cb.get_host_irq_pin_level = get_xwr_irq_pin_lvl;
    mmwl_handle.hw_cb.enable_host_irq = enable_host_irq;
    mmwl_handle.hw_cb.enable_device = enable_device;
    mmwl_handle.hw_cb.disable_device = disable_device;
    mmwl_handle.hw_cb.write_data = write_data;
    mmwl_handle.hw_cb.read_data = read_data;

    // Event callbacks
    mmwl_handle.rf_ae_event_cb.init_calib_status = init_calib_done_cb;
    mmwl_handle.dev_event_cb.power_up_done = pwup_done_cb;
    mmwl_handle.dev_event_cb.rf_power_up_done = rf_pwup_done_cb;
    mmwl_handle.rf_ae_event_cb.frame_trigger_ready = frame_trig_rdy_cb;

    mmwl_esp_client_connect(&mmwl_handle);
}
```

## Using the Simplified Configuration APIs
The library also provided simplified configuration APIs beside the TI's official APIs to help developers start the radar faster.

To use simplified APIs, you can following steps list as follows.
```text
rlDevicePowerOn -> (Wait for pwup_done_cb callback) -> rlDeviceRfStart -> (Wait for rf_pwup_done_cb callback) -> mmwl_esp_pre_rf_init -> mmwl_esp_post_rf_init -> rlRfInit -> (Wait for init_calib_done_cb callback) -> rlSetProfileConfig -> rlSetChirpConfig -> rlSetFrameConfig
```

Functions with prefix `rl` is TI's official APIs and `mmwl` is simplified APIs. The simplified API using the unified configuration manifest `mmwl_esp_config_t`.
