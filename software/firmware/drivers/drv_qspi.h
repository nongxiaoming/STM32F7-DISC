/*
 * File      : drv_qspi.h
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2009-2017 RT-Thread Develop Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2017-06-14    xiaonong       The first version for STM32F7XX
 */

#ifndef __DRV_SPI_H
#define __DRV_SPI_H

#include <stdint.h>
#include <rtthread.h>

#include "stm32f7xx.h"


extern int rt_hw_qspi_init(void);

#endif // __DRV_SPI_H
