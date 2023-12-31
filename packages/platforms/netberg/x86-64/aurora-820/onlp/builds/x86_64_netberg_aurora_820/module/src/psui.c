/************************************************************
 * psui.c
 ************************************************************
 *
 ***********************************************************/
#include <stdio.h>
#include <string.h>
#include "platform_lib.h"
#include <onlp/platformi/psui.h>
#include <onlplib/file.h>

#define VALIDATE(_id)                           \
    do {                                        \
        if(!ONLP_OID_IS_PSU(_id)) {             \
            return ONLP_STATUS_E_INVALID;       \
        }                                       \
    } while(0)

typedef enum hwmon_psu_state_e {
    HWMON_PSU_NORMAL = 0,
    HWMON_PSU_UNPOWERED = 2,       //010
    HWMON_PSU_FAULT = 4,           //100
    HWMON_PSU_NOT_INSTALLED = 7    //111
} hwmon_psu_state_t;

#define PSU_CAPS ONLP_PSU_CAPS_VIN|ONLP_PSU_CAPS_VOUT|ONLP_PSU_CAPS_IIN|ONLP_PSU_CAPS_IOUT|ONLP_PSU_CAPS_PIN| ONLP_PSU_CAPS_POUT

char* psu_i2c_addr[ONLP_PSU_MAX]= {
    "",
    "58",
    "59"
};

/*
 * Get all information about the given PSU oid.
 */
#define MAKE_PSU_NODE_INFO(id)		\
    {					\
        {				\
            ONLP_PSU_ID_CREATE(ONLP_PSU_##id), "PSU-"#id, 0,		\
            {								\
                ONLP_THERMAL_ID_CREATE(ONLP_THERMAL_1_ON_PSU##id),	\
                ONLP_THERMAL_ID_CREATE(ONLP_THERMAL_2_ON_PSU##id),	\
                ONLP_THERMAL_ID_CREATE(ONLP_THERMAL_3_ON_PSU##id),  \
                ONLP_FAN_ID_CREATE(ONLP_FAN_PSU_##id)			\
            }				\
        },				\
        "","", 0, PSU_CAPS,			\
    }

static onlp_psu_info_t __onlp_psu_info[ ] = {
    {},
    MAKE_PSU_NODE_INFO(1),
    MAKE_PSU_NODE_INFO(2)
};

int
onlp_psui_init(void)
{
    return ONLP_STATUS_OK;
}


int
onlp_psui_info_get(onlp_oid_t id, onlp_psu_info_t* info)
{
    int ret   = ONLP_STATUS_OK;
    int len;
    int psu_id = ONLP_OID_ID_GET(id);
    uint8_t buf[ONLP_CONFIG_INFO_STR_MAX] = {0};
    char path[ONLP_CONFIG_INFO_STR_MAX];

    VALIDATE(id);

    if(psu_id >= ONLP_PSU_MAX) {
        return ONLP_STATUS_E_INVALID;
    }

    *info = __onlp_psu_info[psu_id];
    ret = onlp_psui_status_get(id, &info->status);
    if(ret != ONLP_STATUS_OK) {
        return ret;
    }

    if(info->status & ONLP_PSU_STATUS_PRESENT) {
        snprintf(path,ONLP_CONFIG_INFO_STR_MAX, NET_PSU_BASE"%d-00%s/",PSU_I2C_CHAN,psu_i2c_addr[psu_id]);

        memset(buf, 0, ONLP_CONFIG_INFO_STR_MAX);
        ret=onlp_file_read( buf, ONLP_CONFIG_INFO_STR_MAX, &len, "%smfr_id", path );
        if(ret==ONLP_STATUS_OK) {
            buf[strlen((char*)buf)-1] = 0;
            snprintf(info->model, ONLP_CONFIG_INFO_STR_MAX, "%s", buf);
        }
        
        memset(buf, 0, ONLP_CONFIG_INFO_STR_MAX);
        ret=onlp_file_read( buf, ONLP_CONFIG_INFO_STR_MAX, &len, "%smfr_serial_number", path );
        if(ret==ONLP_STATUS_OK) {
            buf[strlen((char*)buf)-1] = 0;
            snprintf(info->serial, ONLP_CONFIG_INFO_STR_MAX, "%s", buf);
        }

        if(ret==ONLP_STATUS_OK) {
            ret=onlp_file_read_int( &info->mvin, "%sin1_input", path );
        }

        if(ret==ONLP_STATUS_OK) {
            ret=onlp_file_read_int( &info->mvout, "%sin2_input", path );
        }

        if(ret==ONLP_STATUS_OK) {
            ret=onlp_file_read_int( &info->miin, "%scurr1_input", path );
        }

        if(ret==ONLP_STATUS_OK) {
            ret=onlp_file_read_int( &info->miout, "%scurr2_input", path );
        }

        if(ret==ONLP_STATUS_OK) {
            ret=onlp_file_read_int( &info->mpin, "%spower1_input", path );
        }

        if(ret==ONLP_STATUS_OK) {
            ret=onlp_file_read_int( &info->mpout, "%spower2_input", path );
        }
    }

    return ret;

}


/**
 * @brief Get the PSU's operational status.
 * @param id The PSU OID.
 * @param rv [out] Receives the operational status.
 */
int onlp_psui_status_get(onlp_oid_t id, uint32_t* rv)
{
    int result = ONLP_STATUS_OK;
    hwmon_psu_state_t psu_state;
    int local_id;
    int len;
    char buf[ONLP_CONFIG_INFO_STR_MAX];

    VALIDATE(id);


    local_id = ONLP_OID_ID_GET(id);

    if(local_id >= ONLP_PSU_MAX) {
        result = ONLP_STATUS_E_INVALID;
    } else {
        result = onlp_file_read((uint8_t*)&buf, ONLP_CONFIG_INFO_STR_MAX, &len, "%spsu%d", NET_INFO_PREFIX, local_id);
        if( result != ONLP_STATUS_OK ) {
            return result;
        }
        psu_state = (uint8_t)strtoul(buf, NULL, 0);
        if( psu_state == HWMON_PSU_UNPOWERED) {
            *rv = ONLP_PSU_STATUS_PRESENT|ONLP_PSU_STATUS_UNPLUGGED;
        } else if ( psu_state == HWMON_PSU_NORMAL) {
            *rv = ONLP_PSU_STATUS_PRESENT;
        } else if( psu_state == HWMON_PSU_FAULT) {
            *rv = ONLP_PSU_STATUS_PRESENT|ONLP_PSU_STATUS_FAILED;
        } else if( psu_state == HWMON_PSU_NOT_INSTALLED) {
            *rv = 0;
        } else {
            result = ONLP_STATUS_E_INVALID;
        }
    }
    return result;
}

/**
 * @brief Get the PSU's oid header.
 * @param id The PSU OID.
 * @param rv [out] Receives the header.
 */
int onlp_psui_hdr_get(onlp_oid_t id, onlp_oid_hdr_t* rv)
{
    int result = ONLP_STATUS_OK;
    onlp_psu_info_t* info;
    int psu_id=ONLP_OID_ID_GET(id);

    VALIDATE(id);

    if(psu_id >= ONLP_PSU_MAX) {
        result = ONLP_STATUS_E_INVALID;
    } else {
        info = &__onlp_psu_info[psu_id];
        *rv = info->hdr;
    }
    return result;
}


int
onlp_psui_ioctl(onlp_oid_t pid, va_list vargs)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

