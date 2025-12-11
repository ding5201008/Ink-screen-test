/**
 * 墨水屏通用驱动接口
 */

#ifndef __EPD_COMMON_H__
#define __EPD_COMMON_H__

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/spi_master.h"

// 驱动类型枚举
typedef enum {
    EPD_UNKNOWN = 0,
    EPD_SSD1619,    // Solomon Systech SSD1619
    EPD_IL3820,     // ILI Technology IL3820
    EPD_UC8151,     // UltraChip UC8151
    EPD_SSD1675,    // Solomon Systech SSD1675
    EPD_MAX_TYPE
} epd_type_t;

// 颜色模式
typedef enum {
    EPD_MODE_1C = 1,    // 黑白
    EPD_MODE_3C = 3,    // 黑白红
    EPD_MODE_4C = 4,    // 黑白红黄
} epd_color_mode_t;

// 颜色定义
typedef enum {
    EPD_COLOR_WHITE = 0,
    EPD_COLOR_BLACK = 1,
    EPD_COLOR_RED   = 2,
    EPD_COLOR_YELLOW = 3,
} epd_color_t;

// 刷新模式
typedef enum {
    EPD_UPDATE_FULL,      // 全刷
    EPD_UPDATE_PARTIAL,   // 局刷
    EPD_UPDATE_FAST,      // 快速刷新
} epd_update_mode_t;

// 设备能力标志
#define EPD_CAP_PARTIAL_REFRESH   (1 << 0)  // 支持局部刷新
#define EPD_CAP_FAST_REFRESH      (1 << 1)  // 支持快速刷新
#define EPD_CAP_POWER_CONTROL     (1 << 2)  // 支持电源控制
#define EPD_CAP_TEMP_COMPENSATION (1 << 3)  // 支持温度补偿
#define EPD_CAP_ROTATION          (1 << 4)  // 支持旋转

// 引脚配置结构体
typedef struct {
    int8_t spi_miso;      // SPI MISO引脚 (-1表示不使用)
    int8_t spi_mosi;      // SPI MOSI引脚
    int8_t spi_clk;       // SPI时钟引脚
    int8_t spi_cs;        // SPI片选引脚
    int8_t dc_pin;        // 数据/命令选择
    int8_t rst_pin;       // 复位引脚
    int8_t busy_pin;      // 忙状态引脚
    int8_t pwr_en_pin;    // 电源使能引脚 (-1表示不使用)
} epd_pins_t;

// 设备信息结构体
typedef struct {
    epd_type_t type;           // 驱动芯片类型
    const char *chip_name;     // 芯片名称
    uint16_t width;           // 屏幕宽度(像素)
    uint16_t height;          // 屏幕高度(像素)
    epd_color_mode_t color_mode; // 颜色模式
    uint8_t capabilities;     // 设备能力标志
    uint32_t version;         // 驱动版本
} epd_info_t;

// 设备操作结构体（函数指针表）
struct epd_device_t;
typedef struct epd_device_t epd_device_t;

struct epd_device_t {
    // 设备信息
    epd_info_t info;
    
    // 硬件接口
    spi_device_handle_t spi_dev;
    epd_pins_t pins;
    
    // 基本操作
    esp_err_t (*init)(epd_device_t *dev);
    esp_err_t (*deinit)(epd_device_t *dev);
    esp_err_t (*reset)(epd_device_t *dev);
    esp_err_t (*clear)(epd_device_t *dev, epd_color_t color);
    
    // 显示操作
    esp_err_t (*display_buffer)(epd_device_t *dev, const uint8_t *buffer, 
                               epd_update_mode_t mode);
    esp_err_t (*display_partial)(epd_device_t *dev, const uint8_t *buffer,
                                uint16_t x, uint16_t y, 
                                uint16_t width, uint16_t height);
    
    // 电源管理
    esp_err_t (*sleep)(epd_device_t *dev);
    esp_err_t (*wakeup)(epd_device_t *dev);
    esp_err_t (*power_on)(epd_device_t *dev);
    esp_err_t (*power_off)(epd_device_t *dev);
    
    // 其他功能
    esp_err_t (*set_rotation)(epd_device_t *dev, uint8_t rotation);
    esp_err_t (*invert)(epd_device_t *dev, bool invert);
    esp_err_t (*get_info)(epd_device_t *dev, epd_info_t *info);
    
    // 私有数据
    void *priv;
};

// 通用工具函数
esp_err_t epd_spi_init(epd_device_t *dev, spi_host_device_t host, int clock_speed);
void epd_delay_ms(uint32_t ms);
bool epd_is_busy(epd_device_t *dev);
void epd_send_command(epd_device_t *dev, uint8_t cmd);
void epd_send_data(epd_device_t *dev, uint8_t data);
void epd_send_data_buffer(epd_device_t *dev, const uint8_t *data, uint32_t length);

// 绘图函数
void epd_draw_pixel(uint8_t *buffer, uint16_t width, uint16_t height,
                   uint16_t x, uint16_t y, epd_color_t color);
void epd_draw_line(uint8_t *buffer, uint16_t width, uint16_t height,
                  uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, 
                  epd_color_t color);
void epd_draw_rect(uint8_t *buffer, uint16_t width, uint16_t height,
                  uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                  epd_color_t color, bool filled);
void epd_draw_circle(uint8_t *buffer, uint16_t width, uint16_t height,
                    uint16_t x0, uint16_t y0, uint16_t r, 
                    epd_color_t color, bool filled);
void epd_draw_text(uint8_t *buffer, uint16_t width, uint16_t height,
                  const char *text, uint16_t x, uint16_t y,
                  epd_color_t color, uint8_t scale);

// 测试图案生成
esp_err_t test_checkerboard_pattern(epd_device_t *dev, uint8_t block_size);
esp_err_t test_gradient_pattern(epd_device_t *dev);
esp_err_t test_line_pattern(epd_device_t *dev);
esp_err_t test_shape_pattern(epd_device_t *dev);

#endif // __EPD_COMMON_H__
