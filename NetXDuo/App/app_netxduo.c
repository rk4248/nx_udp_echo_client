/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_netxduo.c
  * @author  MCD Application Team
  * @brief   NetXDuo applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
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
#include "app_netxduo.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "nx_stm32_eth_config.h"
#include "main.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
TX_THREAD AppMainThread;
TX_THREAD AppUDPThread;
TX_THREAD AppLinkThread;

TX_THREAD IPStatistic;

TX_SEMAPHORE Semaphore;

NX_PACKET_POOL AppPool;

NX_IP IpInstance;
NX_DHCP DHCPClient;
NX_UDP_SOCKET UDPSocket;
ULONG IpAddress;
ULONG NetMask;

CHAR *pointer;

extern TIM_HandleTypeDef htim7;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static VOID App_Main_Thread_Entry(ULONG thread_input);
static VOID App_UDP_Thread_Entry(ULONG thread_input);
static VOID App_UDP_Client_Thread_Entry(ULONG thread_input);
static VOID App_Link_Thread_Entry(ULONG thread_input);

static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr);

void IP_Statiscitc_Thread(ULONG thread_input);

/* USER CODE END PFP */
/**
  * @brief  Application NetXDuo Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT MX_NetXDuo_Init(VOID *memory_ptr)
{
  UINT ret = NX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

  /* USER CODE BEGIN MX_NetXDuo_MEM_POOL */

  /* USER CODE END MX_NetXDuo_MEM_POOL */

  /* USER CODE BEGIN MX_NetXDuo_Init */
 printf("\n\r\t\t\tNxUDPClient application started\n\r");

  /* Allocate the memory for packet_pool.  */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer,  NX_PACKET_POOL_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }
  /* Create the Packet pool to be used for packet allocation, it has the minimum required number of packet
   * to let this application work, if extra NX_PACKET are to be used the NX_PACKET_POOL_SIZE should be increased
   */
  ret = nx_packet_pool_create(&AppPool, "Main Packet Pool", PAYLOAD_SIZE, pointer, NX_PACKET_POOL_SIZE);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_ENABLED;
  }

  /* Allocate the memory for Ip_Instance */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer,   2 * DEFAULT_MEMORY_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create the main NX_IP instance */
//  ret = nx_ip_create(&IpInstance, "Main Ip instance", NULL_ADDRESS, NULL_ADDRESS, &AppPool, nx_stm32_eth_driver,
//                     pointer, 2 * DEFAULT_MEMORY_SIZE, DEFAULT_PRIORITY);

  ret = nx_ip_create(&IpInstance, "Main Ip instance", IP_ADDRESS(192,168,88,81), IP_ADDRESS(255,255,255,0), &AppPool, nx_stm32_eth_driver,
                      pointer, 2 * DEFAULT_MEMORY_SIZE, DEFAULT_PRIORITY);


  if (ret != NX_SUCCESS)
  {
    return NX_NOT_ENABLED;
  }

  /* Allocate the memory for ARP */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, DEFAULT_MEMORY_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Enable the ARP protocol and provide the ARP cache size for the IP instance */
  ret = nx_arp_enable(&IpInstance, (VOID *)pointer, DEFAULT_MEMORY_SIZE);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_ENABLED;
  }

  /* Enable the ICMP */
  ret = nx_icmp_enable(&IpInstance);

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_ENABLED;
  }

  /* Enable the UDP protocol required for  DHCP communication */
  ret = nx_udp_enable(&IpInstance);

  /* Allocate the memory for main thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer,2 *  DEFAULT_MEMORY_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* Create the main thread */
  ret = tx_thread_create(&AppMainThread, "App Main thread", App_Main_Thread_Entry, 0, pointer, 2 * DEFAULT_MEMORY_SIZE,
                         DEFAULT_MAIN_PRIORITY, DEFAULT_MAIN_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

  if (ret != TX_SUCCESS)
  {
    return NX_NOT_ENABLED;
  }

  /* Allocate the memory for UDP client thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer,2 *  DEFAULT_MEMORY_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }
  /* create the UDP client thread */
  ret = tx_thread_create(&AppUDPThread, "App UDP Thread", App_UDP_Client_Thread_Entry, 0, pointer, 2 * DEFAULT_MEMORY_SIZE,
                         DEFAULT_PRIORITY, DEFAULT_PRIORITY, TX_NO_TIME_SLICE, TX_DONT_START);

  if (ret != TX_SUCCESS)
  {
    return NX_NOT_ENABLED;
  }

  /* Allocate the memory for Link thread   */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer,2 *  DEFAULT_MEMORY_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }

  /* create the Link thread */
  ret = tx_thread_create(&AppLinkThread, "App Link Thread", App_Link_Thread_Entry, 0, pointer, 2 * DEFAULT_MEMORY_SIZE,
                         LINK_PRIORITY, LINK_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START);

  if (ret != TX_SUCCESS)
  {
    return NX_NOT_ENABLED;
  }

  /* create the DHCP client */
  ret = nx_dhcp_create(&DHCPClient, &IpInstance, "DHCP Client");

  if (ret != NX_SUCCESS)
  {
    return NX_NOT_ENABLED;
  }

  /* set DHCP notification callback  */

  tx_semaphore_create(&Semaphore, "DHCP Semaphore", 0);




  /* Allocate the memory for statisctic out thread   */
   if (tx_byte_allocate(byte_pool, (VOID **) &pointer,2 *  DEFAULT_MEMORY_SIZE / 2, TX_NO_WAIT) != TX_SUCCESS)
   {
     return TX_POOL_ERROR;
   }

   /* create the Statistic thread */
   ret = tx_thread_create(&IPStatistic, "IP Statistic Thread", IP_Statiscitc_Thread, 0, pointer, 2 * DEFAULT_MEMORY_SIZE, \
		   DEFAULT_MAIN_PRIORITY + 5, DEFAULT_MAIN_PRIORITY +5 , TX_NO_TIME_SLICE, TX_AUTO_START);

   if (ret != TX_SUCCESS)
   {
     return NX_NOT_ENABLED;
   }

  /* USER CODE END MX_NetXDuo_Init */

  return ret;
}

/* USER CODE BEGIN 1 */

/**
* @brief  ip address change callback.
* @param ip_instance: NX_IP instance
* @param ptr: user data
* @retval none
*/
static VOID ip_address_change_notify_callback(NX_IP *ip_instance, VOID *ptr)
{
  /* release the semaphore as soon as an IP address is available */
  tx_semaphore_put(&Semaphore);
}

/**
* @brief  Main thread entry.
* @param thread_input: ULONG user argument used by the thread entry
* @retval none
*/
static VOID App_Main_Thread_Entry(ULONG thread_input)
{
  UINT ret;

  printf("\n\r    --- App_Main_Thread_Entry ---\n\r");

  /* register the IP address change callback */
  ret = nx_ip_address_change_notify(&IpInstance, ip_address_change_notify_callback, NULL);
  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }

//  /* start the DHCP client */
//  ret = nx_dhcp_start(&DHCPClient);
//  if (ret != NX_SUCCESS)
//  {
//    Error_Handler();
//  }
//
//  /* wait until an IP address is ready */
//  if(tx_semaphore_get(&Semaphore, TX_WAIT_FOREVER) != TX_SUCCESS)
//  {
//    Error_Handler();
//  }
  /* get IP address */
  ret = nx_ip_address_get(&IpInstance, &IpAddress, &NetMask);

  /* print the IP address */
  PRINT_IP_ADDRESS(IpAddress);

  if (ret != TX_SUCCESS)
  {
    Error_Handler();
  }
  /* the network is correctly initialized, start the UDP thread */
  tx_thread_resume(&AppUDPThread);

  /* this thread is not needed any more, we relinquish it */
  tx_thread_relinquish();

  return;
}

static VOID App_UDP_Thread_Entry(ULONG thread_input)
{
  UINT ret;
  UINT count = 0;
  ULONG bytes_read;
  NX_PACKET *server_packet;
  UCHAR data_buffer[512];

  NX_PACKET *data_packet;

  /* create the UDP socket */
  ret = nx_udp_socket_create(&IpInstance, &UDPSocket, "UDP Client Socket", NX_IP_NORMAL, NX_FRAGMENT_OKAY, NX_IP_TIME_TO_LIVE, QUEUE_MAX_SIZE);

  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }

  /* bind UDP socket to the DEFAULT PORT */
  ret = nx_udp_socket_bind(&UDPSocket, DEFAULT_PORT, TX_WAIT_FOREVER);

  if (ret != NX_SUCCESS)
  {
    Error_Handler();
  }

  while(count++ < MAX_PACKET_COUNT)
  {
    TX_MEMSET(data_buffer, '\0', sizeof(data_buffer));

    /* create the packet to send over the UDP socket */
    ret = nx_packet_allocate(&AppPool, &data_packet, NX_UDP_PACKET, TX_WAIT_FOREVER);

    if (ret != NX_SUCCESS)
    {
      Error_Handler();
    }

    ret = nx_packet_data_append(data_packet, (VOID *)DEFAULT_MESSAGE, sizeof(DEFAULT_MESSAGE), &AppPool, TX_WAIT_FOREVER);

    if (ret != NX_SUCCESS)
    {
      Error_Handler();
    }

    /* send the message */
    ret = nx_udp_socket_send(&UDPSocket, data_packet, UDP_SERVER_ADDRESS, DEFAULT_PORT);

    /* wait 10 sec to receive response from the server */
    ret = nx_udp_socket_receive(&UDPSocket, &server_packet, DEFAULT_TIMEOUT);

    if (ret == NX_SUCCESS)
    {
      ULONG source_ip_address;
      UINT source_port;

      /* get the server IP address and  port */
      nx_udp_source_extract(server_packet, &source_ip_address, &source_port);

      /* retrieve the data sent by the server */
      nx_packet_data_retrieve(server_packet, data_buffer, &bytes_read);

      /* print the received data */
     PRINT_DATA(source_ip_address, source_port, data_buffer);

      /* release the server packet */
      nx_packet_release(server_packet);

      /* toggle the green led on success */
      BSP_LED_Toggle(LED_GREEN);
    }
    else
    {
      /* connection lost with the server, exit the loop */
      break;
    }
    /* Add a short timeout to let the echool tool correctly
    process the just sent packet before sending a new one */
    tx_thread_sleep(20);
  }
  /* unbind the socket and delete it */
  nx_udp_socket_unbind(&UDPSocket);
  nx_udp_socket_delete(&UDPSocket);

  if (count == MAX_PACKET_COUNT + 1)
  {
    printf("\n\r--------------------------- SUCCESS : %u / %u packets sent ---------------------------\n\r", count - 1, MAX_PACKET_COUNT);
    Success_Handler();
  }
  else
  {
    printf("\n\r--------------------------- FAIL : %u / %u packets sent ----------------------------\n\r", count - 1, MAX_PACKET_COUNT);
    Error_Handler();
  }
}

/**
* @brief  Link thread entry
* @param thread_input: ULONG thread parameter
* @retval none
*/
static VOID App_Link_Thread_Entry(ULONG thread_input)
{
  ULONG actual_status;
  UINT linkdown = 0, status;

  while(1)
  {
    /* Get Physical Link stackavailtus. */
    status = nx_ip_interface_status_check(&IpInstance, 0, NX_IP_LINK_ENABLED,
                                      &actual_status, 10);

    if(status == NX_SUCCESS)
    {
      if(linkdown == 1)
      {
        linkdown = 0;
        status = nx_ip_interface_status_check(&IpInstance, 0, NX_IP_ADDRESS_RESOLVED,
                                      &actual_status, 10);
        if(status == NX_SUCCESS)
        {
          /* The network cable is connected again. */
          printf("The network cable is connected again.\n\r");
          /* Print UDP Echo Client is available again. */
          printf("UDP Echo Client is available again.\n\r");
        }
        else
        {
          /* The network cable is connected. */
          printf("The network cable is connected.\n\r");
          /* Send command to Enable Nx driver. */
          nx_ip_driver_direct_command(&IpInstance, NX_LINK_ENABLE,
                                      &actual_status);
          /* Restart DHCP Client. */
//          nx_dhcp_stop(&DHCPClient);
//          nx_dhcp_start(&DHCPClient);
        }
      }
    }
    else
    {
      if(0 == linkdown)
      {
        linkdown = 1;
        /* The network cable is not connected. */
        printf("The network cable is not connected.\n\r");
      }
    }

    tx_thread_sleep(NX_ETH_CABLE_CONNECTION_CHECK_PERIOD);
  }
}

#define AUDIO_TOTAL_BUF_SIZE	48*4*32		//48 probki (kazda probka zajmuje w odbiorniku 4 bajty, 32 bufory = 768*2 probki (48kHz -> 32ms) w buforze )
uint8_t audioBuff[AUDIO_TOTAL_BUF_SIZE];	//
volatile int8_t rd_enable = 0;
extern I2S_HandleTypeDef hi2s3;

uint32_t ethSamples = 0, ethSamplesStart = 0;
uint32_t i2sSamples = 0;

volatile uint16_t i2s_halfTime  = 0;
volatile uint16_t i2s_period  = 0;
volatile uint16_t i2s_lastTime  = 0;
volatile uint16_t eth_fullTime  = 0;
volatile uint16_t eth_period    = 0;
volatile uint16_t eth_lastTime  = 0;
volatile int16_t  eth_i2s        = 0;

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{


	i2s_halfTime = htim7.Instance->CNT;
	i2s_period   = i2s_halfTime - i2s_lastTime;
	i2s_lastTime = i2s_halfTime;

	BSP_LED_Toggle(LED_BLUE);
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
	i2sSamples += AUDIO_TOTAL_BUF_SIZE/4;
}

UCHAR data_buffer[1536];

static VOID App_UDP_Client_Thread_Entry(ULONG thread_input)
{
  UINT ret;
  ULONG bytes_read;
  UINT source_port;

  ULONG source_ip_address;
  NX_PACKET *data_packet;

  ULONG decymationFactor = 0;

  uint16_t time;

  /* create the UDP socket */
  ret = nx_udp_socket_create(&IpInstance, &UDPSocket, "UDP Client Socket", NX_IP_NORMAL, NX_FRAGMENT_OKAY, NX_IP_TIME_TO_LIVE, QUEUE_MAX_SIZE);

  if (ret != NX_SUCCESS)
  {
     Error_Handler();
  }

  /* bind the socket indefinitely on the required port */
  ret = nx_udp_socket_bind(&UDPSocket, DEFAULT_PORT, TX_WAIT_FOREVER);

  if (ret != NX_SUCCESS)
  {
     Error_Handler();
  }
  else
  {
    printf("UDP Client listening on PORT %d.. \n", DEFAULT_PORT);
  }

  HAL_TIM_Base_Start(&htim7);
  //uint32_t packetRxCnt = 0;
  uint32_t pxPtr = 0;
  uint8_t dmaStart = 0;
  HAL_I2S_Transmit_DMA(&hi2s3, (uint16_t*)audioBuff, (AUDIO_TOTAL_BUF_SIZE / 2) );
  HAL_I2S_DMAPause(&hi2s3);

  uint16_t trash = 0;
  uint16_t tempFlag = 0;
  pxPtr = 0;
  ethSamples = 0;
  i2sSamples = 0;

  while(1)
  {
	  /* wait for data for 100 msec */
	  ret = nx_udp_socket_receive(&UDPSocket, &data_packet, 10);

	  if (ret == NX_SUCCESS)
	  {
		  time = htim7.Instance->CNT;
		  eth_period = time - eth_lastTime;
		  eth_lastTime = time;

		  /* data is available, read it into the data buffer */
		  nx_packet_data_retrieve(data_packet, &data_buffer[0], &bytes_read);

		  if(trash < 500)
		  {
			  trash++;
			  bytes_read = 0;
		  }

		  if(bytes_read)
		  {
			  //BSP_LED_Off(LED_RED);
			  BSP_LED_Toggle(LED_GREEN);
			  for(uint16_t n=0;n<bytes_read/2;n++)
			  {
				  if(pxPtr == AUDIO_TOTAL_BUF_SIZE)
				  {
					  pxPtr = 0;
					  eth_fullTime = time;
					  eth_i2s = eth_fullTime - i2s_halfTime;
					  //BSP_LED_Toggle(LED_RED);
					  if(eth_i2s < 7000)
					  {
						  tempFlag = 1;
					  } else
					  {
						  tempFlag = 0;
					  }
				  }

				  audioBuff[pxPtr + 0] = data_buffer[0+n*2];
				  audioBuff[pxPtr + 1] = data_buffer[1+n*2];
				  audioBuff[pxPtr + 2] = 0;
				  audioBuff[pxPtr + 3] = 0;
				  pxPtr += 4;
				  ethSamples++;

				  decymationFactor++;
				  if(decymationFactor == (1000000 / 40))
				  {
					  decymationFactor = 0;
					  if(tempFlag == 1)
					  {
						  pxPtr-=4;
						  tempFlag = 0;
						  BSP_LED_Toggle(LED_RED);
					  }
					  /*
    				  audioBuff[pxPtr + 0] = audioBuff[pxPtr - 4];
    				  audioBuff[pxPtr + 1] = audioBuff[pxPtr - 3];
    				  audioBuff[pxPtr + 2] = 0;
    				  audioBuff[pxPtr + 3] = 0;
    				  pxPtr += 4;
					   */
				  }

				  if(pxPtr == (AUDIO_TOTAL_BUF_SIZE/2) )
				  {
					  if(dmaStart == 0)
					  {
						  //HAL_I2S_Transmit_DMA(&hi2s3, (uint16_t*)audioBuff, AUDIO_TOTAL_BUF_SIZE / 2);
						  HAL_I2S_DMAResume(&hi2s3);
						  dmaStart = 1;
						  ethSamplesStart = ethSamples;
						  i2sSamples = 0;
					  }
				  }

			  }
		  }
		  nx_packet_release(data_packet);

	  }
	  else
	  {
		  /* the server is in idle state, toggle the green led */
		  if(dmaStart == 1)
		  {
			  //HAL_I2S_DMAStop(&hi2s3);
			  HAL_I2S_DMAPause(&hi2s3);
			  dmaStart = 0;
		  }
		  pxPtr = 0;
		  ethSamples = 0;
		  BSP_LED_Off(LED_GREEN);
		  BSP_LED_Toggle(LED_RED);
	  }
  }
}

void IP_Statiscitc_Thread(ULONG thread_input)
{
	ULONG ip_total_packets_sent, ip_total_bytes_sent;
	ULONG ip_total_packets_received, ip_total_bytes_received;
	ULONG ip_invalid_packets, ip_receive_packets_dropped;
	ULONG ip_receive_checksum_errors, ip_send_packets_dropped;
	ULONG ip_total_fragments_sent, ip_total_fragments_received;

	static ULONG pckRx = 0, bytRx = 0;
	ULONG pckBytes;
	UINT ret;

	while(1)
	{
		ret =  nx_ip_info_get(&IpInstance, &ip_total_packets_sent, &ip_total_bytes_sent, \
				&ip_total_packets_received, &ip_total_bytes_received, \
				&ip_invalid_packets, &ip_receive_packets_dropped, \
				&ip_receive_checksum_errors, &ip_send_packets_dropped, \
				&ip_total_fragments_sent, &ip_total_fragments_received);

		if(ret == NX_SUCCESS)
		{
			if( (ip_total_packets_received - pckRx) != 0)
			{
				pckBytes = (ip_total_bytes_received - bytRx) / (ip_total_packets_received - pckRx);
			} else
			{
				pckBytes = 0;
			}
			pckRx = ip_total_packets_received;
			bytRx = ip_total_bytes_received;
//			printf("PTx:%ld  BTx:%ld  PRx:%ld  BRx:%ld  B/P:%ld  ", \
//					ip_total_packets_sent, ip_total_bytes_sent, \
//					ip_total_packets_received, ip_total_bytes_received, \
//					pckBytes);
			//printf("DMA: %ld\n\r",dmaBufferPlay);

			printf("ETH:%ld I2S:%ld ",ethSamplesStart,i2sSamples);
			//printf("ETH:%ld I2S:%ld E-I:%ld ",ethBufferPlay,dmaBufferPlay, ethSamples-i2sSamples);
			printf("I2SPeriod: %d EthPeriod:%d ETHI2S:%d\r\n", i2s_period, eth_period, eth_i2s);
		} else {
			printf("Blad odczytu statystyk\n\r");
		}
		tx_thread_sleep(50);	//0.5s
	}
	return;
}
/* USER CODE END 1 */
