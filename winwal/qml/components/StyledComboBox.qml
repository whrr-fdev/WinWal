import QtQuick
import QtQuick.Controls
import HySett

ComboBox {
    id: control
    implicitHeight: 26

    contentItem: Text {
        leftPadding: 10
        rightPadding: control.indicator.width + 10
        text: Config ? (Config.language, Config.trAuto(control.displayText)) : control.displayText
        font.pixelSize: Config ? Config.fontSize : 11
        color: "#ffffff"
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - width - 10
        y: control.topPadding + (control.availableHeight - height) / 2
        text: "▾"
        font.pixelSize: 11
        color: control.hovered || control.popup.visible ? "#ffffff" : "#999999"
        rotation: control.popup.visible ? 180 : 0
        Behavior on rotation { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    background: GlassPanel {
        radius: (Theme && Theme.isRounded) ? 5 : 0
        tintColor: control.hovered || control.popup.visible
                   ? Qt.rgba(1, 1, 1, Math.max(0.09, Config ? Config.cardOpacity * 0.25 : 0.10))
                   : Qt.rgba(1, 1, 1, Math.max(0.04, Config ? Config.cardOpacity * 0.16 : 0.06))

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.color: control.popup.visible ? "#ffffff" : (control.hovered ? Qt.rgba(1, 1, 1, 0.42) : Qt.rgba(1, 1, 1, 0.16))
            border.width: 1
            Behavior on border.color { ColorAnimation { duration: 130 } }
        }
    }

    popup: Popup {
        id: comboPopup
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(260, listView.contentHeight + 10)
        padding: 5
        transformOrigin: Item.Top

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 140; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale"; from: 0.95; to: 1.0; duration: 160; easing.type: Easing.OutBack }
            }
        }
        exit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 110 }
                NumberAnimation { property: "scale"; from: 1.0; to: 0.96; duration: 110 }
            }
        }

        contentItem: Item {
            implicitHeight: listView.contentHeight

            // Плавно ездящая плашка подсветки внутри выпадающего списка
            Rectangle {
                id: popupHoverPill
                x: 0
                width: listView.width
                height: 26
                radius: (Theme && Theme.isRounded) ? 4 : 0
                color: "#ffffff"
                opacity: control.highlightedIndex >= 0 ? 1.0 : 0.0
                y: Math.max(0, control.highlightedIndex * 28 - listView.contentY)
                Behavior on y { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
                Behavior on opacity { NumberAnimation { duration: 90 } }
            }

            ListView {
                id: listView
                anchors.fill: parent
                clip: true
                spacing: 2
                implicitHeight: contentHeight
                model: control.popup.visible ? control.delegateModel : null
                currentIndex: control.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator { }
            }
        }

        background: Rectangle {
            radius: (Theme && Theme.isRounded) ? 6 : 0
            color: Qt.rgba(0.05, 0.05, 0.07, Math.max(0.78, Config ? Config.opacity : 0.85))
            border.color: Qt.rgba(1, 1, 1, 0.25)
            border.width: 1

            GlassPanel {
                anchors.fill: parent
                radius: parent.radius
                tintColor: Qt.rgba(0.08, 0.08, 0.10, 0.35)
            }
        }
    }

    delegate: ItemDelegate {
        id: dlgt
        width: control.width - 10
        implicitHeight: 26
        highlighted: control.highlightedIndex === index
        property bool isCurrent: control.currentIndex === index

        contentItem: Row {
            spacing: 6
            leftPadding: dlgt.highlighted ? 10 : 6
            Behavior on leftPadding { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }

            Text {
                text: dlgt.isCurrent ? "●" : ""
                visible: dlgt.isCurrent
                color: dlgt.highlighted ? "#000000" : "#ffffff"
                font.pixelSize: 8
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: Config ? (Config.language, Config.trAuto(modelData)) : modelData
                color: dlgt.highlighted ? "#000000" : (dlgt.isCurrent ? "#ffffff" : "#cccccc")
                font.pixelSize: Config ? Config.fontSize : 11
                font.bold: dlgt.isCurrent || dlgt.highlighted
                verticalAlignment: Text.AlignVCenter
                anchors.verticalCenter: parent.verticalCenter
                Behavior on color { ColorAnimation { duration: 90 } }
            }
        }

        background: Rectangle {
            radius: (Theme && Theme.isRounded) ? 4 : 0
            color: (!dlgt.highlighted && dlgt.isCurrent) ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
            border.color: (!dlgt.highlighted && dlgt.isCurrent) ? Qt.rgba(1, 1, 1, 0.18) : "transparent"
            border.width: 1
        }
    }
}
