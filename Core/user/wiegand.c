/*
 * wiegand.c
 *
 *  Created on: Apr 1, 2026
 *      Author: Administrator
 */


#include "wiegand.h"

WG_Typedef Wiegand1 = {0};
WG_Typedef Wiegand2 = {0};
WG_Typedef Wiegand3 = {0};
WG_Typedef Wiegand4 = {0};
uint8_t WiegandSign = 0;

// 内部解码函数
static void WiegandDecode(WG_Typedef *pWg, uint8_t reader_id);



void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
	// 读头4（GPIOC 上的 WG4_D1_Pin, WG4_D0_Pin）
	    if (GPIO_Pin == WG4_D1_Pin) {
	        if (!Wiegand4.StarFlag) {
	            Wiegand4.StarFlag = 1;
	            Wiegand4.StartTime = HAL_GetTick();
	        }
	        Wiegand4.UID = (Wiegand4.UID << 1) | 1;
	        Wiegand4.BitCount++;
	    } else if (GPIO_Pin == WG4_D0_Pin) {
	        if (!Wiegand4.StarFlag) {
	            Wiegand4.StarFlag = 1;
	            Wiegand4.StartTime = HAL_GetTick();
	        }
	        Wiegand4.UID <<= 1;   // 最低位自动为0
	        Wiegand4.BitCount++;
	    }
	    // 读头3（GPIOE 上的 WG3_D1_Pin, WG3_D0_Pin）
	    else if (GPIO_Pin == WG3_D1_Pin) {
	        if (!Wiegand3.StarFlag) {
	            Wiegand3.StarFlag = 1;
	            Wiegand3.StartTime = HAL_GetTick();
	        }
	        Wiegand3.UID = (Wiegand3.UID << 1) | 1;
	        Wiegand3.BitCount++;
	    } else if (GPIO_Pin == WG3_D0_Pin) {
	        if (!Wiegand3.StarFlag) {
	            Wiegand3.StarFlag = 1;
	            Wiegand3.StartTime = HAL_GetTick();
	        }
	        Wiegand3.UID <<= 1;
	        Wiegand3.BitCount++;
	    }
	    // 读头2（GPIOC 上的 WG2_D0_Pin，GPIOE 上的 WG2_D1_Pin）
	    else if (GPIO_Pin == WG2_D1_Pin) {
	        if (!Wiegand2.StarFlag) {
	            Wiegand2.StarFlag = 1;
	            Wiegand2.StartTime = HAL_GetTick();
	        }
	        Wiegand2.UID = (Wiegand2.UID << 1) | 1;
	        Wiegand2.BitCount++;
	    } else if (GPIO_Pin == WG2_D0_Pin) {
	        if (!Wiegand2.StarFlag) {
	            Wiegand2.StarFlag = 1;
	            Wiegand2.StartTime = HAL_GetTick();
	        }
	        Wiegand2.UID <<= 1;
	        Wiegand2.BitCount++;
	    }
	    // 读头1（GPIOC 上的 WG1_D1_Pin, WG1_D0_Pin）
	    else if (GPIO_Pin == WG1_D1_Pin) {
	        if (!Wiegand1.StarFlag) {
	            Wiegand1.StarFlag = 1;
	            Wiegand1.StartTime = HAL_GetTick();
	        }
	        Wiegand1.UID = (Wiegand1.UID << 1) | 1;
	        Wiegand1.BitCount++;
	    } else if (GPIO_Pin == WG1_D0_Pin) {
	        if (!Wiegand1.StarFlag) {
	            Wiegand1.StarFlag = 1;
	            Wiegand1.StartTime = HAL_GetTick();
	        }
	        Wiegand1.UID <<= 1;
	        Wiegand1.BitCount++;
	    }
}


// 主循环中调用，检测各读头超时
void readwiegand(void)
{
    if (Wiegand4.StarFlag && (HAL_GetTick() - Wiegand4.StartTime) > 240) {
        WiegandSign = 4;
        WiegandDecode(&Wiegand4, 4);
        Wiegand4.BitCount = 0;
        Wiegand4.StarFlag = 0;
        Wiegand4.UID = 0;
    }
    if (Wiegand3.StarFlag && (HAL_GetTick() - Wiegand3.StartTime) > 240) {
        WiegandSign = 3;
        WiegandDecode(&Wiegand3, 3);
        Wiegand3.BitCount = 0;
        Wiegand3.StarFlag = 0;
        Wiegand3.UID = 0;
    }
    if (Wiegand2.StarFlag && (HAL_GetTick() - Wiegand2.StartTime) > 240) {
        WiegandSign = 2;
        WiegandDecode(&Wiegand2, 2);
        Wiegand2.BitCount = 0;
        Wiegand2.StarFlag = 0;
        Wiegand2.UID = 0;
    }
    if (Wiegand1.StarFlag && (HAL_GetTick() - Wiegand1.StartTime) > 240) {
        WiegandSign = 1;
        WiegandDecode(&Wiegand1, 1);
        Wiegand1.BitCount = 0;
        Wiegand1.StarFlag = 0;
        Wiegand1.UID = 0;
    }
}

// 韦根解码（26/34位卡号或4位按键）
static void WiegandDecode(WG_Typedef *pWg, uint8_t reader_id)
{
    uint8_t parity;
    uint8_t card[4] = {0};
    uint8_t i;

    switch (pWg->BitCount) {
        case 4:   // 4位按键
            printf("Reader %d: Keypad - %d\r\n", reader_id, (uint8_t)(pWg->UID & 0x0F));
            WiegandAccess_ProcessKey(reader_id, (uint8_t)(pWg->UID & 0x0F));
            break;

        case 26:  // 26位韦根
            // 偶校验（前12位 + 偶校验位）
            parity = 0;
            for (i = 0; i < 13; i++) {
                parity ^= ((pWg->UID >> (25 - i)) & 0x01);
            }
            if (parity == 0) {
                // 奇校验（后12位 + 奇校验位）
                parity = 0;
                for (i = 0; i < 13; i++) {
                    parity ^= ((pWg->UID >> (12 - i)) & 0x01);
                }
                if (parity != 0) {
                    card[0] = (pWg->UID >> 1) & 0xFF;
                    card[1] = (pWg->UID >> 9) & 0xFF;
                    card[2] = (pWg->UID >> 17) & 0xFF;

                    printf("Reader %d: Wiegand26, Card=0x%02X%02X%02X\r\n",
                           reader_id, card[2], card[1], card[0]);
                    uint32_t card_number = 0;
					card_number = (card[0] << 0) | (card[1] << 8) | (card[2] << 16);
					WiegandAccess_ProcessCard(reader_id, 26, card_number);

                } else {
                    printf("Reader %d: Wiegand26 parity error\r\n", reader_id);
                }
            } else {
                printf("Reader %d: Wiegand26 parity error\r\n", reader_id);
            }
            break;

        case 34:  // 34位韦根
            parity = 0;
            for (i = 0; i < 17; i++) {
                parity ^= ((pWg->UID >> (33 - i)) & 0x01);
            }
            if (parity == 0) {
                parity = 0;
                for (i = 0; i < 17; i++) {
                    parity ^= ((pWg->UID >> (16 - i)) & 0x01);
                }
                if (parity != 0) {
                    card[0] = (pWg->UID >> 1) & 0xFF;
                    card[1] = (pWg->UID >> 9) & 0xFF;
                    card[2] = (pWg->UID >> 17) & 0xFF;
                    card[3] = (pWg->UID >> 25) & 0xFF;

                    printf("Reader %d: Wiegand34, Card=0x%02X%02X%02X%02X\r\n",
                           reader_id, card[3], card[2], card[1], card[0]);

                    uint32_t card_number = 0;
                    card_number = (card[0] << 0) | (card[1] << 8) | (card[2] << 16) | (card[3] << 24);
                    WiegandAccess_ProcessCard(reader_id, 34, card_number);

                } else {
                    printf("Reader %d: Wiegand34 parity error\r\n", reader_id);
                }
            } else {
                printf("Reader %d: Wiegand34 parity error\r\n", reader_id);
            }
            break;

        default:
            printf("Reader %d: Invalid bits = %d\r\n", reader_id, pWg->BitCount);
            break;
    }
}


