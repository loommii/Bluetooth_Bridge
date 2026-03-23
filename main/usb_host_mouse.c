/*
 * SPDX-FileCopyrightText: 2024-2026 ESP32 USB Host Mouse Project
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "usb/usb_host.h"
#include "usb/hid_host.h"
#include "esp_hidd.h"

#include "usb_host_mouse.h"

static const char *TAG = "USB_HOST_MOUSE";

// USB Host 鼠标驱动上下文
typedef struct {
    hid_host_device_handle_t hid_dev_hdl;     ///< HID 设备句柄
    usb_mouse_event_cb_t user_callback;       ///< 用户鼠标事件回调
    void *user_data;                          ///< 用户数据
    TaskHandle_t task_hdl;                    ///< 监控任务句柄
    bool is_connected;                        ///< 连接状态
    bool is_started;                          ///< 是否已启动
} usb_mouse_ctx_t;

// 全局上下文
static usb_mouse_ctx_t s_mouse_ctx = {0};

// 前向声明
static void hid_interface_event_callback(hid_host_device_handle_t hid_device_handle,
                                         const hid_host_interface_event_t event,
                                         void *arg);
static void usb_mouse_bridge_callback(const usb_mouse_report_t *report, void *user_data);

/**
 * @brief USB Host 守护任务
 */
static void usb_daemon_task(void *arg)
{
    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_LOGD(TAG, "All clients deregistered");
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGD(TAG, "All devices freed");
        }
    }
}

/**
 * @brief 解析 HID 鼠标报告数据
 */
static void parse_mouse_report(const uint8_t *report, uint8_t report_len, usb_mouse_report_t *mouse_report)
{
    if (report_len < 3) {
        ESP_LOGW(TAG, "Invalid report length: %d", report_len);
        return;
    }

    // 字节 0: 按键
    mouse_report->buttons = report[0];
    
    // 字节 1: X 轴位移
    mouse_report->x = (report_len >= 2) ? (int8_t)report[1] : 0;
    
    // 字节 2: Y 轴位移
    mouse_report->y = (report_len >= 3) ? (int8_t)report[2] : 0;
    
    // 字节 3: 滚轮
    mouse_report->wheel = (report_len >= 4) ? (int8_t)report[3] : 0;
}

/**
 * @brief 打印鼠标事件
 */
static void print_mouse_event(const usb_mouse_report_t *report)
{
    // 已屏蔽：鼠标移动日志太多，影响观察配对/连接日志
    // 如需调试，取消下面注释
    #if 0
    // 只在按键状态变化时打印 INFO 日志
    static uint8_t last_buttons = 0;

    if (report->buttons != last_buttons) {
        ESP_LOGI(TAG, "Mouse: btn=0x%02X, X=%+d, Y=%+d, W=%+d",
                 report->buttons, report->x, report->y, report->wheel);
        last_buttons = report->buttons;
    } else {
        // 移动时使用 DEBUG 级别
        ESP_LOGD(TAG, "Mouse: X=%+d, Y=%+d, W=%+d",
                 report->x, report->y, report->wheel);
    }
    #endif
}

/**
 * @brief HID 设备事件回调
 */
static void hid_device_event_callback(hid_host_device_handle_t hid_device_handle,
                                      const hid_host_driver_event_t event,
                                      void *arg)
{
    usb_mouse_ctx_t *ctx = (usb_mouse_ctx_t *)arg;

    switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
        ESP_LOGI(TAG, "HID 设备连接事件");

        // 打开设备
        hid_host_device_config_t dev_config = {
            .callback = hid_interface_event_callback,
            .callback_arg = ctx,
        };

        esp_err_t ret = hid_host_device_open(hid_device_handle, &dev_config);
        if (ret == ESP_OK) {
            ctx->hid_dev_hdl = hid_device_handle;
            ctx->is_connected = true;

            // 获取设备参数
            hid_host_dev_params_t dev_params;
            if (hid_host_device_get_params(hid_device_handle, &dev_params) == ESP_OK) {
                ESP_LOGI(TAG, "HID 设备已打开：地址=%d, 接口=%d, 协议=%d",
                         dev_params.addr, dev_params.iface_num, dev_params.proto);
            }

            // 启动设备
            hid_host_device_start(hid_device_handle);
            ctx->is_started = true;

            ESP_LOGI(TAG, "USB HID 鼠标已就绪!");
        } else {
            ESP_LOGE(TAG, "打开 HID 设备失败：%s", esp_err_to_name(ret));
        }
        break;

    default:
        ESP_LOGW(TAG, "未知 HID 事件：%d", event);
        break;
    }
}

/**
 * @brief HID 接口事件回调（处理输入报告）
 */
static void hid_interface_event_callback(hid_host_device_handle_t hid_device_handle,
                                         const hid_host_interface_event_t event,
                                         void *arg)
{
    usb_mouse_ctx_t *ctx = (usb_mouse_ctx_t *)arg;

    if (!ctx->is_connected || !ctx->hid_dev_hdl) {
        return;
    }

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT: {
        // 获取输入报告数据
        uint8_t report[8];
        size_t report_len = 0;

        esp_err_t ret = hid_host_device_get_raw_input_report_data(
            hid_device_handle, report, sizeof(report), &report_len);

        if (ret == ESP_OK && report_len > 0) {
            // 已屏蔽：原始报告日志太频繁
            // ESP_LOGD(TAG, "USB Raw Report (len=%d): %02x %02x %02x %02x %02x %02x %02x %02x",
            //          report_len,
            //          report[0], report[1], report[2], report[3],
            //          report[4], report[5], report[6], report[7]);

            usb_mouse_report_t mouse_report = {0};
            parse_mouse_report(report, report_len, &mouse_report);

            // 打印解析后的鼠标事件（已屏蔽）
            print_mouse_event(&mouse_report);

            // 调用用户回调
            if (ctx->user_callback) {
                ctx->user_callback(&mouse_report, ctx->user_data);
            }

            // 桥接：将 USB 鼠标数据转发到蓝牙
            usb_mouse_bridge_callback(&mouse_report, NULL);
        }
        break;
    }

    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HID 设备已断开");
        ctx->is_connected = false;
        ctx->is_started = false;
        ctx->hid_dev_hdl = NULL;
        break;

    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGW(TAG, "HID 传输错误");
        break;

#ifdef HID_HOST_SUSPEND_RESUME_API_SUPPORTED
    case HID_HOST_INTERFACE_EVENT_SUSPENDED:
        ESP_LOGD(TAG, "HID 设备已挂起");
        break;

    case HID_HOST_INTERFACE_EVENT_RESUMED:
        ESP_LOGD(TAG, "HID 设备已恢复");
        break;
#endif

    default:
        ESP_LOGW(TAG, "未知接口事件：%d", event);
        break;
    }
}

/**
 * @brief 初始化 USB Host 鼠标驱动
 */
esp_err_t usb_host_mouse_init(usb_mouse_event_cb_t callback, void *user_data)
{
    esp_err_t ret;

    if (s_mouse_ctx.hid_dev_hdl) {
        ESP_LOGW(TAG, "已初始化");
        return ESP_ERR_INVALID_STATE;
    }

    // 初始化上下文
    memset(&s_mouse_ctx, 0, sizeof(s_mouse_ctx));
    s_mouse_ctx.user_callback = callback;
    s_mouse_ctx.user_data = user_data;

    // 1. 先安装 USB Host 驱动
    usb_host_config_t usb_host_config = {
        .skip_phy_setup = false,  // 使用内部 PHY（ESP32-S3 内置 USB OTG）
    };

    ret = usb_host_install(&usb_host_config);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "安装 USB Host 失败：%s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "USB Host 已安装");

    // 2. 创建 USB Host 守护任务（增加栈大小到 4KB）
    xTaskCreate(usb_daemon_task, "usb_daemon", 4096, NULL, 5, NULL);

    // 3. 安装 HID Host 驱动（增加任务栈大小到 4KB）
    hid_host_driver_config_t hid_config = {
        .create_background_task = true,      // 创建后台任务处理 USB 事件
        .task_priority = 5,
        .stack_size = 4096,                  // 增加栈大小防止溢出
        .core_id = tskNO_AFFINITY,
        .callback = hid_device_event_callback,
        .callback_arg = &s_mouse_ctx,
    };

    ret = hid_host_install(&hid_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "安装 HID Host 失败：%s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "HID Host 已安装");

    return ESP_OK;
}

/**
 * @brief 反初始化 USB Host 鼠标驱动
 */
esp_err_t usb_host_mouse_deinit(void)
{
    if (!s_mouse_ctx.hid_dev_hdl && !s_mouse_ctx.is_connected) {
        ESP_LOGW(TAG, "未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    // 停止任务
    usb_host_mouse_stop();

    // 关闭设备
    if (s_mouse_ctx.hid_dev_hdl) {
        hid_host_device_close(s_mouse_ctx.hid_dev_hdl);
        s_mouse_ctx.hid_dev_hdl = NULL;
    }

    // 卸载 HID Host
    hid_host_uninstall();

    memset(&s_mouse_ctx, 0, sizeof(s_mouse_ctx));

    ESP_LOGI(TAG, "USB Host Mouse 已反初始化");

    return ESP_OK;
}

/**
 * @brief USB Host 鼠标监控任务
 */
static void usb_host_mouse_monitor_task(void *arg)
{
    usb_mouse_ctx_t *ctx = (usb_mouse_ctx_t *)arg;

    while (1) {
        if (ctx->is_connected && ctx->hid_dev_hdl && !ctx->is_started) {
            // 设备已连接但未启动，重新启动
            hid_host_device_start(ctx->hid_dev_hdl);
            ctx->is_started = true;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief 启动鼠标监控任务
 */
esp_err_t usb_host_mouse_start(size_t stack_size, UBaseType_t priority)
{
    if (s_mouse_ctx.task_hdl) {
        ESP_LOGW(TAG, "任务已启动");
        return ESP_ERR_INVALID_STATE;
    }

    // HID 事件处理已经在后台任务中完成，不需要额外任务
    // 这里创建一个监控任务用于处理连接状态
    BaseType_t ret = xTaskCreate(usb_host_mouse_monitor_task,
                                  "usb_mouse_mon",
                                  stack_size,
                                  &s_mouse_ctx,
                                  priority,
                                  &s_mouse_ctx.task_hdl);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "创建任务失败");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "鼠标监控任务已启动");

    return ESP_OK;
}

/**
 * @brief 停止鼠标监控任务
 */
esp_err_t usb_host_mouse_stop(void)
{
    if (!s_mouse_ctx.task_hdl) {
        return ESP_OK;
    }

    // 停止设备
    if (s_mouse_ctx.hid_dev_hdl && s_mouse_ctx.is_started) {
        hid_host_device_stop(s_mouse_ctx.hid_dev_hdl);
        s_mouse_ctx.is_started = false;
    }

    // 删除任务
    vTaskDelete(s_mouse_ctx.task_hdl);
    s_mouse_ctx.task_hdl = NULL;

    ESP_LOGI(TAG, "鼠标监控任务已停止");

    return ESP_OK;
}

/**
 * @brief 获取鼠标连接状态
 */
bool usb_host_mouse_is_connected(void)
{
    return s_mouse_ctx.is_connected;
}

// ==================== USB 转蓝牙桥接功能 ====================

// 桥接上下文
typedef struct {
    esp_hidd_dev_t *ble_hid_dev;     ///< 蓝牙 HID 设备句柄
    bool bridge_enabled;             ///< 桥接使能
    uint32_t connect_tick;           ///< 连接建立时间戳
    uint32_t last_send_tick;         ///< 上次发送时间戳 (ms)
    uint8_t last_buttons;            ///< 上次按键状态
    int8_t last_x;                   ///< 上次 X 轴
    int8_t last_y;                   ///< 上次 Y 轴
    int8_t last_wheel;               ///< 上次滚轮
} usb_bridge_ctx_t;

static usb_bridge_ctx_t s_bridge_ctx = {0};

// 最小发送间隔 (ms) - 防止发送过快导致拥塞
#define BRIDGE_SEND_INTERVAL_MS  20

// 连接稳定延迟 (ms) - 连接建立后等待这么久再发送数据，让加密和服务发现完成
// Windows 蓝牙连接后需要时间完成加密流程，太早发送会导致断开
#define BRIDGE_CONNECT_STABLE_DELAY_MS  1000

/**
 * @brief 设置桥接目标（蓝牙 HID 设备）
 */
esp_err_t usb_host_mouse_set_bridge_target(esp_hidd_dev_t *hid_dev)
{
    if (hid_dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_bridge_ctx.ble_hid_dev = hid_dev;
    // 重置状态
    s_bridge_ctx.connect_tick = xTaskGetTickCount() * portTICK_PERIOD_MS;
    s_bridge_ctx.last_send_tick = 0;
    s_bridge_ctx.last_buttons = 0;
    s_bridge_ctx.last_x = 0;
    s_bridge_ctx.last_y = 0;
    s_bridge_ctx.last_wheel = 0;

    ESP_LOGD(TAG, "桥接目标已设置：%p", (void*)hid_dev);

    return ESP_OK;
}

/**
 * @brief 启用 USB 到蓝牙的桥接
 */
esp_err_t usb_host_mouse_enable_bridge(void)
{
    if (s_bridge_ctx.ble_hid_dev == NULL) {
        ESP_LOGW(TAG, "未设置桥接目标");
        return ESP_ERR_INVALID_STATE;
    }

    s_bridge_ctx.bridge_enabled = true;
    ESP_LOGD(TAG, "USB 转蓝牙桥接已启用");

    return ESP_OK;
}

/**
 * @brief 禁用 USB 到蓝牙的桥接
 */
esp_err_t usb_host_mouse_disable_bridge(void)
{
    s_bridge_ctx.bridge_enabled = false;
    ESP_LOGD(TAG, "USB 转蓝牙桥接已禁用");

    return ESP_OK;
}

/**
 * @brief 获取桥接状态
 */
bool usb_host_mouse_is_bridge_enabled(void)
{
    return s_bridge_ctx.bridge_enabled;
}

/**
 * @brief 发送鼠标数据到蓝牙 HID 设备
 */
esp_err_t usb_host_mouse_send_to_ble(esp_hidd_dev_t *hid_dev,
                                      uint8_t buttons,
                                      int8_t dx,
                                      int8_t dy,
                                      int8_t wheel)
{
    if (hid_dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 检查连接稳定延迟 - 连接后等待一段时间再发送
    uint32_t current_tick = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (current_tick - s_bridge_ctx.connect_tick < BRIDGE_CONNECT_STABLE_DELAY_MS) {
        ESP_LOGD(TAG, "跳过发送：连接未稳定 (%d ms)",
                 current_tick - s_bridge_ctx.connect_tick);
        return ESP_ERR_INVALID_STATE;
    }

    // 检查发送间隔限制
    if (current_tick - s_bridge_ctx.last_send_tick < BRIDGE_SEND_INTERVAL_MS) {
        // 时间间隔太短，跳过发送（除非按键状态变化）
        if (buttons == s_bridge_ctx.last_buttons &&
            dx == 0 && dy == 0 && wheel == 0) {
            return ESP_OK;
        }
    }

    // 构建鼠标报告数据（4 字节）
    uint8_t buffer[4] = {0};

    // USB 鼠标按键格式转换到蓝牙 HID 格式
    uint8_t hid_buttons = 0;
    if (buttons & 0x01) hid_buttons |= 0x01;  // USB 左 -> HID 左
    if (buttons & 0x02) hid_buttons |= 0x02;  // USB 右 -> HID 右
    if (buttons & 0x04) hid_buttons |= 0x04;  // USB 中 -> HID 中

    buffer[0] = hid_buttons;
    buffer[1] = (uint8_t)dx;
    buffer[2] = (uint8_t)dy;
    buffer[3] = (uint8_t)wheel;

    // 发送到蓝牙 HID 设备
    esp_err_t ret = esp_hidd_dev_input_set(hid_dev, 0, 0, buffer, 4);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "发送到蓝牙失败：%s", esp_err_to_name(ret));
    } else {
        // 更新发送状态
        s_bridge_ctx.last_send_tick = current_tick;
        s_bridge_ctx.last_buttons = buttons;
        s_bridge_ctx.last_x = dx;
        s_bridge_ctx.last_y = dy;
        s_bridge_ctx.last_wheel = wheel;
    }

    return ret;
}

/**
 * @brief 桥接回调函数 - 将 USB 鼠标数据转发到蓝牙
 */
static void usb_mouse_bridge_callback(const usb_mouse_report_t *report, void *user_data)
{
    if (!s_bridge_ctx.bridge_enabled || !s_bridge_ctx.ble_hid_dev) {
        return;
    }

    // 直接转发所有数据（包括按键释放事件）
    usb_host_mouse_send_to_ble(
        s_bridge_ctx.ble_hid_dev,
        report->buttons,
        report->x,
        report->y,
        report->wheel
    );
}
