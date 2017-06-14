#include <string.h>
#include <time.h>
#include <rtthread.h>
#include <board.h>
#include "drv_rtc.h"

/* Defines related to Clock configuration */
#define RTC_ASYNCH_PREDIV          0x7F   /* LSE as RTC clock */
#define RTC_SYNCH_PREDIV           0x00FF /* LSE as RTC clock */
#define RTC_CONFIG_MAGIC           0x32F7
/* RTC handler declaration */
static RTC_HandleTypeDef RtcHandle;
static struct rt_device rtc;

static rt_err_t rt_rtc_open(rt_device_t dev, rt_uint16_t oflag)
{
    if (dev->rx_indicate != RT_NULL)
    {
        /* Open Interrupt */
    }

    return RT_EOK;
}

static rt_size_t rt_rtc_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    return 0;
}

static rt_err_t rt_rtc_control(rt_device_t dev, rt_uint8_t cmd, void *args)
{
    time_t *time;
    RTC_DateTypeDef sdatestructure;
    RTC_TimeTypeDef stimestructure;
    struct tm time_temp;

    RT_ASSERT(dev != RT_NULL);

    memset(&time_temp, 0, sizeof(time_temp));

    switch (cmd)
    {
    case RT_DEVICE_CTRL_RTC_GET_TIME:
		{
        time = (time_t *)args;
        
        /* Get the current Time */
			  HAL_RTC_GetDate(&RtcHandle,&sdatestructure,RTC_FORMAT_BIN);
			  HAL_RTC_GetTime(&RtcHandle, &stimestructure, RTC_FORMAT_BIN);
			
        /* Years since 1900 : 0-99 range */
        time_temp.tm_year = sdatestructure.Year + 2000 - 1900;
        /* Months *since* january 0-11 : RTC_Month_Date_Definitions 1 - 12 */
        time_temp.tm_mon = sdatestructure.Month - 1;
        /* Day of the month 1-31 : 1-31 range */
        time_temp.tm_mday = sdatestructure.Date;
        /* Hours since midnight 0-23 : 0-23 range */
        time_temp.tm_hour = stimestructure.Hours;
        /* Minutes 0-59 : the 0-59 range */
        time_temp.tm_min = stimestructure.Minutes;
        /* Seconds 0-59 : the 0-59 range */
        time_temp.tm_sec = stimestructure.Seconds;

        *time = mktime(&time_temp);
		}
        break;

    case RT_DEVICE_CTRL_RTC_SET_TIME:
    {
        const struct tm *time_new;

        time = (time_t *)args;
        time_new = localtime(time);

			  /*##-1- Configure the Date #################################################*/
  /* Set Date */
	/* 0-99 range              : Years since 1900 */
  sdatestructure.Year = time_new->tm_year + 1900 - 2000;
	/* RTC_Month_Date_Definitions 1 - 12 : Months *since* january 0-11 */
  sdatestructure.Month = time_new->tm_mon + 1;
	/* 1-31 range : Day of the month 1-31 */
  sdatestructure.Date = time_new->tm_mday;
	/* 1 - 7 : Days since Sunday (0-6) */
  sdatestructure.WeekDay = time_new->tm_wday + 1;
  
  if(HAL_RTC_SetDate(&RtcHandle,&sdatestructure,RTC_FORMAT_BIN) != HAL_OK)
  {
    /* Initialization Error */
    //Error_Handler();
  }

  /*##-2- Configure the Time #################################################*/
  /* Set Time */
	 /* 0-23 range : Hours since midnight 0-23 */
  stimestructure.Hours = time_new->tm_hour;
	/* the 0-59 range : Minutes 0-59 */
  stimestructure.Minutes =time_new->tm_min;
	/* the 0-59 range : Seconds 0-59 */
  stimestructure.Seconds = time_new->tm_sec;
  stimestructure.TimeFormat = RTC_HOURFORMAT_24;
  stimestructure.DayLightSaving = RTC_DAYLIGHTSAVING_NONE ;
  stimestructure.StoreOperation = RTC_STOREOPERATION_RESET;

  if (HAL_RTC_SetTime(&RtcHandle, &stimestructure, RTC_FORMAT_BIN) != HAL_OK)
  {
    /* Initialization Error */
    //Error_Handler();
  }

  /*##-3- Writes a data in a RTC Backup data Register1 #######################*/
  HAL_RTCEx_BKUPWrite(&RtcHandle, RTC_BKP_DR1, RTC_CONFIG_MAGIC);
	
    }
    break;
    }

    return RT_EOK;
}
static void RTC_Init(void)
{
   /*##-1- Configure the RTC peripheral #######################################*/
  /* Configure RTC prescaler and RTC data registers */
  /* RTC configured as follows:
      - Hour Format    = Format 24
      - Asynch Prediv  = Value according to source clock
      - Synch Prediv   = Value according to source clock
      - OutPut         = Output Disable
      - OutPutPolarity = High Polarity
      - OutPutType     = Open Drain */ 
  RtcHandle.Instance = RTC; 
  RtcHandle.Init.HourFormat = RTC_HOURFORMAT_24;
  RtcHandle.Init.AsynchPrediv = RTC_ASYNCH_PREDIV;
  RtcHandle.Init.SynchPrediv = RTC_SYNCH_PREDIV;
  RtcHandle.Init.OutPut = RTC_OUTPUT_DISABLE;
  RtcHandle.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  RtcHandle.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;

  if (HAL_RTC_Init(&RtcHandle) != HAL_OK)
  {
    /* Initialization Error */
    //Error_Handler();
  }
}
void HAL_RTC_MspInit(RTC_HandleTypeDef* hrtc)
{
    RCC_OscInitTypeDef RCC_OscInitStruct;
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;

    __HAL_RCC_PWR_CLK_ENABLE();
	  HAL_PWR_EnableBkUpAccess();
    
    RCC_OscInitStruct.OscillatorType=RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.PLL.PLLState=RCC_PLL_NONE;
    RCC_OscInitStruct.LSEState=RCC_LSE_ON;                  
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    PeriphClkInitStruct.PeriphClockSelection=RCC_PERIPHCLK_RTC;
    PeriphClkInitStruct.RTCClockSelection=RCC_RTCCLKSOURCE_LSE;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
        
    __HAL_RCC_RTC_ENABLE();
}

int rt_hw_rtc_init(void)
{
    rtc.type    = RT_Device_Class_RTC;

    RTC_Init();

	/*##-2- Check if Data stored in BackUp register1: No Need to reconfigure RTC#*/
  /* Read the Back Up Register 1 Data */
  if (HAL_RTCEx_BKUPRead(&RtcHandle, RTC_BKP_DR1) != RTC_CONFIG_MAGIC)
  {
       rt_kprintf("rtc is not configured\n");
        rt_kprintf("please configure with set_date and set_time\n");
  }

    /* register rtc device */
    rtc.init    = RT_NULL;
    rtc.open    = rt_rtc_open;
    rtc.close   = RT_NULL;
    rtc.read    = rt_rtc_read;
    rtc.write   = RT_NULL;
    rtc.control = rt_rtc_control;

    /* no private */
    rtc.user_data = RT_NULL;

    rt_device_register(&rtc, "rtc", RT_DEVICE_FLAG_RDWR);

#ifdef RT_USING_FINSH
    {
        extern void list_date(void);
        list_date();
    }
#endif

    return 0;
}
INIT_DEVICE_EXPORT(rt_hw_rtc_init);
