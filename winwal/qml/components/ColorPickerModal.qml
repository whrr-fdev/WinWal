import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HySett

Item {
    id: pickerRoot
    parent: Overlay.overlay
    anchors.fill: parent
    visible: opacity > 0.01
    opacity: 0.0
    z: 2000

    property string titleText: "Выбор цвета"
    property real hueVal: 0.0
    property real satVal: 0.0
    property real valVal: 1.0
    property color currentColor: Qt.hsva(hueVal, satVal, valVal, 1.0)
    signal colorSelected(color newColor)

    Timer {
        id: throttleEmit
        interval: 35
        repeat: false
        onTriggered: pickerRoot.colorSelected(pickerRoot.currentColor)
    }

    function openPicker(label, initialColor) {
        titleText = label
        var c = Qt.color(initialColor)
        hueVal = Math.max(0.0, c.hsvHue)
        satVal = c.hsvSaturation
        valVal = c.hsvValue
        hexInput.text = currentColor.toString().toUpperCase()
        pickerRoot.opacity = 1.0
        pickerCard.scale = 1.0
    }

    function closePicker() {
        pickerRoot.colorSelected(pickerRoot.currentColor)
        pickerRoot.opacity = 0.0
        pickerCard.scale = 0.92
        if (ApplicationWindow.window) ApplicationWindow.window.modalOpen = false
    }

    onCurrentColorChanged: {
        if (pickerRoot.visible) {
            if (!hexInput.activeFocus) {
                hexInput.text = currentColor.toString().toUpperCase()
            }
            if (!throttleEmit.running) throttleEmit.start()
        }
    }

    Behavior on opacity { NumberAnimation { duration: 140 } }

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        MouseArea {
            anchors.fill: parent
            onClicked: pickerRoot.closePicker()
        }
    }

    Rectangle {
        id: pickerCard
        anchors.centerIn: parent
        width: 340
        implicitHeight: pickerCol.implicitHeight + 28
        scale: 0.92
        radius: (Theme && Theme.isRounded) ? 8 : 0
        color: Qt.rgba(0.06, 0.06, 0.08, 0.96)
        border.color: Qt.rgba(1, 1, 1, 0.22)
        border.width: 1

        MouseArea { anchors.fill: parent }
        Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutBack } }

        ColumnLayout {
            id: pickerCol
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Text {
                    renderType: Text.QtRendering
                    text: pickerRoot.titleText
                    color: "#ffffff"
                    font.pixelSize: Config ? Config.fontSize + 1 : 12
                    font.bold: true
                    Layout.fillWidth: true
                }
                StyledButton {
                    text: "✕"
                    implicitWidth: 22
                    implicitHeight: 22
                    onClicked: pickerRoot.closePicker()
                }
            }

            // 2D Поле Насыщенность / Яркость (без rotation: -90, с правильным Gradient.Horizontal!)
            Item {
                id: svBox
                Layout.fillWidth: true
                Layout.preferredHeight: 160

                Rectangle {
                    anchors.fill: parent
                    radius: (Theme && Theme.isRounded) ? 6 : 0
                    color: Qt.hsva(pickerRoot.hueVal, 1.0, 1.0, 1.0)
                    border.color: Qt.rgba(1, 1, 1, 0.22)
                    border.width: 1

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1
                        radius: Math.max(0, parent.radius - 1)
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#ffffff" }
                            GradientStop { position: 1.0; color: "transparent" }
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1
                        radius: Math.max(0, parent.radius - 1)
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 1.0; color: "#000000" }
                        }
                    }
                }

                Rectangle {
                    width: 14
                    height: 14
                    radius: 7
                    x: Math.round(pickerRoot.satVal * (svBox.width - width))
                    y: Math.round((1.0 - pickerRoot.valVal) * (svBox.height - height))
                    color: pickerRoot.currentColor
                    border.color: "#ffffff"
                    border.width: 2

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 2
                        radius: 5
                        color: "transparent"
                        border.color: "#000000"
                        border.width: 1
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    function updateSV(mx, my) {
                        pickerRoot.satVal = Math.max(0.0, Math.min(1.0, mx / width))
                        pickerRoot.valVal = Math.max(0.0, Math.min(1.0, 1.0 - (my / height)))
                    }
                    onPressed: (mouse) => updateSV(mouse.x, mouse.y)
                    onPositionChanged: (mouse) => { if (pressed) updateSV(mouse.x, mouse.y) }
                    onReleased: pickerRoot.colorSelected(pickerRoot.currentColor)
                }
            }

            Item {
                id: hueBar
                Layout.fillWidth: true
                Layout.preferredHeight: 16

                Rectangle {
                    anchors.fill: parent
                    radius: (Theme && Theme.isRounded) ? 8 : 2
                    border.color: Qt.rgba(1, 1, 1, 0.25)
                    border.width: 1
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.00; color: "#ff0000" }
                        GradientStop { position: 0.17; color: "#ffff00" }
                        GradientStop { position: 0.33; color: "#00ff00" }
                        GradientStop { position: 0.50; color: "#00ffff" }
                        GradientStop { position: 0.67; color: "#0000ff" }
                        GradientStop { position: 0.83; color: "#ff00ff" }
                        GradientStop { position: 1.00; color: "#ff0000" }
                    }
                }

                Rectangle {
                    width: 10
                    height: 18
                    y: -1
                    x: Math.round(pickerRoot.hueVal * (hueBar.width - width))
                    radius: 4
                    color: "#ffffff"
                    border.color: "#000000"
                    border.width: 1
                }

                MouseArea {
                    anchors.fill: parent
                    function updateH(mx) {
                        pickerRoot.hueVal = Math.max(0.0, Math.min(0.999, mx / width))
                    }
                    onPressed: (mouse) => updateH(mouse.x)
                    onPositionChanged: (mouse) => { if (pressed) updateH(mouse.x) }
                    onReleased: pickerRoot.colorSelected(pickerRoot.currentColor)
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 8
                columnSpacing: 6
                rowSpacing: 6

                Repeater {
                    model: [
                        "#FFFFFF", "#D8DEE9", "#88C0D0", "#5E81AC",
                        "#81A1C1", "#A3BE8C", "#EBCB8B", "#D08770",
                        "#BF616A", "#B48EAD", "#00E5FF", "#7C4DFF",
                        "#FF4081", "#00E676", "#0A0A0C", "#181820"
                    ]

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 22
                        radius: (Theme && Theme.isRounded) ? 4 : 0
                        color: modelData
                        border.color: Qt.rgba(1, 1, 1, 0.35)
                        border.width: 1

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                var c = Qt.color(modelData)
                                pickerRoot.hueVal = Math.max(0.0, c.hsvHue)
                                pickerRoot.satVal = c.hsvSaturation
                                pickerRoot.valVal = c.hsvValue
                                pickerRoot.colorSelected(pickerRoot.currentColor)
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    width: 28
                    height: 26
                    radius: (Theme && Theme.isRounded) ? 4 : 0
                    color: pickerRoot.currentColor
                    border.color: Qt.rgba(1, 1, 1, 0.35)
                    border.width: 1
                }

                TextField {
                    id: hexInput
                    Layout.fillWidth: true
                    implicitHeight: 26
                    color: "#ffffff"
                    font.pixelSize: Config ? Config.fontSize : 11
                    placeholderText: "#FFFFFF"
                    background: Rectangle {
                        color: Qt.rgba(1, 1, 1, 0.08)
                        border.color: hexInput.activeFocus ? "#ffffff" : Qt.rgba(1, 1, 1, 0.18)
                        border.width: 1
                        radius: (Theme && Theme.isRounded) ? 4 : 0
                    }
                    onEditingFinished: {
                        var c = Qt.color(text.trim())
                        if (c.valid) {
                            pickerRoot.hueVal = Math.max(0.0, c.hsvHue)
                            pickerRoot.satVal = c.hsvSaturation
                            pickerRoot.valVal = c.hsvValue
                            pickerRoot.colorSelected(pickerRoot.currentColor)
                        }
                    }
                }

                StyledButton {
                    text: Config && Config.language === "en" ? "✓ Done" : "✓ Готово"
                    implicitHeight: 26
                    onClicked: pickerRoot.closePicker()
                }
            }
        }
    }
}