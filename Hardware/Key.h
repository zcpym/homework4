#ifndef __KEY_H
#define __KEY_H

#include <stdint.h>
#include "main.h"

/* 机械按键消抖
 *
 * 用法：
 *   1. 用 GPIOx_GPIO_Port / GPIOx_Pin 初始化一个 Key_t
 *   2. 主循环每圈调一次 Key_Scan()
 *   3. 返回 1 表示"这次扫到了一次有效按下"（按住不放只返回一次）
 *
 * 不阻塞，内部用 HAL_GetTick() 做 20ms 时间窗，不占用任何定时器。
 */

typedef struct
{
  GPIO_TypeDef *port;    /* 端口 */
  uint16_t      pin;     /* 引脚 */
  uint8_t       last;    /* 上次稳定的电平 */
  uint32_t      last_ms; /* 上次电平变化时间 */
  uint8_t       fired;   /* 这次按下是否已经触发过 */
} Key_t;

uint8_t Key_Scan(Key_t *k);

#endif
