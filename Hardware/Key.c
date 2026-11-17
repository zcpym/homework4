#include "main.h"
#include "Key.h"

uint8_t Key_Scan(Key_t *k)
{
    uint8_t now = (uint8_t)HAL_GPIO_ReadPin(k->port, k->pin);
    if (now != k->last)
    {
        k->last = now;
        k->last_ms = HAL_GetTick();
        return 0u;
    }

    if (now == 0u)
    {
        if (!k->fired && ((HAL_GetTick() - k->last_ms) >= 20u))
        {
            k->fired = 1u;
            return 1u;
        }
    }
    else
    {
        k->fired = 0u;
    }
    return 0u;
}
