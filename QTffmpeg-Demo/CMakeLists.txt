cmake_minimum_required(VERSION 3.19)
project(QTffmpeg-Demo LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Multimedia)
set(FFMPEG_INCLUDE D:/ffmpeg-gcc11/include)
set(FFMPEG_LIBRARY D:/ffmpeg-gcc11/lib)
include_directories(${FFMPEG_INCLUDE})
link_directories(${FFMPEG_LIBRARY})
qt_standard_project_setup()

qt_add_executable(QTffmpeg-Demo
    WIN32 MACOSX_BUNDLE
    main.cpp
    widget.cpp
    widget.h
    qaudio.h qaudio.cpp
    framequeue.h framequeue.cpp
    decoderworker.h decoderworker.cpp
)

target_link_libraries(QTffmpeg-Demo
    PRIVATE
        Qt::Core
        Qt::Widgets
        avformat
        avcodec
        avutil
        swscale
        swresample
        Qt::Multimedia
)

# Copy FFmpeg GCC 11 DLLs + Qt MinGW runtime to build output for Ctrl+R
add_custom_command(TARGET QTffmpeg-Demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "D:/ffmpeg-gcc11/bin/avutil-59.dll"
        "$<TARGET_FILE_DIR:QTffmpeg-Demo>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "D:/ffmpeg-gcc11/bin/avcodec-61.dll"
        "$<TARGET_FILE_DIR:QTffmpeg-Demo>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "D:/ffmpeg-gcc11/bin/avformat-61.dll"
        "$<TARGET_FILE_DIR:QTffmpeg-Demo>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "D:/ffmpeg-gcc11/bin/swresample-5.dll"
        "$<TARGET_FILE_DIR:QTffmpeg-Demo>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "D:/ffmpeg-gcc11/bin/swscale-8.dll"
        "$<TARGET_FILE_DIR:QTffmpeg-Demo>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "D:/Qt6.7.3/6.7.3/mingw_64/bin/libwinpthread-1.dll"
        "$<TARGET_FILE_DIR:QTffmpeg-Demo>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "D:/Qt6.7.3/6.7.3/mingw_64/bin/libgcc_s_seh-1.dll"
        "$<TARGET_FILE_DIR:QTffmpeg-Demo>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "D:/Qt6.7.3/6.7.3/mingw_64/bin/libstdc++-6.dll"
        "$<TARGET_FILE_DIR:QTffmpeg-Demo>"
)

install(TARGETS QTffmpeg-Demo
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET QTffmpeg-Demo
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
