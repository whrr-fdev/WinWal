import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HySett

RowLayout {
    id: control
    property var options: []
    property int selectedIndex: 0
    signal selected(int index)

    spacing: 2

    Repeater {
        model: control.options
        Button {
            id: btn
            text: modelData
            flat: true
            Layout.fillWidth: true
            implicitHeight: 24

            contentItem: Text {
                renderType: Text.QtRendering
                text: Config ? (Config.language, Config.trAuto(btn.text)) : btn.text
                color: control.selectedIndex === index ? (Config ? Config.accentTextColor : "#000000") : "#d0d0d0"
                font.pixelSize: Config ? Config.fontSize : 11
                font.bold: false
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: (Theme && Theme.isRounded) ? 4 : 0
                color: control.selectedIndex === index ? (Config ? Config.accentColor : "#ffffff") :
                       (btn.hovered ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, Math.max(0.03, Config ? Config.cardOpacity * 0.14 : 0.05)))
                Behavior on color { ColorAnimation { duration: 100 } }
            }

            onClicked: {
                control.selectedIndex = index
                control.selected(index)
            }
        }
    }
}
