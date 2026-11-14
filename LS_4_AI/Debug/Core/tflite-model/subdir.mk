################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Core/tflite-model/tflite_learn_1156345_3_compiled.cpp 

OBJS += \
./Core/tflite-model/tflite_learn_1156345_3_compiled.o 

CPP_DEPS += \
./Core/tflite-model/tflite_learn_1156345_3_compiled.d 


# Each subdirectory must supply rules for building sources it contributes
Core/tflite-model/%.o Core/tflite-model/%.su Core/tflite-model/%.cyclo: ../Core/tflite-model/%.cpp Core/tflite-model/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++14 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F411xE -c -I../Core/Inc -I"C:/Users/Lukino/STM32CubeIDE/workspace_1.18.0/LS_4_AI/Core" -I../Core -I../Core/edge-impulse-sdk -I../Core/model-parameters -I../Core/tflite-model -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -O0 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-tflite-2d-model

clean-Core-2f-tflite-2d-model:
	-$(RM) ./Core/tflite-model/tflite_learn_1156345_3_compiled.cyclo ./Core/tflite-model/tflite_learn_1156345_3_compiled.d ./Core/tflite-model/tflite_learn_1156345_3_compiled.o ./Core/tflite-model/tflite_learn_1156345_3_compiled.su

.PHONY: clean-Core-2f-tflite-2d-model

