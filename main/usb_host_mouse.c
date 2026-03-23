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
 * 标准 HID 鼠标报告格式 (5 字节):
 * [0] Buttons: bit0=左键，bit1=右键，bit2=中键
 * [1] X 轴位移 (有符号)
 * [2] Y 轴位移 (有符号)
 * [3] 滚轮位移 (有符号)
 */
static void parse_mouse_report(const uint8_t *report, uint8_t report_len, usb_mouse_report_t *mouse_report)
{
    if (report_len < 3) {
        ESP_LOGW(TAG, "Invalid report length: %d", report_len);
        return;
    }

    mouse_report->buttons = report[0];
    mouse_report->x = (int8_t)report[1];
    mouse_report->y = (int8_t)report[2];
    
    // 如果有滚轮数据（5 字节报告）
    if (report_len >= 4) {
        mouse_report->wheel = (int8_t)report[3];
    } else {
        mouse_report->wheel = 0;
    }
}

/**
 * @brief 打印鼠标事件
 */
static void print_mouse_event(const usb_mouse_report_t *report)
{
    ESP_LOGI(TAG, "Mouse: Buttons=0x%02X%s%s%s, X=%+4d, Y=%+4d, Wheel=%+4d",
             report->buttons,
             (report->buttons & USB_MOUSE_BUTTON_LEFT) ? " [LEFT]" : "",
             (report->buttons & USB_MOUSE_BUTTON_RIGHT) ? " [RIGHT]" : "",
             (report->buttons & USB_MOUSE_BUTTON_MIDDLE) ? " [MIDDLE]" : "",
             report->x,
             report->y,
             report->wheel);
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
        ESP_LOGI(TAG, "HID device connected event");
        
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
                ESP_LOGI(TAG, "HID device opened: addr=%d, iface=%d, proto=%d",
                         dev_params.addr, dev_params.iface_num, dev_params.proto);
            }
            
            // 启动设备
            hid_host_device_start(hid_device_handle);
            ctx->is_started = true;
            
            ESP_LOGI(TAG, "USB HID Mouse ready!");
        } else {
            ESP_LOGE(TAG, "Failed to open HID device: %s", esp_err_to_name(ret));
        }
        break;

    default:
        ESP_LOGW(TAG, "Unknown HID event: %d", event);
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
            usb_mouse_report_t mouse_report = {0};
            parse_mouse_report(report, report_len, &mouse_report);
            
            // 打印鼠标事件
            print_mouse_event(&mouse_report);
            
            // 调用用户回调
            if (ctx->user_callback) {
                ctx->user_callback(&mouse_report, ctx->user_data);
            }
        }
        break;
    }
    
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HID device disconnected");
        ctx->is_connected = false;
        ctx->is_started = false;
        ctx->hid_dev_hdl = NULL;
        break;
        
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGW(TAG, "HID transfer error");
        break;
        
#ifdef HID_HOST_SUSPEND_RESUME_API_SUPPORTED
    case HID_HOST_INTERFACE_EVENT_SUSPENDED:
        ESP_LOGD(TAG, "HID device suspended");
        break;
        
    case HID_HOST_INTERFACE_EVENT_RESUMED:
        ESP_LOGD(TAG, "HID device resumed");
        break;
#endif
        
    default:
        ESP_LOGW(TAG, "Unknown interface event: %d", event);
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
        ESP_LOGW(TAG, "Already initialized");
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
        ESP_LOGE(TAG, "Failed to install USB Host: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "USB Host installed");

    // 2. 创建 USB Host 守护任务
    xTaskCreate(usb_daemon_task, "usb_daemon", 2048, NULL, 5, NULL);

    // 3. 安装 HID Host 驱动
    hid_host_driver_config_t hid_config = {
        .create_background_task = true,      // 创建后台任务处理 USB 事件
        .task_priority = 5,
        .stack_size = 2048,
        .core_id = tskNO_AFFINITY,
        .callback = hid_device_event_callback,
        .callback_arg = &s_mouse_ctx,
    };

    ret = hid_host_install(&hid_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install HID Host: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "HID Host installed");

    return ESP_OK;
}

/**
 * @brief 反初始化 USB Host 鼠标驱动
 */
esp_err_t usb_host_mouse_deinit(void)
{
    if (!s_mouse_ctx.hid_dev_hdl && !s_mouse_ctx.is_connected) {
        ESP_LOGW(TAG, "Not initialized");
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

    ESP_LOGI(TAG, "USB Host Mouse deinitialized");

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
        ESP_LOGW(TAG, "Task already started");
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
        ESP_LOGE(TAG, "Failed to create task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Mouse monitoring task started");

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

    ESP_LOGI(TAG, "Mouse monitoring task stopped");

    return ESP_OK;
}

/**
 * @brief 获取鼠标连接状态
 */
bool usb_host_mouse_is_connected(void)
{
    return s_mouse_ctx.is_connected;
}
