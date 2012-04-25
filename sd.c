#include <__cross_studio_io.h>
#include "stm32f10x.h"

#include "sd.h"

/*
  SDIO_CK   -> PC12, pin 53
  SDIO_CMD  -> PD2, pin 54
  SDIO_D0   -> PC8, pin 39
*/

#define NUM_COMMANDS      3
#define NUM_APP_COMMANDS  1
uint8_t app_command = 0;

// This is the LUT for normal commands
static const CmdInfo command_data[NUM_COMMANDS] = {
  {SD_CMD_GO_IDLE,        SDIO_CMD_RESPONSE_NONE, 1, 1},
  {SD_CMD_SEND_IF_COND,   SDIO_CMD_RESPONSE_R7,   1, 0},
  {SD_CMD_APP_CMD,        SDIO_CMD_RESPONSE_R1,   0, 0}
};

// and the LUT for application specific commands
static const CmdInfo app_command_data[NUM_APP_COMMANDS] = {
  {SD_ACMD_SEND_OP_COND,  SDIO_CMD_RESPONSE_R3,   0, 0}
};

uint8_t sd_cmd(uint8_t command, uint32_t argument)
{
#ifdef DEBUG
  debug_printf("SD: CMD%i,0x%X\n", command, argument);
#endif
  SDIO->ARG = argument; // set the argument
  CmdInfo *cmd;
  // select the current command out of the LUT
  if (app_command) {
    for (int i=0; i<NUM_APP_COMMANDS; i++) {
      if (command == app_command_data[i].index) {
        cmd = &(app_command_data[i]);
        break;
      }
    }
  } else {
    for (int i=0; i<NUM_COMMANDS; i++) {
      if (command == command_data[i].index) {
        cmd = &(command_data[i]);
        break;
      }
    }
  }

  // if we are using command 55, the next command will be application specific
  if (command == 55) app_command = 1;
  else app_command = 0;

  // we wish to use the internal state machine to track the status of the card
  // this is where we turn it on.
  uint32_t cmd_reg = SDIO_CMD_CPSMEN | command;

  // see what size response we are to set
  switch (cmd->response_type) {
    case SDIO_CMD_RESPONSE_NONE:
      break; // do nothing
    
    case SDIO_CMD_RESPONSE_R7:
    case SDIO_CMD_RESPONSE_R1:
    case SDIO_CMD_RESPONSE_R3:
      // set the short response
      cmd_reg |= SDIO_CMD_WAITRESP_0;
      break;
  }

  // send the command
  SDIO->CMD = cmd_reg;

  // wait for the timeout
  if (wait_for_timeout(cmd) == SD_ERROR) {
    CLEAR_SDIO_STATUS;
    return SD_ERROR;
  }

  // first check the CRC, then check that the returned command is the same as 
  // the command we sent. 
  // note that for Response 3 (OCR register) both of these will always be invalid
  if (cmd->response_type != SDIO_CMD_RESPONSE_R3) {
    if (SDIO->STA & SDIO_STA_CCRCFAIL) {
      CLEAR_SDIO_STATUS;
      return SD_ERROR;
    }

    // wait a little bit for the register to update
    for (uint16_t i=0; i < 100 && SDIO->RESPCMD != command; i++);
    if (SDIO->RESPCMD != command) {
#ifdef DEBUG
      debug_printf("SD: illegal command, got 0x%X\n", SDIO->RESPCMD);
#endif
      CLEAR_SDIO_STATUS;
      return SD_ERROR;
    }
  }

  // if we got here, everything was ok!
  CLEAR_SDIO_STATUS;
  return SD_OK;
}

uint8_t wait_for_timeout(CmdInfo *cmd)
{
  // do any timeout checks
  if (cmd->use_software_timeout == 1 && cmd->ignore_hardware_timeout == 0) {
    // we check the timeout in both software and hardware
    uint32_t timeout = SD_TIMEOUT_THRESHOLD;
    while ((timeout > 0) && 
      !(SDIO->STA & (SDIO_STA_CMDREND | SDIO_STA_CCRCFAIL | SDIO_STA_CTIMEOUT))) 
        timeout--;
    // although we may be ignoring the CRC bit, it will only ever be set if the command
    // was complete, so we can use it in the timeout checking

    if (timeout == 0 || (SDIO->STA & SDIO_STA_CTIMEOUT)) {
#ifdef DEBUG
      debug_printf("SD: soft/hard timeout\n");
#endif
      return SD_ERROR;
    } 

  } else if (cmd->use_software_timeout == 0 && cmd->ignore_hardware_timeout == 0) {
    // just use the hardware timeout system
    while (!(SDIO->STA & (SDIO_STA_CMDREND | SDIO_STA_CCRCFAIL | SDIO_STA_CTIMEOUT)));

    if (SDIO->STA & (SDIO_STA_CTIMEOUT)) {
#ifdef DEBUG
      debug_printf("SD: hard timeout\n", SDIO->STA);
#endif
      return SD_ERROR;
    }
  } else {
    // we do a software only timeout check
    uint32_t timeout = SD_TIMEOUT_THRESHOLD;
    while ((timeout > 0) && !(SDIO->STA & SDIO_STA_CMDSENT)) timeout--;

    if (timeout == 0) {
#ifdef DEBUG
      debug_printf("SD: soft timeout\n", SDIO->STA);
#endif
      return SD_ERROR;
    }
  }

  // if we get here, everything was ok!
  CLEAR_SDIO_STATUS;
  return SD_OK;
}

uint8_t sd_init()
{
  /***** Set up the GPIOs *****/

  // GPIO clocks on
  RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPDEN;

  // set GPIOs to Alternate-function push-pull (0x0B)
  // clear the pins we are interested in first
  GPIOC->CRH &= ~( (0xF) | (0xF << (4*4)));
  GPIOD->CRL &= ~(0xF << (4*2));

  // now set them
  GPIOC->CRH |= (0x0B) | (0x0B << (4*4));
  GPIOD->CRL |= (0x0B << (4*2));

  /***** set up the SDIO interface *****/

  // hardware clocks on
  RCC->AHBENR |= RCC_AHBENR_SDIOEN | RCC_AHBENR_DMA2EN;

  // set the CLKDIV to below 400khz for startup
  SDIO->CLKCR |= 20; // (8E6/(20 + 2) ~ 363E3

  // now switch the power to the external interface on
  SDIO->POWER |= SDIO_POWER_PWRCTRL;

  // turn the SDIO external clock on
  SDIO->CLKCR |= SDIO_CLKCR_CLKEN;

  /***** Send CMD0 (go to idle state) *****/
  if (sd_cmd(0, 0) == SD_ERROR) return SD_ERROR;

  /***** Send CMD8 (check operating conditions) *****/
  // 1 means 2.7-3.6V, AA means test pattern is 0xAA
  if (sd_cmd(8, 0x1AA) == SD_ERROR) return SD_ERROR;

  /***** Send CMD55 and ACMD41 (card startup) *****/
  // CMD55 means that the next command will be application specific
  if (sd_cmd(55, 0) == SD_ERROR) return SD_ERROR;
  // now check that the CMD55 response was ok
  
  // we read it a few times to wait a bit
  uint32_t resp = 0;
  for (uint32_t i=0; i<1000; i++) resp = SDIO->RESP1; 
  if ((resp & SDIO_CMD_RESPONSE_ERROR_BITS) != 0) {
#ifdef DEBUG
    debug_printf("SD: card error: 0x%X (masked: 0x%X)\n", resp, resp & SDIO_CMD_RESPONSE_ERROR_BITS);
#endif
    return SD_ERROR;
  }

  // send ACMD41
  if (sd_cmd(41, SD_VOLTAGE_WINDOW_SD | SD_HIGH_CAPACITY) == SD_ERROR) return SD_ERROR;

  
  
#ifdef DEBUG
  debug_printf("SD: init ok!");
#endif
  return SD_OK;
}
