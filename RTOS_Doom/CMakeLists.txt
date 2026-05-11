cmake_minimum_required(VERSION 3.17)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR RISCV_pyeatt)

# --- Toolchain Configuration ---
# Pointing to the 64-bit multilib compiler found in Arch 'extra' repo
set(TOOLCHAIN_PREFIX riscv64-unknown-elf)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_LD           ${TOOLCHAIN_PREFIX}-ld)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}-ar)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}-objcopy)
set(CMAKE_OBJDUMP      ${TOOLCHAIN_PREFIX}-objdump)
set(SIZE              ${TOOLCHAIN_PREFIX}-size)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# project settings
get_filename_component(DirName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${DirName} C ASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

# --- RISC-V Architecture Flags ---
# These flags tell the 64-bit compiler to generate 32-bit code
set(CPU_ARCH_FLAGS "-march=rv32im_zicsr_zifencei;-mabi=ilp32")

# 2. Use them in compile options
# CMake will automatically strip the semicolons and pass them as separate flags
add_compile_options(${CPU_ARCH_FLAGS})

# 3. For the manual SET(CMAKE_C_FLAGS...) lines, use the list variable carefully
# We use the list variable without quotes here so CMake expands it into separate words
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=rv32im_zicsr_zifencei -mabi=ilp32")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=rv32im_zicsr_zifencei -mabi=ilp32")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -march=rv32im_zicsr_zifencei -mabi=ilp32")

# --- Clean and Maintenance Targets ---
add_custom_target(realclean
  COMMAND make clean
  COMMAND rm -rf CMakeCache.txt CMakeFiles Makefile
  COMMENT "Cleaning build artifacts..."
)

add_custom_target(extra-clean
  COMMAND make realclean
  COMMAND find . -name "*~" -exec rm {} +
  COMMAND find . -name "#*" -exec rm {} +
  COMMENT "Deep cleaning emacs/temp files..."
)

add_custom_target(tarfile
  COMMAND make extra-clean
  COMMAND tar --exclude='CMakeCache.txt' --exclude='CMakeFiles/*' --exclude='CMSIS/*' --exclude='FreeRTOS/*' -czf ../${DirName}.tgz .
  COMMENT "Creating tgz file in parent directory"
)

# --- Compilation Options ---
add_compile_options(-ffunction-sections -fdata-sections -fno-common -fmessage-length=0)
add_compile_options(-g) # Debug info
add_compile_options(-DUSE_FREERTOS)

# Header Includes
include_directories( 
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/RISCV_pyeatt
  ${CMAKE_SOURCE_DIR}/FreeRTOS/include
  ${CMAKE_SOURCE_DIR}/src/doomgeneric
  ${CMAKE_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V
  ${CMAKE_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V/chip_specific_extensions/RISCV_no_extensions
  ${CMAKE_SOURCE_DIR}/FreeRTOS-Plus-Fat/include
)

# Source File Discovery
file(GLOB FREERTOS_SOURCES
   "${CMAKE_SOURCE_DIR}/FreeRTOS/*.c"
   "${CMAKE_SOURCE_DIR}/FreeRTOS/portable/MemMang/heap_3.c"
   "${CMAKE_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V/port.c"
   "${CMAKE_SOURCE_DIR}/FreeRTOS/portable/GCC/RISC-V/portASM.S"
)  

file(GLOB FAT_SOURCES
    "${CMAKE_SOURCE_DIR}/FreeRTOS-Plus-Fat/ff_crc.c"
    "${CMAKE_SOURCE_DIR}/FreeRTOS-Plus-Fat/ff_dir.c"
    "${CMAKE_SOURCE_DIR}/FreeRTOS-Plus-Fat/ff_error.c"
    "${CMAKE_SOURCE_DIR}/FreeRTOS-Plus-Fat/ff_fat.c"
    "${CMAKE_SOURCE_DIR}/FreeRTOS-Plus-Fat/ff_file.c"
    "${CMAKE_SOURCE_DIR}/FreeRTOS-Plus-Fat/ff_format.c"
    "${CMAKE_SOURCE_DIR}/FreeRTOS-Plus-Fat/ff_ioman.c"
    "${CMAKE_SOURCE_DIR}/FreeRTOS-Plus-Fat/ff_locking.c"
    "${CMAKE_SOURCE_DIR}/FreeRTOS-Plus-Fat/ff_memory.c"
    "${CMAKE_SOURCE_DIR}/FreeRTOS-Plus-Fat/ff_stdio.c"
    "${CMAKE_SOURCE_DIR}/FreeRTOS-Plus-Fat/ff_string.c"
    "${CMAKE_SOURCE_DIR}/FreeRTOS-Plus-Fat/ff_sys.c"
    "${CMAKE_SOURCE_DIR}/FreeRTOS-Plus-Fat/ff_time.c"
)

file(GLOB SOURCES
  "${CMAKE_SOURCE_DIR}/src/doomgeneric/*.c"
  "${CMAKE_SOURCE_DIR}/src/*.c"
  "${CMAKE_SOURCE_DIR}/src/sd_driver/*.c"
  "${CMAKE_SOURCE_DIR}/RISCV_pyeatt/*.c"
  "${CMAKE_SOURCE_DIR}/RISCV_pyeatt/*.S"
)

set(ALL_SOURCES ${SOURCES} ${FREERTOS_SOURCES} ${FAT_SOURCES})

# --- Linker Settings ---
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/src/gcc_RISCV.ld)

add_link_options(-Wl,-gc-sections,--print-memory-usage)
add_link_options(-Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map)
add_link_options(-T ${LINKER_SCRIPT} --specs=nosys.specs -nostartfiles)

# --- Output ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_executable(${PROJECT_NAME}.elf ${ALL_SOURCES})
target_link_libraries(${PROJECT_NAME}.elf m)

# Post-Build Steps (Hex, Bin, Disassembly)
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}.hex
    COMMENT "Creating Hex file"
)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}.bin
    COMMENT "Creating Binary file"
)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJDUMP} -dS $<TARGET_FILE:${PROJECT_NAME}.elf> > ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}.dis
    COMMENT "Creating Disassembly file"
)