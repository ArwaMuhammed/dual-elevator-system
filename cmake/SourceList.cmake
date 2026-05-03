file(GLOB_RECURSE ALL_SRC_FILES ${PROJECT_PATH}/*.c)

if (NOT USE_HAL)

    list(FILTER ALL_SRC_FILES EXCLUDE REGEX ".*/(.*(build|STM32-base).*)/.*")

else ()

    list(FILTER ALL_SRC_FILES EXCLUDE REGEX ".*/(.*(build).*)|STM32-base/.*")

    set(EXCLUDED_FILES
            "${PROJECT_PATH}/STM32-base-STM32Cube/HAL/${SERIES_FOLDER}/src/stm32f4xx_hal_timebase_tim_template.c"
            "${PROJECT_PATH}/STM32-base-STM32Cube/HAL/${SERIES_FOLDER}/src/stm32f4xx_hal_timebase_rtc_wakeup_template.c"
            "${PROJECT_PATH}/STM32-base-STM32Cube/HAL/${SERIES_FOLDER}/src/stm32f4xx_hal_timebase_rtc_alarm_template.c"
            "${PROJECT_PATH}/STM32-base-STM32Cube/HAL/${SERIES_FOLDER}/src/stm32f4xx_hal_msp_template.c"
    )

    list(REMOVE_ITEM ALL_SRC_FILES ${EXCLUDED_FILES})

endif ()

list(APPEND ALL_SRC_FILES
        ${PROJECT_PATH}/STM32-base-STM32Cube/CMSIS/${SERIES_FOLDER}/src/system_${SERIES_FOLDER}.c
)

set(MASTER_SOURCE_LIST ${ALL_SRC_FILES})
set(SLAVE_SOURCE_LIST  ${ALL_SRC_FILES})

list(REMOVE_ITEM MASTER_SOURCE_LIST
        ${PROJECT_PATH}/Slave_App/main.c
        ${PROJECT_PATH}/src/main.c
)

list(REMOVE_ITEM SLAVE_SOURCE_LIST
        ${PROJECT_PATH}/Master_App/main.c
        ${PROJECT_PATH}/src/main.c
)