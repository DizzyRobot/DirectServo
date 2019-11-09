#include "main.h"

bool FailsafeCalibrated = false;

void ensureConfigured()
{
    FailsafeCalibrated = FailsafeCalibrated
            || config->Failsafe.TorqueLimit == TorqueLimitMax;
    bool calibConfigured = config->calibrated && FailsafeCalibrated;
    bool idConfigured = config->controllerId != 0 && config->controllerId != -1;
    blinkCalib(!calibConfigured);
    blinkId(!idConfigured);
}

void ResetFlash()
{
    ConfigData DefaultConfig;
    writeFlash((uint16_t*) &DefaultConfig,
            sizeof(ConfigData) / sizeof(uint16_t));
    ensureConfigured();
}

int main(void)
{
    initClockExternal();
    initButtons();
    initUsart();
    initSpi();
    initSysTick();
    initPwm();

    const bool HardResetFlash = false; // this is here in case commands are broken and a hard reset is needed through re-programming
    if (HardResetFlash)
        ResetFlash();

    ensureConfigured();

    buttonIdPressed = false;
    buttonCalibPressed = false;
    bool blinkIDnum = false;
    uint32_t gTickCount_IDPress = gTickCount;

    while (true)
    {
        spiReadAngleFiltered();
        setPwmTorque();

        if (usartCommandReceived)
        {
            processUsartCommand();
            usartCommandReceived = false;
        }

        if (buttonIdPressed)
        {
            incrementIdAndSave();
            buttonIdPressed = false;
            blinkIDnum = true;
            gTickCount_IDPress = gTickCount;
        }

        if (blinkIDnum)
        {
            if ((gTickCount-gTickCount_IDPress)>10000)
            {
                blinkIDnum = false;
                buttonPressId = 0;
                for (int bl = 0; bl < config->controllerId; bl++)
                {
                    GPIOB->BSRR |= 0x02;   // set pin B-1 (ID led on)
                    delay_ms(250);
                    GPIOB->BRR |= 0x02;    // reset pin B-1 (ID led off)
                    delay_ms(250);
                }
            }
        }

        if (buttonCalibPressed)
        {
            calibrate();
            buttonCalibPressed = false;
        }
    }
}

