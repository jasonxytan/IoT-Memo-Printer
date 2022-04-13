/**************************************************************************//**
* @file      BootMain.c
* @brief     Main file for the ESE516 bootloader. Handles updating the main application
* @details   Main file for the ESE516 bootloader. Handles updating the main application
* @author    Eduardo Garcia
* @date      2020-02-15
* @version   2.0
* @copyright Copyright University of Pennsylvania
******************************************************************************/


/******************************************************************************
* Includes
******************************************************************************/
#include <asf.h>
#include "conf_example.h"
#include <string.h>
#include "sd_mmc_spi.h"

#include "SD Card/SdCard.h"
#include "Systick/Systick.h"
#include "SerialConsole/SerialConsole.h"
#include "ASF/sam0/drivers/dsu/crc32/crc32.h"





/******************************************************************************
* Defines
******************************************************************************/
#define APP_START_ADDRESS  ((uint32_t)0x12000) ///<Start of main application. Must be address of start of main application
#define APP_START_RESET_VEC_ADDRESS (APP_START_ADDRESS+(uint32_t)0x04) ///< Main application reset vector address
//#define MEM_EXAMPLE 1 //COMMENT ME TO REMOVE THE MEMORY WRITE EXAMPLE BELOW

/******************************************************************************
* Structures and Enumerations
******************************************************************************/

struct usart_module cdc_uart_module; ///< Structure for UART module connected to EDBG (used for unit test output)

/******************************************************************************
* Local Function Declaration
******************************************************************************/
static void jumpToApplication(void);
static bool StartFilesystemAndTest(void);
static void configure_nvm(void);


/******************************************************************************
* Global Variables
******************************************************************************/
//INITIALIZE VARIABLES
char test_file_name[] = "0:sd_mmc_test.txt";	///<Test TEXT File name
char test_bin_file[] = "0:sd_binary.bin";	///<Test BINARY File name
Ctrl_status status; ///<Holds the status of a system initialization
FRESULT res; //Holds the result of the FATFS functions done on the SD CARD TEST
FATFS fs; //Holds the File System of the SD CARD
FIL file_object; //FILE OBJECT used on main for the SD Card Test

char test_a_file_name[] = "FlagA.txt";	///<Test TEXT File name
char test_a_bin_file[] = "TestA.bin";	///<Test BINARY File name
char test_b_file_name[] = "FlagB.txt";	///<Test TEXT File name
char test_b_bin_file[] = "TestB.bin";	///<Test BINARY File name

uint8_t update = 0;
/******************************************************************************
* Global Functions
******************************************************************************/

/**************************************************************************//**
* @fn		int main(void)
* @brief	Main function for ESE516 Bootloader Application

* @return	Unused (ANSI-C compatibility).
* @note		Bootloader code initiates here.
*****************************************************************************/

int main(void)
{

	/*1.) INIT SYSTEM PERIPHERALS INITIALIZATION*/
	system_init();
	delay_init();
	InitializeSerialConsole();
	system_interrupt_enable_global();
	/* Initialize SD MMC stack */
	sd_mmc_init();

	//Initialize the NVM driver
	configure_nvm();

	irq_initialize_vectors();
	cpu_irq_enable();

	//Configure CRC32
	dsu_crc32_init();

	SerialConsoleWriteString("ESE516 - ENTER BOOTLOADER");	//Order to add string to TX Buffer

	/*END SYSTEM PERIPHERALS INITIALIZATION*/


	/*2.) STARTS SIMPLE SD CARD MOUNTING AND TEST!*/

	//EXAMPLE CODE ON MOUNTING THE SD CARD AND WRITING TO A FILE
	//See function inside to see how to open a file
	SerialConsoleWriteString("\x0C\n\r-- SD/MMC Card Example on FatFs --\n\r");

	if(StartFilesystemAndTest() == false)
	{
		SerialConsoleWriteString("SD CARD failed! Check your connections. System will restart in 5 seconds...");
		delay_cycles_ms(5000);
		system_reset();
	}
	else
	{
		SerialConsoleWriteString("SD CARD mount success! Filesystem also mounted. \r\n");
	}

	/*END SIMPLE SD CARD MOUNTING AND TEST!*/


	/*3.) STARTS BOOTLOADER HERE!*/

	//PERFORM BOOTLOADER HERE!

	//Example
	//Check out the NVM API at http://asf.atmel.com/docs/latest/samd21/html/group__asfdoc__sam0__nvm__group.html#asfdoc_sam0_nvm_examples . It provides important information too!

	//We will ask the NVM driver for information on the MCU (SAMD21)
	struct nvm_parameters parameters;
	char helpStr[64]; //Used to help print values
	nvm_get_parameters (&parameters); //Get NVM parameters
	uint16_t row_size = 4 * parameters.page_size; // NOTICE HERE!!! MUST USE UINT16_T OR LARGER BECAUSE PAGE SIZE IS 256 > UINT8_T_MAX = 255
	snprintf(helpStr, 63,"NVM Info: Number of Pages %u. Size of a page: %u bytes. \r\n", parameters.nvm_number_of_pages, parameters.page_size);
	SerialConsoleWriteString(helpStr);
	
	// check flags
	res = f_open(&file_object, (char const *)test_a_file_name, FA_READ);
	if (res == FR_OK)
	{
		f_close(&file_object);
		res = f_unlink(test_a_file_name);
		update = 1;
	}
	else
	{
		res = f_open(&file_object, (char const *)test_b_file_name, FA_READ);
		if (res == FR_OK)
		{
			f_close(&file_object);
			res = f_unlink(test_b_file_name);
			update = 2;
		}
		else
		{
			update = 0;
		}
	}

	if (update != 0)
	{
		//The SAMD21 NVM (and most if not all NVMs) can only erase and write by certain chunks of data size.
		//The SAMD21 can WRITE on pages. However erase are done by ROW. One row is conformed by four (4) pages.
		//An erase is mandatory before writing to a page.
	

		//We will do the following in this example:
		//Erase the first row of the application data
		//Read a page of data from the SD Card. The SD CARD initialization writes a file with this data as its test for initialization
		//Write it to the first row.
		//CheckCRC32 of both files.
		
		//Read SD Card File
		res = f_open(&file_object, (char const *)(update == 1)?test_a_bin_file:test_b_bin_file, FA_READ);
		if (res != FR_OK)
		{
			SerialConsoleWriteString("Could not open bin file!\r\n");
		}
		uint32_t file_size = f_size(&file_object);
		enum status_code nvmError;
		enum status_code crcres = STATUS_OK;
		uint32_t resultCrcSd = 0;
		uint32_t resultCrcNvm = 0;

		uint8_t readBuffer[row_size]; //Buffer the size of one row
		UINT numBytesRead;
		uint32_t row_addr = APP_START_ADDRESS;
		
// 		f_lseek(&file_object, 0); // rewind read/write pointer

// 		snprintf(helpStr, 63,"file size: %u, row size: %u\r\n", file_size, row_size);
// 		SerialConsoleWriteString(helpStr);
		
		for (uint32_t i = 0; i < file_size/row_size; i++)
		{
			// erase row
			nvmError = nvm_erase_row(row_addr);
			if(nvmError != STATUS_OK)
			{
				SerialConsoleWriteString("Erase error");
			}
		
			//Make sure it got erased - we read the page. Erasure in NVM is an 0xFF
			for(int iter = 0; iter < row_size; iter++)
			{
				char *a = (char *)(row_addr + iter); //Pointer pointing to address APP_START_ADDRESS
				if(*a != 0xFF)
				{
					SerialConsoleWriteString("Error - test page is not erased!");
					break;
				}
			}
			
			// read row from bin file
			numBytesRead = 0;
			// 		FRESULT f_read (
			// 		FIL *fp, 		/* Pointer to the file object */
			// 		void *buff,		/* Pointer to data buffer */
			// 		UINT btr,		/* Number of bytes to read */
			// 		UINT *br		/* Pointer to number of bytes read */
			// 		)
			res = f_read(&file_object, readBuffer, row_size, &numBytesRead);
			
			//Write data to row. Writes are per page, so we need four writes to write a complete row
			for (int pg = 0; pg < 4; pg++)
			{
				res = nvm_write_buffer (row_addr + pg*parameters.page_size, &readBuffer[pg*parameters.page_size], parameters.page_size);
			}

			if (res != FR_OK)
			{
				snprintf(helpStr, 63, "Test write to NVM failed at row address 0x%X!\r\n", row_addr);
				SerialConsoleWriteString(helpStr);
			}
			else
			{
				snprintf(helpStr, 63, "Test write to NVM succeeded at row address 0x%X!\r\n", row_addr);
				SerialConsoleWriteString(helpStr);
			}
			delay_cycles_ms(10); // avoid buffering issues
			
			*((volatile unsigned int*) 0x41007058) &= ~0x30000UL;
			crcres |= dsu_crc32_cal	(readBuffer, row_size, &resultCrcSd);
			*((volatile unsigned int*) 0x41007058) |= 0x20000UL;
			crcres |= dsu_crc32_cal	(row_addr, row_size, &resultCrcNvm);
			
			row_addr += row_size;
		}
		// last row
		nvmError = nvm_erase_row(row_addr);
		if(nvmError != STATUS_OK)
		{
			SerialConsoleWriteString("Erase error");
		}
		for(int iter = 0; iter < row_size; iter++)
		{
			char *a = (char *)(row_addr + iter); //Pointer pointing to address APP_START_ADDRESS
			if(*a != 0xFF)
			{
				SerialConsoleWriteString("Error - test page is not erased!");
				break;
			}
		}
		numBytesRead = 0;
		res = f_read(&file_object, readBuffer, file_size%row_size, &numBytesRead);
		for (int i = numBytesRead; i < row_size; i++)
		{
			readBuffer[i] = 1;
		}
		for (int pg = 0; pg < numBytesRead/parameters.page_size + 1; pg++)
		{
			res = nvm_write_buffer (row_addr + pg*parameters.page_size, &readBuffer[pg*parameters.page_size], parameters.page_size);
		}

		if (res != FR_OK)
		{
			snprintf(helpStr, 63, "Test write to NVM failed at row address 0x%X!\r\n", row_addr);
			SerialConsoleWriteString(helpStr);
		}
		else
		{
			snprintf(helpStr, 63, "Test write to NVM succeeded at row address 0x%X!\r\n", row_addr);
			SerialConsoleWriteString(helpStr);
		}


		//CRC32 Calculation example: Please read http://asf.atmel.com/docs/latest/samd21/html/group__asfdoc__sam0__drivers__crc32__group.html

		//ERRATA Part 1 - To be done before RAM CRC
		//CRC of SD Card. Errata 1.8.3 determines we should run this code every time we do CRC32 from a RAM source. See http://ww1.microchip.com/downloads/en/DeviceDoc/SAM-D21-Family-Silicon-Errata-and-DataSheet-Clarification-DS80000760D.pdf Section 1.8.3
		*((volatile unsigned int*) 0x41007058) &= ~0x30000UL;

		//CRC of SD Card
		crcres |= dsu_crc32_cal	(readBuffer, numBytesRead, &resultCrcSd); //Instructor note: Was it the third parameter used for? Please check how you can use the third parameter to do the CRC of a long data stream in chunks - you will need it!
	
		//Errata Part 2 - To be done after RAM CRC
		*((volatile unsigned int*) 0x41007058) |= 0x20000UL;
	 
	 
		//CRC of memory (NVM)
		crcres |= dsu_crc32_cal	(row_addr, numBytesRead, &resultCrcNvm);
	

		if (crcres != STATUS_OK)
		{
			SerialConsoleWriteString("Could not calculate CRC!!\r\n");
		}
		else
		{
			snprintf(helpStr, 63,"CRC SD CARD: %u  CRC NVM: %u \r\n", resultCrcSd, resultCrcNvm);
			SerialConsoleWriteString(helpStr);
		}
	}

	/*END BOOTLOADER HERE!*/

	//4.) DEINITIALIZE HW AND JUMP TO MAIN APPLICATION!
	SerialConsoleWriteString("ESE516 - EXIT BOOTLOADER");	//Order to add string to TX Buffer
	delay_cycles_ms(100); //Delay to allow print
		
	//Deinitialize HW - deinitialize started HW here!
	DeinitializeSerialConsole(); //Deinitializes UART
	sd_mmc_deinit(); //Deinitialize SD CARD


	//Jump to application
	jumpToApplication();

	//Should not reach here! The device should have jumped to the main FW.
	
}







/******************************************************************************
* Static Functions
******************************************************************************/



/**************************************************************************//**
* function      static void StartFilesystemAndTest()
* @brief        Starts the filesystem and tests it. Sets the filesystem to the global variable fs
* @details      Jumps to the main application. Please turn off ALL PERIPHERALS that were turned on by the bootloader
*				before performing the jump!
* @return       Returns true is SD card and file system test passed. False otherwise.
******************************************************************************/
static bool StartFilesystemAndTest(void)
{
	bool sdCardPass = true;
	uint8_t binbuff[256];

	//Before we begin - fill buffer for binary write test
	//Fill binbuff with values 0x00 - 0xFF
	for(int i = 0; i < 256; i++)
	{
		binbuff[i] = i;
	}

	//MOUNT SD CARD
	Ctrl_status sdStatus= SdCard_Initiate();
	if(sdStatus == CTRL_GOOD) //If the SD card is good we continue mounting the system!
	{
		SerialConsoleWriteString("SD Card initiated correctly!\n\r");

		//Attempt to mount a FAT file system on the SD Card using FATFS
		SerialConsoleWriteString("Mount disk (f_mount)...\r\n");
		memset(&fs, 0, sizeof(FATFS));
		res = f_mount(LUN_ID_SD_MMC_0_MEM, &fs); //Order FATFS Mount
		if (FR_INVALID_DRIVE == res)
		{
			LogMessage(LOG_INFO_LVL ,"[FAIL] res %d\r\n", res);
			sdCardPass = false;
			goto main_end_of_test;
		}
		SerialConsoleWriteString("[OK]\r\n");

		//Create and open a file
		SerialConsoleWriteString("Create a file (f_open)...\r\n");

		test_file_name[0] = LUN_ID_SD_MMC_0_MEM + '0';
		res = f_open(&file_object,
		(char const *)test_file_name,
		FA_CREATE_ALWAYS | FA_WRITE);
		
		if (res != FR_OK)
		{
			LogMessage(LOG_INFO_LVL ,"[FAIL] res %d\r\n", res);
			sdCardPass = false;
			goto main_end_of_test;
		}

		SerialConsoleWriteString("[OK]\r\n");

		//Write to a file
		SerialConsoleWriteString("Write to test file (f_puts)...\r\n");

		if (0 == f_puts("Test SD/MMC stack\n", &file_object))
		{
			f_close(&file_object);
			LogMessage(LOG_INFO_LVL ,"[FAIL]\r\n");
			sdCardPass = false;
			goto main_end_of_test;
		}

		SerialConsoleWriteString("[OK]\r\n");
		f_close(&file_object); //Close file
		SerialConsoleWriteString("Test is successful.\n\r");


		//Write binary file
		//Read SD Card File
		test_bin_file[0] = LUN_ID_SD_MMC_0_MEM + '0';
		res = f_open(&file_object, (char const *)test_bin_file, FA_WRITE | FA_CREATE_ALWAYS);
		
		if (res != FR_OK)
		{
			SerialConsoleWriteString("Could not open binary file!\r\n");
			LogMessage(LOG_INFO_LVL ,"[FAIL] res %d\r\n", res);
			sdCardPass = false;
			goto main_end_of_test;
		}

		//Write to a binaryfile
		SerialConsoleWriteString("Write to test file (f_write)...\r\n");
		uint32_t varWrite = 0;
		if (0 != f_write(&file_object, binbuff,256, (UINT*) &varWrite))
		{
			f_close(&file_object);
			LogMessage(LOG_INFO_LVL ,"[FAIL]\r\n");
			sdCardPass = false;
			goto main_end_of_test;
		}

		SerialConsoleWriteString("[OK]\r\n");
		f_close(&file_object); //Close file
		SerialConsoleWriteString("Test is successful.\n\r");
		
		main_end_of_test:
		SerialConsoleWriteString("End of Test.\n\r");

	}
	else
	{
		SerialConsoleWriteString("SD Card failed initiation! Check connections!\n\r");
		sdCardPass = false;
	}

	return sdCardPass;
}



/**************************************************************************//**
* function      static void jumpToApplication(void)
* @brief        Jumps to main application
* @details      Jumps to the main application. Please turn off ALL PERIPHERALS that were turned on by the bootloader
*				before performing the jump!
* @return       
******************************************************************************/
static void jumpToApplication(void)
{
// Function pointer to application section
void (*applicationCodeEntry)(void);

// Rebase stack pointer
__set_MSP(*(uint32_t *) APP_START_ADDRESS);

// Rebase vector table
SCB->VTOR = ((uint32_t) APP_START_ADDRESS & SCB_VTOR_TBLOFF_Msk);

// Set pointer to application section
applicationCodeEntry =
(void (*)(void))(unsigned *)(*(unsigned *)(APP_START_RESET_VEC_ADDRESS));

// Jump to application. By calling applicationCodeEntry() as a function we move the PC to the point in memory pointed by applicationCodeEntry, 
//which should be the start of the main FW.
applicationCodeEntry();
}



/**************************************************************************//**
* function      static void configure_nvm(void)
* @brief        Configures the NVM driver
* @details      
* @return       
******************************************************************************/
static void configure_nvm(void)
{
    struct nvm_config config_nvm;
    nvm_get_config_defaults(&config_nvm);
    config_nvm.manual_page_write = false;
    nvm_set_config(&config_nvm);
}



