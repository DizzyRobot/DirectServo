#include <main.h>

int ensureConfigured() {
	bool calibConfigured = config->calibrated;
	bool idConfigured = config->controllerId != 0 && config->controllerId != -1;
	
	if (!calibConfigured) blinkCalib(true);
	if (!idConfigured) blinkId(true);
}

//

int main(void) {
	initClockExternal();
	initButtons();
	initUsart();
	initSpi();
	initSysTick();
	//delay(100);
	initPwm();
	
	//calibrate();
	delay(2000);
	
	ensureConfigured();
	
	//usartTorqueCommandValue = -250;	
	
	usartDmaSendRequested = false;
	buttonIdPressed = false;
	buttonCalibPressed = false;

	while (true){
		spiReadAngleFiltered();
		setPwmTorque();
		
		if (usartDmaSendRequested && !usartDmaSendBusy)
		{
			usartSendAngle();
			usartDmaSendRequested = false;
		}
		
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
			usartTorqueCommandValue = 0;
			usartDmaSendRequested = false;
		}
	}
}
