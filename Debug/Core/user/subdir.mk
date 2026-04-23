################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/user/access.c \
../Core/user/log.c \
../Core/user/pcf8563.c \
../Core/user/timApp.c \
../Core/user/usart.c \
../Core/user/user.c \
../Core/user/w25q128.c \
../Core/user/wiegand.c 

OBJS += \
./Core/user/access.o \
./Core/user/log.o \
./Core/user/pcf8563.o \
./Core/user/timApp.o \
./Core/user/usart.o \
./Core/user/user.o \
./Core/user/w25q128.o \
./Core/user/wiegand.o 

C_DEPS += \
./Core/user/access.d \
./Core/user/log.d \
./Core/user/pcf8563.d \
./Core/user/timApp.d \
./Core/user/usart.d \
./Core/user/user.d \
./Core/user/w25q128.d \
./Core/user/wiegand.d 


# Each subdirectory must supply rules for building sources it contributes
Core/user/%.o Core/user/%.su Core/user/%.cyclo: ../Core/user/%.c Core/user/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G0B1xx -c -I../Core/Inc -I../Core/ethernet -I../Core/ethernet/W5500 -I../Core/user -I../Drivers/STM32G0xx_HAL_Driver/Inc -I../Drivers/STM32G0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G0xx/Include -I../Drivers/CMSIS/Include -Os -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-user

clean-Core-2f-user:
	-$(RM) ./Core/user/access.cyclo ./Core/user/access.d ./Core/user/access.o ./Core/user/access.su ./Core/user/log.cyclo ./Core/user/log.d ./Core/user/log.o ./Core/user/log.su ./Core/user/pcf8563.cyclo ./Core/user/pcf8563.d ./Core/user/pcf8563.o ./Core/user/pcf8563.su ./Core/user/timApp.cyclo ./Core/user/timApp.d ./Core/user/timApp.o ./Core/user/timApp.su ./Core/user/usart.cyclo ./Core/user/usart.d ./Core/user/usart.o ./Core/user/usart.su ./Core/user/user.cyclo ./Core/user/user.d ./Core/user/user.o ./Core/user/user.su ./Core/user/w25q128.cyclo ./Core/user/w25q128.d ./Core/user/w25q128.o ./Core/user/w25q128.su ./Core/user/wiegand.cyclo ./Core/user/wiegand.d ./Core/user/wiegand.o ./Core/user/wiegand.su

.PHONY: clean-Core-2f-user

