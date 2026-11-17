#ifndef __SERVO_H
#define __SERVO_H

#include <stdint.h>

/* SG90（180° 型）舵机驱动
 *
 * 前提：CubeMX 里 TIM2_CH1 已配成 50Hz PWM，
 *       PSC = 71, ARR = 19999, 即 1 个计数 = 1us。
 *       所以 CCR 的数值在数值上就等于脉宽的微秒数。
 */

/* 名义行程 0.5ms ~ 2.5ms（0° ~ 180°）。
 * 真实 SG90 机械行程比标称窄，打满会顶限位、堵转发热。
 * 两端各留余量，落到 550us ~ 2450us 这个安全区间。 */
#define SERVO_CCR_MIN   550u
#define SERVO_CCR_MAX  2450u

/* 启动 PWM 输出。必须在 MX_TIM2_Init() 之后调用。 */
void Servo_Init(void);

/* 设置目标角度（单位：度，0 ~ 180，超出范围自动限幅）。
 * 返回实际写入比较寄存器的 CCR 值，方便上层显示/校验。
 *
 * 注意：CCR 是影子寄存器，新的占空比要等到下一个更新事件
 * （即下一个 20ms 周期开始）才真正生效。 */
uint16_t Servo_SetAngle(float angle);

#endif
