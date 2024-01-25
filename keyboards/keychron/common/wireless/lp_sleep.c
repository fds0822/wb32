#define LOWPOWER_ENABLE
#ifdef LOWPOWER_ENABLE
#include "quantum.h"

static const uint32_t pre_lp_code[] = {553863175u,554459777u,1208378049u,4026624001u,688390415u,554227969u,3204472833u,1198571264u,1073807360u,1073808388u};
#define PRE_LP()  ((void(*)(void))((unsigned int)(pre_lp_code) | 0x01))()

static const uint32_t post_lp_code[] = {553863177u,554459777u,1208509121u,51443856u,4026550535u,1745485839u,3489677954u,536895496u,673389632u,1198578684u,1073807360u,536866816u,1073808388u};
#define POST_LP()  ((void(*)(void))((unsigned int)(post_lp_code) | 0x01))()

static ioline_t row_pins[MATRIX_ROWS] = MATRIX_ROW_PINS;
static ioline_t col_pins[MATRIX_COLS] = MATRIX_COL_PINS;

extern void __early_init(void);
extern void matrix_init_pins(void);

void stop_mode_entry(void) {

  EXTI->PR = 0x7FFFF;
  for (uint8_t i = 0; i < 8; i++) {
    for (uint8_t j = 0; j < 32; j++) {
      if (NVIC->ISPR[i] & (0x01UL < j)) {
        NVIC->ICPR[i] = (0x01UL < j);
      }
    }
  }
  SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk; // Clear Systick IRQ Pending

  /* Clear all bits except DBP and FCLKSD bit */
  PWR->CR0 &= 0x09U;

  /* STOP LP4 MODE S32KON */
  PWR->CR0 |= 0x3B004U;
  PWR->CFGR = 0x3B3;

  PRE_LP();

  /* Set SLEEPDEEP bit of Cortex System Control Register */
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

  /* Request Wait For Interrupt */
  __WFI();

  POST_LP();

  /* Clear SLEEPDEEP bit of Cortex System Control Register */
  SCB->SCR &= (~SCB_SCR_SLEEPDEEP_Msk);
}

void wb32_exti_nvic_init(iopadid_t pad) {

  switch (pad) {
    case 0:
      nvicEnableVector(EXTI0_IRQn, WB32_IRQ_EXTI0_PRIORITY);
      break;
    case 1:
      nvicEnableVector(EXTI1_IRQn, WB32_IRQ_EXTI1_PRIORITY);
      break;
    case 2:
      nvicEnableVector(EXTI2_IRQn, WB32_IRQ_EXTI2_PRIORITY);
      break;
    case 3:
      nvicEnableVector(EXTI3_IRQn, WB32_IRQ_EXTI3_PRIORITY);
      break;
    case 4:
      nvicEnableVector(EXTI4_IRQn, WB32_IRQ_EXTI4_PRIORITY);
      break;
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
      nvicEnableVector(EXTI9_5_IRQn, WB32_IRQ_EXTI5_9_PRIORITY);
      break;
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
      nvicEnableVector(EXTI15_10_IRQn, WB32_IRQ_EXTI10_15_PRIORITY);
      break;
  }
}
#if 0
void _pal_lld_enablepadevent(ioportid_t port,
                             iopadid_t pad,
                             ioeventmode_t mode) {
  uint32_t padmask, cridx, croff, crmask, portidx;

  /* Enable EXTI clock.*/
  rccEnableEXTI();

  /* Mask of the pad.*/
  padmask = 1U << (uint32_t)pad;

  /* Multiple channel setting of the same channel not allowed, first
     disable it. This is done because on WB32 the same channel cannot
     be mapped on multiple ports.*/
  osalDbgAssert(((EXTI->RTSR & padmask) == 0U) &&
                ((EXTI->FTSR & padmask) == 0U),
                "channel already in use");

  /* Index and mask of the SYSCFG CR register to be used.*/
  cridx  = (uint32_t)pad >> 2U;
  croff  = ((uint32_t)pad & 3U) * 4U;
  crmask = ~(0xFU << croff);

  /* Port index is obtained assuming that GPIO ports are placed at  regular
     0x400 intervals in memory space. So far this is true for all devices.*/
  portidx = (((uint32_t)port - (uint32_t)GPIOA) >> 10U) & 0xFU;

  /* Port selection in SYSCFG.*/
  AFIO->EXTICR[cridx] = (AFIO->EXTICR[cridx] & crmask) | (portidx << croff);

  /* Programming edge registers.*/
  if (mode & PAL_EVENT_MODE_RISING_EDGE)
    EXTI->RTSR |= padmask;
  else
    EXTI->RTSR &= ~padmask;

  if (mode & PAL_EVENT_MODE_FALLING_EDGE)
    EXTI->FTSR |= padmask;
  else
    EXTI->FTSR &= ~padmask;

  EXTI->PR = padmask;

  /* Programming interrupt and event registers.*/
  EXTI->IMR |= padmask;
  EXTI->EMR &= ~padmask;
}
#endif

void lowpower_before_config(void) {
  chSysLock();
#if DIODE_DIRECTION == ROW2COL
  for (int col = 0; col < MATRIX_COLS; col++) {
    setPinOutput(col_pins[col]);
    writePinLow(col_pins[col]);
  }

  for (int row = 0; row < MATRIX_ROWS; row++) {
    setPinInputHigh(row_pins[row]);
    waitInputPinDelay();

    /* 
     * Enabling events on both edges of the button line.
     */
    _pal_lld_enablepadevent(PAL_PORT(row_pins[row]), PAL_PAD(row_pins[row]), PAL_EVENT_MODE_FALLING_EDGE);

    /*
     * Configure the interrupt priority.
     */
    wb32_exti_nvic_init(PAL_PAD(row_pins[row]));
  }
#endif
  chSysUnlock();
}

void lowpower_after_config(void) {

  RCC->APB1PRE |= RCC_APB1PRE_SRCEN;
  RCC->APB1ENR |= RCC_APB1ENR_EXTIEN;

  __early_init();
  matrix_init_pins();

#if (WB32_SPI_USE_QSPI == TRUE)
  rccEnableQSPI();
#endif
#if (WB32_SPI_USE_SPIM2 == TRUE)
  rccEnableSPIM2();
#endif
#if (WB32_I2C_USE_I2C1 == TRUE)
  rccEnableI2C1();
#endif
#if (WB32_I2C_USE_I2C2 == TRUE)
  rccEnableI2C2();
#endif
}

#if 1
OSAL_IRQ_HANDLER(WB32_EXTI0_IRQ_VECTOR) {
  uint32_t pr;

  OSAL_IRQ_PROLOGUE();

  pr = EXTI->PR & EXTI_PR_PR0;

  if ((pr & EXTI_PR_PR0) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR0);
  }

  NVIC_DisableIRQ(EXTI0_IRQn);

  EXTI->PR = EXTI_PR_PR0;

  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(WB32_EXTI1_IRQ_VECTOR) {
  uint32_t pr;

  OSAL_IRQ_PROLOGUE();

  pr = EXTI->PR & EXTI_PR_PR1;

  if ((pr & EXTI_PR_PR1) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR1);
  }

  NVIC_DisableIRQ(EXTI1_IRQn);

  EXTI->PR = EXTI_PR_PR1;

  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(WB32_EXTI2_IRQ_VECTOR) {
  uint32_t pr;

  OSAL_IRQ_PROLOGUE();

  pr = EXTI->PR & EXTI_PR_PR2;

  if ((pr & EXTI_PR_PR2) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR2);
  }

  NVIC_DisableIRQ(EXTI2_IRQn);

  EXTI->PR = EXTI_PR_PR2;

  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(WB32_EXTI3_IRQ_VECTOR) {
  uint32_t pr;

  OSAL_IRQ_PROLOGUE();

  pr = EXTI->PR & EXTI_PR_PR3;

  if ((pr & EXTI_PR_PR3) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR3);
  }

  NVIC_DisableIRQ(EXTI3_IRQn);

  EXTI->PR = EXTI_PR_PR3;

  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(WB32_EXTI4_IRQ_VECTOR) {
  uint32_t pr;

  OSAL_IRQ_PROLOGUE();

  pr = EXTI->PR & EXTI_PR_PR4;

  if ((pr & EXTI_PR_PR4) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR4);
  }

  NVIC_DisableIRQ(EXTI4_IRQn);

  EXTI->PR = EXTI_PR_PR4;

  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(WB32_EXTI9_5_IRQ_VECTOR) {
  uint32_t pr;

  OSAL_IRQ_PROLOGUE();

  pr = EXTI->PR & (EXTI_PR_PR5 | EXTI_PR_PR6 | EXTI_PR_PR7 | EXTI_PR_PR8 | EXTI_PR_PR9);

  if ((pr & EXTI_PR_PR5) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR5);
  }

  if ((pr & EXTI_PR_PR6) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR6);
  }

  if ((pr & EXTI_PR_PR7) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR7);
  }

  if ((pr & EXTI_PR_PR8) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR8);
  }

  if ((pr & EXTI_PR_PR9) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR9);
  }

  NVIC_DisableIRQ(EXTI9_5_IRQn);

  EXTI->PR = EXTI_PR_PR5 |
             EXTI_PR_PR6 |
             EXTI_PR_PR7 |
             EXTI_PR_PR8 |
             EXTI_PR_PR9;

  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(WB32_EXTI15_10_IRQ_VECTOR) {
  uint32_t pr;

  OSAL_IRQ_PROLOGUE();

  pr = EXTI->PR & (EXTI_PR_PR10 | EXTI_PR_PR11 | EXTI_PR_PR12 | EXTI_PR_PR13 | EXTI_PR_PR14 | EXTI_PR_PR15);

  if ((pr & EXTI_PR_PR10) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR10);
  }

  if ((pr & EXTI_PR_PR11) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR11);
  }

  if ((pr & EXTI_PR_PR12) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR12);
  }

  if ((pr & EXTI_PR_PR13) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR13);
  }

  if ((pr & EXTI_PR_PR14) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR14);
  }

  if ((pr & EXTI_PR_PR15) != 0) {
    EXTI->IMR &= ~(EXTI_IMR_MR15);
  }

  NVIC_DisableIRQ(EXTI15_10_IRQn);

  EXTI->PR = EXTI_PR_PR10 |
             EXTI_PR_PR11 |
             EXTI_PR_PR12 |
             EXTI_PR_PR13 |
             EXTI_PR_PR14 |
             EXTI_PR_PR15;

  OSAL_IRQ_EPILOGUE();
}
#endif
#endif