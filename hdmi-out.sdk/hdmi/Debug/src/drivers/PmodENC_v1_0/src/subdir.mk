################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/drivers/PmodENC_v1_0/src/PmodENC.c 

OBJS += \
./src/drivers/PmodENC_v1_0/src/PmodENC.o 

C_DEPS += \
./src/drivers/PmodENC_v1_0/src/PmodENC.d 


# Each subdirectory must supply rules for building sources it contributes
src/drivers/PmodENC_v1_0/src/%.o: ../src/drivers/PmodENC_v1_0/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM v7 gcc compiler'
	arm-none-eabi-gcc -Wall -O0 -g3 -c -fmessage-length=0 -MT"$@" -mcpu=cortex-a9 -mfpu=vfpv3 -mfloat-abi=hard -I../../hdmi_bsp/ps7_cortexa9_0/include -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


