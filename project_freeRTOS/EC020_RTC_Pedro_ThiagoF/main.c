/*
    FreeRTOS V6.1.1 - Copyright (C) 2011 Real Time Engineers Ltd.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
 ***NOTE*** The exception to the GPL is included to allow you to distribute
    a combined work that includes FreeRTOS without being obliged to provide the
    source code for proprietary components outside of the FreeRTOS kernel.
    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details. You should have received a copy of the GNU General Public 
    License and the FreeRTOS license exception along with FreeRTOS; if not it 
    can be viewed here: http://www.freertos.org/a00114.html and also obtained 
    by writing to Richard Barry, contact details for whom are available on the
    FreeRTOS WEB site.

    1 tab == 4 spaces!

    http://www.FreeRTOS.org - Documentation, latest information, license and
    contact details.

    http://www.SafeRTOS.com - A version that is certified for use in safety
    critical systems.

    http://www.OpenRTOS.com - Commercial support, development, porting,
    licensing and training services.
 */

/* FreeRTOS.org includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Demo includes. */
#include "basic_io.h"

#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_uart.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_ssp.h"

#include "uart2.h"
#include "light.h"
#include "oled.h"

#define UART_DEV LPC_UART3

#define RANGE_1000 1
#define RANGE_4000 2
#define RANGE_16000 3
#define RANGE_64000 4

#define INFO_SENSOR 1
#define INFO_RANGE_1000 2
#define INFO_RANGE_4000 3
#define INFO_RANGE_16000 4
#define INFO_RANGE_64000 5
#define INFO_INVALID_CMD 6

/* The tasks to be created.  Two instances are created of the sender task while
only a single instance is created of the receiver task. */
static void vReadSensor( void *pvParameters );
static void vReadUart( void *pvParameters );
static void vCLI( void *pvParameters );
static void vExecuteCommands( void *pvParameters );
void show_range_selected(uint8_t range);
void show_menu(void);
static void init_uart(void);
static void init_i2c(void);
static void init_ssp(void);
//void SysTick_Handler(void);
static void intToString(int value, uint8_t* pBuf, uint32_t len, uint32_t base);
/*-----------------------------------------------------------*/

/* Declare a variable of type xQueueHandle.  This is used to store the queue
that is accessed by all three tasks. */
xQueueHandle xQueue;
xQueueHandle xCLIQueue;
xQueueHandle xSensorQueue;

static uint8_t buf[10];

static const portTickType xTicksToWait = 100 / portTICK_RATE_MS;

int main( void )
{
	if (SysTick_Config(SystemCoreClock / 1000)) {
		while (1);  // Capture error
	}

	/* The queue is created to hold a maximum of 5 long values. */
	xQueue = xQueueCreate( 5, sizeof( uint8_t ) );
	xCLIQueue = xQueueCreate( 5, sizeof( uint8_t ) );
	xSensorQueue = xQueueCreate( 5, sizeof( uint8_t ) );
	if( xQueue != NULL && xCLIQueue != NULL && xSensorQueue != NULL ) {
		init_i2c();
		init_ssp();
		init_uart();

		xTaskCreate( vExecuteCommands, "vExecuteCommands", 240, NULL, 1, NULL );
		xTaskCreate( vCLI, "vCLI", 240, NULL, 1, NULL );
		xTaskCreate( vReadUart, "vReadUart", 240, NULL, 1, NULL );
		xTaskCreate( vReadSensor, "vReadSensor", 240, NULL, 1, NULL );

		//System initial message
		UART_SendString(UART_DEV, (uint8_t*)"INATEL - Instituto Nacional de Telecomunicacoes\r\n");
		UART_SendString(UART_DEV, (uint8_t*)"Disciplina: EC020 - Topicos Especiais em Computacao\r\n");
		UART_SendString(UART_DEV, (uint8_t*)"Modularizacao de Sistemas Embarcados Usando Orientacao a Objeto\r\n");
		show_menu();

		/* Start the scheduler so the created tasks start executing. */
		vTaskStartScheduler();
	} else {
		/* The queue could not be created. */
	}

	/* If all is well we will never reach here as the scheduler will now be
    running the tasks.  If we do reach here then it is likely that there was 
    insufficient heap memory available for a resource to be created. */
	for( ;; );
	return 0;
}
/*-----------------------------------------------------------*/

static void vExecuteCommands( void *pvParameters )
{
	/* Declare the variable that will hold the values received from the queue. */
	portBASE_TYPE xStatus;
	uint8_t data = 0;
	uint8_t queueCommand = 0;

	light_init(); //initialize light sensor
	light_enable(); //enable light sensor
	light_setRange(LIGHT_RANGE_4000); //configure sensor to 4000 range.

	for( ;; ) {
		if( uxQueueMessagesWaiting( xQueue ) != 0 ) {
			vPrintString( "Queue should have been empty!\r\n" );
		}

		xStatus = xQueueReceive( xQueue, &data, xTicksToWait );

		if( xStatus == pdPASS ){ // Data was successfully received from the queue
			switch (data) {
			case '1': //read sensor.
				queueCommand = INFO_SENSOR;
				xStatus = xQueueSendToBack( xSensorQueue, &queueCommand, 0 );
				break;
			case '2': //configure sensor to 1000 range.
				light_shutdown();
				light_enable();
				light_setRange(LIGHT_RANGE_1000);
				queueCommand = INFO_RANGE_1000;
				xStatus = xQueueSendToBack( xCLIQueue, &queueCommand, 0 );
				break;
			case '3': //configure sensor to 4000 range.
				light_shutdown();
				light_enable();
				light_setRange(LIGHT_RANGE_4000);
				queueCommand = INFO_RANGE_4000;
				xStatus = xQueueSendToBack( xCLIQueue, &queueCommand, 0 );
				break;
			case '4': //configure sensor to 16000 range.
				light_shutdown();
				light_enable();
				light_setRange(LIGHT_RANGE_16000);
				queueCommand = INFO_RANGE_16000;
				xStatus = xQueueSendToBack( xCLIQueue, &queueCommand, 0 );
				break;
			case '5'://configure sensor to 64000 range.
				light_shutdown();
				light_enable();
				light_setRange(LIGHT_RANGE_64000);
				queueCommand = INFO_RANGE_64000;
				xStatus = xQueueSendToBack( xCLIQueue, &queueCommand, 0 );
				break;
			default: //invalid command.
				queueCommand = INFO_INVALID_CMD;
				xStatus = xQueueSendToBack( xCLIQueue, &queueCommand, 0 );
				break;
			}

			if( xStatus != pdPASS ) {
				vPrintString( "Could not send to the queue.\r\n" );
			}
		}
	}
}
/*-----------------------------------------------------------*/

static void vReadUart( void *pvParameters )
{
	portBASE_TYPE xStatus;
	uint8_t dataRead = 0;

	/* As per most tasks, this task is implemented within an infinite loop. */
	for( ;; )
	{
		if (UART_Receive(UART_DEV, &dataRead, 1, NONE_BLOCKING) > 0) { //if receive data from UART
			xStatus = xQueueSendToBack( xQueue, &dataRead, 0 );

			if( xStatus != pdPASS )
			{
				vPrintString( "Could not send to the queue.\r\n" );
			}
		}
	}
}
/*-----------------------------------------------------------*/

static void vCLI( void *pvParameters )
{
	/* Declare the variable that will hold the values received from the queue. */
	portBASE_TYPE xStatus;
	uint8_t dataReceived = 0;


	oled_init(); //initialize OLED
	oled_clearScreen(OLED_COLOR_WHITE);
	oled_putString(1,9,  (uint8_t*)"Light  : ", OLED_COLOR_BLACK, OLED_COLOR_WHITE); //configure oled to show value of light sensor

	/* As per most tasks, this task is implemented within an infinite loop. */
	for( ;; )
	{
		if( uxQueueMessagesWaiting( xCLIQueue ) != 0 )
		{
			vPrintString( "Queue should have been empty!\r\n" );
		}

		xStatus = xQueueReceive( xCLIQueue, &dataReceived, xTicksToWait );

		if( xStatus == pdPASS )// Data was successfully received from the queue
		{
			switch(dataReceived){
			case INFO_SENSOR:
				oled_fillRect((1+9*6),9, 80, 16, OLED_COLOR_WHITE);
				oled_putString((1+9*6),9, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE); //show value read on oled display

				UART_SendString(UART_DEV, (uint8_t*)"\r\nValor lido pelo sensor: ");
				UART_SendString(UART_DEV, (uint8_t*) buf);
				UART_SendString(UART_DEV, (uint8_t*)" lux");
				break;
			case INFO_RANGE_1000:
				UART_SendString(UART_DEV, (uint8_t*)"\r\nSensor configurado para faixa de 0 a 1000.");
				break;
			case INFO_RANGE_4000:
				UART_SendString(UART_DEV, (uint8_t*)"\r\nSensor configurado para faixa de 0 a 4000.");
				break;
			case INFO_RANGE_16000:
				UART_SendString(UART_DEV, (uint8_t*)"\r\nSensor configurado para faixa de 0 a 16000.");
				break;
			case INFO_RANGE_64000:
				UART_SendString(UART_DEV, (uint8_t*)"\r\nSensor configurado para faixa de 0 a 64000.");
				break;
			case INFO_INVALID_CMD:
				UART_SendString(UART_DEV, (uint8_t*)"\r\nError - Opcao invalida!!");
				break;
			}
		}

		/* Allow the other sender task to execute. */
		taskYIELD();
	}
}
/*-----------------------------------------------------------*/

static void vReadSensor( void *pvParameters )
{
	/* Declare the variable that will hold the values received from the queue. */
	portBASE_TYPE xStatus;
	uint8_t dataReceived = 0;
	uint32_t lux = 0;
	uint8_t queueCommand = 0;

	/* This task is also defined within an infinite loop. */
	for( ;; )
	{
		if( uxQueueMessagesWaiting( xQueue ) != 0 )
		{
			vPrintString( "Queue should have been empty!\r\n" );
		}

		xStatus = xQueueReceive( xSensorQueue, &dataReceived, xTicksToWait );

		if( xStatus == pdPASS )// Data was successfully received from the queue
		{
			/* light */
			lux = light_read();
			intToString(lux, buf, 10, 10);

			queueCommand = INFO_SENSOR;
			xStatus = xQueueSendToBack( xCLIQueue, &queueCommand, 0 );
			if( xStatus != pdPASS )
			{
				vPrintString( "Could not send to the queue.\r\n" );
			}
		}
	}
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* This function will only be called if an API call to create a task, queue
	or semaphore fails because there is too little heap RAM remaining. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( xTaskHandle *pxTask, signed char *pcTaskName )
{
	/* This function will only be called if a task overflows its stack.  Note
	that stack overflow checking does slow down the context switch
	implementation. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
	/* This example does not use the idle hook to perform any processing. */
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
	/* This example does not use the tick hook to perform any processing. */
}


/**
 * Converte inteiro para String.
 */
static void intToString(int value, uint8_t* pBuf, uint32_t len, uint32_t base)
{
	static const char* pAscii = "0123456789abcdefghijklmnopqrstuvwxyz";
	int pos = 0;
	int tmpValue = value;

	// the buffer must not be null and at least have a length of 2 to handle one
	// digit and null-terminator
	if (pBuf == NULL || len < 2)
	{
		return;
	}

	// a valid base cannot be less than 2 or larger than 36
	// a base value of 2 means binary representation. A value of 1 would mean only zeros
	// a base larger than 36 can only be used if a larger alphabet were used.
	if (base < 2 || base > 36)
	{
		return;
	}

	// negative value
	if (value < 0)
	{
		tmpValue = -tmpValue;
		value    = -value;
		pBuf[pos++] = '-';
	}

	// calculate the required length of the buffer
	do {
		pos++;
		tmpValue /= base;
	} while(tmpValue > 0);


	if (pos > len)
	{
		// the len parameter is invalid.
		return;
	}

	pBuf[pos] = '\0';

	do {
		pBuf[--pos] = pAscii[value % base];
		value /= base;
	} while(value > 0);

	return;

}

//void SysTick_Handler(void) {
//	msTicks++;
//}

/**
 * Inicializa interface SPI.
 */
static void init_ssp(void)
{
	SSP_CFG_Type SSP_ConfigStruct;
	PINSEL_CFG_Type PinCfg;

	/*
	 * Initialize SPI pin connect
	 * P0.7 - SCK;
	 * P0.8 - MISO
	 * P0.9 - MOSI
	 * P2.2 - SSEL - used as GPIO
	 */
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 7;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 8;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 9;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Funcnum = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);

	SSP_ConfigStructInit(&SSP_ConfigStruct);

	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP1, ENABLE);

}

/**
 * Inicializa interface I²C
 */
static void init_i2c(void)
{
	PINSEL_CFG_Type PinCfg;

	/* Initialize I2C2 pin connect */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 10;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 11;
	PINSEL_ConfigPin(&PinCfg);

	// Initialize I2C peripheral
	I2C_Init(LPC_I2C2, 100000);

	/* Enable I2C1 operation */
	I2C_Cmd(LPC_I2C2, ENABLE);
}

/*
 * Inicializa interface UART
 * */
static void init_uart(void)
{
	PINSEL_CFG_Type PinCfg;
	UART_CFG_Type uartCfg;

	/* Initialize UART3 pin connect */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 0;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 1;
	PINSEL_ConfigPin(&PinCfg);

	uartCfg.Baud_rate = 115200;
	uartCfg.Databits = UART_DATABIT_8;
	uartCfg.Parity = UART_PARITY_NONE;
	uartCfg.Stopbits = UART_STOPBIT_1;

	UART_Init(UART_DEV, &uartCfg);

	UART_TxCmd(UART_DEV, ENABLE);

}

/*
 * Exibe menu através da comunicação UART.
 * */
void show_menu(void){
	UART_SendString(UART_DEV, (uint8_t*)"\n\n********** MENU **********\r\n");
	UART_SendString(UART_DEV, (uint8_t*)"(1) Ler sensor\r\n");
	UART_SendString(UART_DEV, (uint8_t*)"(2) Configurar faixa de resposta do sensor de luz para 1000\r\n");
	UART_SendString(UART_DEV, (uint8_t*)"(3) Configurar faixa de resposta do sensor de luz para 4000\r\n");
	UART_SendString(UART_DEV, (uint8_t*)"(4) Configurar faixa de resposta do sensor de luz para 16000\r\n");
	UART_SendString(UART_DEV, (uint8_t*)"(5) Configurar faixa de resposta do sensor de luz para 64000\r\n");
	UART_SendString(UART_DEV, (uint8_t*)"\r\nDigite uma das opcoes acima: ");
}

/*
 * Exibe faixa de valores configurada no sensor através da comunicação UART.
 * */
void show_range_selected(uint8_t range){
	switch (range) {
	case RANGE_1000:
		UART_SendString(UART_DEV, (uint8_t*)"\r\nFaixa atual do sensor configurada = 0 a 1000\r\n");
		break;
	case RANGE_4000:
		UART_SendString(UART_DEV, (uint8_t*)"\r\nFaixa atual do sensor configurada = 0 a 4000\r\n");
		break;
	case RANGE_16000:
		UART_SendString(UART_DEV, (uint8_t*)"\r\nFaixa atual do sensor configurada = 0 a 16000\r\n");
		break;
	case RANGE_64000:
		UART_SendString(UART_DEV, (uint8_t*)"\r\nFaixa atual do sensor configurada = 0 a 64000\r\n");
		break;
	default:
		break;
	}
}
