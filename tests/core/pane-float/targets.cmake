# Shared behavior fixtures; native adapters are chosen by the platform build.
function(unbound_add_pane_tests backend adapter)
    add_executable(pane-float-smoke "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/main.cpp" "${adapter}")
    target_include_directories(pane-float-smoke PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    target_link_libraries(pane-float-smoke PRIVATE ${backend})
endfunction()
function(unbound_add_audition_tests backend)
    add_executable(audition-activation-test "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/auditionactivationtest.cpp")
    target_link_libraries(audition-activation-test PRIVATE ${backend})
endfunction()
function(unbound_add_layout_tests backend adapter)
    add_executable(UnboundLayoutTests "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/layouttest.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/cataloguetest.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/colorclipstest.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/colorviewertest.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/colorcontroltest.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/pagefocustest.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/windowgrouptest.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/dockingguardtest.cpp"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/indexheaderdragtest.cpp" "${adapter}")
    target_include_directories(UnboundLayoutTests PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    unbound_link_core(UnboundLayoutTests ${backend})
endfunction()
