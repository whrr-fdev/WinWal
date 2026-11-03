import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HySett
import "../components"

ScrollView {
    id: lookScroll
    contentWidth: availableWidth
    clip: true

    property int selColorTarget: 0

    ColorPickerModal {
        id: selColorModal
        onColorSelected: (c) => {
            if (lookScroll.selColorTarget === 0) Look.selectionColor = c
            else Look.selectionFillColor = c
        }
    }

    Item {
        width: lookScroll.availableWidth
        implicitHeight: mainCol.implicitHeight + 40

        ColumnLayout {
            id: mainCol
            width: Math.min(parent.width - 44, 860)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 20
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Text {
                    renderType: Text.QtRendering
                    text: Config ? Config.trAuto("Окна, размытие и выделение системы") : "Окна, размытие и выделение системы"
                    color: "#ffffff"
                    font.pixelSize: Config.fontSize + 4
                    font.bold: true
                    Layout.fillWidth: true
                }
                StyledButton {
                    text: Config ? Config.trAuto("↺  Сбросить прозрачность окон") : "↺  Сбросить прозрачность окон"
                    onClicked: Look.resetDefaults()
                }
            }

            // 1. ПРОЗРАЧНОСТЬ И БЛЮР ОКОН
            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colOp.implicitHeight + 24

                ColumnLayout {
                    id: colOp
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            renderType: Text.QtRendering
                            text: "󰖲  Глобальная прозрачность и размытие открытых окон"
                            color: Config.accentColor
                            font.pixelSize: Config.fontSize + 1
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        StyledSwitch {
                            checked: Look.enabled
                            onToggled: Look.enabled = checked
                        }
                    }

                    RowLayout {
                        visible: Look.enabled
                        Layout.fillWidth: true
                        Text { renderType: Text.QtRendering; text: "Прозрачность активного окна:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 230 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.30; to: 1.0; stepSize: 0.02
                            value: Look.activeOpacity
                            onMoved: Look.activeOpacity = value
                        }
                        Text { renderType: Text.QtRendering; text: Math.round(Look.activeOpacity * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        visible: Look.enabled
                        Layout.fillWidth: true
                        Text { renderType: Text.QtRendering; text: "Прозрачность неактивных окон:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 230 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.30; to: 1.0; stepSize: 0.02
                            value: Look.inactiveOpacity
                            onMoved: Look.inactiveOpacity = value
                        }
                        Text { renderType: Text.QtRendering; text: Math.round(Look.inactiveOpacity * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        visible: Look.enabled
                        Layout.fillWidth: true
                        Text { renderType: Text.QtRendering; text: "Не применять прозрачность к полноэкранным играм и видео:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                        StyledSwitch {
                            checked: Look.ignoreFullscreen
                            onToggled: Look.ignoreFullscreen = checked
                        }
                    }

                    Rectangle { visible: Look.enabled; Layout.fillWidth: true; height: 1; color: "#22ffffff" }

                    RowLayout {
                        visible: Look.enabled
                        Layout.fillWidth: true
                        Text { renderType: Text.QtRendering; text: "Включить размытие фона (DWM Blur) под прозрачными окнами:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                        StyledSwitch {
                            checked: Look.blurEnabled
                            onToggled: Look.blurEnabled = checked
                        }
                    }

                    RowLayout {
                        visible: Look.enabled && Look.blurEnabled
                        Layout.fillWidth: true
                        Text { renderType: Text.QtRendering; text: "Тип размытия окон:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 230 }
                        SegmentedButton {
                            Layout.fillWidth: true
                            options: ["Размытие (Aero Blur)", "Акрил (Fluent Acrylic)"]
                            selectedIndex: Look.blurMode
                            onSelected: (idx) => Look.blurMode = idx
                        }
                    }

                    RowLayout {
                        visible: Look.enabled && Look.blurEnabled
                        Layout.fillWidth: true
                        Text { renderType: Text.QtRendering; text: "Сила размытия окон:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 230 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 1; to: 20; stepSize: 1
                            value: Look.blurSize
                            onMoved: Look.blurSize = Math.round(value)
                        }
                        Text { renderType: Text.QtRendering; text: Math.round(Look.blurSize * 5) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight }
                    }
                }
            }

            // 2. ЦВЕТ ВЫДЕЛЕНИЯ В СИСТЕМЕ (ТЕКСТ И ВЫДЕЛЕНИЕ МЫШКОЙ НА РАБОЧЕМ СТОЛЕ)
            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colSel.implicitHeight + 24

                ColumnLayout {
                    id: colSel
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            renderType: Text.QtRendering
                            text: "󰏘  Системный цвет выделения (Текст и рамка мыши на рабочем столе)"
                            color: Config.accentColor
                            font.pixelSize: Config.fontSize + 1
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        StyledButton {
                            text: "Стандартный синий Windows"
                            onClicked: Look.restoreDefaultSelectionColors()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { renderType: Text.QtRendering; text: "Цвет выделения текста и рамки:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                        Rectangle { width: 24; height: 24; radius: 4; color: Look.selectionColor; border.color: "#66ffffff"; border.width: 1 }
                        StyledButton {
                            text: Look.selectionColor.toString().toUpperCase()
                            onClicked: { lookScroll.selColorTarget = 0; selColorModal.openPicker("Цвет выделения текста и рамки", Look.selectionColor) }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Text { renderType: Text.QtRendering; text: "Цвет заливки прямоугольника мыши на рабочем столе:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                        Rectangle { width: 24; height: 24; radius: 4; color: Look.selectionFillColor; border.color: "#66ffffff"; border.width: 1 }
                        StyledButton {
                            text: Look.selectionFillColor.toString().toUpperCase()
                            onClicked: { lookScroll.selColorTarget = 1; selColorModal.openPicker("Цвет заливки выделения мыши", Look.selectionFillColor) }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Text { renderType: Text.QtRendering; text: "Быстрые пресеты выделения:"; color: "#aaaaaa"; font.pixelSize: Config.fontSize - 1 }
                        Item { Layout.fillWidth: true }
                        StyledButton { text: "Акцент WinWal"; onClicked: Look.applySystemSelectionColors(Config.accentColor, Config.accentColor) }
                        StyledButton { text: "Монохром (Белый)"; onClicked: Look.applySystemSelectionColors("#e2e8f0", "#94a3b8") }
                        StyledButton { text: "Тёмный графит"; onClicked: Look.applySystemSelectionColors("#334155", "#475569") }
                        StyledButton { text: "Фиолетовый неон"; onClicked: Look.applySystemSelectionColors("#8b5cf6", "#7c3aed") }
                        StyledButton { text: "Холодный лёд"; onClicked: Look.applySystemSelectionColors("#38bdf8", "#0284c7") }
                    }
                }
            }
        }
    }
}