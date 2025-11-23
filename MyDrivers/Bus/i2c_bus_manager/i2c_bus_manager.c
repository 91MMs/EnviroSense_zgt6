#include "i2c_bus_manager.h"

#define LOG_MODULE "I2C_MUTEXID"
#include "log.h"

// 定义私有的互斥锁句柄和定义
static osMutexId i2c_mutex_handle = NULL;
static osMutexDef(i2c_mutex);

bool I2C_Bus_Manager_Init(void)
{
    if (i2c_mutex_handle == NULL) {
        i2c_mutex_handle = osMutexCreate(osMutex(i2c_mutex));
    }
    return (i2c_mutex_handle != NULL);
}

bool I2C_Bus_Lock(uint32_t timeout_ms)
{
    if (i2c_mutex_handle == NULL) {
        return false; // 管理器未初始化
    }

    if (osMutexWait(i2c_mutex_handle, timeout_ms) == osOK) {
        return true;
    }
    
    return false;
}

void I2C_Bus_Unlock(void)
{
    if (i2c_mutex_handle != NULL) {
        osMutexRelease(i2c_mutex_handle);
    }
}
