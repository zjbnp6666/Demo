cmake_minimum_required(VERSION 3.19)
project(QCamera-Client LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Multimedia)

set(FFMPEG_INCLUDE D:/msys64/mingw64/include)
set(FFMPEG_LIBRARY D:/msys64/mingw64/lib)
include_directories(${FFMPEG_INCLUDE})
link_directories(${FFMPEG_LIBRARY})
qt_standard_project_setup()
qt_add_executable(QCamera-Client
    WIN32 MACOSX_BUNDLE
    main.cpp
    widget.cpp
    widget.h
    widget.ui
    avdeio.h avdeio.cpp
    tqueue.h tqueue.cpp
    audiosinke.h audiosinke.cpp
    videowidget.h videowidget.cpp
    renderstrategy.h renderstrategy.cpp
    logger.h logger.cpp
)

target_link_libraries(QCamera-Client
    PRIVATE
        Qt::Core
        Qt::Widgets
        avformat avcodec avutil swscale swresample
        Qt::Multimedia
)
set(FFMPEG_DLL_DIR "D:/msys64/mingw64/bin")
add_custom_command(TARGET QCamera-Client POST_BUILD
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
        "$<TARGET_FILE_DIR:QCamera-Client>"
    COMMENT "Copying FFmpeg DLLs..."
)
install(TARGETS QCamera-Client
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET QCamera-Client
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
