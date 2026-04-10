################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/ethernet/W5500/w5500.c 

OBJS += \
./Core/ethernet/W5500/w5500.o 

C_DEPS += \
./Core/ethernet/W5500/w5500.d 


# Each subdirectory must supply rules for building sources it contributes
Core/ethernet/W5500/%.o Core/ethernet/W5500/%.su Core/ethernet/W5500/%.cyclo: ../Core/ethernet/W5500/%.c Core/ethernet/W5500/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G0B1xx -c -I../Core/Inc -I../Core/ethernet -I../Core/ethernet/W5500 -I../Core/user -I../Drivers/STM32G0xx_HAL_Driver/Inc -I../Drivers/STM32G0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G0xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-ethernet-2f-W5500

clean-Core-2f-ethernet-2f-W5500:
	-$(RM) ./Core/ethernet/W5500/w5500.cyclo ./Core/ethernet/W5500/w5500.d ./Core/ethernet/W5500/w5500.o ./Core/ethernet/W5500/w5500.su

.PHONY: clean-Core-2f-ethernet-2f-W5500

