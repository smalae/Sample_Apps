/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>
#include "nvm3_default.h"
#include "nvm3_hal_flash.h"
#include "nvm3_default_config.h"
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/device.h>
#include <soc.h>
#include <stdlib.h>
#include "em_msc.h"

	
void main(void)
{
	uint8_t data[3] = {10,11,12};
	int8_t status = -1;
	
	printk("hello_world\n");
	
	status = nvm3_initDefault();	
	if(status != 0)
	{
		printk("Module is not initialized %d\n", status);
	}
	else
	{
		printk("Module is initilaized successfully\n");
	}
  	status = nvm3_writeData(nvm3_defaultHandle,
                                      1,
                                      (unsigned char *)data,
                                      3);
         if(status == 0)
        {
        	printk("Write Successful\n");
        }
        else
        {
        	printk("Write not successfull, reason = %d", status);
        }
}
