import QtQuick
import QtQuick.Controls
import HySett

CheckBox {
    id: control
    font.pixelSize: Config ? Config.fontSize : 11

    scale: control.hovered ? 1.03 : 1.0
    Behavior on scale { NumberAnimation { duration: 120 } }

    indicator: Rectangle {
        implicitWidth: 16
        implicitHeight: 16
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: (Theme && Theme.isRounded) ? 3 : 0
        color: control.checked ? "#ffffff" : Qt.rgba(1, 1, 1, 0.08)
        border.color: control.checked || control.hovered ? "#ffffff" : Qt.rgba(1, 1, 1, 0.28)
        border.width: 1

        Behavior on color { ColorAnimation { duration: 130 } }
        Behavior on radius { NumberAnimation { duration: 150 } }

        Text {
            anchors.centerIn: parent
            text: "✓"
            font.pixelSize: 11
            color: "#000000"
            scale: control.checked ? 1.0 : 0.0
            Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack } }
        }
    }

    contentItem: Text {
        text: Config ? (Config.language, Config.trAuto(control.text)) : control.text
        font: control.font
        color: control.checked ? "#ffffff" : "#cccccc"
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + 8
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
