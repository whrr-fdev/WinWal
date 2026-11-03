cmake_minimum_required(VERSION 3.20)
project(WinWal LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(POLICY QTP0001)
    qt_policy(SET QTP0001 NEW)
endif()
if(POLICY QTP0004)
    qt_policy(SET QTP0004 OLD)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Multimedia ShaderTools Concurrent)

qt_standard_project_setup()

qt_add_executable(WinWal WIN32
    winwal.rc
    src/main.cpp
    src/AppInstance.cpp
    src/AppInstance.h
    src/ThemeEngine.cpp
    src/ThemeEngine.h
    src/AppConfig.cpp
    src/AppConfig.h
    src/LookManager.cpp
    src/LookManager.h
    src/TaskbarManager.cpp
    src/TaskbarManager.h
    src/WidgetsManager.cpp
    src/WidgetsManager.h
    src/WallpapersManager.cpp
    src/WallpapersManager.h
    src/AtmosphereManager.cpp
    src/AtmosphereManager.h
    src/HyprIpc.cpp
    src/HyprIpc.h
)

qt_add_resources(WinWal "winwal_assets"
    PREFIX "/"
    BASE "assets"
    FILES
        assets/winwal.ico
        assets/fonts/NerdFont.ttf
        assets/fonts/Anurati-Regular.otf
)

qt6_add_shaders(WinWal "winwal_shaders"
    PREFIX "/shaders"
    FILES
        qml/shaders/ambient_backdrop.frag
        qml/shaders/atmosphere_fx.frag
        qml/shaders/click_fx.frag
        qml/shaders/cursor_fx.frag
        qml/shaders/dialog_blur.frag
        qml/shaders/liquid_glass.frag
        qml/shaders/liquid_panel.frag
        qml/shaders/neon_border.frag
        qml/shaders/vortex_suck.frag
        qml/shaders/wallpaper_trans.frag
)

qt_add_qml_module(WinWal
    URI HySett
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        qml/Main.qml
        qml/AtmosphereOverlay.qml
        qml/DesktopWallpaperOverlay.qml
        qml/components/ColorPickerModal.qml
        qml/components/GlassPanel.qml
        qml/components/NavButton.qml
        qml/components/SegmentedButton.qml
        qml/components/Sidebar.qml
        qml/components/StyledButton.qml
        qml/components/StyledCheckBox.qml
        qml/components/StyledComboBox.qml
        qml/components/StyledSlider.qml
        qml/components/StyledSwitch.qml
        qml/views/PlaceholderView.qml
        qml/views/LookSettingsView.qml
        qml/views/AnimsSettingsView.qml
        qml/views/CursorSettingsView.qml
        qml/views/MonitorsSettingsView.qml
        qml/views/DevicesSettingsView.qml
        qml/views/NetworkSettingsView.qml
        qml/views/BluetoothSettingsView.qml
        qml/views/WaybarSettingsView.qml
        qml/views/AutostartSettingsView.qml
        qml/views/BindsSettingsView.qml
        qml/views/RulesSettingsView.qml
        qml/views/AppsSettingsView.qml
        qml/views/ShadersView.qml
        qml/views/SearchView.qml
        qml/views/MakoSettingsView.qml
        qml/views/WelcomeView.qml
        qml/views/WallpapersView.qml
        qml/views/WidgetsSettingsView.qml
        qml/views/AtmosphereView.qml
        qml/views/AppSettingsView.qml
)

target_link_libraries(WinWal PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Qt6::Concurrent
    dwmapi
    oleacc
    user32
    gdi32
    shell32
    psapi
    iphlpapi
    advapi32
    ole32
    oleaut32
    uuid
)