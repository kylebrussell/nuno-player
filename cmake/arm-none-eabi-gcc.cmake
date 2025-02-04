set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

# Toolchain settings
set(CMAKE_C_COMPILER    arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER  arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER  arm-none-eabi-gcc)
set(CMAKE_AR            arm-none-eabi-ar)
set(CMAKE_OBJCOPY      arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP      arm-none-eabi-objdump)
set(SIZE                arm-none-eabi-size)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Processor specific
set(CPU_FLAGS "-mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16")
set(CMAKE_C_FLAGS "${CPU_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CPU_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_ASM_FLAGS "${CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CPU_FLAGS}")
