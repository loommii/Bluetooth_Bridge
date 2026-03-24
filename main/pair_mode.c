/*
 * SPDX-FileCopyrightText: 2024-2026 Bluetooth Bridge Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

#include "pair_mode.h"

static const char *TAG = "PAIR_MODE";

// 配对模式上下文
typedef struct {
    bool is_pairing_mode;      // 是否处于配对模式
    gpio_num_t pair_gpio;      // 配对检测 GPIO
    TaskHandle_t monitor_task; // 监控任务句柄
    pair_mode_callback_t callback; // 回调函数
    void *user_data;           // 用户数据
} pair_mode_ctx_t;

static pair_mode_ctx_t s_pair_ctx = {0};

// 默认配置
#define PAIR_GPIO_DEFAULT     GPIO_NUM_1
#define PAIR_CHECK_INTERVAL   100  // 检查间隔 100ms

/**
 * @brief 配对模式监控任务
 */
static void pair_mode_monitor_task(void *arg)
{
    pair_mode_ctx_t *ctx = (pair_mode_ctx_t *)arg;
    bool last_state = gpio_get_level(ctx->pair_gpio);
    
    ESP_LOGI(TAG, "配对模式监控已启动 (GPIO%d)", ctx->pair_gpio);
    
    while (1) {
        bool current_state = gpio_get_level(ctx->pair_gpio);
        
        // 检测到低电平（杜邦线连接 GND）
        if (current_state == 0 && last_state == 1) {
            ESP_LOGI(TAG, "检测到配对模式触发！");
            ctx->is_pairing_mode = true;
            
            // 调用回调
            if (ctx->callback) {
                ctx->callback(true, ctx->user_data);
            }
        }
        // 检测到高电平（杜邦线断开）
        else if (current_state == 1 && last_state == 0) {
            ESP_LOGI(TAG, "退出配对模式");
            ctx->is_pairing_mode = false;
            
            // 调用回调
            if (ctx->callback) {
                ctx->callback(false, ctx->user_data);
            }
        }
        
        last_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(PAIR_CHECK_INTERVAL));
    }
}

/**
 * @brief 初始化配对模式检测
 */
esp_err_t pair_mode_init(pair_mode_callback_t callback, void *user_data)
{
    if (s_pair_ctx.monitor_task) {
        ESP_LOGW(TAG, "已初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 保存配置
    s_pair_ctx.pair_gpio = PAIR_GPIO_DEFAULT;
    s_pair_ctx.callback = callback;
    s_pair_ctx.user_data = user_data;
    s_pair_ctx.is_pairing_mode = false;
    
    // 配置 GPIO：上拉输入模式
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << s_pair_ctx.pair_gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,   // 启用上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO 配置失败：%s", esp_err_to_name(ret));
        return ret;
    }
    
    // 创建监控任务
    BaseType_t xRet = xTaskCreate(pair_mode_monitor_task, 
                                   "pair_mode_mon", 
                                   2048, 
                                   &s_pair_ctx, 
                                   5, 
                                   &s_pair_ctx.monitor_task);
    
    if (xRet != pdPASS) {
        ESP_LOGE(TAG, "创建监控任务失败");
        s_pair_ctx.monitor_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "配对模式检测已初始化");
    return ESP_OK;
}

/**
 * @brief 反初始化配对模式检测
 */
esp_err_t pair_mode_deinit(void)
{
    if (!s_pair_ctx.monitor_task) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // 删除任务
    vTaskDelete(s_pair_ctx.monitor_task);
    s_pair_ctx.monitor_task = NULL;
    
    // 重置 GPIO
    gpio_reset_pin(s_pair_ctx.pair_gpio);
    
    memset(&s_pair_ctx, 0, sizeof(s_pair_ctx));
    
    ESP_LOGI(TAG, "配对模式检测已反初始化");
    return ESP_OK;
}

/**
 * @brief 获取当前配对模式状态
 */
bool pair_mode_is_active(void)
{
    return s_pair_ctx.is_pairing_mode;
}

/**
 * @brief 手动设置配对模式（用于软件触发）
 */
void pair_mode_set_active(bool active)
{
    s_pair_ctx.is_pairing_mode = active;
    ESP_LOGI(TAG, "配对模式已%s", active ? "启用" : "禁用");
}

/**
 * @brief 获取配对检测 GPIO 编号
 */
gpio_num_t pair_mode_get_gpio(void)
{
    return s_pair_ctx.pair_gpio;
}
