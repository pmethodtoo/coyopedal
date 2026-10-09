cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../esp32/components")
include("${CMAKE_CURRENT_LIST_DIR}/../esp32/app_project.cmake")
set(GEA_EMBEDDED_APP "tic-tac-toe" CACHE STRING "Example app to build (must match a package.json gea.id)")
set(PROJECT_VER "${GEA_EMBEDDED_APP}" CACHE STRING "Firmware app id" FORCE)

# FORK PATCH (lcd-2): rewrite the app-root display defines in the build cache
# to THIS board's geometry before any component consumes them. The app ships
# amoled 502x410 defines globally; this board's main declares 320x240 and
# mirrors those onto the framework, so duplicate colliding -D flags fail
# -Werror. Rewriting the cache list once here keeps every consumer consistent.
set(GEA_EMBEDDED_CPP_BOARD "esp32-s3-waveshare-touch-lcd-2" CACHE STRING "geatsc board profile" FORCE)

if(DEFINED GEA_EMBEDDED_APP_DEFINES)
    set(_lcd2_fixed "")
    foreach(_d IN LISTS GEA_EMBEDDED_APP_DEFINES)
        if(_d MATCHES "^GEA_EMBEDDED_DISPLAY_WIDTH=")
            set(_d "GEA_EMBEDDED_DISPLAY_WIDTH=320")
        elseif(_d MATCHES "^GEA_EMBEDDED_DISPLAY_HEIGHT=")
            set(_d "GEA_EMBEDDED_DISPLAY_HEIGHT=240")
        elseif(_d MATCHES "^GEA_EMBEDDED_DISPLAY_NATIVE_WIDTH=")
            set(_d "GEA_EMBEDDED_DISPLAY_NATIVE_WIDTH=240")
        elseif(_d MATCHES "^GEA_EMBEDDED_DISPLAY_NATIVE_HEIGHT=")
            set(_d "GEA_EMBEDDED_DISPLAY_NATIVE_HEIGHT=320")
        elseif(_d MATCHES "^GEA_EMBEDDED_CSS_DEVICE_PIXEL_RATIO=")
            set(_d "GEA_EMBEDDED_CSS_DEVICE_PIXEL_RATIO=0.75")
        elseif(_d MATCHES "^GEA_EMBEDDED_CSS_LAYOUT_DEVICE_PIXEL_RATIO=")
            set(_d "GEA_EMBEDDED_CSS_LAYOUT_DEVICE_PIXEL_RATIO=0.75")
        elseif(_d MATCHES "^GEA_BOARD_HAS_POWER=")
            set(_d "GEA_BOARD_HAS_POWER=0")
        elseif(_d MATCHES "^GEA_EMBEDDED_DISPLAY_FLUSH_POOL_BYTES=")
            set(_d "GEA_EMBEDDED_DISPLAY_FLUSH_POOL_BYTES=5120")
        endif()
        list(APPEND _lcd2_fixed "${_d}")
    endforeach()
    set(GEA_EMBEDDED_APP_DEFINES "${_lcd2_fixed}" CACHE STRING "gea app defines (lcd-2 patched)" FORCE)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(gea_embedded)
