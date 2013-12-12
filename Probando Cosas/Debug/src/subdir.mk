################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/Probando\ Cosas.c \
../src/select.c 

OBJS += \
./src/Probando\ Cosas.o \
./src/select.o 

C_DEPS += \
./src/Probando\ Cosas.d \
./src/select.d 


# Each subdirectory must supply rules for building sources it contributes
src/Probando\ Cosas.o: ../src/Probando\ Cosas.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"src/Probando Cosas.d" -MT"src/Probando\ Cosas.d" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


