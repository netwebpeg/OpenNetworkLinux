/************************************************************
 * <bsn.cl fy=2014 v=onl>
 *
 *           Copyright 2014 Big Switch Networks, Inc.
 *           Copyright 2017 Accton Technology Corporation.
 *
 * Licensed under the Eclipse Public License, Version 1.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *        http://www.eclipse.org/legal/epl-v10.html
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the
 * License.
 *
 * </bsn.cl>
 ************************************************************
 *
 *
 *
 ***********************************************************/
#include <onlp/platformi/sfpi.h>
#include <onlplib/i2c.h>
#include <onlplib/file.h>
#include "x86_64_accton_as9926_24db_int.h"
#include "x86_64_accton_as9926_24db_log.h"

#define PORT_BUS_INDEX(port) (port+9)

#define PORT_EEPROM_FORMAT \
	"/sys/bus/i2c/devices/%d-0050/eeprom"
#define MODULE_PRESENT_FORMAT \
	"/sys/devices/platform/as9926_24db_sfp/module_present_%d"
#define MODULE_RESET_FORMAT \
	"/sys/devices/platform/as9926_24db_sfp/module_reset_%d"
#define MODULE_RXLOS_FORMAT \
	"/sys/devices/platform/as9926_24db_sfp/module_rx_los_%d"
#define MODULE_TXFAULT_FORMAT \
	"/sys/devices/platform/as9926_24db_sfp/module_tx_fault_%d"
#define MODULE_TXDISABLE_FORMAT \
	"/sys/devices/platform/as9926_24db_sfp/module_tx_disable_%d"
#define MODULE_PRESENT_ALL_ATTR \
	"/sys/devices/platform/as9926_24db_sfp/module_present_all"
#define MODULE_RXLOS_ALL_ATTR \
	"/sys/devices/platform/as9926_24db_sfp/module_rxlos_all"

/************************************************************
 *
 * SFPI Entry Points
 *
 ***********************************************************/

int onlp_sfpi_init(void)
{
	/* Called at initialization time */
	return ONLP_STATUS_OK;
}

int onlp_sfpi_bitmap_get(onlp_sfp_bitmap_t* bmap)
{
	/*
	 * Ports {0, 26}
	 */
	int p;

	for(p = 0; p < 26; p++) {
		AIM_BITMAP_SET(bmap, p);
	}

	return ONLP_STATUS_OK;
}

int onlp_sfpi_is_present(int port)
{
	/*
	 * Return 1 if present.
	 * Return 0 if not present.
	 * Return < 0 if error.
	 */
	int present;

	if (onlp_file_read_int(&present, MODULE_PRESENT_FORMAT, 
			       (port+1)) < 0) {
		AIM_LOG_ERROR("Unable to read present status from port(%d)\r\n",
			      port);
		return ONLP_STATUS_E_INTERNAL;
	}

	return present;
}

int onlp_sfpi_presence_bitmap_get(onlp_sfp_bitmap_t* dst)
{
	uint32_t bytes[4];
	FILE* fp;

	/* Read present status of port 0~25 */
	int count = 0;

	fp = fopen(MODULE_PRESENT_ALL_ATTR, "r");
	if(fp == NULL) {
		AIM_LOG_ERROR("Unable to open the module_present_all \
			      device file from (%s).", 
			      MODULE_PRESENT_ALL_ATTR);
		return ONLP_STATUS_E_INTERNAL;
	}

	count = fscanf(fp, "%x %x %x %x", bytes+0, bytes+1, bytes+2, 
		       bytes+3);
	
	fclose(fp);

	if(count != 4) {
		/* Likely a CPLD read timeout. */
		AIM_LOG_ERROR("Unable to read all fields the \
			       module_present_all device file from(%s).", 
			       MODULE_PRESENT_ALL_ATTR);
		return ONLP_STATUS_E_INTERNAL;
	}

	/* Mask out non-existant SFP ports */
	bytes[3] &= 0x3;

	/* Convert to 64 bit integer in port order */
	int i = 0;
	uint64_t presence_all = 0 ;
	AIM_BITMAP_CLR_ALL(dst);
	
	for(i = AIM_ARRAYSIZE(bytes)-1; i >= 0; i--) {
		presence_all <<= 8;
		presence_all |= bytes[i];
	}

	/* Populate bitmap */
	for(i = 0; presence_all; i++) {
		AIM_BITMAP_MOD(dst, i, (presence_all & 1));
		presence_all >>= 1;
	}

	return ONLP_STATUS_OK;
}

int onlp_sfpi_rx_los_bitmap_get(onlp_sfp_bitmap_t* dst)
{
	uint32_t bytes[4];
	FILE* fp;

	/* Read present status of port 0~25 */
	int count = 0;


	bytes[0] = bytes[1] = bytes[2] = bytes[3] = 0;
	fp = fopen(MODULE_RXLOS_ALL_ATTR, "r");
    
	if(fp == NULL) {
		AIM_LOG_ERROR("Unable to open the module_rxlos_all \
			      device file from (%s).", 
			      MODULE_RXLOS_ALL_ATTR);

		return ONLP_STATUS_E_INTERNAL;
	}

	count = fscanf(fp, "%x", bytes+3);
    
	fclose(fp);

	if(count != 1) {
		/* Likely a CPLD read timeout. */
		AIM_LOG_ERROR("Unable to read all fields the module_rxlos_all \
			      device file from(%s).", MODULE_RXLOS_ALL_ATTR);
	
		return ONLP_STATUS_E_INTERNAL;
	}

	/* Mask out non-existant SFP ports */
	bytes[3] &= 0x3;

	/* Convert to 64 bit integer in port order */
	int i = 0;
	uint64_t rx_los_all = 0 ;
	AIM_BITMAP_CLR_ALL(dst);

	for(i = AIM_ARRAYSIZE(bytes)-1; i >= 0; i--) {
		rx_los_all <<= 8;
		rx_los_all |= bytes[i];
	}

	/* Populate bitmap */
	for(i = 0; rx_los_all; i++) {
		AIM_BITMAP_MOD(dst, i, (rx_los_all & 1));
		rx_los_all >>= 1;
	}

	return ONLP_STATUS_OK;
}

int onlp_sfpi_eeprom_read(int port, uint8_t data[256])
{
	/*
	 * Read the SFP eeprom into data[]
	 *
	 * Return MISSING if SFP is missing.
	 * Return OK if eeprom is read
	 */
    
	int size = 0;
	memset(data, 0, 256);

	if(onlp_file_read(data, 256, &size, PORT_EEPROM_FORMAT, 
			  PORT_BUS_INDEX(port)) != ONLP_STATUS_OK) {
		AIM_LOG_ERROR("Unable to read eeprom from port(%d)\r\n", port);
		return ONLP_STATUS_E_INTERNAL;
	}

	if (size != 256) {
		AIM_LOG_ERROR("Unable to read eeprom from port(%d), size is \
			      different!\r\n", port);
		return ONLP_STATUS_E_INTERNAL;
	}

	return ONLP_STATUS_OK;
}

int onlp_sfpi_dom_read(int port, uint8_t data[256])
{
	FILE* fp;
	char file[64] = {0};
    
	sprintf(file, PORT_EEPROM_FORMAT, PORT_BUS_INDEX(port));
	fp = fopen(file, "r");
	if(fp == NULL) {
		AIM_LOG_ERROR("Unable to open the eeprom device file of \
			      port(%d)", port);
		return ONLP_STATUS_E_INTERNAL;
	}

	if (fseek(fp, 256, SEEK_CUR) != 0) {
		fclose(fp);
		AIM_LOG_ERROR("Unable to set the file position indicator of \
			      port(%d)", port);
		return ONLP_STATUS_E_INTERNAL;
	}

	int ret = fread(data, 1, 256, fp);
	fclose(fp);
	if (ret != 256) {
		AIM_LOG_ERROR("Unable to read the module_eeprom device file of \
			      port(%d)", port);
		return ONLP_STATUS_E_INTERNAL;
	}

	return ONLP_STATUS_OK;
}

int onlp_sfpi_control_set(int port, onlp_sfp_control_t control, int value)
{
	int rv;

	switch(control)
	{
	case ONLP_SFP_CONTROL_TX_DISABLE:
	{
		if (onlp_file_write_int(value, MODULE_TXDISABLE_FORMAT, 
					(port+1)) < 0) {
			AIM_LOG_ERROR("Unable to set tx_disable status to \
				      port(%d)\r\n", port);
			rv = ONLP_STATUS_E_INTERNAL;
		}
		else {
			rv = ONLP_STATUS_OK;
		}
		break;
	}

	case ONLP_SFP_CONTROL_RESET:
	{
		if(port <= 23)
		{
			if (onlp_file_write_int(value, MODULE_RESET_FORMAT, 
						(port+1)) < 0) {
				AIM_LOG_ERROR("Unable to set reset to \
					      port(%d)\r\n", port);
				rv = ONLP_STATUS_E_INTERNAL;
			}
			else {
				rv = ONLP_STATUS_OK;
			}
		}
		else {
			return ONLP_STATUS_E_UNSUPPORTED;
		}
		break;
	}

	default:
		rv = ONLP_STATUS_E_UNSUPPORTED;
		break;
	}

	return rv;
}

int onlp_sfpi_dev_readb(int port, uint8_t devaddr, uint8_t addr)
{
	int bus = PORT_BUS_INDEX(port);
	return onlp_i2c_readb(bus, devaddr, addr, ONLP_I2C_F_FORCE);
}

int
onlp_sfpi_dev_writeb(int port, uint8_t devaddr, uint8_t addr, uint8_t value)
{
	int bus = PORT_BUS_INDEX(port);
	return onlp_i2c_writeb(bus, devaddr, addr, value, ONLP_I2C_F_FORCE);
}

int onlp_sfpi_dev_readw(int port, uint8_t devaddr, uint8_t addr)
{
	int bus = PORT_BUS_INDEX(port);
	return onlp_i2c_readw(bus, devaddr, addr, ONLP_I2C_F_FORCE);
}

int
onlp_sfpi_dev_writew(int port, uint8_t devaddr, uint8_t addr, uint16_t value)
{
	int bus = PORT_BUS_INDEX(port);
	return onlp_i2c_writew(bus, devaddr, addr, value, ONLP_I2C_F_FORCE);
}

int onlp_sfpi_control_get(int port, onlp_sfp_control_t control, int* value)
{
	int rv;

	if (port < 0) {
		return ONLP_STATUS_E_UNSUPPORTED;
	}

	switch(control)
	{
	case ONLP_SFP_CONTROL_RESET:
	{
		if(port <= 23)
		{
			if(onlp_file_read_int(value, MODULE_RESET_FORMAT,
				            (port+1)) < 0) {
				AIM_LOG_ERROR("Unable to read reset status \
					      from port(%d)\r\n", port);
				rv = ONLP_STATUS_E_INTERNAL;
			}
			else {
				rv = ONLP_STATUS_OK;
			}
		}
		else {
			return ONLP_STATUS_E_UNSUPPORTED;
		}
		break;

	}
	case ONLP_SFP_CONTROL_RX_LOS:
	{
		if (port <= 23)
			return ONLP_STATUS_E_UNSUPPORTED;
		
		if (onlp_file_read_int(value, MODULE_RXLOS_FORMAT, 
				       (port+1)) < 0) {
			AIM_LOG_ERROR("Unable to read rx_loss status \
				      from port(%d)\r\n", port);
			rv = ONLP_STATUS_E_INTERNAL;
		}
		else {
			rv = ONLP_STATUS_OK;
		}
		break;
	}

	case ONLP_SFP_CONTROL_TX_FAULT:
	{
		if (port <= 23)
			return ONLP_STATUS_E_UNSUPPORTED;
		
		if (onlp_file_read_int(value, MODULE_TXFAULT_FORMAT,
				       (port+1)) < 0) {
			AIM_LOG_ERROR("Unable to read tx_fault status \
				      from port(%d)\r\n", port);
			rv = ONLP_STATUS_E_INTERNAL;
		}
		else {
			rv = ONLP_STATUS_OK;
		}
		break;
	}

	case ONLP_SFP_CONTROL_TX_DISABLE:
	{
		if(port <= 23)
			return ONLP_STATUS_E_UNSUPPORTED;
		
		if (onlp_file_read_int(value, MODULE_TXDISABLE_FORMAT, 
				       (port+1)) < 0) {
			AIM_LOG_ERROR("Unable to read tx_disabled status \
				      from port(%d)\r\n", port);
			rv = ONLP_STATUS_E_INTERNAL;
		}
		else {
			rv = ONLP_STATUS_OK;
		}
		break;
	}

	default:
		rv = ONLP_STATUS_E_UNSUPPORTED;
	}

	return rv;
}

int onlp_sfpi_denit(void)
{
	return ONLP_STATUS_OK;
}
