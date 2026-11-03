import QtQuick
import QtQuick.Window
import HySett

Window {
    id: overlayWin
    objectName: "atmoGlobalOverlayWin"
    property bool isOverlayWindow: true
    visible: Atmosphere ? Atmosphere.globalOverlayEnabled : false
    x: 0; y: 0
    width: Screen.width
    height: Screen.height - 1
    title: "WinWalAtmosphereOverlay"
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus | Qt.WindowTransparentForInput | Qt.Tool

    property real shaderTime: 0.0
    Timer {
        interval: 33
        running: overlayWin.visible && Atmosphere && Atmosphere.globalOverlayEnabled
        repeat: true
        onTriggered: overlayWin.shaderTime += 0.033
    }

    // Держим технические буферы (зелёную карту воды и карту оверлея) внутри скрытого контейнера opacity: 0,
    // чтобы Qt никогда не рисовал зелёную карту нормалей напрямую на экран!
    Item {
        width: overlayWin.width
        height: overlayWin.height
        opacity: 0.0

        AtmosphereCanvas {
            id: globalWaterMap
            anchors.fill: parent
            visible: true
            enabled: false
            globalMode: true; renderPass: 0
            activeRunning: overlayWin.visible && Atmosphere && Atmosphere.globalOverlayEnabled
        }

        AtmosphereCanvas {
            id: globalOverlayMap
            anchors.fill: parent
            visible: true
            enabled: false
            globalMode: true; renderPass: 1
            masterCanvas: globalWaterMap
            activeRunning: overlayWin.visible && Atmosphere && Atmosphere.globalOverlayEnabled
        }
    }

    ShaderEffectSource {
        id: globalWaterMapSource
        sourceItem: (atmoWin.visible && Atmosphere && Atmosphere.globalOverlayEnabled) ? globalWaterMap : null
        anchors.fill: parent
        visible: false
        hideSource: true
        smooth: true
        live: overlayWin.visible && Atmosphere && Atmosphere.globalOverlayEnabled
    }

    ShaderEffectSource {
        id: globalOverlayMapSource
        sourceItem: (atmoWin.visible && Atmosphere && Atmosphere.globalOverlayEnabled) ? globalOverlayMap : null
        anchors.fill: parent
        visible: false
        hideSource: true
        smooth: true
        live: overlayWin.visible && Atmosphere && Atmosphere.globalOverlayEnabled
    }

    ShaderEffect {
        anchors.fill: parent
        enabled: false
        visible: overlayWin.visible && Atmosphere && Atmosphere.globalOverlayEnabled
        property vector2d u_resolution: Qt.vector2d(width, height)
        property real u_has_scene: 0.0
        property real u_refraction_strength: Atmosphere ? Atmosphere.dropRefraction : 1.0
        property real u_time: overlayWin.shaderTime
        property vector2d u_mouse: Qt.vector2d(globalWaterMap.cursorPos.x, globalWaterMap.cursorPos.y)
        property real u_cursor_strength: (Atmosphere && Atmosphere.cursorWindEnabled) ? 0.55 : 0.0
        property vector3d u_cursor_color: Qt.vector3d(0.75, 0.88, 1.0)
        property real u_border_strength: 0.0
        property real u_border_spread: 0.45
        property real u_border_color_mode: 0
        property vector3d u_border_color: Qt.vector3d(1, 1, 1)
        property real idleBoost: (Atmosphere && Atmosphere.idleFogEnabled) ? (globalWaterMap.idleFogBoost * Atmosphere.idleFogStrength) : 0.0
        property real baseSpread: Atmosphere ? ((Atmosphere.snowEnabled && !Atmosphere.rainEnabled) ? Atmosphere.snowScreenFrostSpread : Atmosphere.overlayFogSpread) : 0.45
        property real baseInt: Atmosphere ? ((Atmosphere.snowEnabled && !Atmosphere.rainEnabled) ? Atmosphere.snowScreenFrostIntensity : Atmosphere.overlayScreenFogIntensity) : 0.60
        property bool screenMistActive: Atmosphere ? ((Atmosphere.snowEnabled && Atmosphere.snowScreenFrostEnabled) || (Atmosphere.condensationEnabled && Atmosphere.overlayScreenFogEnabled)) : false
        property bool winMistActive: Atmosphere ? ((Atmosphere.snowEnabled && Atmosphere.snowWindowFrostIntensity > 0.01) || (Atmosphere.condensationEnabled && Atmosphere.windowFogIntensity > 0.01)) : false
        property real effSpread: Math.min(1.0, baseSpread + idleBoost * (1.0 - baseSpread) * 0.78)
        property real effInt: Math.min(0.995, baseInt + idleBoost * (0.995 - baseInt))
        property real u_fog_strength: screenMistActive ? (Math.round(effSpread * 100) + effInt) : (winMistActive ? -effInt : 0.0)
        property real u_lightning: globalWaterMap.lightningFlash
        property real u_lightning_x: globalWaterMap.lightningPosX
        property real u_drop_tint_strength: Atmosphere ? Atmosphere.dropTintStrength : 0.20
        property real u_drop_highlight: Atmosphere ? Atmosphere.dropHighlightStrength : 1.0
        property vector3d u_drop_tint_color: Atmosphere ? Atmosphere.effectiveDropTintVec : Qt.vector3d(0.015, 0.015, 0.018)
        property real u_wipe_strength: Atmosphere ? Atmosphere.wipeTrailStrength : 0.55
        property real u_frost_strength: (Atmosphere && Atmosphere.snowEnabled) ? (((Atmosphere.snowFrostStyle === 0 && Atmosphere.snowVelvetFlakesEnabled) ? 3 : Atmosphere.snowFrostStyle) * 10.0 + Atmosphere.snowFrostPatternStrength) : 0.0
        property real u_idle_fog_boost: (Atmosphere && Atmosphere.condensationEnabled && Atmosphere.idleFogEnabled) ? (globalWaterMap.idleFogBoost * Atmosphere.idleFogStrength) : 0.0
        property variant u_waterMap: globalWaterMapSource
        property variant u_overlayMap: globalOverlayMapSource
        property variant u_scene: globalWaterMapSource
        fragmentShader: "qrc:/shaders/qml/shaders/atmosphere_fx.frag.qsb"
    }
}