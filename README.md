# 25 电赛 · FreeRTOS 版本

基于 2025 电赛 `7.29/OLED` 工程的 FreeRTOS 移植版本，面向 STM32F103C8，包含串口接收、舵机控制和 OLED 显示。

- **Keil 工程：[OLED.uvprojx](OLED/MDK-ARM/OLED.uvprojx)**，沿用 Arm Compiler 6 配置。
- 架构：接收解析任务 + 10 ms 控制任务 + OLED 显示任务，基于官方 FreeRTOS-Kernel V11.1.0。
- 已完成：Arm GNU 13.2 完整编译/链接、纯算法 ARM 指令测试和工程结构检查。**未完成 Keil 原生重建及实物测试。**
- **手工移植工程：不要直接用现有 OLED.ioc 重新生成覆盖代码。** IOC 只同步了串口/DMA 优先级；如需重新生成，请在副本中生成后合并 RTOS 接入点，避免覆盖任务、异常入口与工程配置。

## 1. 先知道从哪里打开

下面所有路径都相对于本 README 所在的 `Electric-racing-car` 目录，文件链接可以直接点击。

| 你要做什么 | 打开这里 |
| --- | --- |
| 用 Keil 打开整个项目 | [OLED/MDK-ARM/OLED.uvprojx](OLED/MDK-ARM/OLED.uvprojx) |
| 看主程序从哪里开始运行 | [OLED/Core/Src/main.c](OLED/Core/Src/main.c) |
| 看三个任务、队列和串口回调 | [OLED/Core/Src/app_freertos.c](OLED/Core/Src/app_freertos.c) |
| 改串口数据包的解析规则 | [OLED/Core/Src/aim_protocol.c](OLED/Core/Src/aim_protocol.c) |
| 改舵机如何靠近目标 | [OLED/Core/Src/aim_control.c](OLED/Core/Src/aim_control.c) |
| 改 FreeRTOS 内核配置 | [OLED/Core/Inc/FreeRTOSConfig.h](OLED/Core/Inc/FreeRTOSConfig.h) |

## 2. 文件布局

这是主要文件的导航树，HAL/CMSIS 库内部的大量文件、编译中间文件没有全部展开。

```text
Electric-racing-car/                         ← 当前版本的总目录
├─ README.md                          ← 文件导航，也就是本文
├─ OLED/                              ← 单片机工程主体，名字沿用原工程
│  ├─ Core/                           ← 应用逻辑和板级外设配置
│  │  ├─ Inc/                        ← .h 头文件：接口、类型、宏和配置
│  │  │  ├─ app_freertos.h            ← RTOS 启动接口、调试变量声明
│  │  │  ├─ aim_protocol.h            ← 输入快照 AimInput、解析器 AimParser
│  │  │  ├─ aim_control.h             ← 控制状态 AimControl、PWM 上下限
│  │  │  ├─ FreeRTOSConfig.h          ← tick、堆、优先级、检查开关
│  │  │  ├─ main.h                    ← 原工程公共声明和历史宏
│  │  │  ├─ stm32f1xx_it.h            ← 中断处理函数声明
│  │  │  ├─ stm32f1xx_hal_conf.h      ← HAL 模块开关及相关配置
│  │  │  └─ dma/gpio/i2c/tim/usart/pca9685.h
│  │  │                               ← 对应外设或驱动的接口文件
│  │  └─ Src/                        ← .c 源文件：函数实际实现
│  │     ├─ main.c                    ← HAL、时钟、外设初始化 → App_Start
│  │     ├─ app_freertos.c            ← 创建任务/队列、接收/控制/显示任务
│  │     ├─ aim_protocol.c            ← 拼包、校验、小端坐标提取
│  │     ├─ aim_control.c             ← 反馈去重、超时、跟踪一步、PWM 限幅
│  │     ├─ stm32f1xx_it.c            ← SysTick、UART、DMA 等中断入口
│  │     ├─ dma.c                     ← DMA 时钟及中断优先级
│  │     ├─ usart.c                   ← 串口波特率、引脚、收发 DMA 配置
│  │     ├─ tim.c                     ← TIM2 PWM；保留未启用的 TIM4 模板
│  │     ├─ i2c.c                     ← I2C1/I2C2 初始化和引脚配置
│  │     ├─ gpio.c                    ← 板级 GPIO 初始化
│  │     ├─ stm32f1xx_hal_msp.c        ← HAL 底层公共初始化
│  │     ├─ system_stm32f1xx.c         ← CMSIS 系统初始化、系统时钟变量
│  │     └─ pca9685.c                 ← 保留的旧驱动，当前舵机走 TIM2
│  ├─ oled/                          ← OLED 驱动与字体
│  │  ├─ oled.c                      ← 画点、字符串、显存刷新、I2C 写入
│  │  ├─ oled.h                      ← OLED 接口和相关常量
│  │  └─ oledfont.h                  ← 字模数据
│  ├─ Drivers/                       ← 沿用的芯片底层库，日常业务通常不改
│  │  ├─ STM32F1xx_HAL_Driver/        ← ST HAL 外设库
│  │  └─ CMSIS/                      ← Cortex-M/芯片定义、系统及启动模板
│  ├─ Middlewares/Third_Party/FreeRTOS/
│  │  ├─ tasks.c                     ← 官方任务调度实现
│  │  ├─ queue.c                     ← 官方队列实现
│  │  ├─ list.c                      ← 内核链表实现
│  │  ├─ include/                    ← 官方头文件，真正的 task.h 在这里
│  │  ├─ portable/GCC/ARM_CM3/        ← Cortex-M3 移植：port.c、portmacro.h
│  │  ├─ portable/MemMang/heap_4.c    ← RTOS 动态内存管理
│  │  ├─ SOURCE.json                 ← 上游版本、地址、文件 SHA-256
│  │  ├─ LICENSE.md                  ← 官方许可证
│  │  └─ History.txt                 ← 官方版本历史
│  ├─ MDK-ARM/                       ← Keil 构建入口
│  │  ├─ OLED.uvprojx                ← 双击打开的工程文件
│  │  ├─ startup_stm32f103xb.s        ← Keil 启动汇编和中断向量表
│  │  └─ OLED/                       ← Keil 构建后通常生成，初始可能不存在
│  ├─ GCC/                           ← 当前电脑使用的 GCC 验证构建
│  │  ├─ STM32F103C8.ld              ← Flash/RAM 布局与主栈空间约束
│  │  ├─ runtime.c                   ← GCC/newlib 辅助入口与 C 库堆
│  │  └─ build/                      ← 新固件 ELF、HEX、MAP、日志、目标文件
│  ├─ Legacy/                        ← 仅用于对照，未加入当前编译
│  │  ├─ baremetal_task.c            ← 原 task.c：旧解包、PID、路径辅助函数
│  │  └─ baremetal_task.h            ← 原 task.h，改名避开内核同名文件
│  └─ OLED.ioc                       ← 原 CubeMX 配置，非完整 RTOS 生成配置
├─ tests/                            ← 验证应用逻辑，不烧到板上的测试
│  ├─ test_aim.c                     ← 解析和控制算法的 C 测试
│  └─ build/                        ← 测试 ELF、result.txt、project_checks.txt
├─ tools/                            ← 在电脑上运行的 Python 辅助工具
│  ├─ build_gcc.py                   ← 读取 Keil 源文件列表并构建 ARM 固件
│  ├─ test_aim.py                    ← 编译测试并用 Unicorn 执行 ARM 指令
│  ├─ verify_project.py              ← 检查工程、内核校验值、中断向量表
│  └─ requirements.txt               ← ARM 模拟测试及 ELF 检查的 Python 依赖
└─ .gitignore                        ← 版本管理时忽略构建产物和缓存
```

`Inc` 是 include 的缩写，通常放“让其他文件知道怎么调用”的声明；`Src` 是 source 的缩写，放实际执行的代码。例如想看坐标怎么解析，先找 `aim_protocol.c`；想看它使用的数据结构，再找同名的 `aim_protocol.h`。

**容易找错的三个位置：**

- `app_freertos.c` 是我们编写的任务入口；官方 `tasks.c` 是内核调度器。改接收、控制、显示流程时找前者。
- 当前使用的 `task.h` 在 `Middlewares/Third_Party/FreeRTOS/include`；`Legacy/baremetal_task.h` 只是旧业务代码备份。
- Keil 产物位于 `OLED/MDK-ARM/OLED/`，GCC 产物位于 `OLED/GCC/build/`；源码位于 Core、Drivers 等目录，不要把生成的文件当成待修改的源文件。

## 3. 按功能查找：要改什么，去哪里改

| 想查看或修改的内容 | 文件 | 搜索的函数或名称 |
| --- | --- | --- |
| 上电初始化顺序、系统时钟 | [main.c](OLED/Core/Src/main.c) | `main`、`SystemClock_Config` |
| 任务优先级、各任务栈大小、队列容量 | [app_freertos.c](OLED/Core/Src/app_freertos.c) | `App_Start`、`xTaskCreate`、`xQueueCreate` |
| DMA 接收启动、串口空闲回调、错误恢复 | [app_freertos.c](OLED/Core/Src/app_freertos.c) | `StartReceive`、`HAL_UARTEx_RxEventCallback`、`HAL_UART_ErrorCallback`、`ReceiveTask` |
| 控制任务的 10 ms 周期、500 ms 反馈超时 | [app_freertos.c](OLED/Core/Src/app_freertos.c) | `ControlTask` 中两个 `pdMS_TO_TICKS` 参数 |
| OLED 显示内容、位置和刷新等待时间 | [app_freertos.c](OLED/Core/Src/app_freertos.c) | `DisplayTask`、`snprintf`、`OLED_ShowString` |
| 串口帧头、帧尾、校验、字节下标 | [aim_protocol.c](OLED/Core/Src/aim_protocol.c) | `ValidFrame`、`ReadLE16`、`AimParser_Push` |
| 目标/位置数据结构、最大帧长度 | [aim_protocol.h](OLED/Core/Inc/aim_protocol.h) | `AimInput`、`AimParser`、`AIM_FRAME_MAX` |
| 舵机初始 PWM、移动方向、到达判定 | [aim_control.c](OLED/Core/Src/aim_control.c) | `AimControl_Init`、`AimControl_Step` |
| 舵机 PWM 上下限 | [aim_control.h](OLED/Core/Inc/aim_control.h) | `AIM_X_MIN/MAX`、`AIM_Y_MIN/MAX` |
| RTOS 堆大小、tick 频率、断言/栈检查开关 | [FreeRTOSConfig.h](OLED/Core/Inc/FreeRTOSConfig.h) | `configTOTAL_HEAP_SIZE`、`configTICK_RATE_HZ`、`configASSERT` |
| HAL 与 RTOS 如何共用 SysTick | [stm32f1xx_it.c](OLED/Core/Src/stm32f1xx_it.c) | `SysTick_Handler` |
| 串口波特率、PA9/PA10、DMA Normal 模式 | [usart.c](OLED/Core/Src/usart.c) | `MX_USART1_UART_Init`、`HAL_UART_MspInit` |
| 串口/DMA 的 NVIC 优先级 | [usart.c](OLED/Core/Src/usart.c)、[dma.c](OLED/Core/Src/dma.c) | `HAL_NVIC_SetPriority`；相关 ISR 调用 RTOS API，当前均为 5，不能改成不允许调用 RTOS 的 0～4 |
| 50 Hz PWM 周期、预分频、PA0/PA1 | [tim.c](OLED/Core/Src/tim.c) | `MX_TIM2_Init`、`HAL_TIM_MspPostInit` |
| I2C 速度、引脚和初始化 | [i2c.c](OLED/Core/Src/i2c.c) | `MX_I2C1_Init`、`HAL_I2C_MspInit` |
| OLED 实际怎么发数据、怎么画字 | [oled.c](OLED/oled/oled.c) | `WriteCmd`、`WriteDat`、`OLED_ShowString`、`OLED_Refresh_Gram` |
| 内存不足、栈溢出时停在哪里 | [app_freertos.c](OLED/Core/Src/app_freertos.c) | `App_AssertFailed`、`vApplicationMallocFailedHook`、`vApplicationStackOverflowHook` |
| 调试器里观察丢块、UART 错误和栈余量 | [app_freertos.h](OLED/Core/Inc/app_freertos.h) | `g_rx_dropped`、`g_uart_errors`、`g_uart_restarts`、三个 `g_*_stack_free` |
| 编译进了哪些文件、头文件搜索路径 | [OLED.uvprojx](OLED/MDK-ARM/OLED.uvprojx) | Keil 工程树、Options for Target → C/C++ 的包含路径 |
| 协议和控制的测试用例 | [test_aim.c](tests/test_aim.c) | `RunTests` |

注意区分：**控制计算周期**在 `app_freertos.c`，**硬件 PWM 波形周期**在 `tim.c`，**RTOS tick 周期**在 `FreeRTOSConfig.h`。三者不是同一个参数，修改其中一个不代表其他两个自动跟着变化。

## 4. 沿着运行流程读代码

第一次阅读建议按这两条路线走，比从 Drivers 或内核目录开始翻更容易理解项目：

```text
程序启动：
main.c: main()
  → 初始化时钟和外设
  → app_freertos.c: App_Start()
  → 创建三个队列、三个任务
  → vTaskStartScheduler() 开始调度

一份串口数据怎样变成舵机输出：
stm32f1xx_it.c: UART / DMA 中断入口
  → HAL 驱动处理事件
  → app_freertos.c: HAL_UARTEx_RxEventCallback() 复制数据块
  → rx_queue
  → ReceiveTask()
  → aim_protocol.c: AimParser_Push() 拼包、校验、更新坐标
  → input_queue（最新输入快照）
  → ControlTask()
  → aim_control.c: AimControl_Step() 计算下一步 PWM
  → TIM2 CCR 更新，硬件持续产生 PWM
  → display_queue（显示快照）
  → DisplayTask() → oled.c 刷新屏幕
```

## 5. 构建命令与结果去哪里找

先在 PowerShell 中进入本 README 所在的 `Electric-racing-car` 目录，再执行：

```powershell
python tools/build_gcc.py
python tools/test_aim.py
python tools/verify_project.py
```

| 命令/操作 | 结果位置 | 用途 |
| --- | --- | --- |
| `python tools/build_gcc.py` | `OLED/GCC/build/build.log` | 完整编译与链接日志 |
| 同上 | `OLED/GCC/build/OLED-FreeRTOS.elf`、`OLED/GCC/build/OLED-FreeRTOS.hex` | 新版 GCC 固件产物 |
| 同上 | `OLED/GCC/build/OLED-FreeRTOS.map` | 函数、变量和内存占用分布 |
| `python tools/test_aim.py` | `tests/build/result.txt` | 算法 ARM 指令测试结果 |
| `python tools/verify_project.py` | `tests/build/project_checks.txt` | 工程引用、内核文件、中断向量检查结果 |
| 在 Keil 中 Rebuild All | `OLED/MDK-ARM/OLED/` | Keil 自己生成的产物，与 GCC 输出分开 |


构建产物和测试结果在运行命令后生成，不提交到仓库。Keil 工程不依赖 Python；GCC 验证工具需要单独配置，见下节。

## 6. 在新电脑上配置验证工具

Keil：安装 STM32F1 DFP 和 Arm Compiler 6，打开 `OLED/MDK-ARM/OLED.uvprojx` 后执行 Rebuild All。工程沿用 AC6.21 配置，使用其他 AC6 版本时需在 Keil 中选择已安装版本。

GCC 验证脚本面向 Windows，使用 Python 3 和 Arm GNU Toolchain 13.2.Rel1。先设置编译器的 bin 目录，再安装测试依赖：

```powershell
$env:ARM_GCC_BIN = 'C:\ArmGNU\bin'
python -m pip install --target tools/python-libs -r tools/requirements.txt
python tools/build_gcc.py
python tools/test_aim.py
python tools/verify_project.py
```

把示例路径替换为实际安装位置，工具链尽量放在不含中文的目录。官方 FreeRTOS 源码及实际编译所需的 HAL/CMSIS 文件已随工程提供；未使用的 CMSIS DSP 示例和预编译数学库未收录。

## 7. 运行参数与验证边界

- USART1：9600 baud、8N1；DMA 普通模式接收到空闲或满缓冲区，中断先复制数据，再交由任务解析。
- 任务：控制优先级 4、栈 256 字；接收优先级 3、栈 384 字；显示优先级 1、栈 512 字。一个栈字为 4 字节。
- FreeRTOS tick：1 ms；控制任务名义周期：10 ms；OLED 每次刷新后等待 100 ms。控制只消费新的位置序号，位置反馈超过 500 ms 时保持最后 PWM。
- PWM：TIM2 两路 50 Hz；初值 X=1190、Y=1210，范围 X=1100～1280、Y=1050～1210。这些值是定时器计数，不是角度；机构方向与限位须实物核对。
- 协议：22 字节目标帧、11 字节位置帧，帧头 0x24、帧尾 0x23，使用字节和低 8 位校验。原协议未明确类型/长度字段，因此合法长帧前缀若碰巧满足短帧校验，存在识别歧义；可靠区分需要与发送端共同约定类型或长度。
- 只控制第 0 个目标点，未补充原工程未完成的多点巡迹；普通 DMA 重启窗口、高速连续收包和 UART 错误恢复仍需上板测量。
- SysTick 同时推进 HAL 与 RTOS 时间；保持 1 kHz、关闭 tickless，不在 ISR 中调用依赖 tick 的阻塞 HAL 函数。USART/DMA NVIC 优先级为 5，任务优先级与中断优先级的数值方向相反。
- 已完成完整 GCC 编译/链接和真实 ARM 算法测试：44,026 次断言检查通过，其中多数来自噪声及限幅循环。Flash 约 25.7 KiB、静态 RAM 约 12.4 KiB；RTOS 的 10 KiB 堆已计入 RAM。未完成 Keil 原生重建及实物验证。

故障定位看 `g_assert_file`、`g_assert_line`；串口状态看 `g_rx_dropped`、`g_uart_errors`、`g_uart_restarts`；栈余量看三个 `g_*_stack_free`，单位为字。最后的 PWM 在软件停机后可能仍由硬件保持，保持输出不等于关闭执行器。
