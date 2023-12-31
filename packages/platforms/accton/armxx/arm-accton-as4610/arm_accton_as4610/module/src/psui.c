/************************************************************
 * <bsn.cl fy=2014 v=onl>
 *
 *           Copyright 2014, 2015 Big Switch Networks, Inc.
 *           Copyright 2016 Accton Technology Corporation.
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
#include <onlplib/file.h>
#include <onlp/platformi/psui.h>
#include <onlplib/mmap.h>
#include <stdio.h>
#include <string.h>
#include "platform_lib.h"
#include "arm_accton_as4610_int.h"

#define PSU_STATUS_PRESENT    1
#define PSU_STATUS_POWER_GOOD 1

#define PSU_NODE_MAX_INT_LEN  8
#define PSU_NODE_MAX_PATH_LEN 64

#define VALIDATE(_id)                           \
    do {                                        \
        if(!ONLP_OID_IS_PSU(_id)) {             \
            return ONLP_STATUS_E_INVALID;       \
        }                                       \
    } while(0)

static int 
psu_status_info_get(int id, char *node, int *value)
{
    int ret = 0;
    char buf[PSU_NODE_MAX_INT_LEN + 1] = {0};
    char node_path[PSU_NODE_MAX_PATH_LEN] = {0};
    
    *value = 0;

    if (PSU1_ID == id) {
        sprintf(node_path, "%s%s", PSU1_AC_EEPROM_PREFIX, node);
    }
    else if (PSU2_ID == id) {
        sprintf(node_path, "%s%s", PSU2_AC_EEPROM_PREFIX, node);
    }
    
    ret = deviceNodeReadString(node_path, buf, sizeof(buf), 0);

    if (ret == 0) {
        *value = atoi(buf);
    }

    return ret;
}

int
onlp_psui_init(void)
{
    return ONLP_STATUS_OK;
}

/*
 * Get all information about the given PSU oid.
 */
static onlp_psu_info_t pinfo[] =
{
    { }, /* Not used */
    {
        { ONLP_PSU_ID_CREATE(PSU1_ID), "PSU-1", 0 },
    },
    {
        { ONLP_PSU_ID_CREATE(PSU2_ID), "PSU-2", 0 },
    }
};

static int
psu_pmbus_info_get(int id, char *node, int *value)
{
    int  ret = 0;
    *value = 0;
    char *path[] = { PSU1_AC_PMBUS_PREFIX, PSU2_AC_PMBUS_PREFIX };

    ret = onlp_file_read_int(value, "%s%s", path[id-1], node);
    if (ret < 0) {
        return ONLP_STATUS_E_INTERNAL;
    }

    return ret;
}

static int
psu_caps_get(int pid, onlp_psu_info_t* info)
{
    int val   = 0;

    /* Read voltage, current and power */
    if (psu_pmbus_info_get(pid, "psu_v_out", &val) == 0) {
        info->mvout = val;
        info->caps |= ONLP_PSU_CAPS_VOUT;
    }

    if (psu_pmbus_info_get(pid, "psu_i_out", &val) == 0) {
        info->miout = val;
        info->caps |= ONLP_PSU_CAPS_IOUT;
    }

    if (psu_pmbus_info_get(pid, "psu_p_out", &val) == 0) {
        info->mpout = val;
        info->caps |= ONLP_PSU_CAPS_POUT;
    }

    return ONLP_STATUS_OK;
}

int
onlp_psui_info_get(onlp_oid_t id, onlp_psu_info_t* info)
{
    int val   = 0;
    int ret   = ONLP_STATUS_OK;
    int index = ONLP_OID_ID_GET(id);
    psu_type_t psu_type; 

    VALIDATE(id);

    memset(info, 0, sizeof(onlp_psu_info_t));
    *info = pinfo[index]; /* Set the onlp_oid_hdr_t */

    /* Get the present state */
    if (psu_status_info_get(index, "psu_present", &val) != 0) {
        printf("Unable to read PSU(%d) node(psu_present)\r\n", index);
    }

    if (val != PSU_STATUS_PRESENT) {
        info->status &= ~ONLP_PSU_STATUS_PRESENT;
        return ONLP_STATUS_OK;
    }
    info->status |= ONLP_PSU_STATUS_PRESENT;


    /* Get power good status */
    if (psu_status_info_get(index, "psu_power_good", &val) != 0) {
        printf("Unable to read PSU(%d) node(psu_power_good)\r\n", index);
    }

    if (val != PSU_STATUS_POWER_GOOD) {
        info->status |=  ONLP_PSU_STATUS_FAILED;
    }

    /* Get PSU type
     */
    psu_type = get_psu_type(index, info->model, sizeof(info->model));
    get_psu_serial(index, info->serial, sizeof(info->serial));

    switch (psu_type) {
        case PSU_TYPE_AC_F2B:
        case PSU_TYPE_AC_B2F:
            info->caps = ONLP_PSU_CAPS_AC;
            psu_caps_get(index, info);
            ret = ONLP_STATUS_OK;
            break;
        case PSU_TYPE_UNKNOWN:  /* User insert a unknown PSU or unplugged.*/
            info->status |= ONLP_PSU_STATUS_UNPLUGGED;
            info->status &= ~ONLP_PSU_STATUS_FAILED;
            ret = ONLP_STATUS_OK;
            break;
        default:
            ret = ONLP_STATUS_E_UNSUPPORTED;
            break;
    }

    return ret;
}

int
onlp_psui_ioctl(onlp_oid_t pid, va_list vargs)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

