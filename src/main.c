/*****************************************************************************
 *   Reading data from UART1 and writing to UART2 (and vice versa)
 *
 *   Copyright(C) 2010, Embedded Artists AB
 *   All rights reserved.
 *
 ******************************************************************************/

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

static uint32_t msTicks = 0;
static uint8_t buf[10];

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

void SysTick_Handler(void) {
	msTicks++;
}

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

/**
 * Função principal
 */
int main (void) {

	uint8_t data = 0;
	uint32_t len = 0;
	uint8_t menuIsShowing = 0;
	uint8_t range_selected = 2;
	uint32_t lux = 0;

	init_i2c();
	init_ssp();
	init_uart();

	oled_init(); //inicializa OLED
	light_init(); //inicializa sensor de luz

	if (SysTick_Config(SystemCoreClock / 1000)) {
		while (1);  // Capture error
	}

	light_enable(); //habilita sensor de luz
	light_setRange(LIGHT_RANGE_4000); //seta faixa do sensor de luz para 4000.

	oled_clearScreen(OLED_COLOR_WHITE);
	oled_putString(1,9,  (uint8_t*)"Light  : ", OLED_COLOR_BLACK, OLED_COLOR_WHITE); //pre configura oled para mostrar valor lido do sensor de luz

	//mensagem inicial do sistema
	UART_SendString(UART_DEV, (uint8_t*)"INATEL - Instituto Nacional de Telecomunicacoes\r\n");
	UART_SendString(UART_DEV, (uint8_t*)"Disciplina: EC020 - Topicos Especiais em Computacao\r\n");
	UART_SendString(UART_DEV, (uint8_t*)"Modularizacao de Sistemas Embarcados Usando Orientacao a Objeto\r\n");

	while (1) {
		/* light */
		lux = light_read();
		/* output values to OLED display */
		intToString(lux, buf, 10, 10);
		oled_fillRect((1+9*6),9, 80, 16, OLED_COLOR_WHITE);
		oled_putString((1+9*6),9, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE); //mostra valor lido no display oled

		if (menuIsShowing != 1) { //exibe menu
			show_range_selected(range_selected);
			show_menu();
			menuIsShowing = 1;
		}
		len = UART_Receive(UART_DEV, &data,
				1, NONE_BLOCKING);
		if (len > 0) { //se recebeu alguma coisa na UART
			switch (data) {
			case '1': //mostra valor lido pelo sensor atraves da UART.
				UART_SendString(UART_DEV, (uint8_t*)"\r\nValor lido pelo sensor: ");
				UART_SendString(UART_DEV, (uint8_t*) buf);
				UART_SendString(UART_DEV, (uint8_t*)" lux");
				break;
			case '2': //configura faixa do sensor para 1000.
				light_shutdown();
				light_enable();
				light_setRange(LIGHT_RANGE_1000);
				UART_SendString(UART_DEV, (uint8_t*)"\r\nSensor configurado para faixa de 0 a 1000.");
				range_selected = 1;
				break;
			case '3': //configura faixa do sensor para 4000.
				light_shutdown();
				light_enable();
				light_setRange(LIGHT_RANGE_4000);
				UART_SendString(UART_DEV, (uint8_t*)"\r\nSensor configurado para faixa de 0 a 4000.");
				range_selected = 2;
				break;
			case '4': //configura faixa do sensor para 16000.
				light_shutdown();
				light_enable();
				light_setRange(LIGHT_RANGE_16000);
				UART_SendString(UART_DEV, (uint8_t*)"\r\nSensor configurado para faixa de 0 a 16000.");
				range_selected = 3;
				break;
			case '5'://configura faixa do sensor para 64000.
				light_shutdown();
				light_enable();
				light_setRange(LIGHT_RANGE_64000);
				UART_SendString(UART_DEV, (uint8_t*)"\r\nSensor configurado para faixa de 0 a 64000.");
				range_selected = 4;
				break;
			default: //comando inválido.
				UART_SendString(UART_DEV, (uint8_t*)"\r\nError - Opcao invalida!!");
				break;
			}

			menuIsShowing = 0;
		}
		Timer0_Wait(100);
	}


	while(1);
}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1);
}
