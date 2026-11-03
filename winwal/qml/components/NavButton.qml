import QtQuick
import QtQuick.Controls
import HySett

Button {
    id: control
    property string iconSymbol: ""
    property bool isActive: false
    property int tabIdx: 0

    implicitHeight: 25
    flat: true
    leftPadding: 0
    rightPadding: 0

    contentItem: Item {
        Row {
            id: contentRow
            spacing: 7
            anchors.verticalCenter: parent.verticalCenter
            x: (Config && Config.alignCenter)
               ? Math.max(4, Math.round((control.width - contentRow.implicitWidth) / 2))
               : 10

            Behavior on x { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

            Text {
                renderType: Text.QtRendering
                visible: control.iconSymbol.length > 0
                text: control.iconSymbol
                color: control.isActive ? (Config ? Config.accentTextColor : "#000000") : (control.hovered ? "#ffffff" : "#888888")
                font.pixelSize: Config ? Config.fontSize : 11
                anchors.verticalCenter: parent.verticalCenter
                Behavior on color { ColorAnimation { duration: 100 } }
            }

            Text {
                renderType: Text.QtRendering
                text: control.text
                color: control.isActive ? (Config ? Config.accentTextColor : "#000000") : (control.hovered ? "#ffffff" : "#cccccc")
                font.pixelSize: Config ? Config.fontSize : 11
                font.bold: false
                anchors.verticalCenter: parent.verticalCenter
                Behavior on color { ColorAnimation { duration: 100 } }
            }
        }
    }
}