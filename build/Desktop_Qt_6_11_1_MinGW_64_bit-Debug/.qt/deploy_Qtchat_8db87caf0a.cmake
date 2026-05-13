include("D:/project/Qtchat/build/Desktop_Qt_6_11_1_MinGW_64_bit-Debug/.qt/QtDeploySupport.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Qtchat-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase")

qt6_deploy_runtime_dependencies(
    EXECUTABLE "D:/project/Qtchat/build/Desktop_Qt_6_11_1_MinGW_64_bit-Debug/Qtchat.exe"
    GENERATE_QT_CONF
)
