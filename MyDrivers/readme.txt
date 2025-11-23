MyDriver
|-- Common                 # 通用工具 (基础、低依赖)
|   |-- delay
|   |   |-- mydelay.c
|   |   `-- mydelay.h
|-- Bus                    # 总线驱动/管理 (硬件抽象, 软件实现, 总线辅助)
|   |-- i2c_bus_manager    # I2C总线互斥锁 (配合HAL I2C使用, 如传感器任务)
|   |   |-- i2c_bus_manager.c
|   |   `-- i2c_bus_manager.h
|   |-- sw_i2c_eeprom      # 软件I2C (用于EEPROM, 原myiic)
|   |   |-- myiic.c
|   |   `-- myiic.h
|   `-- sw_i2c_touch       # 软件I2C (用于电容触摸屏, 原ctiic)
|       |-- ctiic.c
|       `-- ctiic.h
|-- Peripherals            # 外设驱动与控制
|   |-- eeprom_24cxx       # EEPROM 驱动 (依赖 sw_iic_eeprom)
|   |   |-- 24cxx.c
|   |   `-- 24cxx.h
|   |-- lcd                # LCD 显示驱动 (依赖 FSMC/GPIO HAL)
|   |   |-- lcd.c
|   |   |-- lcd.h
|   |   |-- lcd_ex.c       # (建议编译为独立单元, 而非include)
|   |   `-- lcdfont.h      # (只保留 extern 声明)
|   |-- sensors            # 传感器相关 (驱动 + 适配层, 依赖 I2C/ADC HAL)
|   |   |-- gy30
|   |   |   |-- gy30.c
|   |   |   `-- gy30.h
|   |   |-- mq2
|   |   |   |-- mq2.c
|   |   |   `-- mq2.h
|   |   |-- sht30
|   |   |   |-- sht30.c
|   |   |   `-- sht30.h
|   |   |-- gy30_sensor.c
|   |   |-- gy30_sensor.h
|   |   |-- mq2_sensor.c
|   |   |-- mq2_sensor.h
|   |   |-- sht30_sensor.c
|   |   `-- sht30_sensor.h
|   |-- touch              # 触摸驱动 (依赖 sw_i2c_touch / GPIO HAL)
|   |   |-- touch.c        # 触摸抽象层 (电阻/电容判断和接口)
|   |   |-- touch.h
|   |   |-- ft5206         # FT5206 电容触摸IC驱动
|   |   |   |-- ft5206.c
|   |   |   `-- ft5206.h
|   |   `-- gt9xxx         # GT9xxx 电容触摸IC驱动
|   |       |-- gt9xxx.c
|   |       `-- gt9xxx.h
|   `-- output_devices            # 输出设备控制 (依赖 TIM HAL)
|       |-- buzzer
|       |   |-- buzzer.c
|       |   `-- buzzer.h
|       `-- rgbled
|           |-- rgbled.c
|           `-- rgbled.h
|-- Services               # 系统服务/中间件
|   |-- log                # 日志系统 (依赖 printf_redirect)
|   |   |-- log.c
|   |   `-- log.h
|   |-- printf_redirect    # printf 重定向 (依赖 UART HAL, 可选 DMA/RTOS)
|   |   |-- printf_redirect.c
|   |   `-- printf_redirect.h
|   `-- sensor_manager     # 传感器任务管理 (依赖 RTOS, i2c_bus_manager, sensors)
|       |-- sensor_task.c
|       `-- sensor_task.h