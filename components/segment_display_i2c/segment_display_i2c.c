/*
 * segment_display_i2c.c
 *
 *  Created on: Aug 24, 2022
 *      Author: chand
 */

#include <stdio.h>
#include "argtable3/argtable3.h"
#include "driver/i2c.h"
#include "hal/i2c_hal.h"
#include "soc/i2c_periph.h"
#include "esp_console.h"
#include "esp_log.h"
#include "segment_display_i2c.h"

static const char *TAG = "SEG-DISP";

#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define WRITE_BIT I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ    /*!< I2C master read */
#define ACK_CHECK_EN 0x1            /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                 /*!< I2C ack value */
#define NACK_VAL 0x1                /*!< I2C nack value */

#define I2C_SMS12130B_ADDR	0x38

#define I2C_MASTER_SCL_IO           21      /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           17      /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              0                          /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          100000                     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

static esp_err_t i2c_master_init(void)
{
	int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
    };

    i2c_param_config(i2c_master_port, &conf);

    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

esp_err_t i2c_master_write_to_device(i2c_port_t i2c_num, uint8_t device_address,
                                     uint8_t* write_buffer, size_t write_size,
                                     TickType_t ticks_to_wait)
{
	esp_err_t err = ESP_OK;

	i2c_cmd_handle_t handle = i2c_cmd_link_create();
	err = i2c_master_start(handle);
    if (err != ESP_OK) {
        goto end;
    }
	i2c_master_write_byte(handle, device_address << 1 | WRITE_BIT, ACK_CHECK_EN);
	if (err != ESP_OK) {
	        goto end;
	}

	err = i2c_master_write(handle, write_buffer, write_size, ACK_CHECK_EN);
	if (err != ESP_OK) {
		goto end;
	}

	i2c_master_stop(handle);
	err = i2c_master_cmd_begin(i2c_num, handle, ticks_to_wait);

	end:
		i2c_cmd_link_delete(handle);
	    return err;
}

/**
 * @brief Write a byte to a SMS12130B sensor register
 */
static esp_err_t SMS12130B_write(uint8_t reg_addr, uint8_t data)
{
	esp_err_t err;
    uint8_t write_buf[2] = {reg_addr, data};

    err = i2c_master_write_to_device(I2C_MASTER_NUM, I2C_SMS12130B_ADDR, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    return err;
}

static esp_err_t SMS12130B_displayReset(void)
{
	esp_err_t err;

	err = SMS12130B_write(0xE0,0x48);
	err = SMS12130B_write(0xE0,0x70);

	return err;
}

struct disp_buf
{
	uint8_t SADDR;
	uint8_t ICADDR;
	uint8_t UCDATA;
};

static esp_err_t SMS12130B_fill(uint8_t data)
{
	uint8_t i;
	esp_err_t err;
	struct disp_buf write_buf;

	for(i=0; i<32; i+=2)
	{
		write_buf.SADDR = 0xE0;
		write_buf.ICADDR = i;
		write_buf.UCDATA = data;
		err = i2c_master_write_to_device(I2C_MASTER_NUM, I2C_SMS12130B_ADDR, (uint8_t *)&write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
	}

	return err;
}

static esp_err_t SMS12130B_speedbar(void)
{
	uint8_t i,j,ptr=0;
	esp_err_t err;
	struct disp_buf write_buf;
	uint8_t speedbar[9*16] = {0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
							0x0E, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
							0x0E, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
							0x0E, 0xFF, 0X20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
							0x0E, 0xFF, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
							0x0E, 0xFF, 0xFD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
							0x0E, 0xFF, 0xFF, 0X50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
							0x0E, 0xFF, 0xFF, 0XF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
							0x0E, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	for(j=0;j<9;j++)
	{
		for(i=0; i<32; i+=2)
		{
			write_buf.SADDR = 0xE0;
			write_buf.ICADDR = i;
			write_buf.UCDATA = speedbar[ptr];
			err = i2c_master_write_to_device(I2C_MASTER_NUM, I2C_SMS12130B_ADDR, (uint8_t *)&write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
			ptr++;
		}
		vTaskDelay(5);
	}

	return err;
}

bool Vbat_low = true;

static esp_err_t SMS12130B_speed(uint8_t digit2, uint8_t digit1, uint8_t digit0)
{
	uint8_t i,ptr=0;
	esp_err_t err;
	struct disp_buf write_buf;
	uint8_t speeddisp[16] = {digit2, 0x00, 0x00, 0x00, (0x06 | 0x00 | Vbat_low), (0x01 | digit0), (digit2 | digit1), 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x01};

	for(i=0; i<32; i+=2)
	{
		write_buf.SADDR = 0xE0;
		write_buf.ICADDR = i;
		write_buf.UCDATA = speeddisp[ptr];
		err = i2c_master_write_to_device(I2C_MASTER_NUM, I2C_SMS12130B_ADDR, (uint8_t *)&write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
		ptr++;
	}

	vTaskDelay(25);
	return err;
}

static uint8_t convert_display(uint8_t data)
{
	switch(data)
	{
		case 0:
			return 0xFA;
		case 1:
			return 0x60;
		case 2:
			return 0xBC;
		case 3:
			return 0xF4;
		case 4:
			return 0x66;
		case 5:
			return 0xD6;
		case 6:
			return 0xDE;
		case 7:
			return 0x70;
		case 8:
			return 0xFE;
		case 9:
			return 0xF6;
		default:
			return 0x00;
	}

}

static void SMS12130B_speed_disp_conv(uint8_t speed)
{
	uint32_t remainder_1, remainder_2, quotient;
	uint8_t digit0 = 0x00, digit1 = 0x00, digit2 = 0x00;

	if(speed < 10)
	{
		remainder_1 = (speed % 100);
		digit0 = convert_display(remainder_1);
	}

	else if(speed >= 10 && speed < 100)
	{
		remainder_1 = (speed % 10);
		quotient = (speed / 10);
		remainder_2 = (quotient % 10);
		digit0 = convert_display(remainder_1);
		digit1 = convert_display(remainder_2);
	}

	else if(speed >= 100)
	{
		remainder_1 = (speed % 10);
		quotient = (speed / 10);
		remainder_2 = (quotient % 10);
		digit0 = convert_display(remainder_1);
		digit1 = convert_display(remainder_2);
		digit2 = 0x01;
	}

	SMS12130B_speed(digit2, digit1, digit0);
}

void lcd_init(void)
{
	ESP_ERROR_CHECK(i2c_master_init());
	ESP_LOGI(TAG, "I2C initialized successfully");

	SMS12130B_displayReset();
	ESP_LOGI(TAG, "SMS12130B display Reset successfully");

	SMS12130B_fill(0xff);
	vTaskDelay(100);

	SMS12130B_speedbar();
	vTaskDelay(5);
	SMS12130B_fill(0x00);
	vTaskDelay(5);
	uint8_t i;
	while(1)
	{
		for(i=0;i<200;i++)
		{
			SMS12130B_speed_disp_conv(i);
			Vbat_low = !Vbat_low;
		}
	}
}
