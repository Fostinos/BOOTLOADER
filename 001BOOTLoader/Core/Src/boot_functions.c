/*
 * boot_functions.c
 *
 *  Created on: Nov 25, 2021
 *      Author: KPODAR
 */



#include "boot_functions.h"

uint8_t supported_commands[] = {	BL_GET_VER ,
									BL_GET_HELP,
									BL_GET_CID,
									BL_GET_RDP_STATUS,
									BL_GO_TO_ADDR,
									BL_FLASH_ERASE,
									BL_MEM_WRITE,
									BL_READ_SECTOR_P_STATUS} ;

uint8_t bl_rx_buffer[BL_RX_LEN];


void  bootloader_uart_read_data(void)
{
    uint8_t rcv_len=0;

	while(1)
	{
		memset(bl_rx_buffer, 0, BL_RX_LEN);
		// Here we will read and decode the commands coming from host
		// First read only one byte from the host , which is the "length" field of the command packet
		HAL_UART_Receive(C_UART,bl_rx_buffer,1,HAL_MAX_DELAY);
		rcv_len= bl_rx_buffer[0];
		HAL_UART_Receive(C_UART,&bl_rx_buffer[1],rcv_len,HAL_MAX_DELAY);
		switch(bl_rx_buffer[1])
		{
            case BL_GET_VER:
                bootloader_handle_getver_cmd(bl_rx_buffer);
                break;
            case BL_GET_HELP:
                bootloader_handle_gethelp_cmd(bl_rx_buffer);
                break;
            case BL_GET_CID:
                bootloader_handle_getcid_cmd(bl_rx_buffer);
                break;
            case BL_GET_RDP_STATUS:
                bootloader_handle_getrdp_cmd(bl_rx_buffer);
                break;
            case BL_GO_TO_ADDR:
                bootloader_handle_go_cmd(bl_rx_buffer);
                break;
            case BL_FLASH_ERASE:
                bootloader_handle_flash_erase_cmd(bl_rx_buffer);
                break;
            case BL_MEM_WRITE:
                bootloader_handle_mem_write_cmd(bl_rx_buffer);
                break;
            case BL_EN_RW_PROTECT:
                bootloader_handle_en_rw_protect(bl_rx_buffer);
                break;
            case BL_MEM_READ:
                bootloader_handle_mem_read(bl_rx_buffer);
                break;
            case BL_READ_SECTOR_P_STATUS:
                bootloader_handle_read_sector_protection_status(bl_rx_buffer);
                break;
            case BL_OTP_READ:
                bootloader_handle_read_otp(bl_rx_buffer);
                break;
						case BL_DIS_R_W_PROTECT:
                bootloader_handle_dis_rw_protect(bl_rx_buffer);
                break;
             default:
                printmsg("BL_DEBUG_MSG: Invalid command code received from host \r\n");
                break;


		}

	}

}


/* Code to jump to user application
 * Here we are assuming FLASH_SECTOR2_BASE
 * is where the user application is stored
 */
void bootloader_jump_to_user_app(void)
{

   // Just a function pointer to hold the address of the reset handler of the user app.
    void (*app_reset_handler)(void);

    printmsg("BL_DEBUG_MSG: bootloader_jump_to_user_app\r\n");


    // 1. Configure the MSP by reading the value from the base address of the sector 2
    uint32_t msp_value = *(volatile uint32_t *)FLASH_SECTOR2_BASE;
    printmsg("BL_DEBUG_MSG: MSP value : %#x\r\n",msp_value);

    // This function comes from CMSIS.
    __set_MSP(msp_value);

    /* 2. Now fetch the reset handler address of the USER Application
     * from the location FLASH_SECTOR2_BASE + 4
     */
    uint32_t resethandler_address = *(volatile uint32_t *) (FLASH_SECTOR2_BASE + 4);

    app_reset_handler = (void*) resethandler_address;

    printmsg("BL_DEBUG_MSG: USER Application Reset Handler Address : %#x\r\n", app_reset_handler);

    //3. Jump to reset handler of the user application
    app_reset_handler();

}



/************** Implementation of Boot-loader Command Handle functions *********/

/* Helper function to handle BL_GET_VER command */
void bootloader_handle_getver_cmd(uint8_t *pBuffer)
{
	uint8_t bl_version;

	// 1) Verify the checksum
	printmsg("BL_DEBUG_MSG: bootloader_handle_getver_cmd\r\n");

	// Total length of the command packet
	uint32_t command_packet_len = pBuffer[0] + 1 ;

	// Extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t * ) (pBuffer + command_packet_len - 4) ) ;

	if (! bootloader_verify_crc(&pBuffer[0], command_packet_len - 4, host_crc))
	{
		printmsg("BL_DEBUG_MSG: Checksum success !!\r\n");
		// Checksum is correct..
		uint8_t follow_len = 1;
		bootloader_send_ack(pBuffer[0], follow_len);
		bl_version = get_bootloader_version();
		printmsg("BL_DEBUG_MSG: BL_VER : %d %#x\r\n", bl_version, bl_version);
		bootloader_uart_write_data(&bl_version, follow_len);

	}else
	{
		printmsg("BL_DEBUG_MSG: Checksum fail !!\r\n");
		// Checksum is wrong send nack
		bootloader_send_nack();
	}


}

/* Helper function to handle BL_GET_HELP command
 * Bootloader sends out All supported Command codes
 */
void bootloader_handle_gethelp_cmd(uint8_t *pBuffer)
{
    printmsg("BL_DEBUG_MSG: bootloader_handle_gethelp_cmd\r\n");

	// Total length of the command packet
	uint32_t command_packet_len = pBuffer[0] + 1;

	// Extract the CRC32 sent by the Host
	uint32_t host_crc = *( (uint32_t *)(pBuffer + command_packet_len - 4) );

	if (! bootloader_verify_crc(&pBuffer[0], command_packet_len - 4, host_crc) )
	{
        printmsg("BL_DEBUG_MSG: Checksum success !!\r\n");
        bootloader_send_ack(pBuffer[0], sizeof(supported_commands));
        bootloader_uart_write_data(supported_commands, sizeof(supported_commands));

	}else
	{
        printmsg("BL_DEBUG_MSG: Checksum fail !!\r\n");
        bootloader_send_nack();
	}

}

/* Helper function to handle BL_GET_CID command */
void bootloader_handle_getcid_cmd(uint8_t *pBuffer)
{
	uint16_t bl_cid_num = 0;
	printmsg("BL_DEBUG_MSG: bootloader_handle_getcid_cmd\r\n");

    // Total length of the command packet
	uint32_t command_packet_len = pBuffer[0] + 1 ;

	// Extract the CRC32 sent by the Host
	uint32_t host_crc = *( (uint32_t *) (pBuffer + command_packet_len - 4));

	if (! bootloader_verify_crc(&pBuffer[0], command_packet_len - 4, host_crc) )
	{
        printmsg("BL_DEBUG_MSG: Checksum success !!\r\n");
        uint8_t follow_len = 2;
        bootloader_send_ack(pBuffer[0], follow_len);
        bl_cid_num = get_mcu_chip_id();
        printmsg("BL_DEBUG_MSG: MCU ID = %d %#x !!\r\n", bl_cid_num, bl_cid_num);
        bootloader_uart_write_data((uint8_t *)&bl_cid_num, follow_len);

	}else
	{
        printmsg("BL_DEBUG_MSG: Checksum fail !!\r\n");
        bootloader_send_nack();
	}


}

/* Helper function to handle BL_GET_RDP_STATUS command */
void bootloader_handle_getrdp_cmd(uint8_t *pBuffer)
{
    uint8_t rdp_level = 0x00;
    printmsg("BL_DEBUG_MSG: bootloader_handle_getrdp_cmd\r\n");

    // Total length of the command packet
	uint32_t command_packet_len = pBuffer[0] + 1 ;

	// Extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t * ) (pBuffer + command_packet_len - 4) ) ;

	if (! bootloader_verify_crc(&pBuffer[0], command_packet_len - 4, host_crc))
	{
        printmsg("BL_DEBUG_MSG: Checksum success !!\r\n");
        uint8_t follow_len = 1;
        bootloader_send_ack(pBuffer[0], follow_len);
        rdp_level = get_flash_rdp_level();
        printmsg("BL_DEBUG_MSG: RDP level: %d %#x\r\n", rdp_level, rdp_level);
        bootloader_uart_write_data(&rdp_level, follow_len);

	}else
	{
        printmsg("BL_DEBUG_MSG: Checksum fail !!\r\n");
        bootloader_send_nack();
	}


}

/* Helper function to handle BL_GO_TO_ADDR command */
void bootloader_handle_go_cmd(uint8_t *pBuffer)
{
    uint32_t go_address = 0;
    uint8_t addr_valid = ADDR_VALID;
    uint8_t addr_invalid = ADDR_INVALID;

    printmsg("BL_DEBUG_MSG: bootloader_handle_go_cmd\r\n");

    // Total length of the command packet
	uint32_t command_packet_len = pBuffer[0] + 1 ;

	// Extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t * ) (pBuffer + command_packet_len - 4) ) ;

	if (! bootloader_verify_crc(&pBuffer[0], command_packet_len - 4, host_crc))
	{
        printmsg("BL_DEBUG_MSG: checksum success !!\r\n");

        bootloader_send_ack(pBuffer[0], 1);

        // Extract the go address
        go_address = *((uint32_t *)&pBuffer[2] );
        printmsg("BL_DEBUG_MSG: GO Address: %#x\r\n", go_address);

        if( verify_address(go_address) == ADDR_VALID )
        {
            // Tell host that address is fine
            bootloader_uart_write_data(&addr_valid, 1);

            /* Jump to "go" address.
            We don't care what is being done there.
            Host must ensure that valid code is present over there
            Its not the duty of Bootloader. so just trust and jump */

            /* Not doing the below line will result in hardfault exception for ARM cortex M */
            // Watch : https://www.youtube.com/watch?v=VX_12SjnNhY

            go_address += 1; // Make T bit = 1

            void (*lets_jump)(void) = (void *)go_address;

            printmsg("BL_DEBUG_MSG: Jumping to go address!\r\n");

            lets_jump();

		}else
		{
            printmsg("BL_DEBUG_MSG: GO Address invalid !\r\n");
            // Tell host that address is invalid
            bootloader_uart_write_data(&addr_invalid, 1);
		}

	}else
	{
        printmsg("BL_DEBUG_MSG: Checksum fail !!\r\n");
        bootloader_send_nack();
	}


}

/* Helper function to handle BL_FLASH_ERASE command */
void bootloader_handle_flash_erase_cmd(uint8_t *pBuffer)
{
    uint8_t erase_status = 0x00;
    printmsg("BL_DEBUG_MSG: bootloader_handle_flash_erase_cmd\r\n");

    // Total length of the command packet
	uint32_t command_packet_len = pBuffer[0] + 1 ;

	// Extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t * ) (pBuffer + command_packet_len - 4) ) ;

	if (! bootloader_verify_crc(&pBuffer[0],command_packet_len - 4, host_crc))
	{
        printmsg("BL_DEBUG_MSG: Checksum success !!\r\n");
        bootloader_send_ack(pBuffer[0], 1);
        printmsg("BL_DEBUG_MSG: Initial_sector : %d  no_ofsectors: %d\r\n", pBuffer[2], pBuffer[3]);

        HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_SET);
        erase_status = execute_flash_erase(pBuffer[2], pBuffer[3]);
        HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_RESET);

        printmsg("BL_DEBUG_MSG: Flash erase status: %#x\r\n", erase_status);

        bootloader_uart_write_data(&erase_status, 1);

	}else
	{
        printmsg("BL_DEBUG_MSG: Checksum fail !!\r\n");
        bootloader_send_nack();
	}
}

/* Helper function to handle BL_MEM_WRITE command */
void bootloader_handle_mem_write_cmd(uint8_t *pBuffer)
{
	uint8_t write_status = 0x00;
	uint8_t payload_len = pBuffer[6];

	uint32_t mem_address = *((uint32_t *) (&pBuffer[2]) );
    printmsg("BL_DEBUG_MSG: bootloader_handle_mem_write_cmd\r\n");

    // Total length of the command packet
	uint32_t command_packet_len = pBuffer[0] + 1;

	// Extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t * ) (pBuffer + command_packet_len - 4) ) ;


	if (! bootloader_verify_crc(&pBuffer[0], command_packet_len - 4, host_crc))
	{
        printmsg("BL_DEBUG_MSG: Checksum success !!\r\n");

        bootloader_send_ack(pBuffer[0], 1);

        printmsg("BL_DEBUG_MSG: Memory write Address : %\r\n",mem_address);

		if( verify_address(mem_address) == ADDR_VALID )
		{

            printmsg("BL_DEBUG_MSG: Valid Memory write Address\r\n");

            // Glow the led to indicate Bootloader is currently writing to memory
            HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_SET);

            // Execute Memory write
            write_status = execute_mem_write(&pBuffer[7], mem_address, payload_len);

            // Turn off the led to indicate memory write is over
            HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_RESET);

            // Inform host about the status
            bootloader_uart_write_data(&write_status, 1);

		}else
		{
            printmsg("BL_DEBUG_MSG: Invalid Memory write Address\r\n");
            write_status = ADDR_INVALID;
            // Inform host that address is invalid
            bootloader_uart_write_data(&write_status, 1);
		}


	}else
	{
        printmsg("BL_DEBUG_MSG: Checksum fail !!\r\n");
        bootloader_send_nack();
	}

}

/* Helper function to handle BL_EN_RW_PROTECT  command */
void bootloader_handle_en_rw_protect(uint8_t *pBuffer)
{
    uint8_t status = 0x00;
    printmsg("BL_DEBUG_MSG: bootloader_handle_endis_rw_protect\r\n");

    // Total length of the command packet
	uint32_t command_packet_len = pBuffer[0] + 1 ;

	// Extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t * ) (pBuffer + command_packet_len - 4) ) ;

	if (! bootloader_verify_crc(&pBuffer[0], command_packet_len - 4, host_crc))
	{
        printmsg("BL_DEBUG_MSG: Checksum success !!\r\n");
        bootloader_send_ack(pBuffer[0], 1);

        status = configure_flash_sector_rw_protection(*(uint16_t*)&pBuffer[2], pBuffer[4], 0);

        printmsg("BL_DEBUG_MSG: Flash erase status: %#x\r\n",status);

        bootloader_uart_write_data(&status, 1);

	}else
	{
        printmsg("BL_DEBUG_MSG: Checksum fail !!\r\n");
        bootloader_send_nack();
	}


}


/*Helper function to handle BL_EN_RW_PROTECT  command */
void bootloader_handle_dis_rw_protect(uint8_t *pBuffer)
{
    uint8_t status = 0x00;
    printmsg("BL_DEBUG_MSG: bootloader_handle_dis_rw_protect\r\n");

    //Total length of the command packet
	uint32_t command_packet_len = bl_rx_buffer[0] + 1;

	//extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t * ) (pBuffer + command_packet_len - 4) ) ;

	if (! bootloader_verify_crc(&pBuffer[0], command_packet_len - 4, host_crc))
	{
        printmsg("BL_DEBUG_MSG: Checksum success !!\r\n");
        bootloader_send_ack(pBuffer[0], 1);

        status = configure_flash_sector_rw_protection(0, 0, 1);

        printmsg("BL_DEBUG_MSG: Flash erase status: %#x\r\n",status);

        bootloader_uart_write_data(&status, 1);

	}else
	{
        printmsg("BL_DEBUG_MSG: Checksum fail !!\r\n");
        bootloader_send_nack();
	}


}

/*Helper function to handle BL_MEM_READ command */
void bootloader_handle_mem_read (uint8_t *pBuffer)
{


}

/*Helper function to handle _BL_READ_SECTOR_P_STATUS command */
void bootloader_handle_read_sector_protection_status(uint8_t *pBuffer)
{
	 uint16_t status;
	printmsg("BL_DEBUG_MSG: bootloader_handle_read_sector_protection_status\r\n");

    // Total length of the command packet
	uint32_t command_packet_len = pBuffer[0] + 1;

	// Extract the CRC32 sent by the Host
	uint32_t host_crc = *((uint32_t * ) (pBuffer + command_packet_len - 4) ) ;

	if (! bootloader_verify_crc(&pBuffer[0], command_packet_len - 4, host_crc))
	{
        printmsg("BL_DEBUG_MSG: Checksum success !!\r\n");
        bootloader_send_ack(pBuffer[0], 2);
        status = read_OB_rw_protection_status();
        printmsg("BL_DEBUG_MSG: nWRP status: %#x\r\n", status);
        bootloader_uart_write_data((uint8_t*)&status, 2);

	}else
	{
        printmsg("BL_DEBUG_MSG: Checksum fail !!\r\n");
        bootloader_send_nack();
	}

}

/*Helper function to handle BL_OTP_READ command */
void bootloader_handle_read_otp(uint8_t *pBuffer)
{


}

/* This function sends ACK if CRC matches along with "len to follow"*/
void bootloader_send_ack(uint8_t command_code, uint8_t follow_len)
{
	 // Here we send 2 byte.. first byte is ack and the second byte is len value
	uint8_t ack_buf[2];
	ack_buf[0] = BL_ACK;
	ack_buf[1] = follow_len;
	HAL_UART_Transmit(C_UART, ack_buf, 2, HAL_MAX_DELAY);

}

/* This function sends NACK */
void bootloader_send_nack(void)
{
	uint8_t nack = BL_NACK;
	HAL_UART_Transmit(C_UART, &nack, 1, HAL_MAX_DELAY);
}

// This verifies the CRC of the given buffer in pData .
uint8_t bootloader_verify_crc (uint8_t *pData, uint32_t len, uint32_t crc_host)
{
	uint32_t uwCRCValue = 0xFF;

	for (uint32_t i=0 ; i < len ; i++)
	{
		uint32_t i_data = pData[i];
		uwCRCValue = HAL_CRC_Accumulate(&hcrc, &i_data, 1);
	}

	 /* Reset CRC Calculation Unit */
	__HAL_CRC_DR_RESET(&hcrc);

	if( uwCRCValue == crc_host)
	{
		return VERIFY_CRC_SUCCESS;
	}

	return VERIFY_CRC_FAIL;
}

/* This function writes data in to C_UART */
void bootloader_uart_write_data(uint8_t *pBuffer, uint32_t len)
{
	/* Can replace the below ST's USART driver API call with your MCUs driver API call */
	HAL_UART_Transmit(C_UART, pBuffer, len, HAL_MAX_DELAY);

}


// Just returns the macro value .
uint8_t get_bootloader_version(void)
{
	return (uint8_t)BL_VERSION;
}

// Read the chip identifier or device Identifier
uint16_t get_mcu_chip_id(void)
{
/*
	The STM32F429xx MCUs integrate an MCU ID code. This ID identifies the ST MCU partnumber
	and the die revision. It is part of the DBG_MCU component and is mapped on the
	external PPB bus (see Section 33.16 on page 1304). This code is accessible using the
	JTAG debug pCat.2ort (4 to 5 pins) or the SW debug port (two pins) or by the user software.
	It is even accessible while the MCU is under system reset. */
	uint16_t cid;
	cid = (uint16_t)(DBGMCU->IDCODE) & 0x0FFF;
	return  cid;

}


/* This function reads the RDP ( Read protection option byte) value
 * For more info refer "Table 16. Description of the option bytes" in stm32f4xx RM
 */
uint8_t get_flash_rdp_level(void)
{

	uint8_t rdp_status = 0;
#if 1
	FLASH_OBProgramInitTypeDef  ob_handle;
	HAL_FLASHEx_OBGetConfig(&ob_handle);
	rdp_status = (uint8_t)ob_handle.RDPLevel;
#else
	 volatile uint32_t *pOB_addr = (uint32_t*) 0x1FFFC000;
	 rdp_status = (uint8_t)(*pOB_addr >> 8) ;
#endif

	return rdp_status;

}

// Verify the address sent by the host .
uint8_t verify_address(uint32_t go_address)
{
	// So, what are the valid addresses to which we can jump ?
	// Can we jump to system memory ? yes
	// Can we jump to sram1 memory ?  yes
	// Can we jump to sram2 memory ? yes
	// Can we jump to sram3 memory ? yes
	// Can we jump to backup sram memory ? yes
	// Can we jump to peripheral memory ? its possible , but dont allow. so no
	// Can we jump to external memory ? its possible , but dont allow. so no

	if ( go_address >= FLASH_BASE && go_address <= FLASH_END)
	{
		return ADDR_VALID;
	}
	if ( go_address >= SRAM1_BASE && go_address <= SRAM1_END)
	{
		return ADDR_VALID;
	}
	else if ( go_address >= SRAM2_BASE && go_address <= SRAM2_END)
	{
		return ADDR_VALID;
	}
	else if ( go_address >= SRAM3_BASE && go_address <= SRAM3_END)
	{
		return ADDR_VALID;
	}
	else if ( go_address >= BKPSRAM_BASE && go_address <= BKPSRAM_END)
	{
		return ADDR_VALID;
	}
	else
	{
		return ADDR_INVALID;
	}

}

 uint8_t execute_flash_erase(uint8_t sector_number , uint8_t number_of_sector)
{
    // We have totally 12 sectors in STM32F429ZITX mcu .. sector[0 to 11]
	// number_of_sector has to be in the range of 0 to 11
	// If sector_number = 0xff , that means mass erase !
	// Code needs to modified if your MCU supports more flash sectors
	FLASH_EraseInitTypeDef flashErase_handle;
	uint32_t sectorError;
	HAL_StatusTypeDef status;


	if( number_of_sector >= 12 )
		return INVALID_SECTOR;

	if( (sector_number == 0xFF ) || (sector_number < 12) )
	{
		if(sector_number == (uint8_t) 0xFF)
		{
			flashErase_handle.TypeErase = FLASH_TYPEERASE_MASSERASE;
		}else
		{
		    /* Here we are just calculating how many sectors needs to erased */
			uint8_t remanining_sector = 12 - sector_number;
            if( number_of_sector > remanining_sector)
            {
            	number_of_sector = remanining_sector;
            }
			flashErase_handle.TypeErase = FLASH_TYPEERASE_SECTORS;
			flashErase_handle.Sector = sector_number; // This is the initial sector
			flashErase_handle.NbSectors = number_of_sector;
		}
		flashErase_handle.Banks = FLASH_BANK_1;

		/* Get access to touch the flash registers */
		HAL_FLASH_Unlock();
		flashErase_handle.VoltageRange = FLASH_VOLTAGE_RANGE_3;  // Our mcu will work on this voltage range
		status = (uint8_t) HAL_FLASHEx_Erase(&flashErase_handle, &sectorError);
		HAL_FLASH_Lock();

		return status;
	}

	return INVALID_SECTOR;
}

/* This function writes the contents of pBuffer to  "mem_address" byte by byte */
// Note1 : Currently this function supports writing to Flash only .
// Note2 : This functions does not check whether "mem_address" is a valid address of the flash range.
uint8_t execute_mem_write(uint8_t *pBuffer, uint32_t mem_address, uint32_t len)
{
	uint8_t status = HAL_OK;

	// We have to unlock flash module to get control of registers
	HAL_FLASH_Unlock();

	for(uint32_t i = 0; i<len; i++)
	{
		// Here we program the flash byte by byte
		status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, mem_address + i, pBuffer[i] );
	}

	HAL_FLASH_Lock();

	return status;
}


/*
Modifying user option bytes
To modify the user option value, follow the sequence below :
1. Check that no Flash memory operation is ongoing by checking the BSY bit in the FLASH_SR register
2. Write the desired option value in the FLASH_OPTCR register.
3. Set the option start bit (OPTSTRT) in the FLASH_OPTCR register
4. Wait for the BSY bit to be cleared.
*/
uint8_t configure_flash_sector_rw_protection(uint16_t sector_details, uint8_t protection_mode, uint8_t disable)
{
	// First configure the protection mode
	// Protection_mode =1 , means write protect of the user flash sectors
	// Protection_mode =2, means read/write protect of the user flash sectors
	// According to RM of stm32f429xx TABLE 16, We have to modify the address 0x1FFF C008 bit 15(SPRMOD)

	//Flash option control register (OPTCR)
	volatile uint32_t *pOPTCR = (uint32_t*) 0x40023C14;


	if(disable)
	{
		// Disable all r/w protection on sectors

		// Option byte configuration unlock
		HAL_FLASH_OB_Unlock();

		// Wait till no active operation on flash
		while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET);

		// Clear the 31st bit (default state)
		// Please refer : Flash option control register (FLASH_OPTCR) in RM
		*pOPTCR &= ~(1 << 31);

		// Clear the protection : make all 16 bits belonging to sectors as 1
		*pOPTCR |= (0x0FFF << 16);

		// Set the option start bit (OPTSTRT) in the FLASH_OPTCR register
		*pOPTCR |= ( 1 << 1);

		// Wait till no active operation on flash
		while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET);

		HAL_FLASH_OB_Lock();

		return 0;

	}

	if(protection_mode == (uint8_t) 1)
	{
		// We are putting write protection on the sectors encoded in sector_details argument

		// Option byte configuration unlock
		HAL_FLASH_OB_Unlock();

		// Wait till no active operation on flash
		while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET);

		// Here we are setting just write protection for the sectors
		// Clear the 31st bit
		// Please refer : Flash option control register (FLASH_OPTCR) in RM
		*pOPTCR &= ~(1 << 31);

		// Put write protection on sectors
		// First 12 bits of sector_details must be setting on 16th position of FLASH_OPTCR
		*pOPTCR &= ~ ((sector_details & 0x0FFF) << 16);

		// Set the option start bit (OPTSTRT) in the FLASH_OPTCR register
		*pOPTCR |= ( 1 << 1);

		// Wait till no active operation on flash
		while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET);

		HAL_FLASH_OB_Lock();
	}

	else if (protection_mode == (uint8_t) 2)
	{
		// Option byte configuration unlock
		HAL_FLASH_OB_Unlock();

		// Wait till no active operation on flash
		while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET);

		// Here we are setting read and write protection for the sectors
		// Set the 31st bit
		// Please refer : Flash option control register (FLASH_OPTCR) in RM
		*pOPTCR |= (1 << 31);

		// Put read and write protection on sectors
		// First 12 bits of sector_details must be setting on 16th position of FLASH_OPTCR
		*pOPTCR &= ~(0x0FFF << 16);
		*pOPTCR |= ((sector_details & 0x0FFF) << 16);

		// Set the option start bit (OPTSTRT) in the FLASH_OPTCR register
		*pOPTCR |= ( 1 << 1);

		// Wait till no active operation on flash
		while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY) != RESET);

		HAL_FLASH_OB_Lock();
	}

	return 0;
}

uint16_t read_OB_rw_protection_status(void)
{
	// This structure is given by ST Flash driver to hold the OB(Option Byte) contents .
	FLASH_OBProgramInitTypeDef OBInit;

	// First unlock the OB(Option Byte) memory access
	HAL_FLASH_OB_Unlock();
	// Get the OB configuration details
	HAL_FLASHEx_OBGetConfig(&OBInit);
	// Lock back .
	HAL_FLASH_Lock();

	// We are just interested in r/w protection status of the sectors.
	return (uint16_t)OBInit.WRPSector;

}
