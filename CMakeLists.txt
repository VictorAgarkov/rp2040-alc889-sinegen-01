cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH "~/src/pico/pico-sdk")
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

set(PROJECT_NAME "rp2040-alc889-sinegen-01")

project(${PROJECT_NAME} C CXX ASM)

pico_sdk_init()

add_executable(${PROJECT_NAME}
	src/main.c
	src/rb32.c
	#src/rb8.c
	src/hda_codec.c
	src/hda_link_rp2040.c
	src/data_pack.c
	src/get_sine_approx_1025_0_q1.c  # быстрый синус
	#src/get_sine_approx_4097_0_q1.c  # быстрый синус
)

# Подключение заголовочных файлов
target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(${PROJECT_NAME} PROPERTIES 
	OUTPUT_NAME "../build/${PROJECT_NAME}"
)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/src/hda.pio)

# Линковка с Pico SDK
target_link_libraries(${PROJECT_NAME} 
	pico_stdlib 
	cmsis_core 
	pico_multicore
	hardware_pio
	hardware_dma
)

pico_add_extra_outputs(${PROJECT_NAME})

# выводим сколько ресурсов МК использовано
target_link_options(${PROJECT_NAME} PRIVATE "LINKER:--print-memory-usage")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND arm-none-eabi-size $<TARGET_FILE:${PROJECT_NAME}>
	COMMENT "MCU memory usage:"
)

