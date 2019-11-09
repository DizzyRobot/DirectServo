#include "main.h"

volatile bool buttonIdPressed;
volatile bool buttonCalibPressed;
volatile int32_t buttonPressId = 0;

const int32_t BLINK_PERIOD = 0x100;
const int32_t BLINK_DUTY_CYCLE = 0x40;
const int32_t BLINK_PRESCALER = 0x6000;

void stopIdTimer();
void stopCalibTimer();

extern "C" void EXTI2_3_IRQHandler(void)
{
    delay_ms(5);

    if ((EXTI->IMR & EXTI_IMR_MR2) && (EXTI->PR & EXTI_PR_PR2))
    //if (GPIOA->IDR & GPIO_IDR_2)
    {
        if (GPIOA->IDR & GPIO_IDR_2)
        {
            stopIdTimer();

            GPIOB->BSRR |= 0x02;                                // set pin B-1
            delay_ms(100);
            GPIOB->BRR |= 0x02;                                    // reset pin B-1

            buttonIdPressed = true;
        }

        EXTI->PR |= EXTI_PR_PR2;
    }

    if ((EXTI->IMR & EXTI_IMR_MR3) && (EXTI->PR & EXTI_PR_PR3))
    //if (GPIOA->IDR & GPIO_IDR_3)
    {
        if (GPIOA->IDR & GPIO_IDR_3)
        {
            stopCalibTimer();

            GPIOA->BSRR |= 0x01;                                // set pin A-0
            delay_ms(100);
            GPIOA->BRR |= 0x01;                                    // reset pin A-0

            buttonCalibPressed = true;
        }

        EXTI->PR |= EXTI_PR_PR3;
    }
}

void stopIdTimer()
{
    TIM3->CR1 &= ~TIM_CR1_CEN;                            // disable timer 3
    GPIOB->MODER &= ~GPIO_MODER_MODER1_Msk;
    GPIOB->MODER |= (0x01 << GPIO_MODER_MODER1_Pos);// output mode for pin B-1
    GPIOB->BRR |= 0x02;                                    // reset pin B-1
}
void stopCalibTimer()
{
    TIM2->CR1 &= ~TIM_CR1_CEN;                            // disable timer 2
    GPIOA->MODER &= ~GPIO_MODER_MODER0_Msk;
    GPIOA->MODER |= (0x01 << GPIO_MODER_MODER0_Pos);    // output mode for A-0
    GPIOA->BRR |= 0x01;                                    // reset pin A-0
}

void blinkId(bool onOff)
{
    if (onOff)
    {
        // pin 19 - PB-1 configure as led timer 3 blinker

        RCC->AHBENR |= RCC_AHBENR_GPIOBEN;            // enable clock for GPIOB
        RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;                    // enable timer 3

        TIM3->ARR = BLINK_PERIOD;                            // tim3 period
        TIM3->PSC = BLINK_PRESCALER;                        // prescaler

        // tim3 config channel

        TIM3->CCMR2 |= TIM_CCMR2_OC4PE |    // Output compare 4 preload enable
                (0x06 << TIM_CCMR2_OC4M_Pos);    // Output compare 4 mode = 110

        // GPIOB

        GPIOB->MODER &= ~GPIO_MODER_MODER1_Msk;
        GPIOB->MODER |= (0x02 << GPIO_MODER_MODER1_Pos);// alternative function for pin B-1
        GPIOB->OSPEEDR |= GPIO_OSPEEDR_OSPEEDR1;    // high speed for pin B-1
        GPIOB->AFR[0] |= (0x01 << GPIO_AFRL_AFSEL1_Pos);// alternative funciton 1 for pin B-1

        TIM3->CCR4 = BLINK_DUTY_CYCLE;            // duty cycle of tim3 channel 4
        TIM3->CCER |= TIM_CCER_CC4E;        // enable tim3 channel 4, positive
        TIM3->CR1 |= TIM_CR1_CEN;                            // enable timer 3
    }
    else
    {
        stopIdTimer();
    }
}

void blinkCalib(bool onOff)
{
    if (onOff)
    {
        // pin 11 - PA-0 configure as led timer 2 blinker

        RCC->AHBENR |= RCC_AHBENR_GPIOAEN;            // enable clock for GPIOA
        RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;                    // enable timer 2

        TIM2->ARR = BLINK_PERIOD;                            // tim2 period
        TIM2->PSC = BLINK_PRESCALER;                        // prescaler

        // tim2 config channel

        TIM2->CCMR1 |= TIM_CCMR1_OC1PE |    // Output compare 1 preload enable
                (0x06 << TIM_CCMR1_OC1M_Pos);    // Output compare 1 mode = 110

        // GPIOA

        GPIOA->MODER &= ~GPIO_MODER_MODER0_Msk;
        GPIOA->MODER |= (0x02 << GPIO_MODER_MODER0_Pos);// alternative function for pin A-0
        GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEEDR0;    // high speed for pin A-0
        GPIOA->AFR[0] |= (0x02 << GPIO_AFRL_AFSEL0_Pos);// alternative funciton 2 for pin A-0

        TIM2->CCR1 = BLINK_DUTY_CYCLE;            // duty cycle of tim2 channel 1
        TIM2->CCER |= TIM_CCER_CC1E;        // enable tim2 channel 1, positive
        TIM2->CR1 |= TIM_CR1_CEN;                            // enable timer 2
    }
    else
    {
        stopCalibTimer();
    }
}

void incrementIdAndSave()
{
    ConfigData lc;
    memcpy(&lc, config, sizeof(ConfigData));
    lc.controllerId = ++buttonPressId;

    writeFlash((uint16_t*) &lc, sizeof(ConfigData) / sizeof(uint16_t));
}

void initButtons()
{
    buttonIdPressed = false;
    buttonCalibPressed = false;

    GPIOA->PUPDR |= (0x02 << GPIO_PUPDR_PUPDR2_Pos) |    // pull-down A-2
            (0x02 << GPIO_PUPDR_PUPDR3_Pos);    // pull-down A-3

    EXTI->IMR |= EXTI_IMR_MR2 |        // enable line 2
            EXTI_IMR_MR3;        // enable line 3

    EXTI->RTSR |= EXTI_RTSR_RT2 |    // raizing edge for line 2
            EXTI_RTSR_RT3;    // raizing edge for line 3

    HAL_NVIC_SetPriority(EXTI2_3_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);
}

