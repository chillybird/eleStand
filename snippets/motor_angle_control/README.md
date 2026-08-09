# motor_angle_control

MG90S 180° 位置舵机角度控制测试程序。

## 硬件

- ESP32-C3 SuperMini
- MG90S 舵机 (信号线 GPIO4, 需 5V 外部供电)

## 功能

- 上电归中 90°
- 串口发送 0~180 设角度
- 同目录 `web.html` 提供 Web Serial API 网页控制 (滑块 + 预设按钮)

## 构建 & 运行

```bash
cd snippets/motor_angle_control
idf.py set-target esp32c3
idf.py build flash -p COM3 monitor
```

## 串口命令

| 命令 | 效果 |
|------|------|
| `0` ~ `180` | 设置舵机角度 |
| `Ctrl+]` | 退出监视器 |

## 脉宽映射

| 角度 | 脉宽 |
|------|------|
| 0° | 0.5ms |
| 45° | 1.0ms |
| 90° | 1.5ms |
| 135° | 2.0ms |
| 180° | 2.4ms |
