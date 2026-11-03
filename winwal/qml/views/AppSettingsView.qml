import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HySett
import "../components"

ScrollView {
    id: appSetScroll
    contentWidth: availableWidth
    clip: true

    Item {
        width: appSetScroll.availableWidth
        implicitHeight: appSetCol.implicitHeight + 40

        ColumnLayout {
            id: appSetCol
            width: Math.min(parent.width - 48, 880)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 20
            spacing: 12

            Text {
                text: Config ? (Config.language, Config.trAuto("Настройки центра управления")) : "Настройки центра управления"
                color: "#ffffff"
                font.pixelSize: Config.fontSize + 3
                font.bold: true
                Layout.bottomMargin: 4
            }

            // КАРТОЧКА АВТООБНОВЛЕНИЯ С GITHUB (whrr-fdev/WinWal)
            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colUpd.implicitHeight + 22

                ColumnLayout {
                    id: colUpd
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "󰇚  Обновление программы (GitHub: whrr-fdev/WinWal)"
                            color: "#ffffff"
                            font.pixelSize: Config.fontSize + 1
                            font.bold: true
                            Layout.fillWidth: true
                        }
                        Text {
                            text: "v" + (Hypr ? Hypr.appVersion : "1.2.0")
                            color: Config.accentColor
                            font.pixelSize: Config.fontSize
                            font.bold: true
                        }
                    }

                    Text {
                        text: Hypr ? Hypr.updateStatus : ""
                        color: (Hypr && Hypr.updateAvailable) ? "#7ee787" : "#bbbbbb"
                        font.pixelSize: Config.fontSize
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    // Ползунок (полоса) прогресса скачивания обновления
                    Rectangle {
                        visible: Hypr ? (Hypr.updateProgress > 0.005) : false
                        Layout.fillWidth: true
                        height: 8
                        radius: 4
                        color: Qt.rgba(1, 1, 1, 0.12)
                        clip: true

                        Rectangle {
                            width: parent.width * Math.min(1.0, Math.max(0.0, Hypr ? Hypr.updateProgress : 0.0))
                            height: parent.height
                            radius: 4
                            color: Config ? Config.accentColor : "#ffffff"
                            Behavior on width { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        StyledButton {
                            Layout.fillWidth: true
                            allowFullWidth: true
                            enabled: Hypr ? !Hypr.checkingUpdate : true
                            text: "󰑐   Проверить обновления"
                            onClicked: if (Hypr) Hypr.checkForUpdates()
                        }

                        StyledButton {
                            Layout.fillWidth: true
                            allowFullWidth: true
                            enabled: Hypr ? !Hypr.checkingUpdate : true
                            text: "󰇚   Обновить с GitHub"
                            onClicked: if (Hypr) Hypr.installUpdate()
                        }
                    }
                }
            }

            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colInt.implicitHeight + 18

                ColumnLayout {
                    id: colInt
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Text { text: Config ? (Config.language, Config.trAuto("Интерфейс")) : "Интерфейс"; color: "#ffffff"; font.pixelSize: Config.fontSize + 1; font.bold: true }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Язык интерфейса (Language):")) : "Язык интерфейса (Language):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                        SegmentedButton {
                            options: ["Русский", "English"]
                            selectedIndex: Config.language === "en" ? 1 : 0
                            onSelected: (idx) => Config.language = (idx === 1 ? "en" : "ru")
                        }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Запоминать последнюю открытую вкладку:")) : "Запоминать последнюю открытую вкладку:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                        StyledSwitch {
                            checked: Config.rememberLastTab
                            onToggled: Config.rememberLastTab = checked
                        }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Высота окна (при след. запуске):")) : "Высота окна (при след. запуске):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 560
                            to: 960
                            stepSize: 10
                            value: Config.windowHeight
                            onMoved: Config.windowHeight = Math.round(value)
                        }
                        Text { text: Config.windowHeight + "px"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Масштаб интерфейса (UI Scale):")) : "Масштаб интерфейса (UI Scale):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.80
                            to: 1.25
                            stepSize: 0.05
                            value: Config.uiScale
                            onMoved: Config.uiScale = value
                        }
                        Text { text: Math.round(Config.uiScale * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Размер шрифта:")) : "Размер шрифта:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 9
                            to: 14
                            stepSize: 1
                            value: Config.fontSize
                            onMoved: Config.fontSize = Math.round(value)
                        }
                        Text { text: Config.fontSize + "px"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Текст бокового меню:")) : "Текст бокового меню:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        Item { Layout.fillWidth: true }
                        SegmentedButton {
                            options: ["Слева", "По центру"]
                            selectedIndex: Config.alignCenter ? 1 : 0
                            onSelected: (idx) => Config.alignCenter = (idx === 1)
                        }
                    }
                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Выравнивание экрана приветствия:")) : "Выравнивание экрана приветствия:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                        SegmentedButton {
                            options: ["Слева", "По центру", "Справа"]
                            selectedIndex: Config.welcomeAlignMode
                            onSelected: (idx) => Config.welcomeAlignMode = idx
                        }
                    }
                }
            }


            
            ColorPickerModal {
                id: colorModal
                property int targetRole: 0
                onColorSelected: (c) => {
                    if (targetRole === 0) Config.accentColor = c
                    else if (targetRole === 1) Config.borderGlowCustomColor = c
                    else if (targetRole === 2) Config.cursorCustomColor = c
                    else if (targetRole === 3) Config.windowBgColor = c
                    else if (targetRole === 4) Config.cardBgColor = c
                }
            }

            // КАРТОЧКА: Цвета и палитра оформления
            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colColors.implicitHeight + 18

                ColumnLayout {
                    id: colColors
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: Config ? (Config.language, Config.trAuto("Цвета и палитра оформления")) : "Цвета и палитра оформления"; color: "#ffffff"; font.pixelSize: Config.fontSize + 1; font.bold: true; Layout.fillWidth: true }
                        StyledButton {
                            text: Config ? (Config.language, Config.trAuto("Сбросить цвета")) : "Сбросить цвета"
                            implicitHeight: 20
                            onClicked: Config.resetColors()
                        }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Синхронизировать цвета с основным акцентом:")) : "Синхронизировать цвета с основным акцентом:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                        StyledSwitch {
                            checked: Config.syncColorsWithAccent
                            onToggled: Config.syncColorsWithAccent = checked
                        }
                    }

                    Repeater {
                        model: [
                            { label: "Основной акцентный цвет:", role: 0, col: Config.accentColor },
                            { label: "Цвет свечения границ:", role: 1, col: Config.borderGlowCustomColor },
                            { label: "Цвет курсора и вспышки:", role: 2, col: Config.cursorCustomColor },
                            { label: "Цвет фона окна:", role: 3, col: Config.windowBgColor },
                            { label: "Цвет карточек и панелей:", role: 4, col: Config.cardBgColor }
                        ]

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                text: Config ? (Config.language, Config.trAuto(modelData.label)) : modelData.label
                                color: "#cccccc"
                                font.pixelSize: Config.fontSize
                                Layout.fillWidth: true
                            }
                            Rectangle {
                                width: 22
                                height: 22
                                radius: (Theme && Theme.isRounded) ? 4 : 0
                                color: modelData.col
                                border.color: Qt.rgba(1, 1, 1, 0.35)
                                border.width: 1
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        colorModal.targetRole = modelData.role
                                        colorModal.openPicker(Config.trAuto(modelData.label), modelData.col)
                                    }
                                }
                            }
                            Text {
                                text: modelData.col.toString().toUpperCase()
                                color: "#aaaaaa"
                                font.pixelSize: Config.fontSize - 1
                                Layout.preferredWidth: 62
                            }
                            StyledButton {
                                text: Config ? (Config.language, Config.trAuto("Изменить")) : "Изменить"
                                implicitHeight: 22
                                onClicked: {
                                    colorModal.targetRole = modelData.role
                                    colorModal.openPicker(Config.trAuto(modelData.label), modelData.col)
                                }
                            }
                        }
                    }
                }
            }

            // КАРТОЧКА: Выбор отображаемых пунктов бокового меню
            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colSideTabs.implicitHeight + 18

                ColumnLayout {
                    id: colSideTabs
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: Config ? (Config.language, Config.trAuto("Отображаемые пункты в боковом меню")) : "Отображаемые пункты в боковом меню"; color: "#ffffff"; font.pixelSize: Config.fontSize + 1; font.bold: true; Layout.fillWidth: true }
                        StyledButton {
                            text: Config ? (Config.language, Config.trAuto("Показать все")) : "Показать все"
                            implicitHeight: 20
                            visible: Config.hiddenSidebarTabs.length > 0
                            onClicked: Config.hiddenSidebarTabs = ""
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: 12
                        rowSpacing: 6

                        Repeater {
                            model: [
                                { title: Config ? (Config.language, Config.trAuto("󰖲  Окна")) : "󰖲  Окна", idx: 1 },
                                { title: Config ? (Config.language, Config.trAuto("󱓞  Панель задач")) : "󱓞  Панель задач", idx: 8 },
                                { title: Config ? (Config.language, Config.trAuto("󰸉  Обои")) : "󰸉  Обои", idx: 14 },
                                { title: Config ? (Config.language, Config.trAuto("󰕰  Виджеты")) : "󰕰  Виджеты", idx: 15 },
                                { title: Config ? (Config.language, Config.trAuto("󰖗  Атмосфера")) : "󰖗  Атмосфера", idx: 19 }
                            ]

                            StyledCheckBox {
                                text: modelData.title
                                checked: {
                                    var _ = Config.hiddenSidebarTabs
                                    return Config.isTabVisible(modelData.idx)
                                }
                                onToggled: Config.setTabVisible(modelData.idx, checked)
                            }
                        }
                    }
                }
            }

            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colGlow.implicitHeight + 18

                ColumnLayout {
                    id: colGlow
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Text { text: Config ? (Config.language, Config.trAuto("Подсветка границ окна и курсор")) : "Подсветка границ окна и курсор"; color: "#ffffff"; font.pixelSize: Config.fontSize + 1; font.bold: true }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Режим отображения границ:")) : "Режим отображения границ:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                        SegmentedButton {
                            options: ["На всех экранах", "Только приветствие", "Выкл"]
                            selectedIndex: !Config.borderGlowEnabled ? 2 : (Config.borderGlowAllPages ? 0 : 1)
                            onSelected: (idx) => {
                                if (idx === 0) {
                                    Config.borderGlowEnabled = true
                                    Config.borderGlowAllPages = true
                                } else if (idx === 1) {
                                    Config.borderGlowEnabled = true
                                    Config.borderGlowAllPages = false
                                } else {
                                    Config.borderGlowEnabled = false
                                }
                            }
                        }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Направление движения:")) : "Направление движения:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                        SegmentedButton {
                            options: ["По часовой", "Против часовой", "Навстречу"]
                            selectedIndex: Config.borderGlowDirection
                            onSelected: (idx) => Config.borderGlowDirection = idx
                        }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Палитра свечения:")) : "Палитра свечения:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                        SegmentedButton {
                            options: ["Монохром", "Холодный лед", "Аврора", "Теплый неон", "Свой цвет"]
                            selectedIndex: Config.borderGlowColorMode
                            onSelected: (idx) => Config.borderGlowColorMode = idx
                        }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Яркость свечения границ:")) : "Яркость свечения границ:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.05
                            to: 1.0
                            value: Config.borderGlowIntensity
                            onMoved: Config.borderGlowIntensity = value
                        }
                        Text { text: Math.round(Config.borderGlowIntensity * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Ширина свечения у края:")) : "Ширина свечения у края:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.10
                            to: 1.0
                            value: Config.borderGlowSpread
                            onMoved: Config.borderGlowSpread = value
                        }
                        Text { text: Math.round(Config.borderGlowSpread * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Скорость движения света:")) : "Скорость движения света:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.10
                            to: 2.50
                            value: Config.borderGlowSpeed
                            onMoved: Config.borderGlowSpeed = value
                        }
                        Text { text: Math.round(Config.borderGlowSpeed * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Сила свечения и следа от курсора:")) : "Сила свечения и следа от курсора:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.0
                            to: 1.0
                            value: Config.cursorGlowIntensity
                            onMoved: Config.cursorGlowIntensity = value
                        }
                        Text { text: Math.round(Config.cursorGlowIntensity * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Форма вспышки / волны при клике:")) : "Форма вспышки / волны при клике:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                        SegmentedButton {
                            options: ["Кольцо", "Импульс", "Звезда", "Двойная волна"]
                            selectedIndex: Config.clickFlashShape
                            onSelected: (idx) => Config.clickFlashShape = idx
                        }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Сила вспышки при клике:")) : "Сила вспышки при клике:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.0
                            to: 1.0
                            value: Config.clickFlashIntensity
                            onMoved: Config.clickFlashIntensity = value
                        }
                        Text { text: Math.round(Config.clickFlashIntensity * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }
                }
            }

            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colAnim.implicitHeight + 18

                ColumnLayout {
                    id: colAnim
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Text { text: Config ? (Config.language, Config.trAuto("Анимации переходов между вкладками")) : "Анимации переходов между вкладками"; color: "#ffffff"; font.pixelSize: Config.fontSize + 1; font.bold: true }

                                        ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: Config ? (Config.language, Config.trAuto("Переход с экрана приветствия:")) : "Переход с экрана приветствия:"; color: "#cccccc"; font.pixelSize: Config.fontSize }
                        SegmentedButton {
                            Layout.fillWidth: true
                            options: ["Слайд", "Глубина", "Пружина", "Воронка", "Портал", "Телепорт", "Выкл"]
                            selectedIndex: {
                                var t = Config.welcomeAnimType
                                if (t === "depth") return 1
                                if (t === "bounce") return 2
                                if (t === "blackhole") return 3
                                if (t === "portal") return 4
                                if (t === "shatter") return 5
                                if (t === "none") return 6
                                return 0
                            }
                            onSelected: (idx) => {
                                Config.welcomeAnimType = ["slide", "depth", "bounce", "blackhole", "portal", "shatter", "none"][idx]
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: Config ? (Config.language, Config.trAuto("Тип анимации между вкладками:")) : "Тип анимации между вкладками:"; color: "#cccccc"; font.pixelSize: Config.fontSize }
                        SegmentedButton {
                            Layout.fillWidth: true
                            options: ["Слайд", "Глубина", "Пружина", "Воронка", "Портал", "Телепорт", "Выкл"]
                            selectedIndex: {
                                var t = Config.animType
                                if (t === "depth") return 1
                                if (t === "bounce") return 2
                                if (t === "blackhole") return 3
                                if (t === "portal") return 4
                                if (t === "shatter") return 5
                                if (t === "none") return 6
                                return 0
                            }
                            onSelected: (idx) => {
                                Config.animType = ["slide", "depth", "bounce", "blackhole", "portal", "shatter", "none"][idx]
                            }
                        }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Длительность переходов:")) : "Длительность переходов:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 50
                            to: 600
                            stepSize: 10
                            value: Config.animDuration
                            onMoved: Config.animDuration = Math.round(value)
                        }
                        Text { text: Config.animDuration + "ms"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }
                }
            }

            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: colOp.implicitHeight + 18

                ColumnLayout {
                    id: colOp
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Text { text: Config ? (Config.language, Config.trAuto("Прозрачность и эффекты стекла (Liquid Glass)")) : "Прозрачность и эффекты стекла (Liquid Glass)"; color: "#ffffff"; font.pixelSize: Config.fontSize + 1; font.bold: true }

                    

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Главный фон (Окно):")) : "Главный фон (Окно):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.0
                            to: 1.0
                            value: Config.opacity
                            onMoved: Config.opacity = value
                        }
                        Text { text: Math.round(Config.opacity * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Интенсивность блюра окна:")) : "Интенсивность блюра окна:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.0
                            to: 1.0
                            stepSize: 0.02
                            value: Theme.windowBlur
                            onMoved: Theme.windowBlur = value
                        }
                        Text {
                            text: Theme.windowBlur <= 0.01 ? "Выкл" : Math.round(Theme.windowBlur * 100) + "%"
                            color: "#ffffff"
                            font.pixelSize: Config.fontSize
                            Layout.preferredWidth: 44
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Прозрачность диалоговых окон:")) : "Прозрачность диалоговых окон:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.05
                            to: 1.0
                            value: Config.dialogOpacity
                            onMoved: Config.dialogOpacity = value
                        }
                        Text { text: Math.round(Config.dialogOpacity * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Блюр диалоговых окон:")) : "Блюр диалоговых окон:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.0
                            to: 1.0
                            stepSize: 0.02
                            value: Config.dialogBlur
                            onMoved: Config.dialogBlur = value
                        }
                        Text {
                            text: Config.dialogBlur <= 0.01 ? "Выкл" : Math.round(Config.dialogBlur * 100) + "%"
                            color: "#ffffff"
                            font.pixelSize: Config.fontSize
                            Layout.preferredWidth: 44
                            horizontalAlignment: Text.AlignRight
                        }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Искажение стекла (Refraction):")) : "Искажение стекла (Refraction):"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.0
                            to: 1.0
                            value: Theme.glassRefraction
                            onMoved: Theme.glassRefraction = value
                        }
                        Text { text: Math.round(Theme.glassRefraction * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Боковое меню:")) : "Боковое меню:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.0
                            to: 1.0
                            value: Config.sidebarOpacity
                            onMoved: Config.sidebarOpacity = value
                        }
                        Text { text: Math.round(Config.sidebarOpacity * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }

                    RowLayout {
                        Text { text: Config ? (Config.language, Config.trAuto("Карточки и панели:")) : "Карточки и панели:"; color: "#cccccc"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 200 }
                        StyledSlider {
                            Layout.fillWidth: true
                            from: 0.0
                            to: 1.0
                            value: Config.cardOpacity
                            onMoved: Config.cardOpacity = value
                        }
                        Text { text: Math.round(Config.cardOpacity * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 44; horizontalAlignment: Text.AlignRight }
                    }
                }
            }

            StyledButton {
                text: Config ? (Config.language, Config.trAuto("↺   Сбросить настройки центра по умолчанию")) : "↺   Сбросить настройки центра по умолчанию"
                Layout.alignment: Qt.AlignHCenter
                implicitHeight: 26
                onClicked: Config.resetDefaults()
            }
        }
    }
}
