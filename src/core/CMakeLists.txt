# Common usage requirements; native implementation is supplied by the platform.
add_library(UnboundCoreHeaders INTERFACE)
option(UNBOUND_PALETTE_SIZE_DIAGNOSTICS "Record public Qt palette size measurements (internal investigation only)" OFF)
if(UNBOUND_PALETTE_SIZE_DIAGNOSTICS)
    target_compile_definitions(UnboundCoreHeaders INTERFACE UNBOUND_PALETTE_SIZE_DIAGNOSTICS)
endif()
option(UNBOUND_LAYOUT_TRACE "Bounded layout recall tracing (internal investigation only)" OFF)
option(UNBOUND_LAYOUT_COVER_EXPERIMENT "Layout transition experiment (Windows capture; no presentation on macOS)" OFF)
if(UNBOUND_LAYOUT_COVER_EXPERIMENT)
    set(UNBOUND_LAYOUT_TRACE ON)
    target_compile_definitions(UnboundCoreHeaders INTERFACE UNBOUND_LAYOUT_COVER_EXPERIMENT)
endif()
if(UNBOUND_LAYOUT_TRACE)
    target_compile_definitions(UnboundCoreHeaders INTERFACE UNBOUND_LAYOUT_TRACE)
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
