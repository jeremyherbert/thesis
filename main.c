#include <__cross_studio_io.h>
#include "stm32f10x.h"

#include "sd.h"

void TIM2_IRQHandler(void) {
  if (TIM2->SR & TIM_SR_UIF) GPIOA->ODR ^= 0xFFFFFFFF;
  TIM2->SR = 0; // clear the status reg
//  TIM2->CNT = 0;
  
}

int main(void)
{
  RCC->CR |= RCC_CR_PLLON; // PLL on!
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN; // turn on port a, b, c clocks
  
  sd_init();
  // configure portc lower to be output
  GPIOA->CRL = 0x11111111; // all general purpose push-pull output
  GPIOA->CRH = 0x11111111;
  GPIOA->ODR = 0xFFFFFFFF;

  // enable tim2 (general purpose timer)
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
  
//  TIM2->CCR1 = 0x100; // count to 1
  NVIC->ISER[0] |= (1 << (TIM2_IRQn & 0x1F)); // enable position 28 interrupt (TIM2)
  TIM2->PSC = 0xFF;
  TIM2->DIER |= TIM_DIER_UIE; // compare 1 interrupt enable
  TIM2->ARR = 0xFF;
  TIM2->CR1 |= TIM_CR1_ARPE | TIM_CR1_CEN; // enable the clock
  TIM2->EGR = 1;	
  
  while(1);
}