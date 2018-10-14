#include <main.h>

void writeFlash(uint16_t* data, int count) {
	// unlock
	
	while ((FLASH->SR & FLASH_SR_BSY) != 0) {}						// wait for flash not busy
	
	if ((FLASH->CR & FLASH_CR_LOCK) != 0)							// unlock
	{
		FLASH->KEYR = 0x45670123;
		FLASH->KEYR = 0xCDEF89AB;
	}
	
	// erase
	
	FLASH->CR |= FLASH_CR_PER;										// enable page erasing
	FLASH->AR = flashPageAddress;									// choose page to erase
	FLASH->CR |= FLASH_CR_STRT;										// start erase
	while ((FLASH->SR & FLASH_SR_BSY) != 0) {}						// wait till erase done
	if ((FLASH->SR & FLASH_SR_EOP) != 0)							// check and clear the success bit
	{
		FLASH->SR = FLASH_SR_EOP;
	}
	else
	{
		// todo: something went wrong
	}
	FLASH->CR &= ~FLASH_CR_PER;										// disable page erase
	
	// write
	
	FLASH->CR |= FLASH_CR_PG;										// enable programming
	
	uint16_t* a = (uint16_t*)flashPageAddress;
	for (int i = 0; i < count; i++)
	{
		a[i] = data[i];
		
		while ((FLASH->SR & FLASH_SR_BSY) != 0) {}					// wait till done
		if ((FLASH->SR & FLASH_SR_EOP) != 0)						// check and clear the success bit
		{
			FLASH->SR = FLASH_SR_EOP;
		}
		else
		{
			// todo: something went wrong
		}		
	}
	
	FLASH->CR &= ~FLASH_CR_PG;										// disable programming
}

void memcpy(void *dst, const void *src, int count)
{
	char *d = (char*)dst;
	const char *s = (const char*)src;

	for (int i = 0; i < count; i++)
		d[i] = s[i];
}