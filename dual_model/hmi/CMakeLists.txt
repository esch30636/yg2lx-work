#=============================================================================
# CMakeLists.txt — Battery HMI Qt5 Application
#
# Cross-compile for MYD-YG2LX (RZ/G2L Cortex-A55):
#   source /opt/yg2lx/environment-setup-aarch64-poky-linux
#   cmake -B build \
#       -DCMAKE_TOOLCHAIN_FILE=$OECORE_NATIVE_SYSROOT/usr/share/cmake/OEToolchainConfig.cmake \
#       -DCMAKE_PREFIX_PATH=$SDKTARGETSYSROOT/usr/lib64/cmake \
#       -DCMAKE_BUILD_TYPE=MinSizeRel
#   cmake --build build -j$(nproc)
#   aarch64-poky-linux-strip build/battery_hmi
#
# Host build for testing:
#   cmake -B build-host -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build-host -j$(nproc)
#=============================================================================
cmake_minimum_required(VERSION 3.14)
project(BatteryHMI VERSION 2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# ── Qt5 auto MOC/RCC/UIC ──
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Find Qt5 (from sysroot on cross-compile) ──
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Charts)

# ── Inference engine C sources (relative to dual_model root) ──
set(INFERENCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(INFERENCE_SRCS
    ${INFERENCE_DIR}/src/pinn_inference.c
    ${INFERENCE_DIR}/src/cnn_inference.c
)

# ── Gather HMI C++ sources ──
set(HMI_SOURCES
    main.cpp

    # Inference wrapper
    inference/InferenceEngine.cpp
    inference/CnnPreprocessor.cpp

    # Data providers
    data/DemoDataProvider.cpp
    data/FileDataProvider.cpp
    data/SpiDataProvider.cpp

    # UI widgets
    ui/ChartWidget.cpp
    ui/StatusPanel.cpp
    ui/AlarmPopup.cpp
    ui/AlarmIndicator.cpp
    ui/MainWindow.cpp

    # UI screens (v2.0 wizard)
    ui/screens/InitScreen.cpp
    ui/screens/ModelSelectScreen.cpp
    ui/screens/ResultScreen.cpp

    # Threading
    threads/InferenceWorker.cpp
)

# ── Resources (QSS theme) ──
set(RESOURCES
    resources/resources.qrc
)

# ── Application target ──
add_executable(battery_hmi
    ${HMI_SOURCES}
    ${INFERENCE_SRCS}
    ${RESOURCES}
)

# Include paths
target_include_directories(battery_hmi PRIVATE
    ${INFERENCE_DIR}/include      # battery_inference.h, scalers.h, weights, meta
    ${CMAKE_CURRENT_SOURCE_DIR}   # hmi/* headers
)

# Link Qt5 modules
target_link_libraries(battery_hmi PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Charts
    m         # math library (erff, expf, sqrtf, etc.)
    pthread   # QThread
)

# ── Compiler flags ──
target_compile_options(battery_hmi PRIVATE
    -Wall
    -Wextra
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:MinSizeRel>:-Os>
)

# Cross-compile: ARMv8-A with NEON
if(CMAKE_CROSSCOMPILING)
    target_compile_options(battery_hmi PRIVATE
        -mcpu=cortex-a55
        -march=armv8.2-a
    )
endif()

# ── Install (optional) ──
install(TARGETS battery_hmi RUNTIME DESTINATION bin)

# ── Translations (compile .ts → .qm via Python, lrelease fallback) ──
set(TS_FILE ${CMAKE_CURRENT_SOURCE_DIR}/translations/battery_hmi_zh_CN.ts)
set(QM_FILE ${CMAKE_CURRENT_SOURCE_DIR}/translations/battery_hmi_zh_CN.qm)

add_custom_target(translations
    COMMENT "Compiling Qt translations (.ts → .qm)"
)
add_custom_command(TARGET translations
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/ts2qm.py ${TS_FILE} ${QM_FILE}
    COMMENT "ts2qm: Compiling ${TS_FILE} → ${QM_FILE}"
)

# ── Print summary ──
message(STATUS "Qt5 version: ${Qt5_VERSION}")
message(STATUS "Inference sources: ${INFERENCE_SRCS}")
message(STATUS "Target: battery_hmi")
