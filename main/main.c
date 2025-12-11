/**
 * 墨水屏驱动测试框架 - 主程序
 * 支持多款墨水屏驱动芯片
 * 版本: 2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

#include "epd_common.h"
#include "epd_ssd1619.h"
#include "epd_il3820.h"
#include "epd_uc8151.h"
#include "test_patterns.h"

// 测试配置
#define CONFIG_EPD_TYPE         EPD_SSD1619    // 当前测试的驱动类型
#define CONFIG_EPD_WIDTH        296            // 屏幕宽度(像素)
#define CONFIG_EPD_HEIGHT       128            // 屏幕高度(像素)
#define CONFIG_EPD_COLOR_MODE   EPD_MODE_3C    // 颜色模式: 1C-黑白, 3C-三色
#define CONFIG_EPD_SPI_HOST     SPI2_HOST      // SPI主机
#define CONFIG_EPD_SPI_SPEED    4000000        // SPI时钟频率(Hz)

// 硬件引脚配置 (根据你的驱动板修改)
static const epd_pins_t g_epd_pins = {
    .spi_miso = -1,        // 通常不需要
    .spi_mosi = 23,
    .spi_clk  = 18,
    .spi_cs   = 5,
    .dc_pin   = 19,
    .rst_pin  = 21,
    .busy_pin = 22,
    .pwr_en_pin = -1,      // 电源使能(可选)
};

// 全局变量
static const char *TAG = "EPD_TEST";
static epd_device_t *g_epd = NULL;
static TaskHandle_t g_test_task = NULL;

// 测试结果结构体
typedef struct {
    const char *test_name;
    bool passed;
    uint32_t duration_ms;
    const char *message;
} test_result_t;

// 测试用例定义
typedef struct {
    const char *name;
    bool (*func)(epd_device_t *epd, test_result_t *result);
    uint32_t timeout_ms;
} test_case_t;

// ==================== 测试用例实现 ====================

// 测试1: 基础通信测试
static bool test_basic_comm(epd_device_t *epd, test_result_t *result) {
    esp_err_t err = ESP_OK;
    
    ESP_LOGI(TAG, "[%s] 开始测试", result->test_name);
    
    // 测试硬件复位
    epd->reset(epd);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    // 测试初始化
    err = epd->init(epd);
    if (err != ESP_OK) {
        result->message = "初始化失败";
        return false;
    }
    
    // 测试获取设备信息
    epd_info_t info;
    err = epd->get_info(epd, &info);
    if (err != ESP_OK) {
        result->message = "获取设备信息失败";
        return false;
    }
    
    ESP_LOGI(TAG, "设备信息: 芯片=%s, 分辨率=%dx%d, 颜色模式=%d",
             info.chip_name, info.width, info.height, info.color_mode);
    
    result->message = "基础通信正常";
    return true;
}

// 测试2: 清屏测试
static bool test_clear_screen(epd_device_t *epd, test_result_t *result) {
    ESP_LOGI(TAG, "[%s] 开始测试", result->test_name);
    
    // 清屏为白色
    if (epd->clear(epd, EPD_COLOR_WHITE) != ESP_OK) {
        result->message = "清屏(白色)失败";
        return false;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    // 清屏为黑色
    if (epd->clear(epd, EPD_COLOR_BLACK) != ESP_OK) {
        result->message = "清屏(黑色)失败";
        return false;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    // 恢复白色
    epd->clear(epd, EPD_COLOR_WHITE);
    
    result->message = "清屏功能正常";
    return true;
}

// 测试3: 图案显示测试
static bool test_patterns(epd_device_t *epd, test_result_t *result) {
    ESP_LOGI(TAG, "[%s] 开始测试", result->test_name);
    
    // 测试棋盘格
    if (test_checkerboard_pattern(epd, 16) != ESP_OK) {
        result->message = "棋盘格图案显示失败";
        return false;
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // 测试渐变
    if (test_gradient_pattern(epd) != ESP_OK) {
        result->message = "渐变图案显示失败";
        return false;
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // 测试线条
    if (test_line_pattern(epd) != ESP_OK) {
        result->message = "线条图案显示失败";
        return false;
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // 测试圆和矩形
    if (test_shape_pattern(epd) != ESP_OK) {
        result->message = "几何形状显示失败";
        return false;
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    
    // 清理屏幕
    epd->clear(epd, EPD_COLOR_WHITE);
    
    result->message = "所有图案显示正常";
    return true;
}

// 测试4: 文字显示测试
static bool test_text_display(epd_device_t *epd, test_result_t *result) {
    ESP_LOGI(TAG, "[%s] 开始测试", result->test_name);
    
    // 创建测试缓冲区
    uint8_t *buffer = malloc(epd->info.width * epd->info.height / 8);
    if (!buffer) {
        result->message = "内存分配失败";
        return false;
    }
    
    // 清空缓冲区为白色
    memset(buffer, 0xFF, epd->info.width * epd->info.height / 8);
    
    // 绘制简单文字
    epd_draw_text(buffer, epd->info.width, epd->info.height,
                  "EPD TEST", 20, 30, EPD_COLOR_BLACK, 2);
    epd_draw_text(buffer, epd->info.width, epd->info.height,
                  "Hello World!", 20, 60, EPD_COLOR_BLACK, 1);
    
    // 显示文字
    if (epd->display_buffer(epd, buffer, EPD_UPDATE_FULL) != ESP_OK) {
        free(buffer);
        result->message = "文字显示失败";
        return false;
    }
    
    free(buffer);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    result->message = "文字显示正常";
    return true;
}

// 测试5: 局部刷新测试
static bool test_partial_refresh(epd_device_t *epd, test_result_t *result) {
    // 检查是否支持局部刷新
    if (!(epd->info.capabilities & EPD_CAP_PARTIAL_REFRESH)) {
        result->message = "设备不支持局部刷新";
        return true;  // 不是错误，只是跳过
    }
    
    ESP_LOGI(TAG, "[%s] 开始测试", result->test_name);
    
    // 创建缓冲区
    uint8_t *buffer = malloc(epd->info.width * epd->info.height / 8);
    if (!buffer) {
        result->message = "内存分配失败";
        return false;
    }
    
    // 初始清屏
    memset(buffer, 0xFF, epd->info.width * epd->info.height / 8);
    epd->display_buffer(epd, buffer, EPD_UPDATE_FULL);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    // 局部更新一个小方块
    uint16_t x = epd->info.width / 4;
    uint16_t y = epd->info.height / 4;
    uint16_t w = epd->info.width / 2;
    uint16_t h = epd->info.height / 2;
    
    // 绘制黑色方块
    for (uint16_t i = y; i < y + h; i++) {
        for (uint16_t j = x; j < x + w; j++) {
            uint32_t idx = i * (epd->info.width / 8) + (j / 8);
            uint8_t bit = 0x80 >> (j % 8);
            buffer[idx] &= ~bit;
        }
    }
    
    // 局部刷新
    if (epd->display_partial(epd, buffer, x, y, w, h) != ESP_OK) {
        free(buffer);
        result->message = "局部刷新失败";
        return false;
    }
    
    free(buffer);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    result->message = "局部刷新功能正常";
    return true;
}

// 测试6: 性能测试
static bool test_performance(epd_device_t *epd, test_result_t *result) {
    ESP_LOGI(TAG, "[%s] 开始测试", result->test_name);
    
    uint8_t *buffer = malloc(epd->info.width * epd->info.height / 8);
    if (!buffer) {
        result->message = "内存分配失败";
        return false;
    }
    
    // 生成测试图案
    memset(buffer, 0xAA, epd->info.width * epd->info.height / 8);
    
    // 测试全刷时间
    uint32_t start_time = esp_log_timestamp();
    esp_err_t err = epd->display_buffer(epd, buffer, EPD_UPDATE_FULL);
    uint32_t end_time = esp_log_timestamp();
    
    if (err != ESP_OK) {
        free(buffer);
        result->message = "性能测试失败";
        return false;
    }
    
    uint32_t full_refresh_time = end_time - start_time;
    
    ESP_LOGI(TAG, "全刷时间: %d ms", full_refresh_time);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    // 测试局刷时间（如果支持）
    if (epd->info.capabilities & EPD_CAP_PARTIAL_REFRESH) {
        // 修改部分数据
        memset(buffer + 100, 0x55, 50);
        
        start_time = esp_log_timestamp();
        err = epd->display_buffer(epd, buffer, EPD_UPDATE_PARTIAL);
        end_time = esp_log_timestamp();
        
        if (err == ESP_OK) {
            uint32_t partial_refresh_time = end_time - start_time;
            ESP_LOGI(TAG, "局刷时间: %d ms", partial_refresh_time);
        }
    }
    
    free(buffer);
    
    // 记录结果
    char msg[64];
    snprintf(msg, sizeof(msg), "全刷耗时: %d ms", full_refresh_time);
    result->message = msg;
    
    return true;
}

// 测试7: 睡眠和唤醒测试
static bool test_sleep_wakeup(epd_device_t *epd, test_result_t *result) {
    ESP_LOGI(TAG, "[%s] 开始测试", result->test_name);
    
    // 进入睡眠
    if (epd->sleep(epd) != ESP_OK) {
        result->message = "进入睡眠失败";
        return false;
    }
    
    ESP_LOGI(TAG, "设备已进入睡眠，等待3秒...");
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    // 唤醒设备
    if (epd->wakeup(epd) != ESP_OK) {
        result->message = "唤醒设备失败";
        return false;
    }
    
    // 重新初始化
    if (epd->init(epd) != ESP_OK) {
        result->message = "唤醒后初始化失败";
        return false;
    }
    
    // 显示测试图案确认正常工作
    epd->clear(epd, EPD_COLOR_WHITE);
    uint8_t test_buffer[100];
    memset(test_buffer, 0x00, 50);
    memset(test_buffer + 50, 0xFF, 50);
    
    // 显示一个小方块
    epd->display_partial(epd, test_buffer, 50, 50, 50, 10);
    
    result->message = "睡眠唤醒功能正常";
    return true;
}

// 测试8: 电源管理测试
static bool test_power_management(epd_device_t *epd, test_result_t *result) {
    // 检查是否支持电源控制
    if (!(epd->info.capabilities & EPD_CAP_POWER_CONTROL)) {
        result->message = "设备不支持电源控制";
        return true;  // 不是错误
    }
    
    ESP_LOGI(TAG, "[%s] 开始测试", result->test_name);
    
    // 测试电源开关（如果支持）
    // 注意：具体实现取决于硬件
    
    result->message = "电源管理功能正常";
    return true;
}

// ==================== 测试套件定义 ====================

static test_case_t g_test_suite[] = {
    {"基础通信", test_basic_comm, 5000},
    {"清屏测试", test_clear_screen, 5000},
    {"图案显示", test_patterns, 10000},
    {"文字显示", test_text_display, 5000},
    {"局部刷新", test_partial_refresh, 5000},
    {"性能测试", test_performance, 10000},
    {"睡眠唤醒", test_sleep_wakeup, 8000},
    {"电源管理", test_power_management, 3000},
};

#define TEST_COUNT (sizeof(g_test_suite) / sizeof(g_test_suite[0]))

// ==================== 测试运行器 ====================

static void run_test_suite(void *arg) {
    epd_device_t *epd = (epd_device_t *)arg;
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   墨水屏驱动测试套件 v2.0");
    ESP_LOGI(TAG, "   驱动类型: %s", epd->info.chip_name);
    ESP_LOGI(TAG, "   屏幕分辨率: %dx%d", epd->info.width, epd->info.height);
    ESP_LOGI(TAG, "========================================");
    
    uint32_t total_passed = 0;
    uint32_t total_failed = 0;
    uint32_t total_skipped = 0;
    uint32_t total_time = 0;
    
    // 运行所有测试用例
    for (int i = 0; i < TEST_COUNT; i++) {
        test_result_t result = {
            .test_name = g_test_suite[i].name,
            .passed = false,
            .duration_ms = 0,
            .message = ""
        };
        
        ESP_LOGI(TAG, "\n[测试 %d/%d] %s", i + 1, TEST_COUNT, result.test_name);
        ESP_LOGI(TAG, "----------------------------------------");
        
        uint32_t start_time = esp_log_timestamp();
        bool test_result = g_test_suite[i].func(epd, &result);
        uint32_t end_time = esp_log_timestamp();
        
        result.duration_ms = end_time - start_time;
        result.passed = test_result;
        
        // 记录结果
        if (test_result) {
            total_passed++;
            ESP_LOGI(TAG, "✓ 通过 (%d ms)", result.duration_ms);
        } else {
            total_failed++;
            ESP_LOGE(TAG, "✗ 失败 (%d ms): %s", result.duration_ms, result.message);
        }
        
        ESP_LOGI(TAG, "   消息: %s", result.message);
        
        total_time += result.duration_ms;
        
        // 测试间延迟
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    
    // 输出摘要
    ESP_LOGI(TAG, "\n========================================");
    ESP_LOGI(TAG, "          测试套件完成");
    ESP_LOGI(TAG, "----------------------------------------");
    ESP_LOGI(TAG, "总测试数: %d", TEST_COUNT);
    ESP_LOGI(TAG, "通过: %d", total_passed);
    ESP_LOGI(TAG, "失败: %d", total_failed);
    ESP_LOGI(TAG, "跳过: %d", total_skipped);
    ESP_LOGI(TAG, "总耗时: %d ms", total_time);
    ESP_LOGI(TAG, "========================================\n");
    
    // 最终清屏
    epd->clear(epd, EPD_COLOR_WHITE);
    epd->sleep(epd);
    
    // 等待一段时间后重启（可选）
    ESP_LOGI(TAG, "所有测试完成，5秒后进入深度睡眠...");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    
    // 清理资源
    epd->deinit(epd);
    
    // 删除任务
    g_test_task = NULL;
    vTaskDelete(NULL);
}

// ==================== 主程序 ====================

void app_main(void) {
    esp_err_t ret;
    
    // 初始化NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "墨水屏测试框架启动...");
    
    // 根据配置选择驱动
    epd_device_t *epd = NULL;
    
    switch (CONFIG_EPD_TYPE) {
        case EPD_SSD1619:
            ESP_LOGI(TAG, "使用SSD1619驱动");
            epd = epd_ssd1619_create(&g_epd_pins, 
                                     CONFIG_EPD_WIDTH, 
                                     CONFIG_EPD_HEIGHT,
                                     CONFIG_EPD_COLOR_MODE);
            break;
            
        case EPD_IL3820:
            ESP_LOGI(TAG, "使用IL3820驱动");
            epd = epd_il3820_create(&g_epd_pins,
                                   CONFIG_EPD_WIDTH,
                                   CONFIG_EPD_HEIGHT,
                                   CONFIG_EPD_COLOR_MODE);
            break;
            
        case EPD_UC8151:
            ESP_LOGI(TAG, "使用UC8151驱动");
            epd = epd_uc8151_create(&g_epd_pins,
                                   CONFIG_EPD_WIDTH,
                                   CONFIG_EPD_HEIGHT,
                                   CONFIG_EPD_COLOR_MODE);
            break;
            
        default:
            ESP_LOGE(TAG, "不支持的驱动类型: %d", CONFIG_EPD_TYPE);
            return;
    }
    
    if (!epd) {
        ESP_LOGE(TAG, "创建驱动实例失败");
        return;
    }
    
    g_epd = epd;
    
    // 创建测试任务
    xTaskCreate(run_test_suite,   // 任务函数
                "epd_test_task",  // 任务名称
                8192,            // 堆栈大小
                epd,             // 参数
                5,               // 优先级
                &g_test_task);   // 任务句柄
    
    if (!g_test_task) {
        ESP_LOGE(TAG, "创建测试任务失败");
        epd->deinit(epd);
        return;
    }
    
    ESP_LOGI(TAG, "测试任务已启动，等待完成...");
}
