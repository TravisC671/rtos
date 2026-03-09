
cmake_minimum_required(VERSION 3.17)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
SET(CMAKE_SYSTEM_PROCESSOR microblaze-v) # Can be 'microblaze-v' or similar

# specify cross compilers and tools
set(CMAKE_C_COMPILER riscv32-unknown-elf-gcc)
set(CMAKE_CXX_COMPILER riscv32-unknown-elf-g++)
set(CMAKE_ASM_COMPILER riscv32-unknown-elf-gcc)
set(CMAKE_LD riscv32-unknown-elf-ld)
set(CMAKE_AR riscv32-unknown-elf-ar)
set(CMAKE_OBJCOPY riscv32-unknown-elf-objcopy)
set(CMAKE_OBJDUMP riscv32-unknown-elf-objdump)
set(SIZE riscv32-unknown-elf-size)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Enable compiler-based dependency tracking for cross-compilation.
# Without this, CMake does not detect header changes and skips
# recompilation of affected source files.
set(CMAKE_C_DEPENDS_USE_COMPILER TRUE)
set(CMAKE_CXX_DEPENDS_USE_COMPILER TRUE)

# project settings
get_filename_component(DirName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${DirName} C ASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(PROJECT_BINARY_DIR ${DirName}/bin)

# set the target CPU
# set(MCPU cortex-m3)

SET(CPU_ARCH_FLAGS "-march=rv32imc_zicsr -mabi=ilp32 -mstrict-align")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CPU_ARCH_FLAGS}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CPU_ARCH_FLAGS}")
SET(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${CPU_ARCH_FLAGS}")


# add a custom target to run make clean, then clean up cmake output
add_custom_target(realclean
  COMMAND make clean
  COMMAND rm -rf CMakeCache.txt CMakeFiles Makefile
  COMMENT make clean, then remove everything created by cmake and get ready for another cmake
  )

# add a custom target to run the realclean target, then clean up even
# more
add_custom_target(extra-clean
  COMMAND make realclean
  COMMAND find . -name \\*~ -exec rm \\{\\} \\\;
  COMMAND find . -name \\\#\\* -exec rm \\{\\} \\\;
  COMMENT Make realclean, then find files that emacs leaves around and delete them
  )

# add a custom target to clean up and then create a tarfile, leaving
# out FreeRTOS and CMSIS
add_custom_target(tarfile
  COMMAND make extra-clean
  COMMAND /bin/bash -c "pushd ..\; tar --exclude='CMakeCache.txt' --exclude='CMakeFiles/*' --exclude='CMSIS/*' --exclude='FreeRTOS/*' --exclude='CMSIS' --exclude='FreeRTOS' --exclude='Makefile' -czf ${DirName}.tgz ${DirName}\; popd"
  COMMENT Make realclean, then create a tgz file in the parent directory
  )


#Uncomment for hardware floating point
#add_compile_definitions(ARM_MATH_CM4;ARM_MATH_MATRIX_CHECK;ARM_MATH_ROUNDING)
#add_compile_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)
#add_link_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)

#Uncomment for software floating point
#add_compile_options(-mfloat-abi=soft)
#add_link_options(-mfloat-abi=soft)

# add_compile_options(-mcpu=${MCPU} -mthumb -mthumb-interwork)
add_compile_options(-ffunction-sections -fdata-sections -fno-common -fmessage-length=0)

# uncomment to mitigate c++17 absolute addresses warnings
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-register")

# Choose optimization level.

# Option 1: Optimize for debugging (keeps debug-friendly structure,
# but enables optimizations that don't interfere with debugging)
message(STATUS "Optimize for debugging, with debug info")
add_compile_options(-Og -g)

# Option 2: Debugger may "jump around" in your C code   
#message(STATUS "Maximum optimization for speed, with debug info")
#add_compile_options(-Ofast -g)

# Option 3:
#message(STATUS "Maximum optimization for size, with debug info")
#add_compile_options(-Os -g)

# Option 4:
#     message(STATUS "Maximum optimization for size, no debug info")
# add_compile_options(-Os)

# Option 5: 
#     message(STATUS "Maximum optimization for speed, no debug info")
#add_compile_options(-Ofast)

add_compile_options(-DUSE_FREERTOS)

# List of directories containing header files.
include_directories( 
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/FreeRTOS/include
  ${CMAKE_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V
  ${CMAKE_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V/chip_specific_extensions/RISCV_no_extensions
  ${CMAKE_SOURCE_DIR}/ANSI/drivers
  ${CMAKE_SOURCE_DIR}/ANSI/drivers/FreeRTOS
  ${CMAKE_SOURCE_DIR}/Microblaze_V
  # ${CMAKE_SOURCE_DIR}/PDCurses
  # ${CMAKE_SOURCE_DIR}/PDCurses/common
  # ${CMAKE_SOURCE_DIR}/PDCurses/ANSI/drivers
  # ${CMAKE_SOURCE_DIR}/PDCurses/ANSI/drivers/FreeRTOS
  # ${CMAKE_SOURCE_DIR}/ninvaders
  # ${CMAKE_SOURCE_DIR}/theme
)

# list of FreeRTOS sources that we need
file(GLOB FREERTOS_SOURCES
   "${CMAKE_SOURCE_DIR}/FreeRTOS/tasks.c"
   "${CMAKE_SOURCE_DIR}/FreeRTOS/list.c"
   "${CMAKE_SOURCE_DIR}/FreeRTOS/queue.c"
   "${CMAKE_SOURCE_DIR}/FreeRTOS/mutex.c"
   "${CMAKE_SOURCE_DIR}/FreeRTOS/timers.c"
   "${CMAKE_SOURCE_DIR}/FreeRTOS/stream_buffer.c"
   "${CMAKE_SOURCE_DIR}/FreeRTOS/event_groups.c"
   "${CMAKE_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V/port.c"
   "${CMAKE_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V/portASM.S"
   #"${CMAKE_SOURCE_DIR}/FreeRTOS/portable/MemMang/heap_3.c"
# add more as needed
)  

# Sources needed to build libPDCurses.a
#file(GLOB PDCURSES_SOURCES
  # "${CMAKE_SOURCE_DIR}/PDCurses/pdcurses/*.c"
  # "${CMAKE_SOURCE_DIR}/PDCurses/ANSI/*.c"
  # "${CMAKE_SOURCE_DIR}/PDCurses/ANSI/drivers/*.c"
  # "${CMAKE_SOURCE_DIR}/PDCurses/ANSI/drivers/FreeRTOS/*.c"
#  )

# Sources for ninvaders
#file(GLOB NINVADERS_SOURCES
#  "${CMAKE_SOURCE_DIR}/ninvaders/*.c"
#)

# Sources for the executable
file(GLOB SOURCES
  "${CMAKE_SOURCE_DIR}/src/main.c"
  "${CMAKE_SOURCE_DIR}/src/ks.c"
  "${CMAKE_SOURCE_DIR}/src/songs.c"
  "${CMAKE_SOURCE_DIR}/src/music.c"
  "${CMAKE_SOURCE_DIR}/src/LDP-001_PM_driver.c"
  "${CMAKE_SOURCE_DIR}/src/audio_backend.c"
  "${CMAKE_SOURCE_DIR}/src/stats_task.c"
  "${CMAKE_SOURCE_DIR}/src/AXI_timer.c"
  "${CMAKE_SOURCE_DIR}/src/newlib_functions.c"
  "${CMAKE_SOURCE_DIR}/src/heap_useNewlib.c"
  "${CMAKE_SOURCE_DIR}/ANSI/drivers/ANSI_terminal.c"
  "${CMAKE_SOURCE_DIR}/ANSI/drivers/FreeRTOS/driver_table.c"
  "${CMAKE_SOURCE_DIR}/ANSI/drivers/FreeRTOS/UART_16550.c"
  "${CMAKE_SOURCE_DIR}/Microblaze_V/interrupts.c"
  "${CMAKE_SOURCE_DIR}/Microblaze_V/trap_handler.S"
  "${CMAKE_SOURCE_DIR}/Microblaze_V/startup_RISCV.S"
#  "${CMAKE_SOURCE_DIR}/src/startup_ARMCM3.S"
#  "${CMAKE_SOURCE_DIR}/src/hello_task.c"
#  "${CMAKE_SOURCE_DIR}/src/PM_test_task.c"
#  "${CMAKE_SOURCE_DIR}/src/firework_task.c"
#  "${CMAKE_SOURCE_DIR}/src/testcurs_task.c"
#  "${CMAKE_SOURCE_DIR}/src/heap_useNewlib.c"
#  "${CMAKE_SOURCE_DIR}/src/sincos.c"
#  "${CMAKE_SOURCE_DIR}/sound_effects/sound_effects.c"
#  "${CMAKE_SOURCE_DIR}/sound_effects/sounds.c"
#  "${CMAKE_SOURCE_DIR}/theme/theme.c"
)

set(ALL_SOURCES "${SOURCES}" "${PDCURSES_SOURCES}" "${FREERTOS_SOURCES}" "${NINVADERS_SOURCES}")


# Define our linker script
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/src/gcc_RISCV.ld)

# Additional options for the GNU linker
add_link_options(-Wl,-wrap=malloc -Wl,-wrap=_malloc_r)
add_link_options(-Wl,-gc-sections,--print-memory-usage)
add_link_options(-Wl,-Map=bin/${PROJECT_NAME}.map)
add_link_options(-T ${LINKER_SCRIPT} --specs=nosys.specs -nostartfiles)

# Define our target executable, and everything that it depends on
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
add_executable(${PROJECT_NAME}.elf ${ALL_SOURCES} ${LINKER_SCRIPT})

target_link_libraries(${PROJECT_NAME}.elf m)

set(ELF_FILE ${PROJECT_NAME}.elf)
set(HEX_FILE ${PROJECT_NAME}.hex)
set(BIN_FILE ${PROJECT_NAME}.bin)
set(MAP_FILE ${PROJECT_NAME}.map)
set(DIS_FILE ${PROJECT_NAME}.dis)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMENT "Creating ${HEX_FILE}"
        COMMAND ${CMAKE_OBJCOPY} -Oihex bin/${ELF_FILE} bin/${HEX_FILE}
	)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
	COMMENT "Creating ${BIN_FILE}"
        COMMAND ${CMAKE_OBJCOPY} -Obinary bin/${ELF_FILE} bin/${BIN_FILE}
	)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
	COMMENT "Creating ${DIS_FILE}"
	COMMAND ${CMAKE_OBJDUMP} -dS bin/${ELF_FILE} > bin/${DIS_FILE}
	)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
	COMMENT "Memory Report"
	COMMAND scripts/report_sections.sh ${CMAKE_SOURCE_DIR}/bin/${MAP_FILE}
 	)

set_property(
  TARGET ${PROJECT_NAME}.elf
  APPEND
  PROPERTY ADDITIONAL_CLEAN_FILES "${CMAKE_SOURCE_DIR}/bin/${HEX_FILE};${CMAKE_SOURCE_DIR}/bin/${BIN_FILE};${CMAKE_SOURCE_DIR}/bin/${MAP_FILE}")


set(CMAKE_SKIP_INSTALL_RULES True)
