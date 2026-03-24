/*
 * SPDX-FileCopyrightText: 2024-2026 Bluetooth Bridge Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef _WS2812_LED_H_
#define _WS2812_LED_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED 模式枚举
 */
typedef enum {
    WS2812_MODE_OFF = 0,       ///< 关闭
    WS2812_MODE_NORMAL,        ///< 正常模式（蓝色）
    WS2812_MODE_PAIRING,       ///< 配对模式（红色闪烁）
    WS2812_MODE_CONNECTED,     ///< 已连接（绿色）
    WS2812_MODE_ERROR,         ///< 错误（红色）
    WS2812_MODE_STATIC,        ///< 静态颜色（内部使用）
    WS2812_MODE_BLINK,         ///< 闪烁（内部使用）
} ws2812_mode_t;

/**
 * @brief 初始化 WS2812 LED
 * @return esp_err_t
 */
esp_err_t ws2812_led_init(void);

/**
 * @brief 反初始化 WS2812 LED
 * @return esp_err_t
 */
esp_err_t ws2812_led_deinit(void);

/**
 * @brief 设置 LED 颜色
 * @param red 红色分量 (0-255)
 * @param green 绿色分量 (0-255)
 * @param blue 蓝色分量 (0-255)
 * @return esp_err_t
 */
esp_err_t ws2812_led_set_color(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 关闭 LED
 * @return esp_err_t
 */
esp_err_t ws2812_led_off(void);

/**
 * @brief 开始闪烁
 * @param red 红色分量 (0-255)
 * @param green 绿色分量 (0-255)
 * @param blue 蓝色分量 (0-255)
 * @return esp_err_t
 */
esp_err_t ws2812_led_start_blink(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief 停止闪烁
 * @return esp_err_t
 */
esp_err_t ws2812_led_stop_blink(void);

/**
 * @brief 设置预设模式
 * @param mode 模式枚举
 * @return esp_err_t
 */
esp_err_t ws2812_led_set_mode(ws2812_mode_t mode);

/**
 * @brief 获取当前模式
 * @return ws2812_mode_t
 */
ws2812_mode_t ws2812_led_get_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* _WS2812_LED_H_ */
