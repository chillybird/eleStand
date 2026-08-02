# 项目重构分析

## 当前结构

```
eleStand/
├── CMakeLists.txt          # project(eleStand), MINIMAL_BUILD ON
├── main/
│   ├── CMakeLists.txt      # SRCS: app_main servo switch, PRIV_REQUIRES: driver esp_timer ...
│   ├── app_main.c          # 200行 单体: USB串口 + 命令队列 + 动作执行
│   ├── servo.c/h           # ✅ PWM控制模块, 已封装
│   ├── switch.c/h          # ✅ 开关检测模块, 已封装
│   └── build/              # 编译产物 (gitignore)
├── web_serial.html         # 外部网页 (USB Serial 控制)
├── PLAN_NETWORK.md         # 联网开发计划
├── pytest_hello_world.py   # ⚠️ hello_world 模板残留
├── sdkconfig.ci            # CI 配置
├── sdkconfig / sdkconfig.old  # ⚠️ sdkconfig.old 可删除
├── .cache / .clangd        # IDE 缓存
└── .devcontainer / .vscode # 开发环境配置
```

## 问题诊断

| 问题 | 影响 |
|------|------|
| `app_main.c` 混入 5 种职责 | 添加 HTTP 支持需大面积修改 |
| USB 串口解析与命令队列耦合在同一文件 | 无法复用命令解析逻辑 |
| 无统一命令入口 | HTTP 和 Serial 需要各自复制命令处理 |
| `pytest_hello_world.py` 残留 | 仓库脏文件 |
| `build/` 目录 6.4MB | 占用磁盘, 已 gitignore |
| `sdkconfig.old` | 无用的旧配置 |

## app_main.c 职责拆分

```
app_main.c (200行) 当前承担:
┌─────────────────────────────────────────┐
│ usb_printf()          USB CDC 输出       │  → 可内联, 简单
│ sw_at_target()        开关状态判断       │  → switch 模块已提供基础能力
│ move_to()             启动舵机旋转       │
│ move_poll()           轮询等待到位       │  → 动作执行逻辑
│ post_cmd()            投递命令到队列     │
│ status_timer_cb()     定时状态上报       │  → 状态报告
│ serial_task()         USB 接收+命令解析  │  → 输入源
│ handle_cmd()          命令分发           │  → 已内嵌在 serial_task 中
│ app_main()            启动+主循环        │
└─────────────────────────────────────────┘
```

## 目标架构

```
┌──────────────────────────────────────────────────┐
│ input sources (独立 task)                         │
│ ┌──────────────┐  ┌──────────────────────────┐   │
│ │ serial_task  │  │ http_server (future)      │   │
│ │ USB CDC 读取 │  │ POST /api/down → CMD_DOWN │   │
│ │ 解析 → 投递  │  │ 投递到 cmd_queue           │   │
│ └──────┬───────┘  └───────────┬──────────────┘   │
│        │                      │                   │
│        └──────────┬───────────┘                   │
│                   ▼                               │
│         ┌─────────────────┐                       │
│         │   cmd_queue      │  FreeRTOS Queue<int> │
│         │   (统一入口)     │                       │
│         └────────┬────────┘                       │
│                  ▼                                │
│ ┌─────────────────────────────────────────────┐  │
│ │ 动作执行 (app_main 主循环)                    │  │
│ │  xQueueReceive → move_to → move_poll → stop │  │
│ └─────────────────────────────────────────────┘  │
│                                                   │
│ ┌─────────────────┐  ┌──────────────────────┐    │
│ │ servo.c/h       │  │ switch.c/h            │    │
│ │ PWM 控制        │  │ GPIO 输入 + 状态查询  │    │
│ └─────────────────┘  └──────────────────────┘    │
└──────────────────────────────────────────────────┘
```

## 重构方案

### 文件变动

```
main/
├── cmd_queue.h          [新增] 命令队列 + 状态上报
├── cmd_queue.c          [新增] 队列创建, post_cmd, 状态定时器
├── serial_in.c          [新增] USB CDC 输入源 (原 serial_task)
├── app_main.c           [重写] 精简为主循环, 去重
├── servo.c/h            保持不变
├── switch.c/h           保持不变
└── CMakeLists.txt       [修改] 添加新源文件

删除:
├── pytest_hello_world.py
└── sdkconfig.old         (手动删除, 非代码)
```

### cmd_queue 模块

```c
// cmd_queue.h
void cmd_queue_init(void);
void cmd_post(int cmd);                          // 投递命令
void cmd_post_usb(const char *data, int len);    // 批量发送到 USB
int  cmd_wait(uint32_t timeout_ms);              // 等待命令 (主循环用)
int  cmd_drain_latest(void);                     // 获取最新命令 + 排空

// 状态上报
void cmd_status_start(void);                     // 启动定时状态上报
```

### serial_in 模块

```c
// serial_in.c — 从 app_main.c 剥离
void serial_task(void *arg);  // FreeRTOS task, USB 接收 + 命令解析 + 投递
```

### 重构后的 app_main.c (~80行)

```c
void app_main(void) {
    usb_init();
    servo_init(GPIO);
    sw_init();
    cmd_queue_init();
    serial_in_start();       // xTaskCreate(serial_task)

    move_to(sw_down_gpio());
    move_poll(sw_down_gpio(), 30000, 20);
    servo_stop();

    while (1) {
        int cmd = cmd_drain_latest();
        switch (cmd) {
            case CMD_DOWN: ... break;
            case CMD_UP:   ... break;
            case CMD_STOP: ... break;
            case CMD_CW:   ... break;
            case CMD_CCW:  ... break;
        }
    }
}
```

### 收益

| 项目 | 重构前 | 重构后 |
|------|--------|--------|
| app_main.c 行数 | ~200 | ~80 |
| 命令投递入口 | 分散 | 统一 `cmd_post()` |
| 状态上报 | 内嵌 app_main | 独立 `cmd_queue` 模块 |
| USB 输入 | 与命令混在一起 | 独立 `serial_in` 模块 |
| 添加 HTTP 输入 | 需改 app_main | 只需新增 `http_in` 模块 |

### 实施步骤

1. 提取 `cmd_queue.c/h` — 命令类型 + 队列 + 状态上报
2. 提取 `serial_in.c` — USB CDC 接收 + 解析
3. 重写 `app_main.c` — 仅主循环
4. 更新 `CMakeLists.txt`
5. 删除 `pytest_hello_world.py`
6. 测试编译 + 功能验证

---

请确认是否按此方案执行重构。
