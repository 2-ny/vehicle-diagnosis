################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Modules/Headlight.c 

COMPILED_SRCS += \
Modules/Headlight.src 

C_DEPS += \
Modules/Headlight.d 

OBJS += \
Modules/Headlight.o 


# Each subdirectory must supply rules for building sources it contributes
Modules/Headlight.src: ../Modules/Headlight.c Modules/subdir.mk
	cctc -cs --misrac-version=2004 -D__CPU__=tc37x "-fC:/NGV/TC375LK_NGV_2/TriCore Debug (TASKING)/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -Wc-g3 -Wc-w544 -Wc-w557 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
Modules/Headlight.o: Modules/Headlight.src Modules/subdir.mk
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-Modules

clean-Modules:
	-$(RM) Modules/Headlight.d Modules/Headlight.o Modules/Headlight.src

.PHONY: clean-Modules

