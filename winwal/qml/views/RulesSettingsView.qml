import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HySett
import "../components"

ScrollView {
    id: tileScroll
    contentWidth: availableWidth
    clip: true

    Item {
        width: tileScroll.availableWidth
        implicitHeight: mainCol.implicitHeight + 40

        ColumnLayout {
            id: mainCol
            width: Math.min(parent.width - 44, 860)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 20
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "Тайлинг окон (Hyprland Window Manager)"
                    color: "#ffffff"
                    font.pixelSize: Config.fontSize + 4
                    font.bold: true
                    Layout.fillWidth: true
                }
                StyledButton {
                    text: "󰑐  Перестроить окна"
                    onClicked: Look.retileNow()
                }
            }

            // 1. ОСНОВНОЙ РЕЖИМ ТАЙЛИНГА И АЛГОРИТМ РАСКЛАДКИ
            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colMode.implicitHeight + 28

                ColumnLayout {
                    id: colMode
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "󰙀  Автоматический тайлинг открытых окон"
                            color: Config.accentColor
                            font.pixelSize: Config.fontSize + 1
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        StyledSwitch {
                            checked: Look.tilingEnabled
                            onToggled: Look.tilingEnabled = checked
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Text {
                            text: "Алгоритм раскладки окон на экране:"
                            color: "#cccccc"
                            font.pixelSize: Config.fontSize
                        }
                        SegmentedButton {
                            Layout.fillWidth: true
                            options: ["Умная сетка (2 → Прав÷2 → Лев÷2)", "Master + Стек", "Колонки", "Монокль"]
                            selectedIndex: Look.tilingLayout
                            onSelected: (idx) => Look.tilingLayout = idx
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text {
                            text: "Оставлять окно WinWal плавающим (поверх тайлинга для удобной настройки):"
                            color: "#cccccc"
                            font.pixelSize: Config.fontSize
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                        StyledSwitch {
                            checked: Look.floatWinWal
                            onToggled: Look.floatWinWal = checked
                        }
                    }
                }
            }

            // 2. ЗАЗОРЫ (GAPS IN / GAPS OUT) И ПРОПОРЦИИ MASTER
            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colGaps.implicitHeight + 28

                ColumnLayout {
                    id: colGaps
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 12

                    Text {
                        text: "󰕰  Отступы (Gaps) и пропорции главного окна"
                        color: Config.accentColor
                        font.pixelSize: Config.fontSize + 1
                        font.bold: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text {
                            text: "Внутренний зазор между окнами (Gaps In):"
                            color: "#cccccc"
                            font.pixelSize: Config.fontSize
                            wrapMode: Text.WordWrap
                            Layout.preferredWidth: 250
                        }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0; to: 36; stepSize: 1
                            value: Look.gapsIn
                            onMoved: Look.gapsIn = Math.round(value)
                        }
                        Text {
                            text: Look.gapsIn + " px"
                            color: "#ffffff"
                            font.pixelSize: Config.fontSize
                            Layout.preferredWidth: 52
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text {
                            text: "Внешний отступ от краёв экрана (Gaps Out):"
                            color: "#cccccc"
                            font.pixelSize: Config.fontSize
                            wrapMode: Text.WordWrap
                            Layout.preferredWidth: 250
                        }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0; to: 48; stepSize: 1
                            value: Look.gapsOut
                            onMoved: Look.gapsOut = Math.round(value)
                        }
                        Text {
                            text: Look.gapsOut + " px"
                            color: "#ffffff"
                            font.pixelSize: Config.fontSize
                            Layout.preferredWidth: 52
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    RowLayout {
                        visible: Look.tilingLayout === 0 || Look.tilingLayout === 1
                        Layout.fillWidth: true
                        spacing: 10
                        Text {
                            text: "Ширина главного (Master) окна слева:"
                            color: "#cccccc"
                            font.pixelSize: Config.fontSize
                            wrapMode: Text.WordWrap
                            Layout.preferredWidth: 250
                        }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.30; to: 0.75; stepSize: 0.01
                            value: Look.masterRatio
                            onMoved: Look.masterRatio = value
                        }
                        Text {
                            text: Math.round(Look.masterRatio * 100) + "%"
                            color: "#ffffff"
                            font.pixelSize: Config.fontSize
                            Layout.preferredWidth: 52
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }

            // 3. ГОРЯЧИЕ КЛАВИШИ И УПРАВЛЕНИЕ ПЛАВАЮЩИМИ ОКНАМИ
            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colHotkeys.implicitHeight + 28

                ColumnLayout {
                    id: colHotkeys
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Text {
                        text: "󰌌  Горячие клавиши и управление окнами"
                        color: Config.accentColor
                        font.pixelSize: Config.fontSize + 1
                        font.bold: true
                    }

                    Text {
                        text: "• Alt + T  —  Включить / выключить авто-тайлинг окон\n" +
                              "• Alt + Space  —  Сделать активное окно плавающим (Floating) и отцентрировать посередине экрана\n" +
                              "• Alt + Shift + Стрелки (или W / A / S / D)  —  Поменять активное окно местами с соседним\n" +
                              "• Alt + M  —  Поменять активное окно местами с главным (Master) окном\n" +
                              "• Alt + R  —  Переключить алгоритм раскладки (Master-Stack / Сетка / Колонки / Монокль)\n" +
                              "• Перетаскивание мышью  —  перетащи любое окно влево, чтобы сделать его главным!"
                        color: "#cbd5e1"
                        font.pixelSize: Config.fontSize
                        lineHeight: 1.35
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        StyledButton {
                            Layout.fillWidth: true
                            allowFullWidth: true
                            text: "Сбросить список плавающих исключений (вернуть все окна в тайлинг)"
                            onClicked: Look.clearFloatingExceptions()
                        }
                    }
                }
            }
        }
    }
}