set(_lua_output "${CMAKE_BINARY_DIR}/stage/runtime")
add_library(UnboundLuaLoader SHARED "${CMAKE_CURRENT_LIST_DIR}/loader.mm")
target_include_directories(UnboundLuaLoader PRIVATE "${UNBOUND_ROOT}/src/platforms/macos")
target_link_libraries(UnboundLuaLoader PRIVATE "-framework Foundation")
target_compile_definitions(UnboundLuaLoader PRIVATE UNBOUND_STARTUP_LOG_NAME="lua-bootstrap.log")
set_target_properties(UnboundLuaLoader PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY "${_lua_output}")
add_library(Unbound SHARED "${UNBOUND_ROOT}/src/platforms/macos/bootstrap.mm" "${CMAKE_CURRENT_LIST_DIR}/helperentry.cpp")
unbound_link_core(Unbound UnboundMacWindowing)
target_compile_definitions(Unbound PRIVATE UNBOUND_STARTUP_LOG_NAME="lua-bootstrap.log")
set_target_properties(Unbound PROPERTIES PREFIX "" LIBRARY_OUTPUT_DIRECTORY "${_lua_output}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/load.lua" "${_lua_output}/load.lua" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/startup/Unbound.scriptlib"
    "${CMAKE_BINARY_DIR}/stage/startup/Unbound.scriptlib" COPYONLY)
if(UNBOUND_MAC_TESTS)
    add_executable(unbound-lua-loader-test "${UNBOUND_ROOT}/tests/platforms/macos/loading/lua/loader.cpp")
    target_link_libraries(unbound-lua-loader-test PRIVATE Qt5::Widgets)
    add_executable(unbound-lua-script-test
        "${UNBOUND_ROOT}/tests/platforms/macos/loading/lua/startup/interpreter.cpp")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${UNBOUND_ROOT}/src/scripts/commands.json" "${UNBOUND_ROOT}/infrastructure/scripts/generate.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${UNBOUND_ROOT}/infrastructure/scripts/generate.py"
    --platform macos --output "${CMAKE_BINARY_DIR}/stage/commands" COMMAND_ERROR_IS_FATAL ANY)
