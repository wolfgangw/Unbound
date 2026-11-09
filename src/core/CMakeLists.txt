# Common usage requirements; native implementation is supplied by the platform.
add_library(UnboundCoreHeaders INTERFACE)
option(UNBOUND_PALETTE_SIZE_DIAGNOSTICS "Record public Qt palette size measurements (internal investigation only)" OFF)
if(UNBOUND_PALETTE_SIZE_DIAGNOSTICS)
    target_compile_definitions(UnboundCoreHeaders INTERFACE UNBOUND_PALETTE_SIZE_DIAGNOSTICS)
endif()
# Windows ships capture-backed transitions; macOS keeps native presentation.
if(WIN32)
    target_compile_definitions(UnboundCoreHeaders INTERFACE UNBOUND_LAYOUT_TRANSITIONS)
endif()
target_include_directories(UnboundCoreHeaders INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../platform")
target_link_libraries(UnboundCoreHeaders INTERFACE Qt5::Widgets)

# All production core translation units are listed here, once. Compile them in
# the consumer so each helper/fixture retains its own definitions and runtime.
function(unbound_link_core target backend)
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/controller.cpp")
    target_link_libraries(${target} PRIVATE ${backend})
endfunction()

# One command catalogue drives native Free dispatch and installed Studio scripts.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../scripts/commands.json"
    "${CMAKE_CURRENT_LIST_DIR}/../../infrastructure/scripts/generate.py")
execute_process(COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_LIST_DIR}/../../infrastructure/scripts/generate.py"
    --catalogue-header "${CMAKE_CURRENT_BINARY_DIR}/scriptcommands.generated.h"
    COMMAND_ERROR_IS_FATAL ANY)
target_include_directories(UnboundCoreHeaders INTERFACE "${CMAKE_CURRENT_BINARY_DIR}")
