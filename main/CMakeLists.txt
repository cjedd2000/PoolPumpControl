idf_component_register(SRCS "main.c"
                            "webServer.c"
                            "temperature.c"
                            "../TempSensor/ds18b20.c"
                            "sysTime.c"
                    INCLUDE_DIRS "."
                                 "../TempSensor")

set(WEB_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../front")
if(EXISTS ${WEB_SRC_DIR})
    spiffs_create_partition_image(www ${WEB_SRC_DIR} FLASH_IN_PROJECT)
else()
    message(FATAL_ERROR "${WEB_SRC_DIR} doesn't exit.")
endif()