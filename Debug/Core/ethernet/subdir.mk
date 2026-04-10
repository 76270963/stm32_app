################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/ethernet/net_app.c \
../Core/ethernet/net_services.c \
../Core/ethernet/socket.c \
../Core/ethernet/w5500_hal.c \
../Core/ethernet/wizchip_conf.c 

OBJS += \
./Core/ethernet/net_app.o \
./Core/ethernet/net_services.o \
./Core/ethernet/socket.o \
./Core/ethernet/w5500_hal.o \
./Core/ethernet/wizchip_conf.o 

C_DEPS += \
./Core/ethernet/net_app.d \
./Core/ethernet/net_services.d \
./Core/ethernet/socket.d \
./Core/ethernet/w5500_hal.d \
./Core/ethernet/wizchip_conf.d 


# Each subdirectory must supply rules for building sources it contributes
Core/ethernet/%.o Core/ethernet/%.su Core/ethernet/%.cyclo: ../Core/ethernet/%.c Core/ethernet/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m0plus -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G0B1xx -c -I../Core/Inc -I../Core/ethernet -I../Core/ethernet/W5500 -I../Core/user -I../Drivers/STM32G0xx_HAL_Driver/Inc -I../Drivers/STM32G0xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32G0xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-ethernet

clean-Core-2f-ethernet:
	-$(RM) ./Core/ethernet/net_app.cyclo ./Core/ethernet/net_app.d ./Core/ethernet/net_app.o ./Core/ethernet/net_app.su ./Core/ethernet/net_services.cyclo ./Core/ethernet/net_services.d ./Core/ethernet/net_services.o ./Core/ethernet/net_services.su ./Core/ethernet/socket.cyclo ./Core/ethernet/socket.d ./Core/ethernet/socket.o ./Core/ethernet/socket.su ./Core/ethernet/w5500_hal.cyclo ./Core/ethernet/w5500_hal.d ./Core/ethernet/w5500_hal.o ./Core/ethernet/w5500_hal.su ./Core/ethernet/wizchip_conf.cyclo ./Core/ethernet/wizchip_conf.d ./Core/ethernet/wizchip_conf.o ./Core/ethernet/wizchip_conf.su

.PHONY: clean-Core-2f-ethernet

