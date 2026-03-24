# USB to Bluetooth Mouse Bridge

[![ESP32-S3](https://img.shields.io/badge/ESP32-S3-blue)](https://www.espressif.com/en/products/socs/esp32-s3)
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.0+-green)](https://docs.espressif.com/projects/esp-idf/en/latest/)
[![Bluetooth](https://img.shields.io/badge/Bluetooth-5.0%20LE-lightgrey)](https://www.bluetooth.com/)
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)

**将有线 USB 鼠标转换为无线蓝牙鼠标的 ESP32-S3 桥接器**

---

## 🎯 功能特性

- **USB Host 鼠标驱动** - 支持标准 USB HID 鼠标
- **蓝牙 HID 设备** - BLE 5.0 (NimBLE 协议栈)
- **设备伪装** - 可配置 Vendor ID、Product ID、设备名称（默认伪装成 Dell GM3323D）
- **配对模式** - GPIO1 下拉触发，自动清除旧配对
- **RGB 灯光指示** - WS2812 LED 显示连接状态
- **自动重连** - 支持断电后自动回连

---

## 📦 硬件要求

- **ESP32-S3 开发板** - 需支持 USB OTG
- **USB 鼠标** - 标准 HID 鼠标
- **WS2812 LED** (可选) - GPIO48
- **杜邦线** (可选) - 配对模式用

---

## 🔌 引脚分配

| GPIO | 功能 | 说明 |
|------|------|------|
| GPIO1 | 配对模式检测 | 接 GND 触发配对 |
| GPIO19 | USB D- | USB Host 差分负 |
| GPIO20 | USB D+ | USB Host 差分正 |
| GPIO48 | WS2812 LED | RGB 指示灯 |

---

## 🚀 快速开始

### 1. 环境准备

```bash
git clone -b v5.3 https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32s3 && . ./export.sh
```

### 2. 编译烧录

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

### 3. 配对使用

1. 连接 **GPIO1 → GND** 触发配对模式（LED 红色闪烁）
2. 电脑搜索蓝牙设备 **"Dell GM3323D"**
3. 配对连接（LED 变绿色）
4. 拔掉杜邦线

---

## ⚙️ 配置设备信息

编辑 [`main/device_config.h`](main/device_config.h)：

```c
// 伪装成 Logitech G102
#define DEVICE_VENDOR_ID        0x046D  // Logitech
#define DEVICE_PRODUCT_ID       0xC077  // G102
#define DEVICE_NAME             "Logitech G102"
#define MANUFACTURER_NAME       "Logitech"
```

---

## 💡 LED 状态说明

| 状态 | 灯光效果 |
|------|----------|
| 待机 | ⚫ 关闭 |
| 配对模式 | 🔴 红色闪烁 |
| 已连接 | 🟢 绿色常亮 |
| 启动就绪 | 🔵 蓝色闪 500ms |

---

## 📊 性能指标

| 项目 | 数值 |
|------|------|
| USB 鼠标输入 | 125 Hz (取决于鼠标硬件) |
| 蓝牙输出 | ~70 Hz (ESP32-S3 BLE 在 Windows 下的极限) |
| 延迟 | < 20 ms |
| 有效距离 | ≤ 10 米 |

---

## 📁 项目结构

```
Bluetooth_Bridge/
├── main/
│   ├── esp_hid_device_main.c    # 主程序
│   ├── esp_hid_gap.c/h          # 蓝牙 GAP 配置
│   ├── usb_host_mouse.c/h       # USB Host 鼠标驱动
│   ├── ws2812_led.c/h           # WS2812 LED 驱动
│   ├── pair_mode.c/h            # 配对模式检测
│   └── device_config.h          # 设备信息配置
├── sdkconfig.defaults
├── CMakeLists.txt
├── LICENSE
└── README.md
```

---

## 🔧 故障排除

**搜索不到设备**
- 检查供电，查看串口日志确认广播已启动

**配对后无法连接**
- 连接 GPIO1-GND 清除旧配对，重新配对

**鼠标移动卡顿**
- 这是 ESP32-S3 BLE 在 Windows 下的极限 (~70Hz)
- 更换支持 1000Hz 的 USB 鼠标可提升到 100-150Hz

---

## 📄 许可证

MIT License - 详见 [LICENSE](LICENSE)

---

## 🙏 致谢

- [ESP-IDF](https://github.com/espressif/esp-idf)
- [USB Host HID Component](https://components.espressif.com/components/espressif/usb_host_hid)
- [LED Strip Component](https://components.espressif.com/components/espressif/led_strip)
