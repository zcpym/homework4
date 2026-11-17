#include "Delay.h"

#include "stdint.h"
#include "main.h"

/**
  * @brief  初始化 DWT 周期计数器
  * @param  无
  * @retval 无
  */
void Delay_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;   /* 使能 trace 单元 */
  DWT->CYCCNT = 0u;                                  /* 计数清零 */
  DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;              /* 启动 CYCCNT */
}

/**
  * @brief  微秒级延时
  * @param  xus 延时时长，范围：0~233015
  * @retval 无
  */
void Delay_us(uint32_t us)
{
  uint32_t start;
  uint32_t ticks;
  start = DWT->CYCCNT;
  ticks = us * (SystemCoreClock / 1000000u);

  while ((DWT->CYCCNT - start) < ticks)
  {
  }
}

/**
  * @brief  毫秒级延时
  * @param  xms 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_ms(uint32_t ms)
{
	while(ms--)
	{
		Delay_us(1000);
	}
}
 
/**
  * @brief  秒级延时
  * @param  xs 延时时长，范围：0~4294967295
  * @retval 无
  */
void Delay_s(uint32_t s)
{
	while(s--)
	{
		Delay_ms(1000);
	}
} 
