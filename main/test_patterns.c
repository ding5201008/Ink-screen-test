/**
 * 测试图案生成函数
 */

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "epd_common.h"

#define TAG "EPD_TEST_PATTERNS"

// 生成棋盘格图案
esp_err_t test_checkerboard_pattern(epd_device_t *dev, uint8_t block_size) {
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "生成棋盘格图案，块大小: %d", block_size);
    
    uint32_t buffer_size = dev->info.width * dev->info.height / 8;
    uint8_t *buffer = malloc(buffer_size);
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }
    
    // 生成棋盘格
    for (uint16_t y = 0; y < dev->info.height; y++) {
        for (uint16_t x = 0; x < dev->info.width; x++) {
            bool is_black = ((x / block_size) + (y / block_size)) % 2 == 0;
            
            if (is_black) {
                // 设置像素为黑色
                uint32_t byte_idx = y * (dev->info.width / 8) + (x / 8);
                uint8_t bit_mask = 0x80 >> (x % 8);
                buffer[byte_idx] &= ~bit_mask;
            } else {
                // 设置像素为白色
                uint32_t byte_idx = y * (dev->info.width / 8) + (x / 8);
                uint8_t bit_mask = 0x80 >> (x % 8);
                buffer[byte_idx] |= bit_mask;
            }
        }
    }
    
    // 显示图案
    esp_err_t err = dev->display_buffer(dev, buffer, EPD_UPDATE_FULL);
    
    free(buffer);
    return err;
}

// 生成渐变图案
esp_err_t test_gradient_pattern(epd_device_t *dev) {
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "生成渐变图案");
    
    uint32_t buffer_size = dev->info.width * dev->info.height / 8;
    uint8_t *buffer = malloc(buffer_size);
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }
    
    // 定义8种灰度级别
    uint8_t patterns[8] = {
        0x00,  // 黑色
        0x11,  // 深灰
        0x22,  // 中灰
        0x44,  // 灰
        0x88,  // 浅灰
        0xAA,  // 更浅灰
        0xDD,  // 极浅灰
        0xFF   // 白色
    };
    
    // 计算每个灰度块的高度
    uint16_t block_height = dev->info.height / 8;
    
    // 填充渐变
    for (uint8_t pattern_idx = 0; pattern_idx < 8; pattern_idx++) {
        for (uint16_t y = 0; y < block_height; y++) {
            if (pattern_idx * block_height + y >= dev->info.height) {
                break;
            }
            
            for (uint16_t x_byte = 0; x_byte < dev->info.width / 8; x_byte++) {
                uint32_t byte_idx = (pattern_idx * block_height + y) * 
                                   (dev->info.width / 8) + x_byte;
                buffer[byte_idx] = patterns[pattern_idx];
            }
        }
    }
    
    esp_err_t err = dev->display_buffer(dev, buffer, EPD_UPDATE_FULL);
    
    free(buffer);
    return err;
}

// 生成线条图案
esp_err_t test_line_pattern(epd_device_t *dev) {
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "生成线条图案");
    
    uint32_t buffer_size = dev->info.width * dev->info.height / 8;
    uint8_t *buffer = malloc(buffer_size);
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }
    
    // 清空为白色
    memset(buffer, 0xFF, buffer_size);
    
    // 绘制水平线
    for (uint16_t y = 0; y < dev->info.height; y += 20) {
        for (uint16_t x = 0; x < dev->info.width; x++) {
            uint32_t byte_idx = y * (dev->info.width / 8) + (x / 8);
            uint8_t bit_mask = 0x80 >> (x % 8);
            buffer[byte_idx] &= ~bit_mask;  // 黑色
        }
    }
    
    // 绘制垂直线
    for (uint16_t x = 0; x < dev->info.width; x += 20) {
        for (uint16_t y = 0; y < dev->info.height; y++) {
            uint32_t byte_idx = y * (dev->info.width / 8) + (x / 8);
            uint8_t bit_mask = 0x80 >> (x % 8);
            buffer[byte_idx] &= ~bit_mask;  // 黑色
        }
    }
    
    // 绘制对角线
    for (uint16_t i = 0; i < dev->info.width && i < dev->info.height; i++) {
        uint16_t x = i;
        uint16_t y = i;
        
        uint32_t byte_idx = y * (dev->info.width / 8) + (x / 8);
        uint8_t bit_mask = 0x80 >> (x % 8);
        buffer[byte_idx] &= ~bit_mask;
    }
    
    // 绘制另一条对角线
    for (uint16_t i = 0; i < dev->info.width && i < dev->info.height; i++) {
        uint16_t x = dev->info.width - i - 1;
        uint16_t y = i;
        
        uint32_t byte_idx = y * (dev->info.width / 8) + (x / 8);
        uint8_t bit_mask = 0x80 >> (x % 8);
        buffer[byte_idx] &= ~bit_mask;
    }
    
    esp_err_t err = dev->display_buffer(dev, buffer, EPD_UPDATE_FULL);
    
    free(buffer);
    return err;
}

// 生成几何形状图案
esp_err_t test_shape_pattern(epd_device_t *dev) {
    if (!dev) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "生成几何形状图案");
    
    uint32_t buffer_size = dev->info.width * dev->info.height / 8;
    uint8_t *buffer = malloc(buffer_size);
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }
    
    // 清空为白色
    memset(buffer, 0xFF, buffer_size);
    
    // 绘制矩形边框
    uint16_t rect_x = dev->info.width / 4;
    uint16_t rect_y = dev->info.height / 4;
    uint16_t rect_w = dev->info.width / 2;
    uint16_t rect_h = dev->info.height / 2;
    
    // 上边
    for (uint16_t x = rect_x; x < rect_x + rect_w; x++) {
        uint32_t byte_idx = rect_y * (dev->info.width / 8) + (x / 8);
        uint8_t bit_mask = 0x80 >> (x % 8);
        buffer[byte_idx] &= ~bit_mask;
    }
    
    // 下边
    for (uint16_t x = rect_x; x < rect_x + rect_w; x++) {
        uint32_t byte_idx = (rect_y + rect_h - 1) * (dev->info.width / 8) + (x / 8);
        uint8_t bit_mask = 0x80 >> (x % 8);
        buffer[byte_idx] &= ~bit_mask;
    }
    
    // 左边
    for (uint16_t y = rect_y; y < rect_y + rect_h; y++) {
        uint32_t byte_idx = y * (dev->info.width / 8) + (rect_x / 8);
        uint8_t bit_mask = 0x80 >> (rect_x % 8);
        buffer[byte_idx] &= ~bit_mask;
    }
    
    // 右边
    for (uint16_t y = rect_y; y < rect_y + rect_h; y++) {
        uint32_t byte_idx = y * (dev->info.width / 8) + ((rect_x + rect_w - 1) / 8);
        uint8_t bit_mask = 0x80 >> ((rect_x + rect_w - 1) % 8);
        buffer[byte_idx] &= ~bit_mask;
    }
    
    // 绘制圆形
    uint16_t center_x = dev->info.width / 2;
    uint16_t center_y = dev->info.height / 2;
    uint16_t radius = dev->info.height / 8;
    
    for (int16_t y = -radius; y <= radius; y++) {
        for (int16_t x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                uint16_t px = center_x + x;
                uint16_t py = center_y + y;
                
                if (px < dev->info.width && py < dev->info.height) {
                    uint32_t byte_idx = py * (dev->info.width / 8) + (px / 8);
                    uint8_t bit_mask = 0x80 >> (px % 8);
                    buffer[byte_idx] &= ~bit_mask;
                }
            }
        }
    }
    
    // 绘制三角形
    uint16_t tri_x1 = dev->info.width / 8;
    uint16_t tri_y1 = dev->info.height * 3 / 4;
    uint16_t tri_x2 = dev->info.width / 4;
    uint16_t tri_y2 = dev->info.height / 2;
    uint16_t tri_x3 = dev->info.width * 3 / 8;
    uint16_t tri_y3 = dev->info.height * 3 / 4;
    
    // 绘制三角形边（简单实现）
    epd_draw_line(buffer, dev->info.width, dev->info.height,
                 tri_x1, tri_y1, tri_x2, tri_y2, EPD_COLOR_BLACK);
    epd_draw_line(buffer, dev->info.width, dev->info.height,
                 tri_x2, tri_y2, tri_x3, tri_y3, EPD_COLOR_BLACK);
    epd_draw_line(buffer, dev->info.width, dev->info.height,
                 tri_x3, tri_y3, tri_x1, tri_y1, EPD_COLOR_BLACK);
    
    esp_err_t err = dev->display_buffer(dev, buffer, EPD_UPDATE_FULL);
    
    free(buffer);
    return err;
}
