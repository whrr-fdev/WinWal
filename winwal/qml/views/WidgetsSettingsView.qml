import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HySett
import "../components"

RowLayout {
    id: widgetsRoot
    spacing: 0

    property int subTab: 0
    property int colorTarget: 0 // 0 = text_color, 1 = graph_color

    ColorPickerModal {
        id: sysColorModal
        onColorSelected: (newCol) => {
            var hex = newCol.toString()
            if (widgetsRoot.colorTarget === 0) Widgets.sysTextColor = hex
            else Widgets.sysGraphColor = hex
        }
    }

    Rectangle {
        id: subSidebar
        Layout.fillHeight: true
        Layout.preferredWidth: 195
        color: Qt.rgba(0.04, 0.04, 0.04, Config ? Config.sidebarOpacity : 0.35)

        property Item activeCardObj: subTab === 0 ? wCard0 : (subTab === 1 ? wCard1 : (subTab === 2 ? wCard2 : wCard3))
        property Item hoveredCardObj: null
        property Item lastHoveredCardObj: null

        Rectangle {
            anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
            width: 1; color: Qt.rgba(1, 1, 1, 0.08)
        }

        Rectangle {
            id: widgetHoverPill
            x: 4; width: subSidebar.width - 8; height: 30
            radius: (Theme && Theme.isRounded) ? 4 : 0
            color: Qt.rgba(1, 1, 1, 0.08)
            opacity: subSidebar.hoveredCardObj && subSidebar.hoveredCardObj !== subSidebar.activeCardObj ? 1.0 : 0.0
            y: subSidebar.lastHoveredCardObj ? (subCol.y + subSidebar.lastHoveredCardObj.y) : 34
            Behavior on y { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: 90 } }
        }

        Rectangle {
            id: widgetActivePill
            x: 4; width: subSidebar.width - 8; height: 30
            radius: (Theme && Theme.isRounded) ? 4 : 0
            color: Config ? Config.accentColor : "#ffffff"
            y: subSidebar.activeCardObj ? (subCol.y + subSidebar.activeCardObj.y) : 34
            Behavior on y { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
            Behavior on radius { NumberAnimation { duration: 150 } }
        }

        ColumnLayout {
            id: subCol
            anchors.fill: parent
            anchors.margins: 6
            spacing: 3

            Text {
                text: Config ? (Config.language, Config.trAuto("Виджеты и оверлеи")) : "Виджеты и оверлеи"
                color: "#ffffff"; font.pixelSize: Config.fontSize + 1; font.bold: true
                Layout.topMargin: 4; Layout.bottomMargin: 6; Layout.leftMargin: 4
            }

            Rectangle {
                objectName: "sidebarSepLine"
                Layout.fillWidth: true; height: 1
                color: Qt.rgba(1, 1, 1, 0.06)
                Layout.bottomMargin: 2
            }

            Item {
                id: wCard0
                objectName: "skipAtmoChildren"
                Layout.fillWidth: true; implicitHeight: 30
                MouseArea {
                    id: wMouse0; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onContainsMouseChanged: {
                        if (containsMouse) { subSidebar.lastHoveredCardObj = wCard0; subSidebar.hoveredCardObj = wCard0 }
                        else if (subSidebar.hoveredCardObj === wCard0) subSidebar.hoveredCardObj = null
                    }
                    onClicked: subTab = 0
                }
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 6
                    Text { text: "󰌌"; color: subTab === 0 ? (Config ? Config.accentTextColor : "#000000") : (wMouse0.containsMouse ? "#ffffff" : "#888888"); font.pixelSize: 13 }
                    Text { text: Config ? (Config.language, Config.trAuto("Оверлей клавиш")) : "Оверлей клавиш"; color: subTab === 0 ? (Config ? Config.accentTextColor : "#000000") : (wMouse0.containsMouse ? "#ffffff" : "#cccccc"); font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                    StyledSwitch { z: 10; inverted: subTab === 0; checked: Widgets.kbRunning; onToggled: Widgets.toggleKbProcess(checked) }
                }
            }

            Item {
                id: wCard1
                objectName: "skipAtmoChildren"
                Layout.fillWidth: true; implicitHeight: 30
                MouseArea {
                    id: wMouse1; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onContainsMouseChanged: {
                        if (containsMouse) { subSidebar.lastHoveredCardObj = wCard1; subSidebar.hoveredCardObj = wCard1 }
                        else if (subSidebar.hoveredCardObj === wCard1) subSidebar.hoveredCardObj = null
                    }
                    onClicked: subTab = 1
                }
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 6
                    Text { text: "󰕰"; color: subTab === 1 ? (Config ? Config.accentTextColor : "#000000") : (wMouse1.containsMouse ? "#ffffff" : "#888888"); font.pixelSize: 13 }
                    Text { text: Config ? (Config.language, Config.trAuto("Часы на фоне")) : "Часы на фоне"; color: subTab === 1 ? (Config ? Config.accentTextColor : "#000000") : (wMouse1.containsMouse ? "#ffffff" : "#cccccc"); font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                    StyledSwitch { z: 10; inverted: subTab === 1; checked: Widgets.clockRunning; onToggled: Widgets.toggleClockProcess(checked) }
                }
            }

            Item {
                id: wCard2
                objectName: "skipAtmoChildren"
                Layout.fillWidth: true; implicitHeight: 30
                MouseArea {
                    id: wMouse2; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onContainsMouseChanged: {
                        if (containsMouse) { subSidebar.lastHoveredCardObj = wCard2; subSidebar.hoveredCardObj = wCard2 }
                        else if (subSidebar.hoveredCardObj === wCard2) subSidebar.hoveredCardObj = null
                    }
                    onClicked: subTab = 2
                }
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 6
                    Text { text: "󰍹"; color: subTab === 2 ? (Config ? Config.accentTextColor : "#000000") : (wMouse2.containsMouse ? "#ffffff" : "#888888"); font.pixelSize: 13 }
                    Text { text: Config ? (Config.language, Config.trAuto("Системный оверлей")) : "Системный оверлей"; color: subTab === 2 ? (Config ? Config.accentTextColor : "#000000") : (wMouse2.containsMouse ? "#ffffff" : "#cccccc"); font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                    StyledSwitch { z: 10; inverted: subTab === 2; checked: Widgets.sysRunning; onToggled: Widgets.toggleSysProcess(checked) }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }

    StackLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        currentIndex: subTab

        // Вкладка 0: Оверлей клавиш
        ScrollView {
            id: scrKb; contentWidth: availableWidth; clip: true
            Item {
                width: scrKb.availableWidth; implicitHeight: colKbMain.implicitHeight + 40
                ColumnLayout {
                    id: colKbMain
                    width: Math.min(parent.width - 48, 860)
                    anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.top; anchors.topMargin: 20; spacing: 12

                    Text { text: Config ? (Config.language, Config.trAuto("Оверлей клавиатуры и мыши")) : "Оверлей клавиатуры и мыши"; color: "#ffffff"; font.pixelSize: Config.fontSize + 3; font.bold: true }

                    GlassPanel {
                        objectName: "atmoObstacle"
                        Layout.fillWidth: true; implicitHeight: colKb.implicitHeight + 18
                        ColumnLayout {
                            id: colKb; anchors.fill: parent; anchors.margins: 12; spacing: 8
                            Text { text: Config ? (Config.language, Config.trAuto("Компоненты отображения")) : "Компоненты отображения"; color: "#ffffff"; font.pixelSize: Config.fontSize; font.bold: true }
                            RowLayout {
                                spacing: 14
                                StyledCheckBox { text: Config ? (Config.language, Config.trAuto("Клавиатура")) : "Клавиатура"; checked: Widgets.kbShowKeyboard; onToggled: Widgets.kbShowKeyboard = checked }
                                StyledCheckBox { text: Config ? (Config.language, Config.trAuto("Мышь")) : "Мышь"; checked: Widgets.kbShowMouse; onToggled: Widgets.kbShowMouse = checked }
                                StyledCheckBox { text: Config ? (Config.language, Config.trAuto("Боковые кнопки")) : "Боковые кнопки"; checked: Widgets.kbShowSide; onToggled: Widgets.kbShowSide = checked }
                                StyledCheckBox { text: Config ? (Config.language, Config.trAuto("Колесико")) : "Колесико"; checked: Widgets.kbShowWheel; onToggled: Widgets.kbShowWheel = checked }
                            }
                            StyledCheckBox {
                                text: Config ? (Config.language, Config.trAuto("Отображать только комбинации клавиш (Ctrl/Alt/Super/Shift)")) : "Отображать только комбинации клавиш (Ctrl/Alt/Super/Shift)"
                                checked: Widgets.kbOnlyCombos; onToggled: Widgets.kbOnlyCombos = checked
                            }
                        }
                    }

                    GlassPanel {
                        objectName: "atmoObstacle"
                        Layout.fillWidth: true; implicitHeight: colKb2.implicitHeight + 18
                        ColumnLayout {
                            id: colKb2; anchors.fill: parent; anchors.margins: 12; spacing: 10
                            Text { text: Config ? (Config.language, Config.trAuto("Положение и управление")) : "Положение и управление"; color: "#ffffff"; font.pixelSize: Config.fontSize; font.bold: true }
                            RowLayout {
                                Text { text: Config ? (Config.language, Config.trAuto("Расположение на экране:")) : "Расположение на экране:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 180 }
                                StyledComboBox {
                                    Layout.fillWidth: true
                                    model: ["bottom-right", "bottom-left", "top-right", "top-left", "bottom-center", "top-center"]
                                    currentIndex: Math.max(0, model.indexOf(Widgets.kbPosition))
                                    onActivated: (idx) => Widgets.kbPosition = model[idx]
                                }
                            }
                            RowLayout {
                                Text { text: "Положение по горизонтали (X):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                                StyledSlider { Layout.fillWidth: true; from: -900; to: 900; stepSize: 10; value: Widgets.clockOffsetX; onMoved: Widgets.clockOffsetX = Math.round(value) }
                                Text { text: (Widgets.clockOffsetX > 0 ? "+" : "") + Widgets.clockOffsetX + "px"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 54; horizontalAlignment: Text.AlignRight }
                            }
                            RowLayout {
                                Text { text: "Положение по вертикали (Y):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                                StyledSlider { Layout.fillWidth: true; from: -550; to: 550; stepSize: 10; value: Widgets.clockOffsetY; onMoved: Widgets.clockOffsetY = Math.round(value) }
                                Text { text: (Widgets.clockOffsetY > 0 ? "+" : "") + Widgets.clockOffsetY + "px"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 54; horizontalAlignment: Text.AlignRight }
                            }
                            RowLayout {
                                spacing: 8
                                StyledButton { text: "По центру экрана"; onClicked: { Widgets.clockOffsetX = 0; Widgets.clockOffsetY = -40 } }
                                StyledButton { text: Config ? (Config.language, Config.trAuto("󰈈  Скрыть/Показать (Super + F9)")) : "󰈈  Скрыть/Показать (Super + F9)"; onClicked: Widgets.toggleKbVisibility() }
                                StyledButton { text: Config ? (Config.language, Config.trAuto("↺  Перезапустить процесс")) : "↺  Перезапустить процесс"; onClicked: Widgets.restartKbProcess() }
                            }
                        }
                    }
                }
            }
        }

        // Вкладка 1: Часы на фоне
        ScrollView {
            id: scrClk; contentWidth: availableWidth; clip: true
            Item {
                width: scrClk.availableWidth; implicitHeight: colClkMain.implicitHeight + 40
                ColumnLayout {
                    id: colClkMain
                    width: Math.min(parent.width - 48, 860)
                    anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.top; anchors.topMargin: 20; spacing: 12

                    Text { text: Config ? (Config.language, Config.trAuto("Часы на рабочем столе")) : "Часы на рабочем столе"; color: "#ffffff"; font.pixelSize: Config.fontSize + 3; font.bold: true }

                    GlassPanel {
                        objectName: "atmoObstacle"
                        Layout.fillWidth: true; implicitHeight: colClk.implicitHeight + 18
                        ColumnLayout {
                            id: colClk; anchors.fill: parent; anchors.margins: 12; spacing: 10
                            RowLayout {
                                spacing: 14
                                StyledCheckBox { text: Config ? (Config.language, Config.trAuto("24-часовой формат")) : "24-часовой формат"; checked: Widgets.clock24h; onToggled: Widgets.clock24h = checked }
                                StyledCheckBox { text: Config ? (Config.language, Config.trAuto("Секунды")) : "Секунды"; checked: Widgets.clockShowSeconds; onToggled: Widgets.clockShowSeconds = checked }
                                StyledCheckBox { text: Config ? (Config.language, Config.trAuto("Дата")) : "Дата"; checked: Widgets.clockShowDate; onToggled: Widgets.clockShowDate = checked }
                                StyledCheckBox { text: Config ? (Config.language, Config.trAuto("День недели")) : "День недели"; checked: Widgets.clockShowWeekday; onToggled: Widgets.clockShowWeekday = checked }
                            }
                            RowLayout {
                                Text { text: Config ? (Config.language, Config.trAuto("Размер часов:")) : "Размер часов:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 160 }
                                StyledSlider { Layout.fillWidth: true; from: 24; to: 140; stepSize: 2; value: Widgets.clockSize; onMoved: Widgets.clockSize = Math.round(value) }
                                Text { text: Widgets.clockSize + "px"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 40 }
                            }
                            RowLayout {
                                Text { text: Config ? (Config.language, Config.trAuto("Прозрачность часов:")) : "Прозрачность часов:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 160 }
                                StyledSlider { Layout.fillWidth: true; from: 0.1; to: 1.0; stepSize: 0.05; value: Widgets.clockOpacity; onMoved: Widgets.clockOpacity = value }
                                Text { text: Math.round(Widgets.clockOpacity * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 40 }
                            }
                            RowLayout {
                                Text { text: "Положение по горизонтали (X):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                                StyledSlider { Layout.fillWidth: true; from: -900; to: 900; stepSize: 10; value: Widgets.clockOffsetX; onMoved: Widgets.clockOffsetX = Math.round(value) }
                                Text { text: (Widgets.clockOffsetX > 0 ? "+" : "") + Widgets.clockOffsetX + "px"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 54; horizontalAlignment: Text.AlignRight }
                            }
                            RowLayout {
                                Text { text: "Положение по вертикали (Y):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                                StyledSlider { Layout.fillWidth: true; from: -550; to: 550; stepSize: 10; value: Widgets.clockOffsetY; onMoved: Widgets.clockOffsetY = Math.round(value) }
                                Text { text: (Widgets.clockOffsetY > 0 ? "+" : "") + Widgets.clockOffsetY + "px"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 54; horizontalAlignment: Text.AlignRight }
                            }
                            RowLayout {
                                spacing: 8
                                StyledButton { text: "По центру экрана"; onClicked: { Widgets.clockOffsetX = 0; Widgets.clockOffsetY = -40 } }
                                StyledButton { text: Config ? (Config.language, Config.trAuto("󰆓  Сохранить параметры")) : "󰆓  Сохранить параметры"; onClicked: Widgets.saveClock() }
                                StyledButton { text: Config ? (Config.language, Config.trAuto("↺  Перезапустить часы")) : "↺  Перезапустить часы"; onClicked: Widgets.restartClockProcess() }
                            }
                        }
                    }
                }
            }
        }

        // Вкладка 2: Системный оверлей (sysinfo_overlay)
        ScrollView {
            id: scrSysInfo; contentWidth: availableWidth; clip: true
            Item {
                width: scrSysInfo.availableWidth; implicitHeight: colSysInfoMain.implicitHeight + 40
                ColumnLayout {
                    id: colSysInfoMain
                    width: Math.min(parent.width - 48, 860)
                    anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.top; anchors.topMargin: 20; spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: Config ? (Config.language, Config.trAuto("Системный оверлей (SysInfo Overlay)")) : "Системный оверлей (SysInfo Overlay)"; color: "#ffffff"; font.pixelSize: Config.fontSize + 3; font.bold: true; Layout.fillWidth: true }
                        StyledButton { text: Config ? (Config.language, Config.trAuto("↺  Перезапустить")) : "↺  Перезапустить"; onClicked: Widgets.restartSysProcess() }
                        StyledButton { text: Config ? (Config.language, Config.trAuto("Сброс")) : "Сброс"; onClicked: Widgets.resetSysDefaults() }
                    }

                    GlassPanel {
                        objectName: "atmoObstacle"
                        Layout.fillWidth: true; implicitHeight: colSysMetrics.implicitHeight + 20
                        ColumnLayout {
                            id: colSysMetrics; anchors.fill: parent; anchors.margins: 12; spacing: 10
                            Text { text: Config ? (Config.language, Config.trAuto("Отображаемые графики и сводка")) : "Отображаемые графики и сводка"; color: Config.accentColor; font.pixelSize: Config.fontSize + 1; font.bold: true }
                            RowLayout {
                                spacing: 16
                                StyledCheckBox { text: "󰻠  CPU"; checked: Widgets.sysShowCpu; onToggled: Widgets.sysShowCpu = checked }
                                StyledCheckBox { text: "󰍛  RAM"; checked: Widgets.sysShowRam; onToggled: Widgets.sysShowRam = checked }
                                StyledCheckBox { text: "󰛳  NET"; checked: Widgets.sysShowNet; onToggled: Widgets.sysShowNet = checked }
                                StyledCheckBox { text: "  TEMP"; checked: Widgets.sysShowTemp; onToggled: Widgets.sysShowTemp = checked }
                                StyledCheckBox { text: Config ? (Config.language, Config.trAuto("Диск / Uptime / Ядро")) : "Диск / Uptime / Ядро"; checked: Widgets.sysShowExtra; onToggled: Widgets.sysShowExtra = checked }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: Config ? (Config.language, Config.trAuto("Поверх всех окон (Top Layer / Overlay):")) : "Поверх всех окон (Top Layer / Overlay):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                                StyledSwitch { checked: Widgets.sysTopLayer; onToggled: Widgets.sysTopLayer = checked }
                            }
                        }
                    }

                    GlassPanel {
                        objectName: "atmoObstacle"
                        Layout.fillWidth: true; implicitHeight: colSysStyle.implicitHeight + 20
                        ColumnLayout {
                            id: colSysStyle; anchors.fill: parent; anchors.margins: 12; spacing: 10
                            Text { text: Config ? (Config.language, Config.trAuto("Позиция, размеры и цвета")) : "Позиция, размеры и цвета"; color: Config.accentColor; font.pixelSize: Config.fontSize + 1; font.bold: true }

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: Config ? (Config.language, Config.trAuto("Позиция на экране:")) : "Позиция на экране:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 190 }
                                SegmentedButton {
                                    Layout.fillWidth: true
                                    options: ["top-right", "top-left", "bottom-right", "bottom-left"]
                                    selectedIndex: Math.max(0, options.indexOf(Widgets.sysPosition))
                                    onSelected: (idx) => Widgets.sysPosition = options[idx]
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: Config ? (Config.language, Config.trAuto("Масштаб оверлея:")) + " " + Widgets.sysScale.toFixed(2) + "x" : "Масштаб оверлея: " + Widgets.sysScale.toFixed(2) + "x"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 220 }
                                StyledSlider { Layout.fillWidth: true; from: 0.4; to: 2.2; stepSize: 0.05; value: Widgets.sysScale; onMoved: Widgets.sysScale = value }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: Config ? (Config.language, Config.trAuto("Прозрачность:")) + " " + Math.round(Widgets.sysOpacity * 100) + "%" : "Прозрачность: " + Math.round(Widgets.sysOpacity * 100) + "%"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 220 }
                                StyledSlider { Layout.fillWidth: true; from: 0.1; to: 1.0; stepSize: 0.05; value: Widgets.sysOpacity; onMoved: Widgets.sysOpacity = value }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: Config ? (Config.language, Config.trAuto("Шрифт заголовков:")) + " " + Widgets.sysTitleSize + " px" : "Шрифт заголовков: " + Widgets.sysTitleSize + " px"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 220 }
                                StyledSlider { Layout.fillWidth: true; from: 8; to: 28; stepSize: 1; value: Widgets.sysTitleSize; onMoved: Widgets.sysTitleSize = Math.round(value) }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: Config ? (Config.language, Config.trAuto("Шрифт сводки (Диск/Ядро):")) + " " + Widgets.sysExtraSize + " px" : "Шрифт сводки (Диск/Ядро): " + Widgets.sysExtraSize + " px"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 220 }
                                StyledSlider { Layout.fillWidth: true; from: 8; to: 28; stepSize: 1; value: Widgets.sysExtraSize; onMoved: Widgets.sysExtraSize = Math.round(value) }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: Config ? (Config.language, Config.trAuto("Отступ по горизонтали (X):")) + " " + Widgets.sysMarginX + " px" : "Отступ по горизонтали (X): " + Widgets.sysMarginX + " px"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 220 }
                                StyledSlider { Layout.fillWidth: true; from: 0; to: 240; stepSize: 2; value: Widgets.sysMarginX; onMoved: Widgets.sysMarginX = Math.round(value) }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: Config ? (Config.language, Config.trAuto("Отступ по вертикали (Y):")) + " " + Widgets.sysMarginY + " px" : "Отступ по вертикали (Y): " + Widgets.sysMarginY + " px"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 220 }
                                StyledSlider { Layout.fillWidth: true; from: 0; to: 240; stepSize: 2; value: Widgets.sysMarginY; onMoved: Widgets.sysMarginY = Math.round(value) }
                            }

                            RowLayout {
                                Layout.fillWidth: true; spacing: 16
                                Text { text: Config ? (Config.language, Config.trAuto("Цвет текста:")) : "Цвет текста:"; color: "#cccccc"; font.pixelSize: Config.fontSize }
                                Rectangle { width: 22; height: 22; radius: 4; color: Widgets.sysTextColor; border.color: "#55ffffff"; border.width: 1 }
                                StyledButton {
                                    text: Widgets.sysTextColor
                                    onClicked: { widgetsRoot.colorTarget = 0; sysColorModal.openPicker("Цвет текста оверлея", Widgets.sysTextColor) }
                                }
                                Item { Layout.fillWidth: true }
                                Text { text: Config ? (Config.language, Config.trAuto("Цвет графиков:")) : "Цвет графиков:"; color: "#cccccc"; font.pixelSize: Config.fontSize }
                                Rectangle { width: 22; height: 22; radius: 4; color: Widgets.sysGraphColor; border.color: "#55ffffff"; border.width: 1 }
                                StyledButton {
                                    text: Widgets.sysGraphColor
                                    onClicked: { widgetsRoot.colorTarget = 1; sysColorModal.openPicker("Цвет графиков оверлея", Widgets.sysGraphColor) }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Вкладка 3: Информеры
        ScrollView {
            id: scrSys; contentWidth: availableWidth; clip: true
            Item {
                width: scrSys.availableWidth; implicitHeight: colSysMain.implicitHeight + 40
                ColumnLayout {
                    id: colSysMain
                    width: Math.min(parent.width - 48, 860)
                    anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.top; anchors.topMargin: 20; spacing: 12

                    Text { text: Config ? (Config.language, Config.trAuto("Мониторы процессов и системная сводка")) : "Мониторы процессов и системная сводка"; color: "#ffffff"; font.pixelSize: Config.fontSize + 3; font.bold: true }

                    GlassPanel {
                        objectName: "atmoObstacle"
                        Layout.fillWidth: true; implicitHeight: infWrapper.implicitHeight + 24
                        Item {
                            id: infWrapper; anchors.fill: parent; anchors.margins: 12; implicitHeight: colSys.implicitHeight
                            property Item hoveredInfObj: null
                            property Item lastHoveredInfObj: null

                            Rectangle {
                                id: infHoverPill
                                x: 0; width: infWrapper.width
                                height: infWrapper.lastHoveredInfObj ? infWrapper.lastHoveredInfObj.height : 30
                                y: infWrapper.lastHoveredInfObj ? infWrapper.lastHoveredInfObj.y : 0
                                radius: (Theme && Theme.isRounded) ? 4 : 0
                                color: Qt.rgba(1, 1, 1, 0.10); border.color: Qt.rgba(1, 1, 1, 0.38); border.width: 1
                                opacity: infWrapper.hoveredInfObj ? 1.0 : 0.0
                                Behavior on y { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                Behavior on opacity { NumberAnimation { duration: 90 } }
                            }

                            ColumnLayout {
                                id: colSys; width: parent.width; spacing: 8
                                Repeater {
                                    model: [
                                        { title: Config ? (Config.language, Config.trAuto("󰥔   Часы Peaclock (Foot)")) : "󰥔   Часы Peaclock (Foot)", act: () => Widgets.launchPeaclock() },
                                        { title: Config ? (Config.language, Config.trAuto("󰣇   Информер Fastfetch (Foot)")) : "󰣇   Информер Fastfetch (Foot)", act: () => Widgets.launchFastfetch() },
                                        { title: Config ? (Config.language, Config.trAuto("󰍹   Монитор процессов Btop (Kitty)")) : "󰍹   Монитор процессов Btop (Kitty)", act: () => Widgets.launchBtop() }
                                    ]
                                    Button {
                                        id: infBtn; Layout.fillWidth: true; implicitHeight: 30; flat: true
                                        onClicked: modelData.act()
                                        onHoveredChanged: {
                                            if (hovered) { infWrapper.lastHoveredInfObj = infBtn; infWrapper.hoveredInfObj = infBtn }
                                            else if (infWrapper.hoveredInfObj === infBtn) infWrapper.hoveredInfObj = null
                                        }
                                        contentItem: Text {
                                            text: modelData.title; color: infBtn.hovered ? "#ffffff" : "#cccccc"
                                            font.pixelSize: Config.fontSize; verticalAlignment: Text.AlignVCenter; leftPadding: 10
                                        }
                                        background: Rectangle {
                                            radius: (Theme && Theme.isRounded) ? 4 : 0
                                            color: Qt.rgba(1, 1, 1, 0.03); border.color: Qt.rgba(1, 1, 1, 0.08); border.width: 1
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
