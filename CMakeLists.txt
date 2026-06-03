cmake_minimum_required(VERSION 3.19)
project(Qtchat LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql Network)

qt_standard_project_setup()

qt_add_executable(Qtchat
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    model/User.h
    model/User.cpp
    model/Message.h
    model/Message.cpp
    model/Conversation.h
    model/Conversation.cpp
    data/MockData.h
    data/MockData.cpp
    database/DatabaseManager.h
    database/DatabaseManager.cpp
    service/AiService.h
    service/AiService.cpp
    network/NetworkClient.h
    network/NetworkClient.cpp
    config/AppConfig.h
    config/AppConfig.cpp
)
qt_add_executable(ChatServer
    server/main.cpp
    server/ChatServer.h
    server/ChatServer.cpp
)

target_link_libraries(Qtchat
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Sql
        Qt::Network

        
)
target_link_libraries(ChatServer
    PRIVATE
        Qt::Core
        Qt::Network
        Qt::Sql
)

install(TARGETS Qtchat
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Qtchat
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
