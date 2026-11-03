import QtQuick
import QtQuick.Controls
import HySett

Slider {
    id: control
    implicitHeight: 24

    background: Rectangle {
        id: groove
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 180
        implicitHeight: control.hovered || control.pressed ? 7 : 5
        width: control.availableWidth
        height: implicitHeight
        radius: (Theme && Theme.isRounded) ? height / 2 : 1
        color: Qt.rgba(1, 1, 1, 0.09)
        border.color: control.hovered ? Qt.rgba(1, 1, 1, 0.24) : Qt.rgba(1, 1, 1, 0.12)
        border.width: 1
        clip: true

        Behavior on implicitHeight { NumberAnimation { duration: 130; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 130 } }

        Rectangle {
            width: Math.max(parent.radius * 2, control.visualPosition * parent.width)
            height: parent.height
            radius: parent.radius
            color: Config ? Config.accentColor : "#ffffff"
            visible: control.visualPosition > 0.005
        }
    }

    handle: Rectangle {
        id: handleRect
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: control.pressed ? 18 : (control.hovered ? 16 : 14)
        implicitHeight: control.pressed ? 14 : (control.hovered ? 14 : 12)
        radius: (Theme && Theme.isRounded) ? height / 2 : 2
        color: Config ? Config.accentColor : "#ffffff"
        border.color: Qt.rgba(0, 0, 0, 0.45)
        border.width: 1

        Behavior on implicitWidth { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
        Behavior on implicitHeight { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
    }
}
