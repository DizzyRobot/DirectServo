#include <main.h>

const int timer_scale = 7;
const int sin_zero = sin_range / 2;
const int phase2 = sin_period / 3;
const int phase3 = sin_period * 2 / 3;
const int ninetyDeg = sin_period / 4;

/// A sine approximation via a third-order approx.
/// @param x    Angle (with 2^15 units/circle)
/// @return     Sine value (Q12)
int isin_S3(int x)
{
    // S(x) = x * ( (3<<p) - (x*x>>r) ) >> s
    // n : Q-pos for quarter circle             13
    // A : Q-pos for output                     12
    // p : Q-pos for parentheses intermediate   15
    // r = 2n-p                                 11
    // s = A-1-p-n                              17

	static const int qN = 13, qA = 12, qP = 15, qR = 2*qN - qP, qS = qN + qP + 1 - qA;

	x = x << (30 - qN);          // shift to full s32 range (Q13->Q30)

	if ((x ^ (x << 1)) < 0)     // test for quadrant 1 or 2
		x = (1 << 31) - x;

	x = x >> (30 - qN);

	return x * ((3 << qP) - (x*x >> qR)) >> qS;
}

void initPwm() {
	TIM1->ARR = sin_range / timer_scale;							// tim1 period, about 20kHz

	// tim1 config channel

	TIM1->CCMR1 |= TIM_CCMR1_OC1PE |					// Output compare on channel 1 preload enable (for pin B-13, low-side 1)
		           (0x06 << TIM_CCMR1_OC1M_Pos) |		// Output compare on channel 1 mode = 110 (PWM mode 1)
				   TIM_CCMR1_OC2PE |					// Output compare on channel 2 preload enable (for pin B-14, low-side 2)
		           (0x06 << TIM_CCMR1_OC2M_Pos);		// Output compare on channel 2 mode = 110 (PWM mode 1)
	
	TIM1->CCMR2 |= TIM_CCMR2_OC3PE |					// Output compare channel 3 preload enable (for pin A-10, high-side and pin B-15, low-side)
 			       (0x06 << TIM_CCMR2_OC3M_Pos);		// Output compare 3 mode = 110 (PWM mode 1)
	
	// GPIOA
	
	GPIOA->MODER |= (0x02 << GPIO_MODER_MODER8_Pos) |	// alternative function for pin A-8 (pwm channel 1, positive)
					(0x02 << GPIO_MODER_MODER9_Pos) |	// alternative function for pin A-9 (pwm channel 2, positive)
				    (0x02 << GPIO_MODER_MODER10_Pos) |	// alternative function for pin A-10 (pwm channel 3, positive)
		
					(0x01 << GPIO_MODER_MODER11_Pos);	// output for pin A-11 (overcurrent control)
	
	GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEEDR8 |			// high speed for pin A-8 (pwm channel 1, positive)
				      GPIO_OSPEEDR_OSPEEDR9 |			// high speed for pin A-9 (pwm channel 2, positive)
		              GPIO_OSPEEDR_OSPEEDR10 |			// high speed for pin A-10 (pwm channel 3, positive)
		
					  (0x01 << GPIO_OSPEEDR_OSPEEDR11_Pos);	// medium speed for pin A-11 (overcurrent control)
	
	GPIOA->AFR[1] |= (0x02 << GPIO_AFRH_AFSEL8_Pos) |	// for pin A-8 alternative funciton 2
					 (0x02 << GPIO_AFRH_AFSEL9_Pos) |	// for pin A-9 alternative funciton 2
					 (0x02 << GPIO_AFRH_AFSEL10_Pos);	// for pin A-10 alternative funciton 2
	
	GPIOA->PUPDR |= (0x01 << GPIO_PUPDR_PUPDR11_Pos);	// pull-up for pin A-11 (overcurrent control)
	
	// GPIOB
	
	GPIOB->MODER |= (0x02 << GPIO_MODER_MODER13_Pos) |	// alternate function for pin B-13 (PWM channel 1, negative)
		            (0x02 << GPIO_MODER_MODER14_Pos) |	// alternate function for pin B-14 (PWM channel 2, negative)
		            (0x02 << GPIO_MODER_MODER15_Pos);	// alternate function for pin B-15 (PWM channel 3, negative)
	
	GPIOB->OSPEEDR |= GPIO_OSPEEDR_OSPEEDR13 |			// high speed for pin B-13 (pwm channel 1, negative)
		              GPIO_OSPEEDR_OSPEEDR14 |			// high speed for pin B-14 (pwm channel 2, negative)
		              GPIO_OSPEEDR_OSPEEDR15;			// high speed for pin B-15 (pwm channel 3, negative)
	
	GPIOB->AFR[1] |= (0x02 << GPIO_AFRH_AFSEL13_Pos) |	// for pin B-13 alternative funciton 2
		             (0x02 << GPIO_AFRH_AFSEL14_Pos) |	// for pin B-14 alternative funciton 2
	                 (0x02 << GPIO_AFRH_AFSEL15_Pos);	// for pin B-15 alternative funciton 2
	
	//
	
	TIM1->CR1 |= (0x01 << TIM_CR1_CMS_Pos);				// center-aligned mode 1 - up&down
	
	TIM1->RCR = 0x1;									// repetition counter
	
	TIM1->BDTR |= TIM_BDTR_MOE |						// main output enable
		          TIM_BDTR_OSSR |						// Off-state selection for Run mode
				  TIM_BDTR_OSSI |						// Off-state selection for Idle mode
		          0x30;									// dead-time = 1uS, 48 counts
	
	TIM1->CR1 |= TIM_AUTORELOAD_PRELOAD_ENABLE;			// enable timer 1
	
	TIM1->CCER |= TIM_CCER_CC1E |						// enable channel1, positive
			      TIM_CCER_CC2E |						// enable channel2, positive
		          TIM_CCER_CC3E |						// enable channel3, positive
		
		          TIM_CCER_CC1NE |						// enable channel 1, negative
		          TIM_CCER_CC2NE |						// enable channel 2, negative
		          TIM_CCER_CC3NE;						// enable channel 3, negative

	TIM1->CCR1 = 0;										// zero duty-cycle on all channels
	TIM1->CCR2 = 0;
	TIM1->CCR3 = 0;
	
	TIM1->CR1 |= TIM_CR1_CEN;							// enable timer 1
	
	// GPIOF - turn MOSFET driver on
	
	GPIOF->MODER |= (0x01 << GPIO_MODER_MODER6_Pos) |	// output mode for pin F-6 (standby mode)
		            (0x01 << GPIO_MODER_MODER7_Pos);	// output mode for pin F-7 (standby mode)
	
	GPIOF->PUPDR |= (0x01 << GPIO_PUPDR_PUPDR6_Pos) |	// pull-up for pin F-6
			        (0x01 << GPIO_PUPDR_PUPDR7_Pos);	// pull-up for pin F-7
	
	// over-current config (pin A-11)

	GPIOA->BRR = (1 << 11);								// reset pin 11 (overcurrent does not effect gate driver directly)
	GPIOF->BSRR = (1 << 7);								// disable stand-by mode	
}
void setPwm(int angle, int power) {
//	angle = POSITIVE_MODULO(angle, sin_period);
//	
//	int a1 = angle;
//	int a2 = POSITIVE_MODULO((angle + phase2), sin_period);
//	int a3 = POSITIVE_MODULO((angle + phase3), sin_period);
	
	int a1 = angle % sin_period;
	int a2 = (angle + phase2) % sin_period;
	int a3 = (angle + phase3) % sin_period;
	
	if (a1 < 0) a1 += sin_period;
	if (a2 < 0) a2 += sin_period;
	if (a3 < 0) a3 += sin_period;
	
	a1 = isin_S3(a1) + sin_zero;
	a2 = isin_S3(a2) + sin_zero;
	a3 = isin_S3(a3) + sin_zero;
	
	TIM1->CCR1 = a1 * power / sin_range / timer_scale;
	TIM1->CCR2 = a2 * power / sin_range / timer_scale;
	TIM1->CCR3 = a3 * power / sin_range / timer_scale;
}
void setPwmTorque() {
	int a = getElectricDegrees();
	
	if (usartTorqueCommandValue > 0)
	{
		a += ninetyDeg;
		setPwm(a, usartTorqueCommandValue);
	}
	else
	{
		a -= ninetyDeg;
		setPwm(a, -usartTorqueCommandValue);
	}
}