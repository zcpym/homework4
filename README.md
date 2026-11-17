# 电控组第四次培训作业

- 姓名：**弓顺开**
- 学号：**20253006793**

本工程用 STM32F103C8T6 的 TIM2_CH1 输出 50Hz PWM 驱动 SG90（180° 型）舵机，
按 0° → 45° → 90° → 135° → 180° 的定角序列转动，并把「目标角度 / CCR / 脉宽」
三行信息同步显示在 OLED 上、打印到串口助手。另完成进阶 A：两个按键对角度做
±15° 微调，范围限制在 0°~180°。

---

## 一、完成情况

| 层级 | 内容 | 状态 |
|---|---|---|
| 必做 | 定角序列 0→45→90→135→180，每点停 1 秒 | ✅ |
| 必做 | OLED 三行同步显示 | ✅ |
| 必做 | 串口打印同样内容 | ✅ |
| 进阶 A | 两个按键 ±15°，限幅 0~180° | ✅ |

---

## 二、硬件连接与供电

| 信号 | 接至 | 说明 |
|---|---|---|
| 舵机 橙线（信号） | **PA0**（TIM2_CH1） | 输出 50Hz PWM |
| 舵机 红线（VCC） | **USB 转 TTL 模块的 5V 脚** | 即电脑 USB 的 VBUS，不经板子 |
| 舵机 棕线（GND） | USB 转 TTL 模块的 GND | **同时与板子 GND 相连** |
| OLED SCL / SDA | PB6 / PB7 | 软件 I²C |
| 按键 KEY_UP（+15°） | PA1 | 内部上拉，另一端接 GND |
| 按键 KEY_DOWN（−15°） | PA2 | 内部上拉，另一端接 GND |
| CH340 TX / RX | PA10 / PA9 | 115200 8N1，交叉接 |


---

## 三、PSC / ARR 的推导

### 第 1 步：TIM2 的时钟是多少

`SystemClock_Config()` 里：

```c
RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;   /* HCLK  = 72MHz */
RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;    /* PCLK1 = 36MHz */
```

所以 PCLK1 = 36MHz。

**但 TIM2 拿到的不是 36MHz。** F1 系列的定时器时钟有一条容易被忽略的规则：

> **只要 APB 的分频系数不等于 1，硬件会自动把送给该 APB 上定时器的时钟乘以 2。**

这里 APB1 的分频系数是 2（≠ 1），所以：

```
TIM2CLK = PCLK1 × 2 = 36MHz × 2 = 72MHz
```


### 第 2 步：定 PSC，让 1 个计数 = 1µs

**让 1 个计数正好是 1µs**，
那么 CCR 里填的数字就直接等于脉宽的微秒数。

```
计数频率 = 1 / 1µs = 1MHz

PSC = TIM2CLK / 计数频率 − 1 = 72MHz / 1MHz − 1 = 71
```

### 第 3 步：定 ARR，得到 20ms 周期

舵机只认 50Hz，也就是周期 20ms：

```
ARR = 周期 / 计数周期 − 1 = 20000µs / 1µs − 1 = 19999
```

### 第 4 步：回代验算

```
f = TIM2CLK / (PSC + 1) / (ARR + 1)
  = 72,000,000 / 72 / 20000
  = 50 Hz                     ✅
```

| 参数 | 值 | 推导 |
|---|---|---|
| 定时器时钟 | 72 MHz | PCLK1 36MHz × 2（APB1 分频 ≠ 1） |
| 1 个计数 | 1 µs | 1MHz 计数频率 |
| PSC | **71** | 72M ÷ 1M − 1 |
| ARR | **19999** | 20000µs ÷ 1µs − 1 |
| 结果 | **50 Hz** | 72M ÷ 72 ÷ 20000 |

---

## 四、角度换算与两端余量

### 换算公式

脉宽 0.5ms 对应 0°，2.5ms 对应 180°，线性关系。

```
脉宽(µs) = 500 + 角度 / 180 × 2000
```

因为 PSC 让 1 个计数 = 1µs，所以 **CCR 的数值在数值上就等于脉宽的微秒数**：

```c
uint16_t ccr = (uint16_t)(500.0f + angle / 180.0f * 2000.0f + 0.5f);
```

`+ 0.5f` 是为了四舍五入。不加的话 `float → uint16_t` 是直接截断，
15° 会算成 666 而不是 667。

### 两端为什么这样取

0.5ms ~ 2.5ms 是**名义值**。真实 SG90 的齿轮箱行程比标称窄，打满会「咔咔」
顶住机械限位，此时电机被卡住不转但仍然通电（**堵转**），电流会飙到 700mA 左右，
时间长了发热烧齿轮。

所以两端各留约 4.5° 余量：

```
1° 对应 (2500 − 500) / 180 ≈ 11.11 µs

550µs  → (550 − 500) / 11.11 ≈ 4.5°
2450µs → 180 − (2500 − 2450) / 11.11 ≈ 175.5°
```

最终限幅到 **550µs ~ 2450µs**（本工程 `Servo.h` 里的 `SERVO_CCR_MIN/MAX`），
实际可用行程约 4.5° ~ 175.5°。

### 为什么夹的是 CCR 而不是只夹角度

角度换算完还是可能落到 500µs 这种"名义上合法、实际会顶限位"的值上。
所以要做两级限幅：

```c
/* 第一级：夹角度，防止非法输入 */
if (angle < 0.0f)   angle = 0.0f;
if (angle > 180.0f) angle = 180.0f;

uint16_t ccr = (uint16_t)(500.0f + angle / 180.0f * 2000.0f + 0.5f);

/* 第二级：夹 CCR，真正防顶限位的是这一级 */
if (ccr < SERVO_CCR_MIN) ccr = SERVO_CCR_MIN;
if (ccr > SERVO_CCR_MAX) ccr = SERVO_CCR_MAX;
```

**结果是：传入 0°，实际写入的 CCR 是 550 而不是 500。**
OLED 上显示 `Target:000deg / CCR:0550`，看起来"对不上"，但这是故意的 ——
屏幕上显示的是"你要的角度"和"实际写进寄存器的值"，差的这 50µs 就是安全余量。

---

## 五、OLED 显示（软件 I²C）

三行格式：

```
Target:090deg
CCR   :1500
Pulse :1.500ms
```

（上例为 90° 中位。若在 0°，因为有限幅，显示的是 `Target:000deg / CCR:0550`。）

| 行 | 内容 | 来源 |
|---|---|---|
| 1 | 目标角度，固定 3 位 | `s_target` |
| 2 | 实际写入比较寄存器的值，固定 4 位 | `Servo_SetAngle()` 的返回值 |
| 3 | 换算出的脉宽 | `ccr / 1000` 和 `ccr % 1000` |

### 只在角度变化时才刷新

软件 I²C 刷一次三行大约几十毫秒。如果主循环每圈都刷：

- 屏幕会明显闪烁
- 主循环每圈耗时几十毫秒，按键响应变迟钝

所以用一个 "上次显示过的 CCR" 做门控：

```c
uint16_t ccr = Servo_SetAngle(s_target);
if (ccr != s_show_ccr)      /* 只有 CCR 真的变了才刷 */
{
  s_show_ccr = ccr;
  /* OLED 三行 + printf */
}
```

`s_show_ccr` 初值故意设成 `0xFFFF`（真实的 CCR 永远在 550~2450 之间，不可能相等），
保证上电后第一圈必定刷新一次。

---

## 六、串口输出

`printf` 重定向由 CubeMX 生成的 `syscalls.c` 完成：它自带一个弱符号 `_write`，
内部调用 `__io_putchar`。本工程在 `Core/Src/usart.c` 里实现了：

```c
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 100);
  return ch;
}
```

因此直接 `#include <stdio.h>` 后调用 `printf` 即可，**不需要再自己写 `_write`**。

打印格式与 OLED 完全一致：

```c
printf("Target=%3d CCR=%4d Pulse=%d.%03dms\r\n",
       (int)s_target, ccr, ccr / 1000, ccr % 1000);
```

```
Target= 90 CCR=1500 Pulse=1.500ms
```

波特率 115200，8 数据位，无校验，1 停止位。

### 实际输出

![串口输出截图](docs/serial.png)

这张图正好能看出**两级限幅在起作用**：

```
Target=  0  CCR= 550  Pulse=0.550ms     ← 夹到 550，不是 500
Target= 45  CCR=1000  Pulse=1.000ms
Target= 90  CCR=1500  Pulse=1.500ms
Target=135  CCR=2000  Pulse=2.000ms
Target=180  CCR=2450  Pulse=2.450ms     ← 夹到 2450，不是 2500
```

> 脉宽拆成整数和小数两部分打印（`ccr / 1000` 和 `ccr % 1000`），
> 是为了避开在单片机上打印浮点数——会引入不小的 printf 浮点库开销。

---

## 七、进阶 A：按键微调

### 两个按键

| 按键 | 引脚 | 作用 |
|---|---|---|
| KEY_UP | PA1 | 每次 +15° |
| KEY_DOWN | PA2 | 每次 −15° |

均为内部上拉输入，另一端接 GND：松开为高电平，按下为低电平。

消抖采用「记录上次电平 + 20ms 时间窗 + fired 标志」，不阻塞主循环：

- 电平一变就更新时间戳，本次不作数
- 电平稳定 ≥ 20ms 且为低，才确认为一次有效按下
- `fired` 标志保证按住不放只触发一次，松开才清零

### 序列 → 手动的切换

序列每 1 秒会把目标角度改成下一个定值，如果让按键和序列同时工作，
按键改完的角度会在 1 秒后被序列覆盖掉，看起来像按键坏了。

所以设计成**先跑完一轮序列，再进入手动模式**：

```
上电 → 0° → 45° → 90° → 135° → 180°（各停 1 秒）
                                    ↓
                          进入手动模式，只受按键控制
```

用 `s_seq_step` 做状态：`1~4` 走序列，到 `5` 后进入 `else` 分支交给按键。

### `HAL_Delay` 全部去掉

第一版用 `HAL_Delay(1000)` 跑序列，这期间 CPU 在空转，按键完全没反应。
改用 `HAL_GetTick()` 计时后，主循环每圈不到 1ms，按键随时能响应。

### App 层也要限幅

`Servo_SetAngle` 内部的限幅保护的是**硬件**（不顶限位）。但如果只靠它，
`App` 里的 `s_target` 会越界：

```c
/* 180° 时再按一次加 */
s_target += 15.0f;          /* → 195，但舵机被夹在 180° 不动 */
```

用户看到的现象是"按了三下没反应"，因为要把 195 减回 180 才看得见变化。
所以在 App 层也要夹：

```c
if (Key_Scan(&s_key_up))
{
  s_target += 15.0f;
  if (s_target > 180.0f) s_target = 180.0f;
}
if (Key_Scan(&s_key_down))
{
  s_target -= 15.0f;
  if (s_target < 0.0f) s_target = 0.0f;
}
```

**两层限幅各管各的：**

| 层 | 夹的是什么 | 防的是 |
|---|---|---|
| `Servo_SetAngle` | `angle` / `ccr` | 舵机顶限位烧齿轮（硬件） |
| `App` | `s_target` | 状态变量漂移导致按键失灵（软件） |

---

## 八、工程结构

```
homework4/
├── Core/
│   ├── Inc/            CubeMX 生成的头文件（main.h 里有引脚宏）
│   └── Src/
│       ├── main.c      主逻辑：序列 + 按键 + 显示刷新
│       ├── tim.c       TIM2 PWM 初始化（PSC/ARR/Pulse）
│       ├── usart.c     USART1 + __io_putchar 重定向
│       ├── gpio.c      引脚配置（按键上拉、OLED 开漏输出）
│       └── syscalls.c  CubeMX 自带，printf 走这里
├── Hardware/
│   ├── MyI2C.c/.h      软件 I²C 时序
│   ├── OLED.c/.h       SSD1306 驱动
│   ├── OLED_Font.h     8x16 字模
│   ├── Delay.c/.h      基于 DWT->CYCCNT 的微秒延时 + Delay_Init
│   ├── Servo.c/.h      舵机驱动（角度换算 + 两级限幅）
│   └── Key.c/.h        按键消抖（不阻塞）
├── docs/
│   ├── serial.png      串口输出截图
│   └── demo.mp4        演示视频
├── cmake/              构建配置
├── CMakeLists.txt
├── homework2.ioc       CubeMX 工程文件
├── STM32F103XX_FLASH.ld
└── README.md
```

---

## 九、参考资料

- **OLED 驱动与软件 I²C**：江协科技（江科大）STM32 入门教程的 `MyI2C.c/.h`、
  `OLED.c/.h`、`OLED_Font.h`。移植时做了三处改动：
  1. 标准外设库（SPL）API 全部换成 HAL API
  2. 引脚由教程的 PB10/PB11 改为本工程的 PB6/PB7
  3. 删除 `MyI2C_Init()` 里的 GPIO 配置（已由 CubeMX 完成）

- **微秒延时**：未使用教程的 `Delay.c`（它直接改写 SysTick 寄存器，会让
  `HAL_GetTick()` 的时基停掉），改用 Cortex-M3 内核的 `DWT->CYCCNT` 周期计数器。

- **舵机驱动**：按作业引导文档的换算思路自行实现，并补充了两级限幅。

---

## 十、演示视频

[`docs/demo.mp4`](docs/demo.mp4)（约 30 秒，舵机与 OLED 同画面）

| 时间段 | 内容 | 验证的点 |
|---|---|---|
| 0–8s | 上电，序列自动跑 0→45→90→135→180 | 必做：定角序列 |
| 8–12s | 按"减"，165 → 150 → 135 | 进阶 A：按键生效 |
| 12–18s | 按到 0°，继续按"减"无反应 | App 层限幅（`s_target` 不会漂） |
| 18–22s | 按"加"，立刻到 15° | 限幅后状态没跑偏 |
| 22–30s | 按住"加"不放，只跳一次 | 按键消抖的 `fired` 标志 |

全程 OLED 三行与串口输出数值一致。

---

## 附：关键代码位置

| 内容 | 文件 | 位置 |
|---|---|---|
| PSC / ARR / Pulse 配置 | `Core/Src/tim.c` | `MX_TIM2_Init()` |
| 角度换算与两级限幅 | `Hardware/Servo.c` | `Servo_SetAngle()` |
| 序列 + 手动模式主循环 | `Core/Src/main.c` | `USER CODE BEGIN 3` |
| 按键消抖 | `Hardware/Key.c` | `Key_Scan()` |
| printf 重定向 | `Core/Src/usart.c` | `__io_putchar()` |
