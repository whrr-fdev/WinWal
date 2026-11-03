import QtQuick
import QtQuick.Controls
import HySett

Switch {
    id: control
    property bool inverted: false
    padding: 0
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    implicitHeight: 16
    implicitWidth: 28

    indicator: Rectangle {
        id: track
        implicitWidth: 28
        implicitHeight: 14
        x: 0
        y: Math.round((parent.height - height) / 2)
        radius: (Theme && Theme.isRounded) ? 7 : 2
        color: control.inverted ?
               (control.checked ? "#111111" : Qt.rgba(0, 0, 0, 0.22)) :
               (control.checked ? (Config ? Config.accentColor : "#ffffff") : Qt.rgba(1, 1, 1, 0.08))
        border.color: control.inverted ?
                      (control.checked ? "#000000" : Qt.rgba(0, 0, 0, 0.45)) :
                      (control.checked ? "#ffffff" : (control.hovered ? Qt.rgba(1, 1, 1, 0.45) : Qt.rgba(1, 1, 1, 0.22)))
        border.width: 1

        Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }
        Behavior on border.color { ColorAnimation { duration: 140 } }
        Behavior on radius { NumberAnimation { duration: 150 } }

        Rectangle {
            id: thumb
            property real baseW: control.down ? 13 : 10
            width: baseW
            height: 10
            y: 2
            x: control.checked ? (parent.width - width - 2) : 2
            radius: (Theme && Theme.isRounded) ? 5 : 1
            color: control.inverted ?
                   (control.checked ? "#ffffff" : "#444444") :
                   (control.checked ? (Config ? Config.accentTextColor : "#0a0a0c") : "#f0f0f0")

            Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }
            Behavior on width { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
            Behavior on color { ColorAnimation { duration: 140 } }
            Behavior on radius { NumberAnimation { duration: 150 } }
        }
    }
}
