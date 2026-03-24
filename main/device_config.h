/*
 * SPDX-FileCopyrightText: 2024-2026 Bluetooth Bridge Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/**
 * @file device_config.h
 * @brief 设备信息配置文件
 * 
 * 用于伪装蓝牙鼠标为其他厂商的设备
 * 修改此文件后重新编译即可
 */

#ifndef _DEVICE_CONFIG_H_
#define _DEVICE_CONFIG_H_

// ==================== 设备信息配置 ====================

/**
 * @brief 厂商 ID (Vendor ID)
 * 
 * 常见厂商 VID:
 * - 0x046D: Logitech (罗技)
 * - 0x413C: Dell (戴尔)
 * - 0x045E: Microsoft (微软)
 * - 0x04F2: Chicony
 * - 0x1532: Razer (雷蛇)
 */
#define DEVICE_VENDOR_ID        0x413C  // Dell

/**
 * @brief 产品 ID (Product ID)
 * 
 * Dell 鼠标常见 PID:
 * - 0x3016: Dell MS116 / 5-Button Mouse
 * - 0x2501: Dell MS116 Wireless
 * - 0x3012: Dell Optical Mouse
 * - 0x301A: Dell Premium Mouse
 */
#define DEVICE_PRODUCT_ID       0x3016  // Dell GM3323D (使用 MS116 的 PID)

/**
 * @brief 设备版本号 (BCD 格式)
 * 例如：0x0100 = 1.00
 */
#define DEVICE_VERSION          0x0100

/**
 * @brief 设备名称 (蓝牙广播名称)
 * 最大 31 字符 (BLE 广播限制)
 *
 * 建议值:
 * - "Dell GM3323D"
 * - "Dell MS116 Mouse"
 * - "Dell Gaming Mouse"
 */
#define DEVICE_NAME             "Dell GM3323D"

/**
 * @brief 制造商名称
 * 
 * 建议值:
 * - "Dell Inc."
 * - "DELL"
 */
#define MANUFACTURER_NAME       "Dell Inc."

/**
 * @brief 序列号
 * 可以自定义，用于唯一标识设备
 */
#define SERIAL_NUMBER           "GM3323D-2024"

// ==================== 高级配置 ====================

/**
 * @brief 鼠标 DPI 设置
 * 范围：100-16000
 * 
 * Dell GM3323D 参数:
 * - 最大 DPI: 5000
 * - 默认 DPI: 1000
 */
#define MOUSE_DPI               1000

/**
 * @brief 鼠标轮询率 (Hz)
 * 可选值：125, 250, 500, 1000
 * 
 * Dell GM3323D 参数:
 * - 轮询率：1000 Hz
 */
#define MOUSE_POLLING_RATE      1000

/**
 * @brief MAC 地址随机化
 * 启用后每次上电使用不同的 MAC 地址
 * 
 * 可选值：
 * - 1: 启用 (推荐，防止被追踪)
 * - 0: 禁用 (使用固定 MAC)
 */
#define ENABLE_MAC_RANDOMIZATION  1

// ==================== LED 配置 ====================

/**
 * @brief WS2812 LED GPIO 引脚
 */
#define WS2812_GPIO             48

/**
 * @brief LED 亮度 (0-255)
 */
#define WS2812_BRIGHTNESS       50

// ==================== 配对模式配置 ====================

/**
 * @brief 配对模式检测 GPIO
 * 连接到此 GPIO 和 GND 触发配对模式
 */
#define PAIR_MODE_GPIO          1

#endif /* _DEVICE_CONFIG_H_ */
