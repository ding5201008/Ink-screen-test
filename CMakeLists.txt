cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(epd_test_framework)

# 主程序组件
idf_component_register(SRCS "main/main.c" "main/test_patterns.c"
                    INCLUDE_DIRS "main"
                    PRIV_REQUIRES freertos driver spi_flash nvs_flash)

# 添加墨水屏驱动组件
add_subdirectory(components/epd_drivers)

# 设置组件目录
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/components)

# 目标芯片设置
set_target_properties(${CMAKE_PROJECT_NAME}.elf PROPERTIES
                    LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/linker.lf)
