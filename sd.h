#ifndef SD_H
#define SD_H

// SD commands
#define SD_CMD_GO_IDLE                      0     // Go to idle state
#define SD_CMD_ALL_SEND_CID                 2     // ask the card for its info registers
#define SD_CMD_SET_REL_ADDR                 3     // set the RCA
#define SD_CMD_SEND_IF_COND                 8     // send interface conditions (voltage, etc)
#define SD_CMD_SEND_CSD                     9
#define SD_CMD_APP_CMD                      55    // notify sd of an incoming application specific command
#define SD_ACMD_SEND_OP_COND                41    // send operating conditions

// simple return values for all of the functions
#define SD_OK   0
#define SD_ERROR  1

// a def-ed function to clear all of the SDIO status bits 
#define SDIO_CMD_STATUS_BITS  0x5FF
#define CLEAR_SDIO_STATUS   SDIO->ICR |= SDIO_CMD_STATUS_BITS;

#define SDIO_CMD_RESPONSE_ERROR_BITS 0xFDFFE008

// SD command response types
#define SDIO_CMD_RESPONSE_NONE  0
#define SDIO_CMD_RESPONSE_R1    1
#define SDIO_CMD_RESPONSE_R2    2
#define SDIO_CMD_RESPONSE_R3    3
#define SDIO_CMD_RESPONSE_R6    4
#define SDIO_CMD_RESPONSE_R7    5

// software timeout threshold
#define SD_TIMEOUT_THRESHOLD  10000

// for ACMD41
#define SD_HIGH_CAPACITY                ((u32)0x40000000)
#define SD_VOLTAGE_WINDOW_SD            ((u32)0x80100000)

// for CMD3
#define SD_R6_GENERAL_UNKNOWN_ERROR     ((u32)0x00002000)
#define SD_R6_ILLEGAL_CMD               ((u32)0x00004000)
#define SD_R6_COM_CRC_FAILED            ((u32)0x00008000)

/* structs */

typedef struct Cmd_Info {
  uint8_t index;
  uint8_t response_type;
  uint8_t use_software_timeout;
  uint8_t ignore_hardware_timeout;
} CmdInfo;

/* prototypes */

uint8_t sd_init();
uint8_t sd_cmd(uint8_t command, uint32_t argument);
uint8_t wait_for_timeout(CmdInfo *cmd);
#endif