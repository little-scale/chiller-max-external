cmake_minimum_required(VERSION 3.19)

project(chiller~)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../source/max-sdk-base/script/max-pretarget.cmake)

include_directories( 
	"${MAX_SDK_INCLUDES}"
	"${MAX_SDK_MSP_INCLUDES}"
	"${MAX_SDK_JIT_INCLUDES}"
)

file(GLOB PROJECT_SRC "*.h" "*.c" "*.cpp")
add_library(${PROJECT_NAME} MODULE ${PROJECT_SRC})

include(${CMAKE_CURRENT_SOURCE_DIR}/../../source/max-sdk-base/script/max-posttarget.cmake)