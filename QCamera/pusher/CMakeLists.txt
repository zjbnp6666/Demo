cmake_minimum_required(VERSION 3.19)
project(QCamera LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Multimedia)

set(FFMPEG_INCLUDE D:/msys64/mingw64/include)
set(FFMPEG_LIBRARY D:/msys64/mingw64/lib)
include_directories(${FFMPEG_INCLUDE})
link_directories(${FFMPEG_LIBRARY})
qt_standard_project_setup()

qt_add_executable(QCamera
    WIN32 MACOSX_BUNDLE
    main.cpp
    widget.cpp
    widget.h
    widget.ui
    camera.h camera.cpp
    audioformat.h audioformat.cpp
    logger.h logger.cpp
    av.h av.cpp
    framequeue.h framequeue.cpp
    streamlane.h streamlane.cpp
)

target_link_libraries(QCamera
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Multimedia
        avformat avcodec avutil swscale swresample
)
# 编译后只拷 FFmpeg DLL + 运行时（不用 GLOB，Qt自带GCC11会冲突）
set(FFMPEG_DLL_DIR "D:/msys64/mingw64/bin")
add_custom_command(TARGET QCamera POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FFMPEG_DLL_DIR}/avformat-62.dll"
        "${FFMPEG_DLL_DIR}/avcodec-62.dll"
        "${FFMPEG_DLL_DIR}/avutil-60.dll"
        "${FFMPEG_DLL_DIR}/swscale-9.dll"
        "${FFMPEG_DLL_DIR}/swresample-6.dll"
        "${FFMPEG_DLL_DIR}/libx265-215.dll"
        "${FFMPEG_DLL_DIR}/libx264-165.dll"
        "${FFMPEG_DLL_DIR}/zlib1.dll"
        "${FFMPEG_DLL_DIR}/libstdc++-6.dll"
        "${FFMPEG_DLL_DIR}/libgcc_s_seh-1.dll"
        "${FFMPEG_DLL_DIR}/libwinpthread-1.dll"
        "$<TARGET_FILE_DIR:QCamera>"
    COMMENT "Copying FFmpeg DLLs..."
)
install(TARGETS QCamera
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET QCamera
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
