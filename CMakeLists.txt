cmake_minimum_required(VERSION 3.16)
project(ao3-client VERSION 3.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
)

qt_add_executable(ao3-client
    src/readerwidget.h
    src/readerwidget.cpp
    src/translationclient.h
    src/translationclient.cpp
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/ao3client.h
    src/ao3client.cpp
    src/taglineedit.h
    src/taglineedit.cpp
    src/searchfilters.h
    src/searchfilters.cpp
    src/workcontent.h
    src/workcontent.cpp
    src/workmodel.h
    src/loginmanager.h
    src/loginmanager.cpp
    src/logindialog.h
    src/logindialog.cpp
    src/workdetailwidget.h
    src/workdetailwidget.cpp
    src/dashboardwidget.h
    src/dashboardwidget.cpp
    src/workitemwidget.h
    src/workitemwidget.cpp
    resources/resources.qrc
)

target_link_libraries(ao3-client PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

if(ANDROID)
    set_target_properties(ao3-client PROPERTIES
        QT_ANDROID_PACKAGE_NAME "org.ao3client.reader"
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE 2
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_TARGET_SDK_VERSION 35
        QT_ANDROID_SDK_BUILD_TOOLS_REVISION "35.0.0"
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    if(NOT DEFINED AO3_ANDROID_OPENSSL_DIR)
        message(FATAL_ERROR "Set AO3_ANDROID_OPENSSL_DIR to the ABI directory containing libssl_3.so and libcrypto_3.so")
    endif()
    foreach(ssl_library libssl_3.so libcrypto_3.so)
        if(NOT EXISTS "${AO3_ANDROID_OPENSSL_DIR}/${ssl_library}")
            message(FATAL_ERROR "Missing Android TLS library: ${AO3_ANDROID_OPENSSL_DIR}/${ssl_library}")
        endif()
    endforeach()
    if(DEFINED AO3_ANDROID_OPENSSL_DIR)
        set_property(TARGET ao3-client PROPERTY QT_ANDROID_EXTRA_LIBS
            "${AO3_ANDROID_OPENSSL_DIR}/libssl_3.so;${AO3_ANDROID_OPENSSL_DIR}/libcrypto_3.so")
    endif()
endif()

# Install
if(NOT ANDROID)
    install(TARGETS ao3-client RUNTIME DESTINATION bin)
    install(FILES icons/ao3.png DESTINATION share/icons/hicolor/256x256/apps RENAME ao3-client.png)
    install(FILES ao3-client.desktop DESTINATION share/applications)
    install(FILES LICENSE README.md PRIVACY.md THIRD-PARTY-NOTICES.md DESTINATION share/doc/ao3-client)
    install(DIRECTORY licenses/ DESTINATION share/doc/ao3-client/licenses)

endif()

include(CTest)
if(BUILD_TESTING AND NOT ANDROID)
    add_executable(workcontent-test tests/workcontent_test.cpp src/workcontent.cpp)
    target_include_directories(workcontent-test PRIVATE src)
    target_link_libraries(workcontent-test PRIVATE Qt6::Gui)
    add_test(NAME workcontent COMMAND workcontent-test)
    set_tests_properties(workcontent PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(BUILD_TESTING AND NOT ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(functional-test
        tests/functional_test.cpp
        src/mainwindow.cpp src/ao3client.cpp src/loginmanager.cpp
        src/logindialog.cpp src/workdetailwidget.cpp src/dashboardwidget.cpp
        src/workitemwidget.cpp src/workcontent.cpp src/searchfilters.cpp src/taglineedit.cpp src/readerwidget.cpp src/translationclient.cpp resources/resources.qrc)
    target_compile_definitions(functional-test PRIVATE AO3_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    target_include_directories(functional-test PRIVATE src)
    target_link_libraries(functional-test PRIVATE Qt6::Test Qt6::Widgets Qt6::Network)
    add_test(NAME functional COMMAND functional-test)
    set_tests_properties(functional PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(BUILD_TESTING AND NOT ANDROID)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    find_program(AO3_GIO_EXECUTABLE gio)
    if(Python3_Interpreter_FOUND AND AO3_GIO_EXECUTABLE)
        add_test(NAME desktop-launcher COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/launcher_test.py")
    endif()
endif()
