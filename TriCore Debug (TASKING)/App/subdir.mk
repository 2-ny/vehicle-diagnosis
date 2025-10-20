################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../App/App_AEB.c 

COMPILED_SRCS += \
App/App_AEB.src 

C_DEPS += \
App/App_AEB.d 

OBJS += \
App/App_AEB.o 


# Each subdirectory must supply rules for building sources it contributes
App/App_AEB.src: ../App/App_AEB.c App/subdir.mk
	cctc -cs --misrac-version=2004 -D__CPU__=tc37x "-fC:/NGV/TC375LK_NGV_2/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
App/App_AEB.o: App/App_AEB.src App/subdir.mk
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-App

clean-App:
	-$(RM) App/App_AEB.d App/App_AEB.o App/App_AEB.src

.PHONY: clean-App

