idf_component_register(
        SRCS
        "src/mmwavelink_esp.c"
        "src/mmwavelink_crc.c"
        "src/mmwavelink_firmware.c"
        "src/rl_controller.c"
        "src/rl_device.c"
        "src/rl_driver.c"
        "src/rl_monitoring.c"
        "src/rl_sensor.c"
        "port/freertos/mmwl_esp_freertos.c"
        INCLUDE_DIRS
        "include"
        "port"
        "port/freertos"
        REQUIRES driver
)
