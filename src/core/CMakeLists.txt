# Common usage requirements; native implementation is supplied by the platform.
add_library(UnboundCoreHeaders INTERFACE)
target_include_directories(UnboundCoreHeaders INTERFACE
    "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../platform")
target_link_libraries(UnboundCoreHeaders INTERFACE Qt5::Widgets)

# All production core translation units are listed here, once. Compile them in
# the consumer so each helper/fixture retains its own definitions and runtime.
function(unbound_link_core target backend)
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/controller.cpp")
    target_link_libraries(${target} PRIVATE ${backend})
endfunction()
