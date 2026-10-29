# Included after the shared game/UI/item source lists have been defined.
set(TOP_WEB_FONT "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf" CACHE FILEPATH
    "Redistributable TrueType font embedded in the web build")
if(NOT EXISTS "${TOP_WEB_FONT}")
    message(FATAL_ERROR "Set TOP_WEB_FONT to a redistributable .ttf font")
endif()
set(TOP_WEB_FONT_LICENSE "/usr/share/doc/fonts-dejavu-core/copyright" CACHE FILEPATH
    "License notice distributed with TOP_WEB_FONT")
if(EXISTS "${TOP_WEB_FONT_LICENSE}")
    configure_file("${TOP_WEB_FONT_LICENSE}" "${CMAKE_CURRENT_BINARY_DIR}/font-license.txt" COPYONLY)
endif()

add_executable(top_web ${TOP_GAME_SOURCES}
    Platform/web/main.cpp
    Platform/web/WebWindow.cpp
    Platform/web/WebFiles.cpp
    Platform/web/FontFreeType.cpp
)
target_include_directories(top_web PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Engine/include)
target_compile_definitions(top_web PRIVATE TOP_USE_OPENGL)
target_link_libraries(top_web PRIVATE top_ui top_item)
set_target_properties(top_web PROPERTIES OUTPUT_NAME index SUFFIX ".html")
# The large native self-test function is only used by --check; optimizing it
# adds minutes and considerable compiler memory without affecting gameplay.
set_source_files_properties(Engine/src/top/PlaySelfTest.cpp PROPERTIES COMPILE_OPTIONS "-O0;-g0")

# Ports must be available while compiling headers and while linking libraries.
foreach(target top_web top_item top_ui)
    target_compile_options(${target} PRIVATE -fexceptions -sUSE_ZLIB=1
        -sUSE_SDL=2 -sUSE_SDL_MIXER=2 -sUSE_FREETYPE=1)
endforeach()
target_link_options(top_web PRIVATE
    -g0
    -fexceptions -sDISABLE_EXCEPTION_CATCHING=0
    -sUSE_ZLIB=1 -sUSE_SDL=2 -sUSE_SDL_MIXER=2 -sSDL2_MIXER_FORMATS=["ogg"] -sUSE_FREETYPE=1
    -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
    -sASYNCIFY=1 -sASYNCIFY_STACK_SIZE=1048576
    -sSTACK_SIZE=8388608 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728
    -sMAXIMUM_MEMORY=2147483648 -sFORCE_FILESYSTEM=1
    "-sEXPORTED_RUNTIME_METHODS=['callMain','FS','PATH','IDBFS']"
    -lidbfs.js -Wl,--wrap=fopen -Wl,--wrap=fclose
    "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/Platform/web/shell.html"
    "SHELL:--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/Platform/web/bootstrap.js"
    "SHELL:--embed-file ${TOP_WEB_FONT}@/web-font.ttf"
)
set_property(TARGET top_web APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Platform/web/shell.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/Platform/web/bootstrap.js"
    "${TOP_WEB_FONT}")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
foreach(file asset-store.js mobile-ui.js app.css manifest.webmanifest icon.svg)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Platform/web/${file}" "${CMAKE_CURRENT_BINARY_DIR}/${file}" COPYONLY)
endforeach()
add_custom_target(top_web_assets
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Tools/package_web_assets.py
        --client ${CMAKE_CURRENT_SOURCE_DIR}/../Client --output ${CMAKE_CURRENT_BINARY_DIR} --compact
    COMMENT "Preparing public game resources (excluding user saves and archives)"
    VERBATIM)
