import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HySett
import "../components"

ScrollView {
    id: tbScroll
    contentWidth: availableWidth
    clip: true

    property int colorRole: 0

    ColorPickerModal {
        id: tbColorModal
        onColorSelected: (c) => {
            if (tbScroll.colorRole === 0) Taskbar.tintColor = c
            else if (tbScroll.colorRole === 1) Taskbar.maxTintColor = c
            else Taskbar.flyoutTintColor = c
        }
    }

    Item {
        width: tbScroll.availableWidth
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
                    text: Config ? Config.trAuto("Настройка панели задач") : "Настройка панели задач"
                    color: "#ffffff"
                    font.pixelSize: Config.fontSize + 4
                    font.bold: true
                    Layout.fillWidth: true
                }
                StyledButton {
                    text: Config ? Config.trAuto("↺  Сбросить по умолчанию") : "↺  Сбросить по умолчанию"
                    onClicked: Taskbar.resetDefaults()
                }
            }

            // 1. ПРЕСЕТЫ И РЕЖИМ ПАНЕЛИ ЗАДАЧ
            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colTbPresets.implicitHeight + 24

                ColumnLayout {
                    id: colTbPresets
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "󰒓  Быстрые пресеты и основной режим"
                            color: Config.accentColor
                            font.pixelSize: Config.fontSize + 1
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        StyledSwitch {
                            checked: Taskbar.enabled
                            onToggled: Taskbar.enabled = checked
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Repeater {
                            model: [
                                { name: "WinWal (3 плитки)", idx: 0 },
                                { name: "Hyprland (3 капсулы)", idx: 1 },
                                { name: "Win11 (1 капсула)", idx: 2 },
                                { name: "macOS (Бар ↑ Док ↓)", idx: 3 },
                                { name: "Призрак (Полоса)", idx: 4 }
                            ]

                            StyledButton {
                                Layout.fillWidth: true
                                allowFullWidth: true
                                text: modelData.name
                                onClicked: Taskbar.applyPreset(modelData.idx)
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#22ffffff" }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text {
                            text: "Запускать WinWal автоматически вместе с Windows (в фоне):"
                            color: "#ffffff"
                            font.pixelSize: Config.fontSize
                            Layout.fillWidth: true
                        }
                        StyledSwitch { checked: Taskbar.autostart; onToggled: Taskbar.autostart = checked }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text {
                            text: "Формат панели задач:"
                            color: "#cccccc"
                            font.pixelSize: Config.fontSize
                            Layout.fillWidth: true
                        }
                        SegmentedButton {
                            options: ["3 отдельные таблетки", "1 единая капсула (Win11)", "Полоса во всю ширину"]
                            selectedIndex: !Taskbar.dockMode ? 2 : (Taskbar.splitThreePills ? 0 : 1)
                            onSelected: (idx) => {
                                if (idx === 0) {
                                    Taskbar.dockMode = true
                                    Taskbar.splitThreePills = true
                                } else if (idx === 1) {
                                    Taskbar.dockMode = true
                                    Taskbar.splitThreePills = false
                                } else {
                                    Taskbar.dockMode = false
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Text {
                            text: "Расположение на экране:"
                            color: "#cccccc"
                            font.pixelSize: Config.fontSize
                        }
                        SegmentedButton {
                            Layout.fillWidth: true
                            options: (Taskbar.dockMode && Taskbar.splitThreePills)
                                     ? ["Все снизу", "Все сверху", "Пуск+Трей ↑ / Док ↓", "Док ↑ / Пуск+Трей ↓"]
                                     : ["Внизу экрана", "Вверху экрана"]
                            selectedIndex: (Taskbar.dockMode && Taskbar.splitThreePills) ? Taskbar.screenEdge : (Taskbar.screenEdge === 1 ? 1 : 0)
                            onSelected: (idx) => Taskbar.screenEdge = idx
                        }
                    }
                }
            }

            // 2. ФОН, ПРОЗРАЧНОСТЬ И РАЗМЫТИЕ ПАНЕЛИ ЗАДАЧ
            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colTtb.implicitHeight + 24

                ColumnLayout {
                    id: colTtb
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Text {
                        text: "󱓞  Стиль фона и размытие панели задач"
                        color: Config.accentColor
                        font.pixelSize: Config.fontSize + 1
                        font.bold: true
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Text {
                            text: "Стиль панели задач на рабочем столе:"
                            color: "#cccccc"
                            font.pixelSize: Config.fontSize
                        }
                        SegmentedButton {
                            Layout.fillWidth: true
                            options: ["Прозрачная", "Размытие (Aero)", "Акрил (Fluent)", "Сплошной", "Стандарт"]
                            selectedIndex: Taskbar.style
                            onSelected: (idx) => Taskbar.style = idx
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Оттенок:"; color: "#cccccc"; font.pixelSize: Config.fontSize }
                        Rectangle { width: 22; height: 22; radius: 4; color: Taskbar.tintColor; border.color: "#55ffffff"; border.width: 1 }
                        StyledButton {
                            text: Taskbar.tintColor.toString().toUpperCase()
                            onClicked: { tbScroll.colorRole = 0; tbColorModal.openPicker("Цвет панели задач", Taskbar.tintColor) }
                        }
                        Text { text: "Плотность:"; color: "#cccccc"; font.pixelSize: Config.fontSize }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.0; to: 1.0; stepSize: 0.02
                            value: Taskbar.tintOpacity
                            onMoved: Taskbar.tintOpacity = value
                        }
                        Text { text: Math.round(Taskbar.tintOpacity * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Яркая контурная рамка вокруг панели / капсул:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                        StyledSwitch { checked: Taskbar.showBorder; onToggled: Taskbar.showBorder = checked }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#22ffffff" }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Менять стиль при развёрнутом на весь экран окне:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                        StyledSwitch { checked: Taskbar.maxTriggerEnabled; onToggled: Taskbar.maxTriggerEnabled = checked }
                    }

                    ColumnLayout {
                        visible: Taskbar.maxTriggerEnabled
                        Layout.fillWidth: true
                        spacing: 6
                        SegmentedButton {
                            Layout.fillWidth: true
                            options: ["Прозрачная", "Размытие (Aero)", "Акрил (Fluent)", "Сплошной", "Стандарт"]
                            selectedIndex: Taskbar.maxStyle
                            onSelected: (idx) => Taskbar.maxStyle = idx
                        }
                    }

                    RowLayout {
                        visible: Taskbar.maxTriggerEnabled
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Цвет (Макс.):"; color: "#cccccc"; font.pixelSize: Config.fontSize }
                        Rectangle { width: 22; height: 22; radius: 4; color: Taskbar.maxTintColor; border.color: "#55ffffff"; border.width: 1 }
                        StyledButton {
                            text: Taskbar.maxTintColor.toString().toUpperCase()
                            onClicked: { tbScroll.colorRole = 1; tbColorModal.openPicker("Цвет при развернутом окне", Taskbar.maxTintColor) }
                        }
                        Text { text: "Плотность:"; color: "#cccccc"; font.pixelSize: Config.fontSize }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.0; to: 1.0; stepSize: 0.02
                            value: Taskbar.maxTintOpacity
                            onMoved: Taskbar.maxTintOpacity = value
                        }
                        Text { text: Math.round(Taskbar.maxTintOpacity * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }
                }
            }

            // 3. ГЕОМЕТРИЯ, РАБОЧИЕ СТОЛЫ [1..5] И ИКОНКИ (без дублей!)
            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colDock.implicitHeight + 24

                ColumnLayout {
                    id: colDock
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Text {
                        text: "󰖲  Геометрия, рабочие столы [1..5] и значки"
                        color: Config.accentColor
                        font.pixelSize: Config.fontSize + 1
                        font.bold: true
                    }

                    ColumnLayout {
                        visible: Taskbar.dockMode
                        Layout.fillWidth: true
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Text { text: "Показывать и переключать рабочие столы [1..5] (и хоткеи Win+1..5):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                            StyledSwitch { checked: Taskbar.showWorkspaces; onToggled: Taskbar.showWorkspaces = checked }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Text { text: "Автоскрытие блока приложений (показывать при наведении мыши к краю):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                            StyledSwitch { checked: Taskbar.autoHideDock; onToggled: Taskbar.autoHideDock = checked }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Text { text: "Радиус скругления углов капсул:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 240 }
                            StyledSlider {
                                Layout.fillWidth: true
                                from: 0; to: 24; stepSize: 1
                                value: Taskbar.dockRadius
                                onMoved: Taskbar.dockRadius = Math.round(value)
                            }
                            Text { text: Taskbar.dockRadius + " px"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 52; horizontalAlignment: Text.AlignRight }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Text { text: "Вертикальный отступ капсул (Margin Y):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 240 }
                            StyledSlider {
                                Layout.fillWidth: true
                                from: 0; to: 12; stepSize: 1
                                value: Taskbar.dockMarginY
                                onMoved: Taskbar.dockMarginY = Math.round(value)
                            }
                            Text { text: Taskbar.dockMarginY + " px"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 52; horizontalAlignment: Text.AlignRight }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Text { text: "Автоматическая ширина дока ровно по числу открытых иконок:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                            StyledSwitch { checked: Taskbar.dynamicDockWidth; onToggled: Taskbar.dynamicDockWidth = checked }
                        }

                        RowLayout {
                            visible: !Taskbar.dynamicDockWidth
                            Layout.fillWidth: true
                            spacing: 10
                            Text { text: "Дополнительный отступ внутри дока по бокам (Margin X):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 240 }
                            StyledSlider {
                                Layout.fillWidth: true
                                from: 0; to: 100; stepSize: 2
                                value: Taskbar.dockMarginX
                                onMoved: Taskbar.dockMarginX = Math.round(value)
                            }
                            Text { text: Taskbar.dockMarginX + " px"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 52; horizontalAlignment: Text.AlignRight }
                        }
                    }

                    ColumnLayout {
                        visible: !Taskbar.dockMode
                        Layout.fillWidth: true
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10
                            Text { text: "Центрировать иконки приложений по центру экрана:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                            StyledSwitch { checked: Taskbar.centerIcons; onToggled: Taskbar.centerIcons = checked }
                        }

                        RowLayout {
                            visible: Taskbar.centerIcons
                            Layout.fillWidth: true
                            spacing: 10
                            Text { text: "Плавная пружинящая анимация при открытии/закрытии окон:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                            StyledSwitch { checked: Taskbar.smoothAnimation; onToggled: Taskbar.smoothAnimation = checked }
                        }

                        RowLayout {
                            visible: Taskbar.centerIcons && Taskbar.smoothAnimation
                            Layout.fillWidth: true
                            spacing: 10
                            Text { text: "Длительность анимации центровки:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 240 }
                            StyledSlider {
                                Layout.fillWidth: true
                                from: 60; to: 600; stepSize: 20
                                value: Taskbar.animSpeed
                                onMoved: Taskbar.animSpeed = Math.round(value)
                            }
                            Text { text: Taskbar.animSpeed + " мс"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 56; horizontalAlignment: Text.AlignRight }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: "#22ffffff" }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Отступ открытых окон от панели задач (зазор рабочей области):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 240; wrapMode: Text.WordWrap }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: -14; to: 24; stepSize: 1
                            value: Taskbar.workAreaGap
                            onMoved: Taskbar.workAreaGap = Math.round(value)
                        }
                        Text { text: (Taskbar.workAreaGap > 0 ? "+" : "") + Taskbar.workAreaGap + " px"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 52; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Смещение центральных иконок по горизонтали (X Offset):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 240; wrapMode: Text.WordWrap }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: -600; to: 600; stepSize: 4
                            value: Taskbar.centerOffset
                            onMoved: Taskbar.centerOffset = Math.round(value)
                        }
                        Text { text: (Taskbar.centerOffset > 0 ? "+" : "") + Taskbar.centerOffset + " px"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 56; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 18
                        Text { text: "Скрыть элементы:"; color: "#cccccc"; font.pixelSize: Config.fontSize }
                        StyledCheckBox { text: "Кнопку «Пуск»"; checked: Taskbar.hideStart; onToggled: Taskbar.hideStart = checked }
                        StyledCheckBox { visible: !Taskbar.dockMode; text: "Кнопку «Поиск»"; checked: Taskbar.hideSearch; onToggled: Taskbar.hideSearch = checked }
                        StyledCheckBox { text: "Системный трей и часы"; checked: Taskbar.hideTray; onToggled: Taskbar.hideTray = checked }
                    }
                }
            }

            // 4. ВСПЛЫВАЮЩИЕ МЕНЮ ПУСК, ПОИСК, КАЛЕНДАРЬ И ТРЕЙ
            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colFlyout.implicitHeight + 24

                ColumnLayout {
                    id: colFlyout
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "󰂚  Всплывающие меню «Пуск», Уведомлений и Трея"
                            color: Config.accentColor
                            font.pixelSize: Config.fontSize + 1
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        StyledSwitch {
                            checked: Taskbar.flyoutGlassEnabled
                            onToggled: Taskbar.flyoutGlassEnabled = checked
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Text { text: "Позиция открытия меню «Пуск» и «Поиска»:"; color: "#cccccc"; font.pixelSize: Config.fontSize }
                        SegmentedButton {
                            Layout.fillWidth: true
                            options: ["По кнопке Пуск", "По центру экрана", "Слева (Стандарт)"]
                            selectedIndex: Taskbar.startPopupPos
                            onSelected: (idx) => Taskbar.startPopupPos = idx
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Text { text: "Позиция всплывающих окон трея и календаря:"; color: "#cccccc"; font.pixelSize: Config.fontSize }
                        SegmentedButton {
                            Layout.fillWidth: true
                            options: ["Над треем", "По центру экрана", "Справа (Стандарт)"]
                            selectedIndex: Taskbar.trayPopupPos
                            onSelected: (idx) => Taskbar.trayPopupPos = idx
                        }
                    }

                    Rectangle { visible: Taskbar.flyoutGlassEnabled; Layout.fillWidth: true; height: 1; color: "#22ffffff" }

                    ColumnLayout {
                        visible: Taskbar.flyoutGlassEnabled
                        Layout.fillWidth: true
                        spacing: 6
                        Text { text: "Стиль фона всплывающих меню Windows:"; color: "#cccccc"; font.pixelSize: Config.fontSize }
                        SegmentedButton {
                            Layout.fillWidth: true
                            options: ["Прозрачный", "Размытие (Aero)", "Акрил (Fluent)", "Сплошной"]
                            selectedIndex: Taskbar.flyoutStyle
                            onSelected: (idx) => Taskbar.flyoutStyle = idx
                        }
                    }

                    RowLayout {
                        visible: Taskbar.flyoutGlassEnabled
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: "Оттенок:"; color: "#cccccc"; font.pixelSize: Config.fontSize }
                        Rectangle { width: 22; height: 22; radius: 4; color: Taskbar.flyoutTintColor; border.color: "#55ffffff"; border.width: 1 }
                        StyledButton {
                            text: Taskbar.flyoutTintColor.toString().toUpperCase()
                            onClicked: { tbScroll.colorRole = 2; tbColorModal.openPicker("Цвет меню Пуск, Уведомлений и Трея", Taskbar.flyoutTintColor) }
                        }
                        Text { text: "Непрозрачность:"; color: "#cccccc"; font.pixelSize: Config.fontSize }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.10; to: 1.0; stepSize: 0.02
                            value: Taskbar.flyoutOpacity
                            onMoved: Taskbar.flyoutOpacity = value
                        }
                        Text { text: Math.round(Taskbar.flyoutOpacity * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }
                }
            }
        }
    }
}