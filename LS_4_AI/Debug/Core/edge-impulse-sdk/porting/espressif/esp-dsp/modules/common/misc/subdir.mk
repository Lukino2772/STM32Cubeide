################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Core/edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/misc/dsps_pwroftwo.cpp 

OBJS += \
./Core/edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/misc/dsps_pwroftwo.o 

CPP_DEPS += \
./Core/edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/misc/dsps_pwroftwo.d 


# Each subdirectory must supply rules for building sources it contributes
Core/edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/misc/%.o Core/edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/misc/%.su Core/edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/misc/%.cyclo: ../Core/edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/misc/%.cpp Core/edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/misc/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F411xE -c -I../Core/Inc -I"C:/Users/Lukino/STM32CubeIDE/workspace_1.18.0/LS_4_AI/Core" -I../Core -I../Core/edge-impulse-sdk -I../Core/model-parameters -I../Core/tflite-model -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-edge-2d-impulse-2d-sdk-2f-porting-2f-espressif-2f-esp-2d-dsp-2f-modules-2f-common-2f-misc

clean-Core-2f-edge-2d-impulse-2d-sdk-2f-porting-2f-espressif-2f-esp-2d-dsp-2f-modules-2f-common-2f-misc:
	-$(RM) ./Core/edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/misc/dsps_pwroftwo.cyclo ./Core/edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/misc/dsps_pwroftwo.d ./Core/edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/misc/dsps_pwroftwo.o ./Core/edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/misc/dsps_pwroftwo.su

.PHONY: clean-Core-2f-edge-2d-impulse-2d-sdk-2f-porting-2f-espressif-2f-esp-2d-dsp-2f-modules-2f-common-2f-misc

