cmake_minimum_required(VERSION 3.22)
project(hcscoin-vulkan-miner LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_package(glslangValidator QUIET)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(vulkan_miner
    vulkan_miner.cpp
)

target_include_directories(vulkan_miner PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/../src
)

target_link_libraries(vulkan_miner PRIVATE
    Vulkan::Vulkan
)

# Compile GLSL shaders to SPIR-V at build time
set(SHADER_SOURCES
    shaders/sha256d.comp
    shaders/quantum_gates.comp
)

if(glslangValidator_FOUND)
    foreach(shader ${SHADER_SOURCES})
        get_filename_component(shader_name ${shader} NAME_WE)
        set(spv_output ${CMAKE_CURRENT_BINARY_DIR}/${shader_name}.spv)
        add_custom_command(
            OUTPUT ${spv_output}
            COMMAND glslangValidator -V ${shader} -o ${spv_output}
            DEPENDS ${shader}
            COMMENT "Compiling ${shader} → ${spv_output}"
        )
        target_sources(vulkan_miner PRIVATE ${spv_output})
    endforeach()
else()
    message(WARNING "glslangValidator not found — shaders must be pre-compiled to SPIR-V")
endif()

# Copy SPIR-V binaries to build directory
file(COPY shaders/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/shaders)
