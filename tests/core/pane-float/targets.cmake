# Shared behavior fixtures; native adapters are chosen by the platform build.
function(unbound_add_pane_tests backend adapter)
    add_executable(pane-float-smoke "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/main.cpp" "${adapter}")
    target_include_directories(pane-float-smoke PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    target_link_libraries(pane-float-smoke PRIVATE ${backend})
endfunction()
function(unbound_add_preset_tests backend adapter)
    add_executable(UnboundPresetTests "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/presettest.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/cataloguetest.cpp" "${adapter}")
    target_include_directories(UnboundPresetTests PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    unbound_link_core(UnboundPresetTests ${backend})
endfunction()
