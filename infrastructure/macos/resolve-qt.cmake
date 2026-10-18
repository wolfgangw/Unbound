# Development headers only. Never build, bundle or load a second Qt runtime.
set(UNBOUND_QT_SOURCE "${UNBOUND_ROOT}/build/macos/qtbase-source" CACHE PATH "Configured Qt 5.15.2 source tree")
set(UNBOUND_RESOLVE_APP "/Applications/DaVinci Resolve/DaVinci Resolve.app" CACHE PATH "Resolve application")
set(_frameworks "${UNBOUND_RESOLVE_APP}/Contents/Frameworks")
if(NOT EXISTS "${UNBOUND_QT_SOURCE}/src/corelib/global/qconfig.h")
    message(FATAL_ERROR "Run infrastructure/macos/prepare-qt.sh first (headers only)")
endif()
set(_generated "${CMAKE_BINARY_DIR}/resolve-qt-headers")
configure_file("${UNBOUND_QT_SOURCE}/src/corelib/global/qconfig.h" "${_generated}/QtCore/qconfig.h" COPYONLY)
foreach(pair "Core|corelib" "Gui|gui" "Widgets|widgets")
    string(REPLACE "|" ";" parts "${pair}")
    list(GET parts 0 module)
    list(GET parts 1 directory)
    string(TOLOWER "${module}" lower)
    string(TOUPPER "${module}" upper)
    configure_file("${UNBOUND_QT_SOURCE}/src/${directory}/qt${lower}-config.h"
        "${_generated}/Qt${module}/qt${lower}-config.h" COPYONLY)
    if(NOT TARGET Qt5::${module})
        add_library(Qt5::${module} SHARED IMPORTED GLOBAL)
        set_target_properties(Qt5::${module} PROPERTIES
            IMPORTED_LOCATION "${_frameworks}/libQt5${module}.5.dylib"
            INTERFACE_INCLUDE_DIRECTORIES "${_generated};${_generated}/Qt${module};${UNBOUND_QT_SOURCE}/include;${UNBOUND_QT_SOURCE}/include/Qt${module};${UNBOUND_QT_SOURCE}/mkspecs/macx-clang"
            INTERFACE_COMPILE_DEFINITIONS "QT_${upper}_LIB")
    endif()
endforeach()
set_property(TARGET Qt5::Gui PROPERTY INTERFACE_LINK_LIBRARIES Qt5::Core)
set_property(TARGET Qt5::Widgets PROPERTY INTERFACE_LINK_LIBRARIES Qt5::Gui)
set(CMAKE_BUILD_RPATH "${_frameworks}")
