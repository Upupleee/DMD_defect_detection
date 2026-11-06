cmake_minimum_required(VERSION 3.16)

project(DMDInspection VERSION 0.1 LANGUAGES CXX)

# --- 1. Qt 自动化工具设置 ---
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# --- 2. 查找依赖包 ---
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Network)

# --- 3. 按模块管理源文件 ---
set(TS_FILES DMDInspection_zh_CN.ts)
set(QM_FILES DMDInspection_zh_CN.qm)
set(RESOURCE_FILES resources.qrc) 

set(CORE_SOURCES main.cpp)

set(UI_SOURCES
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

set(CONTROLLER_SOURCES
    Controller/detect.h 
    Controller/detect.cpp
    Controller/vision_manager.h 
    Controller/vision_manager.cpp
    Controller/camera_manager.h
    Controller/camera_manager.cpp
    Controller/dataserver_manager.h
    Controller/dataserver_manager.cpp
    Controller/light_manager.h
    Controller/light_manager.cpp
    Controller/dlp_usb_manager.h
    Controller/dlp_usb_manager.cpp
)

# 新增算法源文件模块
set(ALGORITHM_SOURCES
    Algorithm/GetDefectLocation.cpp
    Algorithm/dmd_defect_detection.cpp
    Algorithm/scale_image_range.cpp
    Algorithm/OCR_opt.cpp
    Algorithm/paint_defect.cpp
)

set(CAMERA_SOURCES
    Camera/camera_base.h 
    Camera/camera_base.cpp
    Camera/camera_opt.h 
    Camera/camera_opt.cpp
)

set(UTILS_SOURCES
    Utils/image_viewer.h 
    Utils/image_viewer.cpp
    Utils/export_manager.h
    Utils/export_manager.cpp
)

# 汇总所有源文件
set(PROJECT_SOURCES
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${CONTROLLER_SOURCES}
    ${ALGORITHM_SOURCES}
    ${CAMERA_SOURCES}
    ${UTILS_SOURCES}
    ${RESOURCE_FILES}
)

# 为 Visual Studio 等 IDE 建立清晰的文件过滤器和目录层级
source_group("Core" FILES ${CORE_SOURCES})
source_group("UI" FILES ${UI_SOURCES})
source_group("Controller" FILES ${CONTROLLER_SOURCES})
source_group("Algorithm" FILES ${ALGORITHM_SOURCES})
source_group("Camera" FILES ${CAMERA_SOURCES})
source_group("Utils" FILES ${UTILS_SOURCES})
source_group("Translations" FILES ${TS_FILES} ${QM_FILES})

# --- 4. 定义构建目标 ---
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(DMDInspection
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
    
    # 自动处理 .ts 和 .qm 的更新与编译
    qt_add_translations(DMDInspection
        TS_FILES ${TS_FILES}
        QM_FILES_OUTPUT_VARIABLE QM_FILES
    )
    
    # 将生成的 .qm 文件加入可执行文件中 (如果需要打包到静态资源中可自行调整)
    target_sources(DMDInspection PRIVATE ${QM_FILES})
else()
    if(ANDROID)
        add_library(DMDInspection SHARED ${PROJECT_SOURCES})
    else()
        add_executable(DMDInspection ${PROJECT_SOURCES})
    endif()
    
    # Qt5 的翻译处理逻辑
    qt5_create_translation(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR} ${TS_FILES})
    target_sources(DMDInspection PRIVATE ${QM_FILES})
endif()

# --- 5. 配置 Target 的属性 ---

# 统一包含目录
target_include_directories(DMDInspection PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Camera
    ${CMAKE_CURRENT_SOURCE_DIR}/Controller
    ${CMAKE_CURRENT_SOURCE_DIR}/Algorithm
    ${CMAKE_CURRENT_SOURCE_DIR}/Utils
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/halcon/include
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/halcon/include/halconcpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/HIKSDK/Includes
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/OPTSDK/Includes
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/OPTControllerSDK/OPTControllerSDK/include
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/dlp-usb-c-msvc
)

# 统一链接目录
target_link_directories(DMDInspection PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/halcon/lib/x64-win64
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/HIKSDK/Libraries/win64
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/OPTSDK/Libraries/win64
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/OPTControllerSDK/OPTControllerSDK/lib/x64
    ${CMAKE_CURRENT_SOURCE_DIR}/SDK/dlp-usb-c-msvc
)

# 统一链接库
target_link_libraries(DMDInspection PRIVATE 

    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    halconcpp 
    halcon
    MvCameraControl
    SciCamSDK
    OPTController
    dlp_usb_c
)

# 平台特殊属性设置配置
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.DMDInspection)
endif()
set_target_properties(DMDInspection PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Windows: 自动拷贝运行时 DLL
if (WIN32)
    file(GLOB_RECURSE HIK_RUNTIME_DLLS "${CMAKE_CURRENT_SOURCE_DIR}/SDK/HIKSDK/Runtime/*.*")
    file(GLOB_RECURSE OPT_RUNTIME_DLLS "${CMAKE_CURRENT_SOURCE_DIR}/SDK/OPTSDK/Runtime/*.*")

    # 获取 Utils/datasvr_tcp (或相应的真实存放目录) 中的 .dll 文件
    file(GLOB_RECURSE DATASVR_RUNTIME_DLLS 
        "${CMAKE_CURRENT_SOURCE_DIR}/SDK/datasvr_tcp/*.dll"
    )
    file(GLOB_RECURSE DLP_USB_RUNTIME_DLLS "${CMAKE_CURRENT_SOURCE_DIR}/SDK/dlp-usb-c-msvc/*.dll")
    file(GLOB_RECURSE OPT_CTRL_RUNTIME_DLLS "${CMAKE_CURRENT_SOURCE_DIR}/SDK/OPTControllerSDK/OPTControllerSDK/bin/x64/*.dll")

    file(GLOB_RECURSE HALCON_RUNTIME_DLLS    "${CMAKE_CURRENT_SOURCE_DIR}/SDK/halcon/lib/x64-win64/*.dll")
    add_custom_command(TARGET DMDInspection POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${HALCON_RUNTIME_DLLS}
        ${OPT_RUNTIME_DLLS}
        ${HIK_RUNTIME_DLLS}
        ${DATASVR_RUNTIME_DLLS}    # 拷贝 DataServer 相关 DLL
        ${OPT_CTRL_RUNTIME_DLLS}   # 拷贝 OPT 光源控制器 dll
        ${DLP_USB_RUNTIME_DLLS}
        $<TARGET_FILE_DIR:DMDInspection>

        # 新增：拷贝 Assets 文件夹到输出目录
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Assets"
        "$<TARGET_FILE_DIR:DMDInspection>/Assets"

        COMMENT "Copying Runtime DLLs and Assets to output directory..."
    )
endif()

# --- 6. 安装与收尾逻辑 (必须放在 Target 属性配置的最后) ---
include(GNUInstallDirs)
install(TARGETS DMDInspection
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(DMDInspection)
endif()

