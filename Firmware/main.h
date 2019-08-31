
#ifndef MAIN_H
#define MAIN_H

#include <stm32f0xx_hal.h>
#include <stm32_hal_legacy.h>

#define POSITIVE_MODULO(A, B)	((A % B + B) %B)

const uint32_t flashPageAddress = 0x08007800;
const int32_t numQuadrants = 32;

struct QuadrantData
{
    uint32_t maxAngle = 0;
    uint32_t minAngle = 0;
    uint32_t range = 0;
};

const int32_t TorqueLimitMax  = 0xFF*32;
const int32_t DefaultTorqueSearchMagnitude  = TorqueLimitMax*0.4;  // by default use 40% torque to find limits
const int32_t DefaultAngleOffsetBuffer  = (0xFFFF/360)*10; // by default use 10 deg of angular buffer when setting the min/max angles
struct FailsafeConfig
{
	// when the max or min angle is violated, the torque will be limited to TorqueLimit.
	// The sign of TorqueLimit is in the direction of the limited angle, so a +3 torque limit will cut the torque to
	// 3 in the direction of the limit, while a -10 limit will cut the torque to 10 pushing away from the limited angle.
	// If the TorqueLimit is TorqueLimitMax (the default) the limit has no effect, because the commanded torque can't exceed that value anyway.
	int32_t TorqueLimit = TorqueLimitMax;
	int32_t TorqueSearchMagnitude =  DefaultTorqueSearchMagnitude;
	int32_t AngleOffsetBuffer = DefaultAngleOffsetBuffer;
	int32_t MaxAngle = 0x7FFF0000; // 0x7FFF0000 is the max value ReportedAngle. Which is the max of TrackedAngle (a int32_t) minus the max value of StartupAngle (a uint16_t)
	int32_t MinAngle = -0x7FFF0000;
};
extern bool FailsafeCalibrated;

struct ConfigData
{
    int32_t controllerId = 0;
    QuadrantData quadrants[numQuadrants];
    bool up = false;
    bool calibrated = false;
    FailsafeConfig Failsafe;
};

extern ConfigData* config;

void ensureConfigured();
void ResetFlash();

// clock ----------------------------------------------------------------------

extern volatile uint32_t gTickCount;

void initClockInternal();
void initClockExternal();
void initSysTick();
void delay(uint32_t ms);

// pwm ------------------------------------------------------------------------

#define MIN_TPOWER_THRESHOLD							(3 * 32)
#define GPIO_BRR_00_STANDBY_MODE_ENABLE					((1UL << 6) | (1UL << 7))
#define GPIO_BSRR_00_STANDBY_MODE_DISABLE_TO_OC_100MV	(1UL << 7)
#define GPIO_BSRR_00_STANDBY_MODE_DISABLE_TO_OC_250MV	(1UL << 6)
#define GPIO_BSRR_00_STANDBY_MODE_DISABLE_TO_OC_500MV	((1UL << 6) (1UL << 7))

#define sin_period		(1 << 15)		// 32K or 0x8000
#define sin_range		(1 << 13)		//  8K or 0x2000

void initPwm();
void setPwm(int32_t angle, int32_t power);
void setPwmTorque();

// calibrate ------------------------------------------------------------------

#define SENSOR_MAX sin_period	// 32K

bool calibrate();
int32_t getElectricDegrees();
bool LimitCalibrate(bool UseFlash,
				int32_t NewTorqueLimit = TorqueLimitMax,
				int32_t NewTorqueSearchMagnitude = DefaultTorqueSearchMagnitude,
				int32_t NewAngleOffsetBuffer = DefaultAngleOffsetBuffer
				);

// buttons --------------------------------------------------------------------

extern volatile bool buttonIdPressed;
extern volatile bool buttonCalibPressed;
extern volatile int32_t buttonPressId;

void initButtons();
void blinkId(bool onOff);
void blinkCalib(bool onOff);
void incrementIdAndSave();

// spi ------------------------------------------------------------------------

extern int32_t TrackedAngle;
extern uint16_t StartupAngle;
extern int16_t spiCurrentAngle;
extern int32_t ReportedAngle;

void initSpi();
uint16_t SpiWriteRead(uint16_t data);
int16_t spiReadAngle();
void spiReadAngleFiltered();

// usart ----------------------------------------------------------------------

extern volatile int32_t usartTorqueCommandValue;
extern volatile bool usartDmaSendRequested;
extern volatile bool usartDmaSendBusy;
extern volatile bool usartCommandReceived;

void initUsart();
void usartSendAngle();
void usartSendError();
void processUsartCommand();

// flash ----------------------------------------------------------------------

void writeFlash(uint16_t* data, int32_t count);
void memcpy(void *dst, const void *src, int32_t count);

#endif

