#include "Servo.h"
#include "tim.h"

void Servo_Init(void)
{
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
}

uint16_t Servo_SetAngle(float angle)
{
    if (angle < 0.0f)    angle = 0.0f;
    if (angle > 180.0f)  angle = 180.0f;

    uint16_t ccr = (uint16_t)(500.0f + angle / 180.0f * 2000.0f + 0.5f);

    if (ccr < SERVO_CCR_MIN) ccr = SERVO_CCR_MIN;
    if (ccr > SERVO_CCR_MAX) ccr = SERVO_CCR_MAX;

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, ccr);

    return ccr;
}