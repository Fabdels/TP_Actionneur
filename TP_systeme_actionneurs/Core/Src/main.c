/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 * @authors		   : Fabien DELSANTI, Mikael DELLA SETA
 * @version		   : final
 * @date		   : 25 novembre 2022
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2022 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/// Taille du buffer d'écriture pour afficher des caractères sur le shell
#define UART_TX_BUFFER_SIZE 300
/// Taille du buffer de lecture des caractères entrés dans le shell
#define UART_RX_BUFFER_SIZE 1
/// Taille du buffer pour lancer des fonctions dans shell
#define CMD_BUFFER_SIZE 64
/// Nombre maximum d'arguments pour une fonction shell
#define MAX_ARGS 9
/// Vitesse maximale dans le sens positif, en nombre de tick de timer
#define MAX_SPEED_VALUE 5312
/// Valeur maximum des CCR du timer générant les PWM du moteur
#define MAX_PULSE 5312
/// LF = line feed, saut de ligne
#define ASCII_LF 0x0A
/// CR = carriage return, retour chariot
#define ASCII_CR 0x0D
/// DEL = delete
#define ASCII_DEL 0x7F
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/// Flag pour le controle de l'ADC. =1 -> l'ADC est en cours de mesure
int ADCmeasuring = 1;
/// Message de shell pour l'invite d'une commande
uint8_t prompt[]="\r\nuser@Nucleo-STM32G431>>";
/// Message de démarage du shell
uint8_t started[]=
		"\r\n*-----------------------------*"
		"\r\n| Welcome on Nucleo-STM32G431 |"
		"\r\n*-----------------------------*"
		"\r\n";
/// Combinaison de caractères pour le saut de ligne
uint8_t newline[]="\r\n";
/// Message d'erreure pour les fonctions shell non reconnues
uint8_t cmdNotFound[]="Command not found\r\n";
/// Flag mis à 1 à la réception d'un caractère sur la liaison UART
uint32_t uartRxReceived;
/// Buffer de caractères pour la réception de données via l'UART
uint8_t uartRxBuffer[UART_RX_BUFFER_SIZE];
/// Buffer de caractères pour l'envoi de données via l'UART
uint8_t uartTxBuffer[UART_TX_BUFFER_SIZE];
/// Message d'affichage de la liste des commandes du shell
uint8_t commandList[] =
		"\r\nhelp:		affiche cette liste"
		"\r\npinout:	affiche toutes les broches utilisées"
		"\r\nstart:		allume l'étage de puissance du moteur"
		"\r\nstop:		éteind l'étage de puissance du moteur"

		"\r\nset PA5:	allume ou éteint la LED de la carte en fonction de l'argument 0 ou 1"
		"\r\nspeed=:	change la commande de vitesse du moteur"
		"\r\nADC?:		lance une mesure de courant"
		"\r\nrealSpeed:	affiche la vitesse mesurée"
		;
/// Message d'affichage pour la liste des pins et leur équivalent sur le hacheur
uint8_t pinList[] =
		"\r\nPA8:	TIM1_C1		-> CMD_R_TOP:	p13"
		"\r\nPA11:	TIM1_C1N	-> CMD_R_BOT:	p31"
		"\r\nPA9:	TIM1_C2		-> CMD_Y_TOP:	p12"
		"\r\nPA12:	TIM1_C2N	-> CMD_Y_BOT:	p30"
		//"\r\nPC13:		Button"
		"\r\nPC3:	ISO_RESET	-> ISO_RESET:	p33"
		"\r\nPA0:	ADC			-> R_HALL:		p35"
		"\r\nPA15:	TIM2_C1		-> R_VPH_SENSE	p24";
//"\r\nPA5:		LED\r\n";

/*		"\r\nPF0:		RCC_OSC_IN"
		"\r\nPF1:		RCC_OSC_OUT"
		"\r\nPA2:		USART_TX"
		"\r\nPA3:		USART_RX"	*/


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*
int cnt = 0;
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc1)
{
	cnt++;

	float current = (((HAL_ADC_GetValue(&hadc1)/4096)*3.3)-2.5)*12;

	ADCmeasuring = 0;

	sprintf(uartTxBuffer,"ADC : %f\r\n", current);
	HAL_UART_Transmit(&huart2, uartTxBuffer, strlen(uartTxBuffer), HAL_MAX_DELAY);

	if (cnt>30)
	{
		ADCmeasuring = 1;
	}
}
 */
/// temps haut et temps bas (indéterminé) de la phase Red en tick de HCLK
uint32_t redPhasePeriod1 = 0, redPhasePeriod2 = 0;
/// Sauvegarde de la dernière valeur du CCR pour la mesure de temps
uint32_t memtim2CCR = 0;
/**
 * \fn void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef* htim)
 * \brief Fonction de callback pour la gestion de la détection d'un front
 *		sur le signal du channel. Permet le calcul des temps haut et bas de la phase Red
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef* htim){
	uint32_t timCCR;
	if(htim==&htim2){
		timCCR = TIM2->CCR2;
		if(memtim2CCR < timCCR){
			redPhasePeriod1 = redPhasePeriod2;
			redPhasePeriod2 = (timCCR-memtim2CCR);
		}
		else{
			redPhasePeriod1 = 0;
			redPhasePeriod2 = 0;
		}
		memtim2CCR = timCCR;
	}
}


/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int erreur de sortie de la main
 */
int main(void)
{
	/* USER CODE BEGIN 1 */
	char	 	cmdBuffer[CMD_BUFFER_SIZE];
	int 		idx_cmd;
	char* 		argv[MAX_ARGS];
	int		 	argc = 0;
	char*		token;
	int 		newCmdReady = 0;
	int			speedValue = 0;

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_TIM1_Init();
	MX_USART2_UART_Init();
	MX_ADC1_Init();
	MX_TIM2_Init();
	/* USER CODE BEGIN 2 */
	HAL_TIM_Base_Start_IT(&htim2);
	HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);


	HAL_GPIO_WritePin(ISO_RESET_GPIO_Port, ISO_RESET_Pin, GPIO_PIN_RESET);

	memset(argv,NULL,MAX_ARGS*sizeof(char*));
	memset(cmdBuffer,NULL,CMD_BUFFER_SIZE*sizeof(char));
	memset(uartRxBuffer,NULL,UART_RX_BUFFER_SIZE*sizeof(char));
	memset(uartTxBuffer,NULL,UART_TX_BUFFER_SIZE*sizeof(char));

	HAL_UART_Receive_IT(&huart2, uartRxBuffer, UART_RX_BUFFER_SIZE);
	HAL_Delay(10);
	HAL_UART_Transmit(&huart2, started, sizeof(started), HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart2, prompt, sizeof(prompt), HAL_MAX_DELAY);



	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{/*
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
		HAL_UART_Receive(USART2, usartBuffer, 1, HAL_MAX_DELAY);
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
		HAL_UART_Transmit(USART2, usartBuffer, 1, HAL_MAX_DELAY);
	 */

		/*sprintf(usartBuffer, "helloworld!\r\n");
		HAL_UART_Transmit(&huart2, usartBuffer, sizeof(usartBuffer), HAL_MAX_DELAY);
		HAL_Delay(1000);
		HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);*/


		if (!ADCmeasuring){

			HAL_ADC_Start_IT(&hadc1);

		}

		if(uartRxReceived){
			switch(uartRxBuffer[0]){
			/// . Nouvelle ligne, instruction à traiter
			case ASCII_CR:
				HAL_UART_Transmit(&huart2, newline, sizeof(newline), HAL_MAX_DELAY);
				cmdBuffer[idx_cmd] = '\0';
				argc = 0;
				token = strtok(cmdBuffer, " ");
				while(token!=NULL){
					argv[argc++] = token;
					token = strtok(NULL, " ");
				}

				idx_cmd = 0;
				newCmdReady = 1;
				break;

				/// Suppression du dernier caractère
			case ASCII_DEL:
				cmdBuffer[idx_cmd--] = '\0';
				HAL_UART_Transmit(&huart2, uartRxBuffer, UART_RX_BUFFER_SIZE, HAL_MAX_DELAY);
				break;

				/// Nouveau caractère
			default:
				cmdBuffer[idx_cmd++] = uartRxBuffer[0];
				HAL_UART_Transmit(&huart2, uartRxBuffer, UART_RX_BUFFER_SIZE, HAL_MAX_DELAY);
			}
			uartRxReceived = 0;
		}

		if(newCmdReady){

			/// Showing commands
			if(strcmp(argv[0],"help")==0){
				sprintf(uartTxBuffer,commandList);
				HAL_UART_Transmit(&huart2, uartTxBuffer, sizeof(commandList), HAL_MAX_DELAY);
			}
			/// Showing pinouts
			else if(strcmp(argv[0],"pinout")==0){
				sprintf(uartTxBuffer,pinList);
				HAL_UART_Transmit(&huart2, uartTxBuffer, sizeof(pinList), HAL_MAX_DELAY);
			}

			/// Switching the LED on or off
			else if(strcmp(argv[0],"set")==0){
				if(strcmp(argv[1],"PA5")==0){
					HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, atoi(argv[2]));
					sprintf(uartTxBuffer,"Switch on/off led : %d\r\n",atoi(argv[2]));
					HAL_UART_Transmit(&huart2, uartTxBuffer, CMD_BUFFER_SIZE, HAL_MAX_DELAY);
				}

				else{
					HAL_UART_Transmit(&huart2, cmdNotFound, sizeof(cmdNotFound), HAL_MAX_DELAY);
				}
			}

			/// Starting the motor
			else if(strcmp(argv[0],"start")==0)
			{

				TIM1->CCR1 = MAX_SPEED_VALUE/2;
				TIM1->CCR2 = MAX_SPEED_VALUE/2;


				HAL_GPIO_WritePin(ISO_RESET_GPIO_Port, ISO_RESET_Pin, GPIO_PIN_SET);
				HAL_Delay(1);
				HAL_GPIO_WritePin(ISO_RESET_GPIO_Port, ISO_RESET_Pin, GPIO_PIN_RESET);

				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

				HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
				HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
				HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
				HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);

				sprintf(uartTxBuffer,"Power ON\r\n");
				HAL_UART_Transmit(&huart2, uartTxBuffer, strlen(uartTxBuffer), HAL_MAX_DELAY);
			}


			/// Stopping the motor
			else if(strcmp(argv[0],"stop")==0)
			{

				HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
				HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
				HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
				HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_2);

				HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

				sprintf(uartTxBuffer,"Power OFF\r\n");
				HAL_UART_Transmit(&huart2, uartTxBuffer, strlen(uartTxBuffer), HAL_MAX_DELAY);


			}

			/// Setting the speed of the motor
			else if (strcmp(argv[0],"speed=")==0)
			{
				speedValue = atoi(argv[1]);

				if (speedValue > 100)
				{
					speedValue = 100;
				}

				TIM1->CCR1 = MAX_SPEED_VALUE*speedValue/100;

				TIM1->CCR2 = MAX_PULSE - MAX_SPEED_VALUE*speedValue/100;

				sprintf(uartTxBuffer,"Setting the speed to %d %\r\n",speedValue);
				HAL_UART_Transmit(&huart2, uartTxBuffer, strlen(uartTxBuffer), HAL_MAX_DELAY);

				ADCmeasuring = 0;

			}


			///Getting the current's value
			else if (strcmp(argv[0],"ADC?")==0)
			{
				ADCmeasuring = 0;

				HAL_ADC_Start(&hadc1);
				HAL_ADC_PollForConversion(&hadc1,HAL_MAX_DELAY);
				float adcVal = HAL_ADC_GetValue(&hadc1);	//Getting the value of the ADC

				//Converting the value
				float current = (((adcVal/4096)*3.3)-2.5)*12;

				//Showing the value
				sprintf(uartTxBuffer,"ADC current value : %f A\r\n",current);
				HAL_UART_Transmit(&huart2, uartTxBuffer, strlen(uartTxBuffer), HAL_MAX_DELAY);

			}

			///Getting measured speed
			else if (strcmp(argv[0],"realSpeed")==0)
			{
				HAL_RCC_GetPCLK1Freq();
				//Showing the value
				sprintf(uartTxBuffer,"vitesse: ...\r\n");
				HAL_UART_Transmit(&huart2, uartTxBuffer, strlen(uartTxBuffer), HAL_MAX_DELAY);

			}

			///Command not found
			/*else if(strcmp(argv[0],"get")==0)
			{
				HAL_UART_Transmit(&huart2, cmdNotFound, sizeof(cmdNotFound), HAL_MAX_DELAY);
			}*/
			else{
				HAL_UART_Transmit(&huart2, cmdNotFound, sizeof(cmdNotFound), HAL_MAX_DELAY);
			}
			HAL_UART_Transmit(&huart2, prompt, sizeof(prompt), HAL_MAX_DELAY);
			newCmdReady = 0;
		}


		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	return -1;
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	 */
	HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
	RCC_OscInitStruct.PLL.PLLN = 85;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
	RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
			|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
	{
		Error_Handler();
	}
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback (UART_HandleTypeDef * huart){
	uartRxReceived = 1;
	HAL_UART_Receive_IT(&huart2, uartRxBuffer, UART_RX_BUFFER_SIZE);
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
