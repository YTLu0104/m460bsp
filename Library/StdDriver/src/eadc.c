/**************************************************************************//**
 * @file     eadc.c
 * @version  V2.00
 * @brief    EADC driver source file
 *
 * SPDX-License-Identifier: Apache-2.0
 * @copyright (C) 2021 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/
#include "NuMicro.h"

/** @addtogroup Standard_Driver Standard Driver
  @{
*/

/** @addtogroup EADC_Driver EADC Driver
  @{
*/

/** @addtogroup EADC_EXPORTED_FUNCTIONS EADC Exported Functions
  @{
*/

/**
  * @brief This function make EADC_module be ready to convert.
  * @param[in] eadc The pointer of the specified EADC module.
  * @param[in] u32InputMode Decides the input mode.
  *                       - \ref EADC_CTL_DIFFEN_SINGLE_END      :Single end input mode.
  *                       - \ref EADC_CTL_DIFFEN_DIFFERENTIAL    :Differential input type.
  * @retval 0                EADC operation OK.
  * @retval EADC_TIMEOUT_ERR EADC operation abort due to timeout error.
  * @retval EADC_CAL_ERR     EADC has calibration error.
  * @retval EADC_CLKDIV_ERR  EADC clock frequency is configured error.
  * @details This function is used to set analog input mode and enable A/D Converter.
  *         Before starting A/D conversion function, ADCEN bit (EADC_CTL[0]) should be set to 1.
  * @note This API will reset and calibrate EADC if EADC never be calibrated after chip power on.
  * @note This function retrun EADC_TIMEOUT_ERR if CALIF(CALSR[16]) is not set to 1.
  */
int32_t EADC_Open(EADC_T *eadc, uint32_t u32InputMode)
{
    uint32_t u32Delay = SystemCoreClock >> 4;
    uint32_t u32ClkSel0Backup, u32ClkDivBackup, u32PclkDivBackup, u32RegLockBackup = 0;

    eadc->CTL &= (~EADC_CTL_DIFFEN_Msk);

    eadc->CTL |= (u32InputMode | EADC_CTL_ADCEN_Msk);

    /* Do calibration for EADC to decrease the effect of electrical random noise. */
    if ((eadc->CALSR & EADC_CALSR_CALIF_Msk) == 0)
    {
        /* Must reset ADC before ADC calibration */
        eadc->CTL |= EADC_CTL_ADCRST_Msk;
        while((eadc->CTL & EADC_CTL_ADCRST_Msk) == EADC_CTL_ADCRST_Msk)
        {
            if (--u32Delay == 0)
            {
                return EADC_TIMEOUT_ERR;
            }
        }

        /* Registers backup */
        u32ClkSel0Backup = CLK->CLKSEL0;
        u32PclkDivBackup = CLK->PCLKDIV;

        u32RegLockBackup = SYS_IsRegLocked();

        /* Unlock protected registers */
        SYS_UnlockReg();

        /* Set EADC clock is less than 2*PCLK to do calibration correctly. */
        if (eadc == EADC0)
        {
            u32ClkDivBackup = CLK->CLKDIV0;
            CLK->CLKDIV0 = (CLK->CLKDIV0 & ~CLK_CLKDIV0_EADC0DIV_Msk) | (2 << CLK_CLKDIV0_EADC0DIV_Pos);
            CLK->CLKSEL0 = (CLK->CLKSEL0 & ~CLK_CLKSEL0_EADC0SEL_Msk) | CLK_CLKSEL0_EADC0SEL_HCLK;
        }
        else if (eadc == EADC1)
        {
            u32ClkDivBackup = CLK->CLKDIV2;
            CLK->CLKDIV2 = (CLK->CLKDIV2 & ~CLK_CLKDIV2_EADC1DIV_Msk) | (2 << CLK_CLKDIV2_EADC1DIV_Pos);
            CLK->CLKSEL0 = (CLK->CLKSEL0 & ~CLK_CLKSEL0_EADC1SEL_Msk) | CLK_CLKSEL0_EADC1SEL_HCLK;
        }
        else if (eadc == EADC2)
        {
            u32ClkDivBackup = CLK->CLKDIV5;
            CLK->CLKDIV5 = (CLK->CLKDIV5 & ~CLK_CLKDIV5_EADC2DIV_Msk) | (2 << CLK_CLKDIV5_EADC2DIV_Pos);
            CLK->CLKSEL0 = (CLK->CLKSEL0 & ~CLK_CLKSEL0_EADC2SEL_Msk) | CLK_CLKSEL0_EADC2SEL_HCLK;
        }
        CLK->PCLKDIV = (CLK->PCLKDIV & ~CLK_PCLKDIV_APB1DIV_Msk);

        eadc->CALSR |= EADC_CALSR_CALIF_Msk;  /* Clear Calibration Finish Interrupt Flag */
        eadc->CALCTL = (eadc->CALCTL & ~(0x000F0000))|0x00020000;
        eadc->CALCTL |= EADC_CALCTL_CAL_Msk;  /* Enable Calibration function */

        u32Delay = SystemCoreClock >> 4;
        while((eadc->CALSR & EADC_CALSR_CALIF_Msk) != EADC_CALSR_CALIF_Msk)
        {
            if (--u32Delay == 0)
            {
                return EADC_CAL_ERR;
            }
        }

        /* Restore registers */
        CLK->PCLKDIV = u32PclkDivBackup;
        CLK->CLKSEL0 = u32ClkSel0Backup;
        if (eadc == EADC0)
        {
            CLK->CLKDIV0 = u32ClkDivBackup;
        }
        else if (eadc == EADC1)
        {
            CLK->CLKDIV2 = u32ClkDivBackup;
        }
        else if (eadc == EADC2)
        {
            CLK->CLKDIV5 = u32ClkDivBackup;
        }
        if (u32RegLockBackup)
        {
            /* Lock protected registers */
            SYS_LockReg();
        }
    }

    /* Check EADC clock frequency must not faster than PCLK */
    if (eadc == EADC0)
    {
        if (((CLK->PCLKDIV & CLK_PCLKDIV_APB1DIV_Msk) >> CLK_PCLKDIV_APB1DIV_Pos) >
            ((CLK->CLKDIV0 & CLK_CLKDIV0_EADC0DIV_Msk) >> CLK_CLKDIV0_EADC0DIV_Pos))
        {
            return EADC_CLKDIV_ERR;
        }
    }
    else if (eadc == EADC1)
    {
        if (((CLK->PCLKDIV & CLK_PCLKDIV_APB1DIV_Msk) >> CLK_PCLKDIV_APB1DIV_Pos) >
            ((CLK->CLKDIV2 & CLK_CLKDIV2_EADC1DIV_Msk) >> CLK_CLKDIV2_EADC1DIV_Pos))
        {
            return EADC_CLKDIV_ERR;
        }
    }
    else if (eadc == EADC2)
    {
        if (((CLK->PCLKDIV & CLK_PCLKDIV_APB1DIV_Msk) >> CLK_PCLKDIV_APB1DIV_Pos) >
            ((CLK->CLKDIV5 & CLK_CLKDIV5_EADC2DIV_Msk) >> CLK_CLKDIV5_EADC2DIV_Pos))
        {
            return EADC_CLKDIV_ERR;
        }
    }

    return 0;
}

/**
  * @brief Disable EADC_module.
  * @param[in] eadc The pointer of the specified EADC module.
  * @return None
  * @details Clear ADCEN bit (EADC_CTL[0]) to disable A/D converter analog circuit power consumption.
  */
void EADC_Close(EADC_T *eadc)
{
    eadc->CTL &= ~EADC_CTL_ADCEN_Msk;
}

/**
  * @brief Configure the sample control logic module.
  * @param[in] eadc The pointer of the specified EADC module.
  * @param[in] u32ModuleNum Decides the sample module number, valid value are from 0 to 15.
  * @param[in] u32TriggerSrc Decides the trigger source. Valid values are:
  *                            - \ref EADC_SOFTWARE_TRIGGER              : Disable trigger
  *                            - \ref EADC_FALLING_EDGE_TRIGGER          : STADC pin falling edge trigger
  *                            - \ref EADC_RISING_EDGE_TRIGGER           : STADC pin rising edge trigger
  *                            - \ref EADC_FALLING_RISING_EDGE_TRIGGER   : STADC pin both falling and rising edge trigger
  *                            - \ref EADC_ADINT0_TRIGGER                : EADC ADINT0 interrupt EOC pulse trigger
  *                            - \ref EADC_ADINT1_TRIGGER                : EADC ADINT1 interrupt EOC pulse trigger
  *                            - \ref EADC_TIMER0_TRIGGER                : Timer0 overflow pulse trigger
  *                            - \ref EADC_TIMER1_TRIGGER                : Timer1 overflow pulse trigger
  *                            - \ref EADC_TIMER2_TRIGGER                : Timer2 overflow pulse trigger
  *                            - \ref EADC_TIMER3_TRIGGER                : Timer3 overflow pulse trigger
  *                            - \ref EADC_EPWM0TG0_TRIGGER              : EPWM0TG0 trigger
  *                            - \ref EADC_EPWM0TG1_TRIGGER              : EPWM0TG1 trigger
  *                            - \ref EADC_EPWM0TG2_TRIGGER              : EPWM0TG2 trigger
  *                            - \ref EADC_EPWM0TG3_TRIGGER              : EPWM0TG3 trigger
  *                            - \ref EADC_EPWM0TG4_TRIGGER              : EPWM0TG4 trigger
  *                            - \ref EADC_EPWM0TG5_TRIGGER              : EPWM0TG5 trigger
  *                            - \ref EADC_EPWM1TG0_TRIGGER              : EPWM1TG0 trigger
  *                            - \ref EADC_EPWM1TG1_TRIGGER              : EPWM1TG1 trigger
  *                            - \ref EADC_EPWM1TG2_TRIGGER              : EPWM1TG2 trigger
  *                            - \ref EADC_EPWM1TG3_TRIGGER              : EPWM1TG3 trigger
  *                            - \ref EADC_EPWM1TG4_TRIGGER              : EPWM1TG4 trigger
  *                            - \ref EADC_EPWM1TG5_TRIGGER              : EPWM1TG5 trigger
  *                            - \ref EADC_BPWM0TG_TRIGGER               : BPWM0TG trigger
  *                            - \ref EADC_BPWM1TG_TRIGGER               : BPWM1TG trigger
  * @param[in] u32Channel Specifies the sample module channel, valid value are from 0 to 15.
  * @return None
  * @details Each of ADC control logic modules 0~15 which is configurable for ADC converter channel EADC_CH0~15 and trigger source.
  *         sample module 16~18 is fixed for ADC channel 16, 17, 18 input sources as band-gap voltage, temperature sensor, and battery power (VBAT).
  */
void EADC_ConfigSampleModule(EADC_T *eadc, \
                             uint32_t u32ModuleNum, \
                             uint32_t u32TriggerSrc, \
                             uint32_t u32Channel)
{
    eadc->SCTL[u32ModuleNum] &= ~(EADC_SCTL_EXTFEN_Msk | EADC_SCTL_EXTREN_Msk | EADC_SCTL_TRGSEL_Msk | EADC_SCTL_CHSEL_Msk);
    eadc->SCTL[u32ModuleNum] |= (u32TriggerSrc | u32Channel);
}


/**
  * @brief Set trigger delay time.
  * @param[in] eadc The pointer of the specified EADC module.
  * @param[in] u32ModuleNum Decides the sample module number, valid value are from 0 to 15.
  * @param[in] u32TriggerDelayTime Decides the trigger delay time, valid range are between 0~0xFF.
  * @param[in] u32DelayClockDivider Decides the trigger delay clock divider. Valid values are:
    *                                - \ref EADC_SCTL_TRGDLYDIV_DIVIDER_1    : Trigger delay clock frequency is ADC_CLK/1
    *                                - \ref EADC_SCTL_TRGDLYDIV_DIVIDER_2    : Trigger delay clock frequency is ADC_CLK/2
    *                                - \ref EADC_SCTL_TRGDLYDIV_DIVIDER_4    : Trigger delay clock frequency is ADC_CLK/4
    *                                - \ref EADC_SCTL_TRGDLYDIV_DIVIDER_16   : Trigger delay clock frequency is ADC_CLK/16
  * @return None
  * @details User can configure the trigger delay time by setting TRGDLYCNT (EADC_SCTLn[15:8], n=0~15) and TRGDLYDIV (EADC_SCTLn[7:6], n=0~15).
  *         Trigger delay time = (u32TriggerDelayTime) x Trigger delay clock period.
  */
void EADC_SetTriggerDelayTime(EADC_T *eadc, \
                              uint32_t u32ModuleNum, \
                              uint32_t u32TriggerDelayTime, \
                              uint32_t u32DelayClockDivider)
{
    eadc->SCTL[u32ModuleNum] &= ~(EADC_SCTL_TRGDLYDIV_Msk | EADC_SCTL_TRGDLYCNT_Msk);
    eadc->SCTL[u32ModuleNum] |= ((u32TriggerDelayTime << EADC_SCTL_TRGDLYCNT_Pos) | u32DelayClockDivider);
}

/**
  * @brief Set ADC extend sample time.
  * @param[in] eadc The pointer of the specified EADC module.
  * @param[in] u32ModuleNum Decides the sample module number, valid value are from 0 to 18.
  * @param[in] u32ExtendSampleTime Decides the extend sampling time, the range is from 0~255 ADC clock. Valid value are from 0 to 0xFF.
  * @return None
  * @details When A/D converting at high conversion rate, the sampling time of analog input voltage may not enough if input channel loading is heavy,
  *         user can extend A/D sampling time after trigger source is coming to get enough sampling time.
  */
void EADC_SetExtendSampleTime(EADC_T *eadc, uint32_t u32ModuleNum, uint32_t u32ExtendSampleTime)
{
    eadc->SCTL[u32ModuleNum] &= ~EADC_SCTL_EXTSMPT_Msk;

    eadc->SCTL[u32ModuleNum] |= (u32ExtendSampleTime << EADC_SCTL_EXTSMPT_Pos);

}

/*@}*/ /* end of group EADC_EXPORTED_FUNCTIONS */

/*@}*/ /* end of group EADC_Driver */

/*@}*/ /* end of group Standard_Driver */
