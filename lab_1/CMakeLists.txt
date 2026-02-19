
cmake_minimum_required(VERSION 3.17)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)

# specify cross compilers and tools
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_LD arm-none-eabi-ld)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(SIZE arm-none-eabi-size)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# project settings
get_filename_component(DirName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${DirName} C ASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)
set(PROJECT_BINARY_DIR ${DirName}/bin)

# set the target CPU
set(MCPU cortex-m3)

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
  COMMAND pushd ..\; tar --exclude='CMakeCache.txt' --exclude='CMakeFiles/*' --exclude='CMSIS/*' --exclude='FreeRTOS/*' --exclude='CMSIS' --exclude='FreeRTOS' --exclude='Makefile' -czf ${DirName}.tgz ${DirName}\; popd
  COMMENT Make realclean, then create a tgz file in the parent directory
  )


#Uncomment for hardware floating point
#add_compile_definitions(ARM_MATH_CM4;ARM_MATH_MATRIX_CHECK;ARM_MATH_ROUNDING)
#add_compile_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)
#add_link_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)

#Uncomment for software floating point
#add_compile_options(-mfloat-abi=soft)

add_compile_options(-mcpu=${MCPU} -mthumb -mthumb-interwork)
add_compile_options(-ffunction-sections -fdata-sections -fno-common -fmessage-length=0)

# uncomment to mitigate c++17 absolute addresses warnings
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-register")

# Choose optimization level.

# Option 1: Debugger will follow your C code
message(STATUS "Minimal optimization, include debug info")
add_compile_options(-g)

# Option 2: Debugger may "jump around" in your C code   
#     message(STATUS "Maximum optimization for speed, include debug info")
#     add_compile_options(-Ofast -g)

# Option 3:
#     message(STATUS "Maximum optimization for size, no debug info")
#     add_compile_options(-Os)

# Option 4: 
#     message(STATUS "Maximum optimization for speed, no debug info")
#     add_compile_options(-Ofast)

# List of directories containing header files.
include_directories( 
  ${CMAKE_SOURCE_DIR}/include
)
  
# Sources for the executable
file(GLOB SOURCES
  "${CMAKE_SOURCE_DIR}/src/main.c"
  "${CMAKE_SOURCE_DIR}/src/uart.c"
  "${CMAKE_SOURCE_DIR}/src/startup_ARMCM3.S"
)

# Define our linker script
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/src/gcc_arm.ld)

# Additional options for the GNU linker
add_link_options(-Wl,-gc-sections,--print-memory-usage)
#  add_link_options(-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)
add_link_options(-mcpu=${MCPU} -mthumb -mthumb-interwork)
add_link_options(-T ${LINKER_SCRIPT} --specs=nosys.specs -nostartfiles)

# Define our target executable, and everything that it depends on
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)
add_executable(${PROJECT_NAME}.elf ${SOURCES} ${LINKER_SCRIPT})

set(ELF_FILE ${PROJECT_NAME}.elf)
set(HEX_FILE ${PROJECT_NAME}.hex)
set(BIN_FILE ${PROJECT_NAME}.bin)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -Oihex bin/${ELF_FILE} bin/${HEX_FILE}
        COMMAND ${CMAKE_OBJCOPY} -Obinary bin/${ELF_FILE} bin/${BIN_FILE}
        COMMENT "Building ${HEX_FILE} Building ${BIN_FILE}")
      
set_property(
  TARGET ${PROJECT_NAME}.elf
  APPEND
  PROPERTY ADDITIONAL_CLEAN_FILES ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map ${HEX_FILE} ${BIN_FILE}
  )


set(CMAKE_SKIP_INSTALL_RULES True)
