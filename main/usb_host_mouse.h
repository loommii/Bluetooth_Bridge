/*
 * SPDX-FileCopyrightText: 2024-2026 ESP32 USB Host Mouse Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

// 前向声明
typedef struct esp_hidd_dev_s esp_hidd_dev_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB 鼠标报告数据结构
 */
typedef struct {
    uint8_t buttons;    ///< 按键状态：bit0=左键，bit1=右键，bit2=中键
    int8_t x;           ///< X 轴位移 (-127~127)
    int8_t y;           ///< Y 轴位移 (-127~127)
    int8_t wheel;       ///< 滚轮位移 (-127~127)
} usb_mouse_report_t;

/**
 * @brief 鼠标按键掩码
 */
#define USB_MOUSE_BUTTON_LEFT   0x01
#define USB_MOUSE_BUTTON_RIGHT  0x02
#define USB_MOUSE_BUTTON_MIDDLE 0x04

/**
 * @brief 鼠标事件回调函数类型
 */
typedef void (*usb_mouse_event_cb_t)(const usb_mouse_report_t *report, void *user_data);

/**
 * @brief 初始化 USB Host 鼠标驱动
 *
 * @param callback 鼠标事件回调函数
 * @param user_data 用户数据指针
 * @return esp_err_t
 *         - ESP_OK: 成功
 *         - ESP_ERR_NO_MEM: 内存不足
 *         - ESP_ERR_INVALID_STATE: USB Host 未安装
 */
esp_err_t usb_host_mouse_init(usb_mouse_event_cb_t callback, void *user_data);

/**
 * @brief 反初始化 USB Host 鼠标驱动
 *
 * @return esp_err_t
 *         - ESP_OK: 成功
 *         - ESP_ERR_INVALID_STATE: 驱动未初始化
 */
esp_err_t usb_host_mouse_deinit(void);

/**
 * @brief 启动鼠标监控任务
 *
 * @param stack_size 任务栈大小（字节）
 * @param priority 任务优先级
 * @return esp_err_t
 *         - ESP_OK: 成功
 *         - ESP_ERR_NO_MEM: 内存不足
 */
esp_err_t usb_host_mouse_start(size_t stack_size, UBaseType_t priority);

/**
 * @brief 停止鼠标监控任务
 *
 * @return esp_err_t
 *         - ESP_OK: 成功
 */
esp_err_t usb_host_mouse_stop(void);

/**
 * @brief 获取鼠标连接状态
 *
 * @return true: 已连接
 * @return false: 未连接
 */
bool usb_host_mouse_is_connected(void);

// ==================== USB 转蓝牙桥接功能 ====================

/**
 * @brief 设置桥接目标（蓝牙 HID 设备）
 *
 * @param hid_dev 蓝牙 HID 设备句柄
 * @return esp_err_t
 *         - ESP_OK: 成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 */
esp_err_t usb_host_mouse_set_bridge_target(esp_hidd_dev_t *hid_dev);

/**
 * @brief 启用 USB 到蓝牙的桥接
 *
 * @return esp_err_t
 *         - ESP_OK: 成功
 */
esp_err_t usb_host_mouse_enable_bridge(void);

/**
 * @brief 禁用 USB 到蓝牙的桥接
 *
 * @return esp_err_t
 *         - ESP_OK: 成功
 */
esp_err_t usb_host_mouse_disable_bridge(void);

/**
 * @brief 获取桥接状态
 *
 * @return true: 桥接已启用
 * @return false: 桥接已禁用
 */
bool usb_host_mouse_is_bridge_enabled(void);

/**
 * @brief 发送鼠标数据到蓝牙 HID 设备
 *
 * @param hid_dev 蓝牙 HID 设备句柄
 * @param buttons 按键状态
 * @param dx X 轴位移
 * @param dy Y 轴位移
 * @param wheel 滚轮位移
 * @return esp_err_t
 *         - ESP_OK: 成功
 *         - ESP_ERR_INVALID_STATE: 蓝牙设备未就绪
 */
esp_err_t usb_host_mouse_send_to_ble(esp_hidd_dev_t *hid_dev, 
                                      uint8_t buttons, 
                                      int8_t dx, 
                                      int8_t dy, 
                                      int8_t wheel);

#ifdef __cplusplus
}
#endif
