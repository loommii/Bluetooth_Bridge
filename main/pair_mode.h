/*
 * SPDX-FileCopyrightText: 2024-2026 Bluetooth Bridge Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef _PAIR_MODE_H_
#define _PAIR_MODE_H_

#include <stdbool.h>
#include "esp_err.h"
#include "hal/gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 配对模式回调函数类型
 * @param active 配对模式是否激活
 * @param user_data 用户数据
 */
typedef void (*pair_mode_callback_t)(bool active, void *user_data);

/**
 * @brief 初始化配对模式检测
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return esp_err_t
 */
esp_err_t pair_mode_init(pair_mode_callback_t callback, void *user_data);

/**
 * @brief 反初始化配对模式检测
 * @return esp_err_t
 */
esp_err_t pair_mode_deinit(void);

/**
 * @brief 获取当前配对模式状态
 * @return true: 配对模式激活，false: 正常模式
 */
bool pair_mode_is_active(void);

/**
 * @brief 手动设置配对模式（用于软件触发）
 * @param active true: 激活，false: 停用
 */
void pair_mode_set_active(bool active);

/**
 * @brief 获取配对检测 GPIO 编号
 * @return gpio_num_t
 */
gpio_num_t pair_mode_get_gpio(void);

#ifdef __cplusplus
}
#endif

#endif /* _PAIR_MODE_H_ */
