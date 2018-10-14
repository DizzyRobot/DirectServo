#include <main.h>

int ensureConfigured() {
	bool calibConfigured = config->calibrated;
	bool idConfigured = config->controllerId != 0 && config->controllerId != -1;
	
	if (!calibConfigured) blinkCalib(true);
	if (!idConfigured) blinkId(true);
	
	while (!calibConfigured || !idConfigured)
	{
		if (buttonCalibPressed)
		{
			calibrate();
			blinkCalib(false);
			
			calibConfigured = true;
			buttonCalibPressed = false;
		}
		
		if (buttonIdPressed)
		{
			incrementIdAndSave();
			blinkId(false);
			
			idConfigured = true;
			buttonIdPressed = false;
		}		
	}
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
	//A1335InitFromFlash();
	
	//usartTorqueCommandValue = -250;	
	
	usartDmaSendRequested = false;
	buttonIdPressed = false;
	buttonCalibPressed = false;

	while (true){
		//spiUpdateTorque();
		spiCurrentAngle = spiReadAngle();
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
