/*
 * SPDX-FileCopyrightText: 2024-2026 Bluetooth Bridge Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"

#include "ws2812_led.h"
#include "device_config.h"

static const char *TAG = "WS2812_LED";

// LED 上下文
typedef struct {
    led_strip_handle_t strip;
    bool initialized;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    ws2812_mode_t current_mode;
    TaskHandle_t blink_task;
    bool blink_running;
} ws2812_ctx_t;

static ws2812_ctx_t s_led_ctx = {0};

// LED 配置（来自 device_config.h）
#define WS2812_GPIO_NUM       WS2812_GPIO
#define WS2812_LED_BRIGHTNESS WS2812_BRIGHTNESS

/**
 * @brief 闪烁任务 - 用于配对模式
 */
static void blink_task(void *arg)
{
    ws2812_ctx_t *ctx = (ws2812_ctx_t *)arg;
    bool on = true;
    
    while (ctx->blink_running) {
        if (on) {
            led_strip_set_pixel(ctx->strip, 0, ctx->red, ctx->green, ctx->blue);
            led_strip_refresh(ctx->strip);
        } else {
            led_strip_clear(ctx->strip);
        }
        on = !on;
        vTaskDelay(pdMS_TO_TICKS(300));  // 300ms 间隔，600ms 周期
    }
    
    // 任务退出前关闭 LED
    led_strip_clear(ctx->strip);
    vTaskDelete(NULL);
}

/**
 * @brief 初始化 WS2812 LED
 */
esp_err_t ws2812_led_init(void)
{
    if (s_led_ctx.initialized) {
        ESP_LOGW(TAG, "已初始化");
        return ESP_ERR_INVALID_STATE;
    }

    // 创建 LED 灯条配置
    led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812_GPIO_NUM,
        .max_leds = 1,
    };

    // 创建 RMT 通道配置
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10000000,  // 10MHz
    };

    // 安装 LED 灯条
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_ctx.strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建 LED 灯条失败：%s", esp_err_to_name(ret));
        return ret;
    }

    // 初始化状态
    s_led_ctx.initialized = true;
    s_led_ctx.current_mode = WS2812_MODE_OFF;
    s_led_ctx.blink_running = false;
    s_led_ctx.blink_task = NULL;
    
    // 默认关闭
    led_strip_clear(s_led_ctx.strip);
    
    ESP_LOGI(TAG, "WS2812 初始化完成 (GPIO%d)", WS2812_GPIO);
    return ESP_OK;
}

/**
 * @brief 反初始化
 */
esp_err_t ws2812_led_deinit(void)
{
    if (!s_led_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // 停止闪烁任务
    ws2812_led_stop_blink();
    
    // 关闭 LED
    led_strip_clear(s_led_ctx.strip);
    
    // 释放资源（led_strip 没有直接的 deinit 函数）
    s_led_ctx.initialized = false;
    
    ESP_LOGI(TAG, "WS2812 已反初始化");
    return ESP_OK;
}

/**
 * @brief 设置 LED 颜色
 */
esp_err_t ws2812_led_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    if (!s_led_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // 停止之前的闪烁
    ws2812_led_stop_blink();
    
    s_led_ctx.red = red;
    s_led_ctx.green = green;
    s_led_ctx.blue = blue;
    
    led_strip_set_pixel(s_led_ctx.strip, 0, red, green, blue);
    led_strip_refresh(s_led_ctx.strip);
    
    s_led_ctx.current_mode = WS2812_MODE_STATIC;
    
    return ESP_OK;
}

/**
 * @brief 关闭 LED
 */
esp_err_t ws2812_led_off(void)
{
    if (!s_led_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ws2812_led_stop_blink();
    led_strip_clear(s_led_ctx.strip);
    
    s_led_ctx.red = 0;
    s_led_ctx.green = 0;
    s_led_ctx.blue = 0;
    s_led_ctx.current_mode = WS2812_MODE_OFF;
    
    return ESP_OK;
}

/**
 * @brief 开始闪烁
 */
esp_err_t ws2812_led_start_blink(uint8_t red, uint8_t green, uint8_t blue)
{
    if (!s_led_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // 如果已经在闪烁，先停止
    ws2812_led_stop_blink();
    
    s_led_ctx.red = red;
    s_led_ctx.green = green;
    s_led_ctx.blue = blue;
    s_led_ctx.blink_running = true;
    
    // 创建闪烁任务
    BaseType_t ret = xTaskCreate(blink_task, "ws2812_blink", 2048, &s_led_ctx, 5, &s_led_ctx.blink_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建闪烁任务失败");
        s_led_ctx.blink_running = false;
        return ESP_ERR_NO_MEM;
    }
    
    s_led_ctx.current_mode = WS2812_MODE_BLINK;
    
    return ESP_OK;
}

/**
 * @brief 停止闪烁
 */
esp_err_t ws2812_led_stop_blink(void)
{
    if (!s_led_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (s_led_ctx.blink_running && s_led_ctx.blink_task) {
        s_led_ctx.blink_running = false;
        // 等待任务自行退出
        vTaskDelay(pdMS_TO_TICKS(100));
        s_led_ctx.blink_task = NULL;
    }
    
    return ESP_OK;
}

/**
 * @brief 设置预设模式
 */
esp_err_t ws2812_led_set_mode(ws2812_mode_t mode)
{
    switch (mode) {
    case WS2812_MODE_OFF:
        return ws2812_led_off();
        
    case WS2812_MODE_NORMAL:
        // 正常模式：蓝色常亮
        return ws2812_led_set_color(0, 0, WS2812_LED_BRIGHTNESS);

    case WS2812_MODE_PAIRING:
        // 配对模式：红色慢闪
        return ws2812_led_start_blink(WS2812_LED_BRIGHTNESS, 0, 0);

    case WS2812_MODE_CONNECTED:
        // 连接成功：绿色常亮
        return ws2812_led_set_color(0, WS2812_LED_BRIGHTNESS, 0);

    case WS2812_MODE_ERROR:
        // 错误：红色常亮
        return ws2812_led_set_color(WS2812_LED_BRIGHTNESS, 0, 0);
        
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

/**
 * @brief 获取当前模式
 */
ws2812_mode_t ws2812_led_get_mode(void)
{
    return s_led_ctx.current_mode;
}
