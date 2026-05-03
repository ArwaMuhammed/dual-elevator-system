/**
 * Gpio.c
 *
 *  Created on: 4/15/2025
 *  Author    : AbdallahDarwish
 */


#include <Std_Types.h>
#include "Gpio.h"
#include "Gpio_Private.h"
#include "Bit_Operations.h"

uint32 addressMap[4] = {GPIOA_BASE_ADDR, GPIOB_BASE_ADDR, GPIOC_BASE_ADDR, GPIOD_BASE_ADDR};

void Gpio_Init(uint8 PortName, uint8 PinNumber, uint8 PinMode, uint8 DefaultState) {

    uint8 addressIndex = PortName - GPIO_A;

    GpioType* gpioDevice = (GpioType*) addressMap[addressIndex];

    gpioDevice->GPIO_MODER  &= ~(0x03 << (PinNumber * 2));
    gpioDevice->GPIO_MODER |= (PinMode << (PinNumber * 2));

    if (PinMode == GPIO_INPUT) {
        gpioDevice->GPIO_PUPDR &= ~(0x03 << (PinNumber * 2));
        gpioDevice->GPIO_PUPDR |= (DefaultState << (PinNumber * 2));
    } else {
        gpioDevice->GPIO_OTYPER &= ~(0x1 << PinNumber);
        gpioDevice->GPIO_OTYPER |= (DefaultState << (PinNumber));
    }
}

uint8 Gpio_WritePin(uint8 PortName, uint8 PinNumber, uint8 Data) {
    uint8 status = NOK;
    uint8 addressIndex = PortName - GPIO_A;
    GpioType* gpioDevice = (GpioType*) addressMap[addressIndex];
    uint8 pinMode = (gpioDevice->GPIO_MODER & (0x03 << (PinNumber * 2))) >> (PinNumber * 2);
    if (pinMode == GPIO_OUTPUT || pinMode == GPIO_AF) {
        gpioDevice->GPIO_ODR &= ~(0x1U << PinNumber);
        gpioDevice->GPIO_ODR  |= (Data << PinNumber);
        status = OK;
    }
    return status;
}

uint8 Gpio_ReadPin(uint8 PortName, uint8 PinNum) {
    uint8 data = 0;
    uint8 addressIndex = PortName - GPIO_A;
    GpioType* gpioDevice = (GpioType*) addressMap[addressIndex];

    data = (gpioDevice->GPIO_IDR & (0x1 << PinNum)) >> PinNum;

    return data;
}
