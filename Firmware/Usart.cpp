#include <main.h>

const uint sendBufferSize = 10;
volatile unsigned char sendBuffer[sendBufferSize] = { 0 };

const uint recvBufferSize = 32;
volatile unsigned char recvBuffer[recvBufferSize] = { 0 };
char* recvBufferEnd = (char*)recvBuffer + recvBufferSize - 1;
volatile char* inp = (char*)recvBuffer;
volatile char* outp;

volatile bool usartDmaSendRequested;
volatile bool usartDmaSendBusy;
volatile int usartTorqueCommandValue;
volatile bool usartCommandReceived;

const int COMMAND_TORQUE = 1;

extern "C"
void DMA1_Channel2_3_IRQHandler(){
	if (DMA1->ISR & DMA_ISR_TCIF2)				// transfer complete on channel 2
	{
		DMA1->IFCR |= DMA_IFCR_CTCIF2;			// clear "transfer complete" flag of channel 2
		DMA1_Channel2->CCR &= ~DMA_CCR_EN;		// disable channel 2
		//USART1->CR1 |= USART_CR1_RE;			// enable receiver TODO: not needed once RE connected to DE
		usartDmaSendBusy = false;
	}
}

extern "C"
void USART1_IRQHandler(void) {
	if (USART1->ISR & USART_ISR_CMF)
	{
		USART1->ICR |= USART_ICR_CMCF;			// clear CMF flag bit
		//USART1->CR1 &= ~USART_CR1_RE;			// disable receiver TODO: not needed once RE connected to DE
		usartCommandReceived = true;
	}
}

void initUsart() {
	usartDmaSendRequested = false;
	usartTorqueCommandValue = 0;
	usartDmaSendBusy = false;
	
	// config B-6 and B-7 as TX and RX
	
	GPIOB->MODER |= (0x02 << GPIO_MODER_MODER6_Pos) |		// alt function for pin B-6 (TX)
					(0x02 << GPIO_MODER_MODER7_Pos);		// alt function for pin B-7 (RX)
	
	GPIOB->OSPEEDR |= (0b11 << GPIO_OSPEEDR_OSPEEDR6_Pos) |	// high speed for pin B-6 (TX)
		              (0b11 << GPIO_OSPEEDR_OSPEEDR7_Pos);	// high speed for pin B-7 (RX)
	
	GPIOB->AFR[0] |= (0x00 << GPIO_AFRL_AFSEL6_Pos) |		// alt function 00 for pin B-6 (TX)
		             (0x00 << GPIO_AFRL_AFSEL7_Pos);		// alt function 00 for pin B-7 (RX)

	// config USART
		
	int baud = 115200;
	
	//USART1->BRR = (8000000U + baud / 2U) / baud;			// baud rate (should be 0x45)
	USART1->BRR = (48000000U + baud / 2U) / baud;			// baud rate (should be 0x1A1)
	
	CLEAR_BIT(USART1->CR2, (USART_CR2_LINEN | USART_CR2_CLKEN));
	CLEAR_BIT(USART1->CR3, (USART_CR3_SCEN | USART_CR3_HDSEL | USART_CR3_IREN));
	
	USART1->CR3 |= USART_CR3_OVRDIS |						// disable overrun error interrupt
				   USART_CR3_DMAT |							// enable DMA transmit
		           USART_CR3_DMAR;							// enable DMA receive
	
	USART1->CR1 |= USART_CR1_TE |							// enable transmitter
		           USART_CR1_RE |							// enable receiver
		           USART_CR1_CMIE;							// char match interrupt enable
	
	USART1->CR2 |= ('\n' << USART_CR2_ADD_Pos);				// stop char is '\n'
	
	// config A-1 pin as DE (manual)

	//GPIOA->MODER |= (0x01 << GPIO_MODER_MODER1_Pos);
	//GPIOA->OSPEEDR |= (0b11 << GPIO_OSPEEDR_OSPEEDR1_Pos);
	
	// config A-1 pin as DE (hardware control)
	
	GPIOA->MODER |= (0b10 << GPIO_MODER_MODER1_Pos);		// alternative function for A-1
	GPIOA->AFR[0] |= (0b01 << GPIO_AFRL_AFSEL1_Pos);		// alternative funciton #1 for pin A-1
	GPIOA->OSPEEDR |= (0b11 << GPIO_OSPEEDR_OSPEEDR1_Pos);	// high speed
	
	USART1->CR3 |= USART_CR3_DEM;							// enable automatic DriverEnable mode
	USART1->CR1 |= (4 << UART_CR1_DEAT_ADDRESS_LSB_POS) |	// 4/16th of a bit assertion time on DriverEnable output
				   (4 << UART_CR1_DEDT_ADDRESS_LSB_POS);	// 4/16th of a bit de-assertion time on DriverEnable output	
	
	USART1->CR1 |= USART_CR1_UE;							// enable usart	
	
	//////////////////////////////
	
	while ((USART1->ISR & USART_ISR_TEACK) == 0) {}			// wait for transmitter to enable
	while ((USART1->ISR & USART_ISR_REACK) == 0) {}			// wait for receiver to enable
	
	NVIC_EnableIRQ(USART1_IRQn);
	NVIC_SetPriority(USART1_IRQn, 0);
	
	// config DMA
	
	RCC->AHBENR |= RCC_AHBENR_DMA1EN;						// enable clock for DMA
	
	// transmit channel 2

	DMA1_Channel2->CPAR = (uint32_t)(&(USART1->TDR));		// USART TDR is destination
	DMA1_Channel2->CMAR = (uint32_t)(sendBuffer);			// source
	
	DMA1_Channel2->CCR |= DMA_CCR_MINC |					// increment memory
		                  DMA_CCR_DIR |						// memory to peripheral
		                  DMA_CCR_TCIE |					// interrupt on full transfer
						  (0b10 << DMA_CCR_PL_Pos);			// priority = high
	
	// receive channel 3
	
	DMA1_Channel3->CPAR = (uint32_t)(&(USART1->RDR));		// USART RDR is source
	DMA1_Channel3->CMAR = (uint32_t)(recvBuffer);			// destination
	DMA1_Channel3->CNDTR = recvBufferSize;					// buffer size	
	
	DMA1_Channel3->CCR |= DMA_CCR_MINC |					// increment memory
						  DMA_CCR_CIRC |					// circular mode
					      DMA_CCR_EN |						// enable DMA
					      (0b10 << DMA_CCR_PL_Pos);			// priority = high
	
	//
	
	HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
}
void usartSendError(){
	sendBuffer[0] = 'e';
	sendBuffer[1] = 'r';
	sendBuffer[2] = 'r';
	sendBuffer[3] = 'o';
	sendBuffer[4] = 'r';
	sendBuffer[5] = '\n';

	DMA1_Channel2->CNDTR = 6;									// buffer size	
	DMA1_Channel2->CCR |= DMA_CCR_EN;							// enable DMA channel 2
	usartDmaSendBusy = true;	
}
bool readByte(uint8_t* output) {
	uint8_t b1;
	uint8_t b2;
	
	*output = 0;
	
	if (*inp >= '0' && *inp <= '9') b1 = *inp - '0';
	else if (*inp >= 'A' && *inp <= 'F') b1 = *inp - '7';
	else if (*inp >= 'a' && *inp <= 'f') b1 = *inp - 'a';
	else return false;
	
	inp++;
	if (inp > recvBufferEnd) inp = (char*)recvBuffer;
		
	if (*inp >= '0' && *inp <= '9') b2 = *inp - '0';
	else if (*inp >= 'A' && *inp <= 'F') b2 = *inp - '7';
	else if (*inp >= 'a' && *inp <= 'f') b2 = *inp - 'a';
	else return false;
	
	inp++;
	if (inp > recvBufferEnd) inp = (char*)recvBuffer;

	*output = (b1 << 4) | b2;
	return true;
}
void readChar(char* output){
	*output = *inp;
	inp++;
	if (inp > recvBufferEnd) inp = (char*)recvBuffer;	
}
bool writeByte(uint8_t byte) {
	uint8_t b1 = (byte >> 4) & 0x0F;
	uint8_t b2 = byte & 0x0F;
	
	if (b1 <= 9) *outp++ = '0' + b1;
	else *outp++ = '7' + b1;
	
	if (b2 <= 9) *outp++ = '0' + b2;
	else *outp++ = '7' + b2;	
}
bool processTorque(){
	char sign;
	uint8_t value;
	
	readChar(&sign);
	if (!readByte(&value)) return false;
	
	if (sign == '-')
	{
		usartTorqueCommandValue = -(int)value * 32; // fit 256 into +-8K as required by SIN		
	}
	else if (sign == '+')
	{
		usartTorqueCommandValue = (int)value * 32;
	}
	else return false;
	
	return true;	
}
void processUsartCommand(){
	uint8_t b1, b2, b3, b4;
	bool success = true;
	
	// skip noice. todo: why!?
	if (*inp >= '0' && *inp <= '9' || *inp >= 'A' && *inp <= 'F' || *inp >= 'a' && *inp <= 'f') {}
	else 
	{
		inp++;
		if (inp > recvBufferEnd) inp = (char*)recvBuffer;
	}
	
	if (readByte(&b1) && b1 == config->controllerId)
	{
		// message addressed to this controller
		
		char cmd;
		while (true)
		{
			readChar(&cmd);
			
			switch (cmd)
			{
			case '\r':
			case '\n': goto _done;
				
			case 'T': if (!processTorque())
				{
					success = false;
					goto _done;
				}
				break;
				
			case 'a':
				usartDmaSendRequested = true;
				break;
				
			default:
				{
					success = false;
					goto _done;
				}				
			}
		}
		
_done:
		if (!success) usartSendError();
	}
	else
	{
		//USART1->CR1 |= USART_CR1_RE;			// enable receiver TODO: not needed once RE connected to DE
	}
	
	uint bufferPosition = recvBufferSize - DMA1_Channel3->CNDTR;
	inp = (char*)recvBuffer + bufferPosition;
}

void usartSendAngle() {
	outp = (char*)sendBuffer;
	writeByte(0);												// to main controller
	writeByte(config->controllerId);							// id of the sender	
	writeByte((uint8_t)((spiCurrentAngle >> 8) & (uint8_t)0x00FFU));
	writeByte((uint8_t)(spiCurrentAngle & (uint8_t)0x00FFU));
	//*outp++ = '\r';
	*outp++ = '\n';
	
	uint32_t cnt = outp - (char*)sendBuffer;

	DMA1_Channel2->CNDTR = cnt;									// transmit size	
	DMA1_Channel2->CCR |= DMA_CCR_EN;							// enable DMA channel 2
	usartDmaSendBusy = true;
}