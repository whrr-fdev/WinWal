import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HySett

Button {
    id: btn
    property bool isIconOnly: text.length <= 2
    property bool allowFullWidth: false

    implicitHeight: 24
    implicitWidth: isIconOnly ? 24 : implicitContentWidth + 18
    Layout.maximumWidth: allowFullWidth ? 9999 : (isIconOnly ? 28 : 280)
    flat: true

    scale: btn.down ? 0.96 : 1.0
    Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }

    contentItem: Text {
                renderType: Text.QtRendering
        text: Config ? (Config.language, Config.trAuto(btn.text)) : btn.text
        color: btn.down || btn.hovered ? (Config ? Config.accentTextColor : "#000000") : (btn.enabled ? "#ececec" : "#666666")
        font.pixelSize: Config ? Config.fontSize : 11
        font.bold: false
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Behavior on color { ColorAnimation { duration: 110 } }
    }

    background: Item {
        id: bgWrap

        Rectangle {
            id: bgFill
            anchors.fill: parent
            radius: (Theme && Theme.isRounded) ? 5 : 0
            color: btn.down || btn.hovered ? (Config ? Config.accentColor : "#ffffff") :
                   Qt.rgba(1, 1, 1, Math.max(0.05, Config ? Config.cardOpacity * 0.18 : 0.07))
            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on radius { NumberAnimation { duration: 150 } }
        }

        ShaderEffect {
            anchors.fill: parent
            anchors.margins: 1
            visible: Theme && (Theme.controlsLiquidGlass || Theme.glassRefraction > 0.05) && !btn.hovered && !btn.down
            property vector2d u_resolution: Qt.vector2d(width, height)
            property real u_refraction: Theme ? Math.max(Theme.glassRefraction, Theme.controlsLiquidGlass ? 0.70 : 0.0) : 0.0
            property real u_radius: Math.max(0, bgFill.radius - 1)
            property real u_time: 1.0
            property vector4d u_base_color: Qt.vector4d(bgFill.color.r, bgFill.color.g, bgFill.color.b, bgFill.color.a)
            fragmentShader: "qrc:/shaders/qml/shaders/liquid_glass.frag.qsb"
        }

        Rectangle {
            anchors.fill: parent
            radius: bgFill.radius
            color: "transparent"
            border.color: btn.down || btn.hovered ? (Config ? Config.accentColor : "#ffffff") : Qt.rgba(1, 1, 1, 0.18)
            border.width: 1
            z: 2
            Behavior on border.color { ColorAnimation { duration: 120 } }
        }
    }
}
