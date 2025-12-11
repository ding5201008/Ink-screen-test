/**
 * SSD1619 驱动实现
 * 适用于黑白/三色墨水屏
 */

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "epd_common.h"
#include "epd_ssd1619.h"

#define TAG "EPD_SSD1619"

// SSD1619命令定义
#define SSD1619_CMD_DRIVER_OUTPUT_CONTROL        0x01
#define SSD1619_CMD_GATE_VOLTAGE                 0x03
#define SSD1619_CMD_SOURCE_VOLTAGE               0x04
#define SSD1619_CMD_INIT_SETTING                 0x05
#define SSD1619_CMD_BOOSTER_SOFTSTART            0x0C
#define SSD1619_CMD_GATE_SCAN_START              0x0F
#define SSD1619_CMD_DEEP_SLEEP                   0x10
#define SSD1619_CMD_DATA_ENTRY_MODE              0x11
#define SSD1619_CMD_SW_RESET                     0x12
#define SSD1619_CMD_TEMP_SENSOR                  0x1A
#define SSD1619_CMD_MASTER_ACTIVATION            0x20
#define SSD1619_CMD_DISP_UPDATE_CTRL1            0x21
#define SSD1619_CMD_DISP_UPDATE_CTRL2            0x22
#define SSD1619_CMD_WRITE_RAM_BW                 0x24
#define SSD1619_CMD_WRITE_RAM_RED                0x26
#define SSD1619_CMD_READ_RAM                     0x27
#define SSD1619_CMD_VCOM_SENSE                   0x28
#define SSD1619_CMD_VCOM_DURATION                0x29
#define SSD1619_CMD_VCOM_SETTING                 0x2C
#define SSD1619_CMD_BORDER_WAVEFORM              0x3C
#define SSD1619_CMD_RAM_X_START_END              0x44
#define SSD1619_CMD_RAM_Y_START_END              0x45
#define SSD1619_CMD_RAM_X_COUNTER                0x4E
#define SSD1619_CMD_RAM_Y_COUNTER                0x4F

// 私有数据结构
typedef struct {
    uint8_t lut_full[30];      // 全刷LUT
    uint8_t lut_partial[30];   // 局刷LUT
    uint8_t rotation;          // 旋转角度
    bool initialized;          // 初始化标志
} ssd1619_priv_t;

// 创建SSD1619设备实例
epd_device_t* epd_ssd1619_create(const epd_pins_t *pins, 
                                 uint16_t width, 
                                 uint16_t height,
                                 epd_color_mode_t color_mode) {
    epd_device_t *dev = calloc(1, sizeof(epd_device_t));
    if (!dev) {
        ESP_LOGE(TAG, "分配设备内存失败");
        return NULL;
    }
    
    ssd1619_priv_t *priv = calloc(1, sizeof(ssd1619_priv_t));
    if (!priv) {
        free(dev);
        ESP_LOGE(TAG, "分配私有数据内存失败");
        return NULL;
    }
    
    // 初始化设备信息
    dev->info.type = EPD_SSD1619;
    dev->info.chip_name = "SSD1619";
    dev->info.width = width;
    dev->info.height = height;
    dev->info.color_mode = color_mode;
    dev->info.capabilities = EPD_CAP_PARTIAL_REFRESH | EPD_CAP_POWER_CONTROL;
    dev->info.version = 0x0100;
    
    // 保存引脚配置
    memcpy(&dev->pins, pins, sizeof(epd_pins_t));
    
    // 设置私有数据
    dev->priv = priv;
    
    // 设置函数指针
    dev->init = ssd1619_init;
    dev->deinit = ssd1619_deinit;
    dev->reset = ssd1619_reset;
    dev->clear = ssd1619_clear;
    dev->display_buffer = ssd1619_display_buffer;
    dev->display_partial = ssd1619_display_partial;
    dev->sleep = ssd1619_sleep;
    dev->wakeup = ssd1619_wakeup;
    dev->power_on = ssd1619_power_on;
    dev->power_off = ssd1619_power_off;
    dev->set_rotation = ssd1619_set_rotation;
    dev->invert = ssd1619_invert;
    dev->get_info = ssd1619_get_info;
    
    return dev;
}

// 初始化函数
static esp_err_t ssd1619_init(epd_device_t *dev) {
    if (!dev || !dev->priv) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ssd1619_priv_t *priv = (ssd1619_priv_t *)dev->priv;
    
    ESP_LOGI(TAG, "初始化SSD1619，分辨率: %dx%d", 
             dev->info.width, dev->info.height);
    
    // 初始化SPI
    esp_err_t err = epd_spi_init(dev, CONFIG_EPD_SPI_HOST, CONFIG_EPD_SPI_SPEED);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI初始化失败: %d", err);
        return err;
    }
    
    // 初始化GPIO
    gpio_set_direction(dev->pins.dc_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(dev->pins.rst_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(dev->pins.busy_pin, GPIO_MODE_INPUT);
    
    if (dev->pins.pwr_en_pin >= 0) {
        gpio_set_direction(dev->pins.pwr_en_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(dev->pins.pwr_en_pin, 1);
    }
    
    // 硬件复位
    dev->reset(dev);
    
    // 发送初始化序列
    ssd1619_send_init_sequence(dev);
    
    priv->initialized = true;
    ESP_LOGI(TAG, "SSD1619初始化完成");
    
    return ESP_OK;
}

// 发送初始化序列
static void ssd1619_send_init_sequence(epd_device_t *dev) {
    // 软复位
    epd_send_command(dev, SSD1619_CMD_SW_RESET);
    epd_delay_ms(10);
    
    // 等待就绪
    while (epd_is_busy(dev)) {
        vTaskDelay(1);
    }
    
    // 设置驱动输出控制
    epd_send_command(dev, SSD1619_CMD_DRIVER_OUTPUT_CONTROL);
    epd_send_data(dev, (dev->info.height - 1) & 0xFF);
    epd_send_data(dev, ((dev->info.height - 1) >> 8) & 0xFF);
    epd_send_data(dev, 0x00);  // GD = 0, SM = 0, TB = 0
    
    // 设置数据入口模式
    epd_send_command(dev, SSD1619_CMD_DATA_ENTRY_MODE);
    epd_send_data(dev, 0x03);  // X增量, Y增量
    
    // 设置RAM地址
    ssd1619_set_memory_area(dev, 0, 0, dev->info.width - 1, dev->info.height - 1);
    ssd1619_set_memory_pointer(dev, 0, 0);
    
    // 设置边框波形
    epd_send_command(dev, SSD1619_CMD_BORDER_WAVEFORM);
    epd_send_data(dev, 0x05);
    
    // 设置温度传感器
    epd_send_command(dev, SSD1619_CMD_TEMP_SENSOR);
    epd_send_data(dev, 0x80);  // 内部温度传感器
    
    // 设置显示更新控制
    epd_send_command(dev, SSD1619_CMD_DISP_UPDATE_CTRL2);
    epd_send_data(dev, 0xC0);
    
    // 主激活
    epd_send_command(dev, SSD1619_CMD_MASTER_ACTIVATION);
    
    // 等待就绪
    while (epd_is_busy(dev)) {
        vTaskDelay(1);
    }
}

// 设置内存区域
static void ssd1619_set_memory_area(epd_device_t *dev, 
                                    uint16_t x_start, uint16_t y_start,
                                    uint16_t x_end, uint16_t y_end) {
    // 设置X范围
    epd_send_command(dev, SSD1619_CMD_RAM_X_START_END);
    epd_send_data(dev, (x_start >> 3) & 0xFF);
    epd_send_data(dev, (x_end >> 3) & 0xFF);
    
    // 设置Y范围
    epd_send_command(dev, SSD1619_CMD_RAM_Y_START_END);
    epd_send_data(dev, y_start & 0xFF);
    epd_send_data(dev, (y_start >> 8) & 0xFF);
    epd_send_data(dev, y_end & 0xFF);
    epd_send_data(dev, (y_end >> 8) & 0xFF);
}

// 设置内存指针
static void ssd1619_set_memory_pointer(epd_device_t *dev, 
                                       uint16_t x, uint16_t y) {
    // 设置X指针
    epd_send_command(dev, SSD1619_CMD_RAM_X_COUNTER);
    epd_send_data(dev, (x >> 3) & 0xFF);
    
    // 设置Y指针
    epd_send_command(dev, SSD1619_CMD_RAM_Y_COUNTER);
    epd_send_data(dev, y & 0xFF);
    epd_send_data(dev, (y >> 8) & 0xFF);
}

// 清屏
static esp_err_t ssd1619_clear(epd_device_t *dev, epd_color_t color) {
    if (!dev || !dev->priv) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "清屏，颜色: %d", color);
    
    uint8_t fill_value = (color == EPD_COLOR_WHITE) ? 0xFF : 0x00;
    uint32_t buffer_size = dev->info.width * dev->info.height / 8;
    uint8_t *buffer = malloc(buffer_size);
    
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }
    
    memset(buffer, fill_value, buffer_size);
    
    esp_err_t err = dev->display_buffer(dev, buffer, EPD_UPDATE_FULL);
    
    free(buffer);
    return err;
}

// 显示缓冲区
static esp_err_t ssd1619_display_buffer(epd_device_t *dev, 
                                        const uint8_t *buffer,
                                        epd_update_mode_t mode) {
    if (!dev || !buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 设置内存区域
    ssd1619_set_memory_area(dev, 0, 0, dev->info.width - 1, dev->info.height - 1);
    ssd1619_set_memory_pointer(dev, 0, 0);
    
    // 发送黑白数据
    epd_send_command(dev, SSD1619_CMD_WRITE_RAM_BW);
    epd_send_data_buffer(dev, buffer, dev->info.width * dev->info.height / 8);
    
    // 如果是三色屏，发送红色数据
    if (dev->info.color_mode == EPD_MODE_3C) {
        // 对于三色屏，需要分别发送黑白和红色数据
        // 这里简化处理，将红色数据置零
        uint32_t buffer_size = dev->info.width * dev->info.height / 8;
        uint8_t *red_buffer = calloc(1, buffer_size);
        if (red_buffer) {
            epd_send_command(dev, SSD1619_CMD_WRITE_RAM_RED);
            epd_send_data_buffer(dev, red_buffer, buffer_size);
            free(red_buffer);
        }
    }
    
    // 触发更新
    epd_send_command(dev, SSD1619_CMD_DISP_UPDATE_CTRL2);
    
    switch (mode) {
        case EPD_UPDATE_FULL:
            epd_send_data(dev, 0xC7);  // 全刷
            break;
        case EPD_UPDATE_PARTIAL:
            epd_send_data(dev, 0x04);  // 局刷
            break;
        case EPD_UPDATE_FAST:
            epd_send_data(dev, 0x0C);  // 快速刷新
            break;
    }
    
    epd_send_command(dev, SSD1619_CMD_MASTER_ACTIVATION);
    
    // 等待刷新完成
    while (epd_is_busy(dev)) {
        vTaskDelay(10);
    }
    
    return ESP_OK;
}

// 局部显示
static esp_err_t ssd1619_display_partial(epd_device_t *dev, 
                                         const uint8_t *buffer,
                                         uint16_t x, uint16_t y,
                                         uint16_t width, uint16_t height) {
    if (!dev || !buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 计算字节边界
    uint16_t x_start = x;
    uint16_t x_end = x + width - 1;
    uint16_t y_start = y;
    uint16_t y_end = y + height - 1;
    
    // 设置局部区域
    ssd1619_set_memory_area(dev, x_start, y_start, x_end, y_end);
    ssd1619_set_memory_pointer(dev, x_start, y_start);
    
    // 发送数据
    epd_send_command(dev, SSD1619_CMD_WRITE_RAM_BW);
    
    // 只发送需要更新的部分
    uint16_t bytes_per_line = width / 8;
    if (width % 8) bytes_per_line++;
    
    for (uint16_t row = 0; row < height; row++) {
        epd_send_data_buffer(dev, buffer + row * bytes_per_line, bytes_per_line);
    }
    
    // 触发局部更新
    epd_send_command(dev, SSD1619_CMD_DISP_UPDATE_CTRL2);
    epd_send_data(dev, 0x04);  // 局刷模式
    epd_send_command(dev, SSD1619_CMD_MASTER_ACTIVATION);
    
    // 等待完成
    while (epd_is_busy(dev)) {
        vTaskDelay(10);
    }
    
    return ESP_OK;
}

// 进入睡眠
static esp_err_t ssd1619_sleep(epd_device_t *dev) {
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "进入睡眠模式");
    
    epd_send_command(dev, SSD1619_CMD_DEEP_SLEEP);
    epd_send_data(dev, 0x01);  // 进入深度睡眠
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    return ESP_OK;
}

// 唤醒
static esp_err_t ssd1619_wakeup(epd_device_t *dev) {
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 硬件复位唤醒
    dev->reset(dev);
    
    // 重新初始化
    return dev->init(dev);
}

// 复位
static esp_err_t ssd1619_reset(epd_device_t *dev) {
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "硬件复位");
    
    // 拉低复位引脚
    gpio_set_level(dev->pins.rst_pin, 0);
    epd_delay_ms(10);
    
    // 拉高复位引脚
    gpio_set_level(dev->pins.rst_pin, 1);
    epd_delay_ms(10);
    
    // 等待就绪
    epd_delay_ms(100);
    
    return ESP_OK;
}

// 电源控制
static esp_err_t ssd1619_power_on(epd_device_t *dev) {
    // SSD1619通过复位唤醒
    return ssd1619_reset(dev);
}

static esp_err_t ssd1619_power_off(epd_device_t *dev) {
    return ssd1619_sleep(dev);
}

// 设置旋转
static esp_err_t ssd1619_set_rotation(epd_device_t *dev, uint8_t rotation) {
    if (!dev || !dev->priv) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ssd1619_priv_t *priv = (ssd1619_priv_t *)dev->priv;
    priv->rotation = rotation % 4;
    
    // 根据旋转角度调整宽高
    if (rotation & 1) {
        // 旋转90或270度，交换宽高
        uint16_t temp = dev->info.width;
        dev->info.width = dev->info.height;
        dev->info.height = temp;
    }
    
    return ESP_OK;
}

// 反色显示
static esp_err_t ssd1619_invert(epd_device_t *dev, bool invert) {
    // SSD1619不支持硬件反色，需要在软件层处理
    return ESP_OK;
}

// 获取设备信息
static esp_err_t ssd1619_get_info(epd_device_t *dev, epd_info_t *info) {
    if (!dev || !info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(info, &dev->info, sizeof(epd_info_t));
    return ESP_OK;
}

// 反初始化
static esp_err_t ssd1619_deinit(epd_device_t *dev) {
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 进入睡眠
    ssd1619_sleep(dev);
    
    // 释放SPI设备
    if (dev->spi_dev) {
        spi_bus_remove_device(dev->spi_dev);
        dev->spi_dev = NULL;
    }
    
    // 释放私有数据
    if (dev->priv) {
        free(dev->priv);
        dev->priv = NULL;
    }
    
    return ESP_OK;
}
