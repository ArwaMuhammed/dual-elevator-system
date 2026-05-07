set(INCLUDE_LIST ${INCLUDE_LIST}
        ${ARM_DIR}/arm-none-eabi/include
        ${PROJECT_PATH}/STM32-base/startup
        ${PROJECT_PATH}/STM32-base-STM32Cube/CMSIS/ARM/inc
        ${PROJECT_PATH}/STM32-base-STM32Cube/CMSIS/${SERIES_FOLDER}/inc
        ${PROJECT_PATH}/include
        ${PROJECT_PATH}/Lib
        ${PROJECT_PATH}/App_Common
        ${PROJECT_PATH}/Master_App
        ${PROJECT_PATH}/Slave_App
        ${PROJECT_PATH}/Drivers/Exti
        ${PROJECT_PATH}/Drivers/Gpio
        ${PROJECT_PATH}/Drivers/Nvic
        ${PROJECT_PATH}/Drivers/Pwm
        ${PROJECT_PATH}/Drivers/Rcc
        ${PROJECT_PATH}/Drivers/Spi
        ${PROJECT_PATH}/Drivers/Timer
        ${PROJECT_PATH}/Drivers/Usart
)

if (USE_HAL)
    set(INCLUDE_LIST ${INCLUDE_LIST} ${PROJECT_PATH}/STM32-base-STM32Cube/HAL/${SERIES_FOLDER}/inc)
endif ()
