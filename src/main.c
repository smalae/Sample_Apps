/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/zephyr.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdio.h>
#include "em_eusart.h"
#include "em_cmu.h"

#define SPI_DRV_NAME	DT_NODELABEL(eusart1)
#define BUF_SIZE 36
#define BUF2_SIZE 17
#define TOTAL_TX_COUNT	2
#define TOTAL_RX_COUNT	2

uint8_t buffer_tx[] = "Thequickbrownfoxjumpsoverthelazydog";
uint8_t buffer_rx[BUF_SIZE] = {0};

uint8_t buffer2_tx[] = "0123456789abcdef\0";
uint8_t buffer2_rx[BUF2_SIZE] = {};


struct spi_gecko_config {
	EUSART_TypeDef *base;
	CMU_Clock_TypeDef clock;
	struct soc_gpio_pin pin_rx;
	struct soc_gpio_pin pin_tx;
	struct soc_gpio_pin pin_clk;
	struct soc_gpio_pin pin_cs;
};

struct spi_config spi_cfg = {
	.frequency = 1000000,
	.operation = SPI_OP_MODE_MASTER | SPI_MODE_CPOL | SPI_MODE_LOOP |
	SPI_MODE_CPHA | SPI_WORD_SET(8) | SPI_LINES_SINGLE,
	.slave = 0,
	.cs = NULL,
};


void main(void)
{
	const struct device *spi;
	struct spi_buf tx_bufs[2];

	const struct spi_buf_set tx = {
		.buffers = tx_bufs,
		.count = TOTAL_TX_COUNT
	};	
	
	tx_bufs[0].buf = buffer_tx;
	tx_bufs[0].len = BUF_SIZE;

	tx_bufs[1].buf = buffer2_tx;
	tx_bufs[1].len = BUF2_SIZE;

	struct spi_buf rx_bufs[2];	
	const struct spi_buf_set rx = {
		.buffers = rx_bufs,
		.count = TOTAL_RX_COUNT
	};
	rx_bufs[0].buf = buffer_rx;
	rx_bufs[0].len = BUF_SIZE;

	rx_bufs[1].buf = buffer2_rx;
	rx_bufs[1].len = BUF2_SIZE;
	
	spi = DEVICE_DT_GET(SPI_DRV_NAME);
	if (!spi) {
		//LOG_ERR("Cannot find %s!\n", SPI_DRV_NAME);
		//zassert_not_null(spi, "Invalid SPI device");
		return;
	}

	spi_transceive(spi, &spi_cfg, &tx, &rx);
	
	printk("tx buffer is\n");
	for(int i = 0;i < BUF_SIZE; i++)
	{
		printk("%c ", buffer_tx[i]);
	}
	printk("\n");
	printk("rx buffer is\n");
	for(int i = 0;i < BUF_SIZE; i++)
	{
		printk("%c ", buffer_rx[i]);
	}
	printk("\n");
	
	printk("tx2 buffer is\n");
	for(int i = 0;i < BUF2_SIZE; i++)
	{
		printk("%c ", buffer2_tx[i]);
	}
	printk("\n");
	printk("rx2 buffer is\n");
	for(int i = 0;i < BUF2_SIZE; i++)
	{
		printk("%c ", buffer2_rx[i]);
	}
	printk("\n");
	
	while(1);
}
