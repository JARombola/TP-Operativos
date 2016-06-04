################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Funciones/json.c \
../Funciones/sockets.c 

OBJS += \
./Funciones/json.o \
./Funciones/sockets.o 

C_DEPS += \
./Funciones/json.d \
./Funciones/sockets.d 


# Each subdirectory must supply rules for building sources it contributes
Funciones/%.o: ../Funciones/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


