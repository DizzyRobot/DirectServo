#include "main.h"

bool FailsafeCalibrated = false;

void ensureConfigured() {
	FailsafeCalibrated = config->Failsafe.TorqueLimit == TorqueLimitMax;
	bool calibConfigured = config->calibrated && FailsafeCalibrated;
	bool idConfigured = config->controllerId != 0 && config->controllerId != -1;

	if (!calibConfigured)
		blinkCalib(true);
	if (!idConfigured)
		blinkId(true);
}

void ResetFlash()
{
	ConfigData DefaultConfig;
	writeFlash((uint16_t*)&DefaultConfig, sizeof(ConfigData) / sizeof(uint16_t));
	ensureConfigured();
}

int main(void) {
	initClockExternal();
	initButtons();
	initUsart();
	initSpi();
	initSysTick();
	initPwm();

	const bool HartResetFlash = false; // this is here in case commands are broken and a hard reset is needed through re-programming
	if (HartResetFlash)
		ResetFlash();

	ensureConfigured();

	buttonIdPressed = false;
	buttonCalibPressed = false;

	while (true) {
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
		}

		if (buttonCalibPressed)
		{
			calibrate();
			buttonCalibPressed = false;
		}
	}
}

