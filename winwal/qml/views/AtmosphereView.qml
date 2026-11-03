import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import HySett
import "../components"

ScrollView {
    id: atmoScroll
    contentWidth: availableWidth
    clip: true
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    component SwitchRow: RowLayout {
        property string label: ""
        property bool checkedVal: false
        signal toggledVal(bool v)
        Layout.fillWidth: true
        spacing: 10
        Text {
            text: Config.trAuto(parent.label)
            color: "#e2e8f0"
            font.pixelSize: Config.fontSize
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        StyledSwitch {
            checked: parent.checkedVal
            onToggled: parent.toggledVal(checked)
        }
    }

    component SliderRow: RowLayout {
        property string label: ""
        property string valText: ""
        property real fromVal: 0.0
        property real toVal: 1.0
        property real stepVal: 0.05
        property real curVal: 0.5
        signal movedVal(real v)
        Layout.fillWidth: true
        spacing: 10
        Text {
            text: Config.trAuto(parent.label)
            color: "#cbd5e1"
            font.pixelSize: Config.fontSize
            Layout.preferredWidth: 255
            elide: Text.ElideRight
        }
        Text {
            text: parent.valText
            color: Config.accentColor
            font.pixelSize: Config.fontSize
            font.bold: true
            horizontalAlignment: Text.AlignRight
            Layout.minimumWidth: 74
        }
        StyledSlider {
            Layout.fillWidth: true
            from: parent.fromVal
            to: parent.toVal
            stepSize: parent.stepVal
            value: parent.curVal
            onMoved: parent.movedVal(value)
        }
    }

    Item {
        width: atmoScroll.availableWidth
        implicitHeight: mainCol.implicitHeight + 40

        ColumnLayout {
            id: mainCol
            width: Math.min(parent.width - 36, 860)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 18
            spacing: 14

            readonly property bool anyAtmoActive: Atmosphere.inAppEnabled || Atmosphere.globalOverlayEnabled
            readonly property bool isRainMode: Atmosphere.presetProfile === 0 || Atmosphere.presetProfile === 2
            readonly property bool isSnowMode: Atmosphere.presetProfile === 1 || Atmosphere.presetProfile === 3
            readonly property bool isHalloweenMode: Atmosphere.presetProfile === 2

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text {
                    text: Config.trAuto("Живая атмосфера")
                    color: "#ffffff"
                    font.pixelSize: Config.fontSize + 5
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                StyledButton {
                    visible: mainCol.anyAtmoActive && mainCol.isRainMode
                    text: Config.trAuto("󱐋 Гром")
                    onClicked: Atmosphere.triggerThunderstorm()
                }
                StyledButton {
                    visible: mainCol.anyAtmoActive
                    text: Config.trAuto("󰖝 Ветер")
                    onClicked: Atmosphere.triggerWindGust()
                }
                StyledButton {
                    visible: mainCol.anyAtmoActive
                    text: Config.trAuto(mainCol.isSnowMode ? "󰃢 Сбросить снег и следы" : "󰃢 Сбросить капли")
                    onClicked: Atmosphere.clearAccumulations()
                }
            }

            GlassPanel {
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: modeCol.implicitHeight + 28
                ColumnLayout {
                    id: modeCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10
                    Text { text: Config.trAuto("Режим работы и сезонные профили"); color: Config.accentColor; font.pixelSize: Config.fontSize + 2; font.bold: true }
                    SwitchRow { label: "Внутри окна WinWal (Песочница с физикой UI):"; checkedVal: Atmosphere.inAppEnabled; onToggledVal: (v) => Atmosphere.inAppEnabled = v }
                    SwitchRow { label: "Глобальный оверлей всего экрана (Рабочий стол и открытые окна):"; checkedVal: Atmosphere.globalOverlayEnabled; onToggledVal: (v) => Atmosphere.globalOverlayEnabled = v }
                    SegmentedButton {
                        visible: mainCol.anyAtmoActive
                        Layout.fillWidth: true
                        options: ["󰖗  Дождь", "󰼶  Снег"]
                        selectedIndex: (Atmosphere.presetProfile === 1 || Atmosphere.presetProfile === 3) ? 1 : 0
                        onSelected: (idx) => Atmosphere.applySeasonalProfile(idx)
                    }
                }
            }

            // КАРТОЧКА НАСТРОЕК ГЛОБАЛЬНОГО ОВЕРЛЕЯ (ВИДНА ТОЛЬКО ПРИ ВКЛЮЧЕННОМ ГЛОБАЛЬНОМ ОВЕРЛЕЕ!)
            GlassPanel {
                visible: mainCol.anyAtmoActive
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: ovCol.implicitHeight + 28
                ColumnLayout {
                    id: ovCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10
                    Text { text: Config.trAuto("Настройки глобального оверлея и синхронизации"); color: Config.accentColor; font.pixelSize: Config.fontSize + 2; font.bold: true }
                    SwitchRow {
                        label: "Синхронизация с окном WinWal (убирать дубли осадков в окне):"
                        checkedVal: Atmosphere.syncWithOverlay
                        onToggledVal: (v) => Atmosphere.syncWithOverlay = v
                    }
                    SwitchRow {
                        label: "Взаимодействие с открытыми окнами Windows (капли, сугробы, промерзание):"
                        checkedVal: Atmosphere.windowInteractEnabled
                        onToggledVal: (v) => Atmosphere.windowInteractEnabled = v
                    }

                    // Настройки оверлея ТОЛЬКО для режима ДОЖДЯ / ХЭЛЛОУИНА
                    SwitchRow {
                        visible: mainCol.isRainMode
                        label: "Запотевание по краям экрана на рабочем столе:"
                        checkedVal: Atmosphere.overlayScreenFogEnabled
                        onToggledVal: (v) => Atmosphere.overlayScreenFogEnabled = v
                    }
                    SliderRow {
                        visible: mainCol.isRainMode && Atmosphere.overlayScreenFogEnabled
                        label: "Сила запотевания краёв экрана:"
                        valText: Math.round(Atmosphere.overlayScreenFogIntensity * 100) + "%"
                        fromVal: 0.05; toVal: 1.0
                        curVal: Atmosphere.overlayScreenFogIntensity
                        onMovedVal: (v) => Atmosphere.overlayScreenFogIntensity = v
                    }
                    SliderRow {
                        visible: mainCol.isRainMode && Atmosphere.overlayScreenFogEnabled
                        label: "Глубина запотевания оверлея (от краёв к центру):"
                        valText: Math.round(Atmosphere.overlayFogSpread * 100) + "%"
                        fromVal: 0.0; toVal: 1.0
                        curVal: Atmosphere.overlayFogSpread
                        onMovedVal: (v) => Atmosphere.overlayFogSpread = v
                    }
                    SliderRow {
                        visible: mainCol.isRainMode
                        label: "Обилие собирающихся капель на окнах:"
                        valText: Math.round(Atmosphere.overlayDropsIntensity * 100) + "%"
                        fromVal: 0.20; toVal: 2.0; stepVal: 0.10
                        curVal: Atmosphere.overlayDropsIntensity
                        onMovedVal: (v) => Atmosphere.overlayDropsIntensity = v
                    }
                    SliderRow {
                        visible: mainCol.isRainMode
                        label: "Количество капель по краям экрана:"
                        valText: Math.round(Atmosphere.screenDropsAmount * 100) + "%"
                        fromVal: 0.0; toVal: 2.0; stepVal: 0.05
                        curVal: Atmosphere.screenDropsAmount
                        onMovedVal: (v) => Atmosphere.screenDropsAmount = v
                    }
                    SliderRow {
                        visible: mainCol.isRainMode
                        label: "Количество капель на открытых окнах:"
                        valText: Math.round(Atmosphere.windowDropsAmount * 100) + "%"
                        fromVal: 0.0; toVal: 2.0; stepVal: 0.05
                        curVal: Atmosphere.windowDropsAmount
                        onMovedVal: (v) => Atmosphere.windowDropsAmount = v
                    }
                    
                    
                    SliderRow {
                        visible: mainCol.isRainMode && Atmosphere.windowInteractEnabled && Atmosphere.condensationEnabled
                        label: "Сила запотевания открытых окон:"
                        valText: Math.round(Atmosphere.windowFogIntensity * 100) + "%"
                        fromVal: 0.0; toVal: 1.0
                        curVal: Atmosphere.windowFogIntensity
                        onMovedVal: (v) => Atmosphere.windowFogIntensity = v
                    }


                }
            }

            // КАРТОЧКА ТЕКУЩЕГО ПОГОДНОГО РЕЖИМА (ПОКАЗЫВАЕТ ТОЛЬКО НАСТРОЙКИ АКТИВНОГО РЕЖИМА!)
            GlassPanel {
                visible: mainCol.anyAtmoActive
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: weatherCol.implicitHeight + 28
                ColumnLayout {
                    id: weatherCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10
                    Text {
                        text: Config.trAuto(mainCol.isSnowMode
                            ? "Настройки снегопада, сугробов и кристаллической изморози"
                            : (mainCol.isHalloweenMode ? "Настройки дождя, грозы и осенних листьев" : "Настройки дождя и оптики капель"))
                        color: Config.accentColor
                        font.pixelSize: Config.fontSize + 2
                        font.bold: true
                    }

                    // === БЛОК ДОЖДЯ (виден ТОЛЬКО в режимах Дождь и Хэллоуин) ===
                    SwitchRow { visible: mainCol.isRainMode; label: "Включить падающие струи дождя (линии сверху):"; checkedVal: Atmosphere.rainEnabled; onToggledVal: (v) => Atmosphere.rainEnabled = v }
                                        SliderRow { visible: mainCol.isRainMode && Atmosphere.rainEnabled; label: "Интенсивность дождя:"; valText: Math.round(Atmosphere.rainIntensity * 100) + "%"; fromVal: 0.05; toVal: 1.0; curVal: Atmosphere.rainIntensity; onMovedVal: (v) => Atmosphere.rainIntensity = v }
                    SliderRow { visible: mainCol.isRainMode && Atmosphere.rainEnabled; label: "Размер и длина падающих струй дождя:"; valText: Math.round(Atmosphere.particleScale * 100) + "%"; fromVal: 0.25; toVal: 2.20; stepVal: 0.05; curVal: Atmosphere.particleScale; onMovedVal: (v) => Atmosphere.particleScale = v }
                                        SwitchRow { visible: mainCol.isRainMode; label: "Эффект «мокрого стекла» и стекающие капли (работает и без линий дождя):"; checkedVal: Atmosphere.wetGlassEnabled; onToggledVal: (v) => Atmosphere.wetGlassEnabled = v }
                    SliderRow { visible: mainCol.isRainMode && Atmosphere.wetGlassEnabled; label: "Количество собирающихся и падающих капель:"; valText: Math.round(Atmosphere.overlayDropsIntensity * 100) + "%"; fromVal: 0.10; toVal: 3.00; stepVal: 0.10; curVal: Atmosphere.overlayDropsIntensity; onMovedVal: (v) => { Atmosphere.overlayDropsIntensity = v; Atmosphere.screenDropsAmount = v } }
                    SliderRow { visible: mainCol.isRainMode && Atmosphere.wetGlassEnabled; label: "Размер собирающихся и падающих капель:"; valText: Math.round(Atmosphere.dropScale * 100) + "%"; fromVal: 0.40; toVal: 2.00; stepVal: 0.05; curVal: Atmosphere.dropScale; onMovedVal: (v) => Atmosphere.dropScale = v }
                    SliderRow { visible: mainCol.isRainMode && Atmosphere.wetGlassEnabled; label: "Скорость набухания, срыва и падения капель:"; valText: Math.round(Atmosphere.dropSlideSpeed * 100) + "%"; fromVal: 0.20; toVal: 6.00; stepVal: 0.10; curVal: Atmosphere.dropSlideSpeed; onMovedVal: (v) => Atmosphere.dropSlideSpeed = v }
                    SwitchRow { visible: mainCol.isRainMode && Atmosphere.wetGlassEnabled; label: "Визуальное скопление воды на плоскостях (с убыванием при стекании):"; checkedVal: Atmosphere.surfacePuddlesEnabled; onToggledVal: (v) => Atmosphere.surfacePuddlesEnabled = v }
                    RowLayout {
                        Layout.fillWidth: true; visible: mainCol.isRainMode && Atmosphere.wetGlassEnabled; spacing: 8
                        Text { text: Config.trAuto("Следы за стекающими каплями:"); color: "#cbd5e1"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 210 }
                        SegmentedButton {
                            Layout.fillWidth: true
                            options: ["Микро-капли (Жемчуг)", "Мягкий след", "Без следов"]
                            selectedIndex: Atmosphere.trailStyle
                            onSelected: (idx) => Atmosphere.trailStyle = idx
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true; visible: mainCol.isRainMode && Atmosphere.wetGlassEnabled; spacing: 8
                        Text { text: Config.trAuto("Цвет и оттенок капель:"); color: "#cbd5e1"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 190 }
                        SegmentedButton {
                            Layout.fillWidth: true
                            options: ["󰖌 Прозрачный", "■ Тёмный", "◆ Голубой", "󰏘 Свой цвет"]
                            selectedIndex: Atmosphere.dropTintMode
                            onSelected: (idx) => Atmosphere.dropTintMode = idx
                        }
                    }
                    SliderRow {
                        visible: mainCol.isRainMode && Atmosphere.wetGlassEnabled && Atmosphere.dropTintMode === 3
                        label: "Оттенок капли — Красный (R):"
                        valText: Math.round(Atmosphere.dropTintR * 100) + "%"
                        fromVal: 0.0; toVal: 1.0; stepVal: 0.02
                        curVal: Atmosphere.dropTintR
                        onMovedVal: (v) => Atmosphere.dropTintR = v
                    }
                    SliderRow {
                        visible: mainCol.isRainMode && Atmosphere.wetGlassEnabled && Atmosphere.dropTintMode === 3
                        label: "Оттенок капли — Зелёный (G):"
                        valText: Math.round(Atmosphere.dropTintG * 100) + "%"
                        fromVal: 0.0; toVal: 1.0; stepVal: 0.02
                        curVal: Atmosphere.dropTintG
                        onMovedVal: (v) => Atmosphere.dropTintG = v
                    }
                    SliderRow {
                        visible: mainCol.isRainMode && Atmosphere.wetGlassEnabled && Atmosphere.dropTintMode === 3
                        label: "Оттенок капли — Синий (B):"
                        valText: Math.round(Atmosphere.dropTintB * 100) + "%"
                        fromVal: 0.0; toVal: 1.0; stepVal: 0.02
                        curVal: Atmosphere.dropTintB
                        onMovedVal: (v) => Atmosphere.dropTintB = v
                    }
                    SliderRow {
                        visible: mainCol.isRainMode && Atmosphere.wetGlassEnabled
                        label: "Затемнение / плотность цвета капель:"
                        valText: Math.round(Atmosphere.dropTintStrength * 100) + "%"
                        fromVal: 0.0; toVal: 1.0; stepVal: 0.02
                        curVal: Atmosphere.dropTintStrength
                        onMovedVal: (v) => Atmosphere.dropTintStrength = v
                    }
                    SliderRow {
                        visible: mainCol.isRainMode && Atmosphere.wetGlassEnabled
                        label: "Яркость бликов и линзы капель:"
                        valText: Math.round(Atmosphere.dropHighlightStrength * 100) + "%"
                        fromVal: 0.10; toVal: 2.0; stepVal: 0.05
                        curVal: Atmosphere.dropHighlightStrength
                        onMovedVal: (v) => Atmosphere.dropHighlightStrength = v
                    }
                    SliderRow {
                        visible: mainCol.isRainMode && Atmosphere.wetGlassEnabled
                        label: "Сила 3D-преломления капель:"
                        valText: Math.round(Atmosphere.dropRefraction * 100) + "%"
                        fromVal: 0.20; toVal: 2.0; stepVal: 0.05
                        curVal: Atmosphere.dropRefraction
                        onMovedVal: (v) => Atmosphere.dropRefraction = v
                    }
                    SliderRow {
                        visible: mainCol.isRainMode && Atmosphere.wetGlassEnabled && Atmosphere.condensationEnabled
                        label: "Заметность протёртого следа в тумане:"
                        valText: Math.round(Atmosphere.wipeTrailStrength * 100) + "%"
                        fromVal: 0.0; toVal: 1.0; stepVal: 0.05
                        curVal: Atmosphere.wipeTrailStrength
                        onMovedVal: (v) => Atmosphere.wipeTrailStrength = v
                    }
                    SwitchRow { visible: mainCol.isRainMode && Atmosphere.rainEnabled; label: "Гроза и вспышки молнии сверху:"; checkedVal: Atmosphere.thunderEnabled; onToggledVal: (v) => Atmosphere.thunderEnabled = v }

                    // === БЛОК СНЕГА (виден ТОЛЬКО в режимах Снег и Новый год) ===
                    SwitchRow { visible: mainCol.isSnowMode; label: "Включить снегопад:"; checkedVal: Atmosphere.snowEnabled; onToggledVal: (v) => Atmosphere.snowEnabled = v }
                    SliderRow { visible: mainCol.isSnowMode && Atmosphere.snowEnabled; label: "Интенсивность снега:"; valText: Math.round(Atmosphere.snowIntensity * 100) + "%"; fromVal: 0.05; toVal: 1.0; curVal: Atmosphere.snowIntensity; onMovedVal: (v) => Atmosphere.snowIntensity = v }
                    SliderRow { visible: mainCol.isSnowMode && Atmosphere.snowEnabled; label: "Размер кристаллических снежинок:"; valText: Math.round(Atmosphere.snowflakeScale * 100) + "%"; fromVal: 0.30; toVal: 6.00; stepVal: 0.10; curVal: Atmosphere.snowflakeScale; onMovedVal: (v) => Atmosphere.snowflakeScale = v }
                    SwitchRow { visible: mainCol.isSnowMode && Atmosphere.snowEnabled; label: "Прилипание снежинок с коротким замерзающим следом:"; checkedVal: Atmosphere.snowStickMeltEnabled; onToggledVal: (v) => Atmosphere.snowStickMeltEnabled = v }
                    SliderRow { visible: mainCol.isSnowMode && Atmosphere.snowEnabled && Atmosphere.snowStickMeltEnabled; label: "Количество налипающего и тающего снега:"; valText: Math.round(Atmosphere.snowMeltAmount * 100) + "%"; fromVal: 0.10; toVal: 2.00; stepVal: 0.05; curVal: Atmosphere.snowMeltAmount; onMovedVal: (v) => Atmosphere.snowMeltAmount = v }
                    SwitchRow { visible: mainCol.isSnowMode && Atmosphere.snowEnabled; label: "Накопление снежных горок на окнах и карточках:"; checkedVal: Atmosphere.snowDriftEnabled; onToggledVal: (v) => Atmosphere.snowDriftEnabled = v }
                    SliderRow { visible: mainCol.isSnowMode && Atmosphere.snowEnabled && Atmosphere.snowDriftEnabled; label: "Высота и объём снежных горок:"; valText: Math.round(Atmosphere.snowDriftScale * 100) + "%"; fromVal: 0.30; toVal: 2.50; stepVal: 0.05; curVal: Atmosphere.snowDriftScale; onMovedVal: (v) => Atmosphere.snowDriftScale = v }
                    SwitchRow { visible: mainCol.isSnowMode && Atmosphere.snowEnabled; label: "Кристаллическая изморозь по краям экрана:"; checkedVal: Atmosphere.snowScreenFrostEnabled; onToggledVal: (v) => Atmosphere.snowScreenFrostEnabled = v }
                    SliderRow { visible: mainCol.isSnowMode && Atmosphere.snowEnabled && Atmosphere.snowScreenFrostEnabled; label: "Сила изморози по краям экрана:"; valText: Math.round(Atmosphere.snowScreenFrostIntensity * 100) + "%" ; fromVal: 0.01; toVal: 1.00; stepVal: 0.01; curVal: Atmosphere.snowScreenFrostIntensity; onMovedVal: (v) => Atmosphere.snowScreenFrostIntensity = v }
                    SliderRow { visible: mainCol.isSnowMode && Atmosphere.snowEnabled && Atmosphere.snowScreenFrostEnabled; label: "Глубина изморози экрана (к центру):"; valText: Math.round(Atmosphere.snowScreenFrostSpread * 100) + "%" ; fromVal: 0.00; toVal: 1.00; stepVal: 0.05; curVal: Atmosphere.snowScreenFrostSpread; onMovedVal: (v) => Atmosphere.snowScreenFrostSpread = v }
                    SliderRow { visible: mainCol.isSnowMode && Atmosphere.snowEnabled; label: "Промерзание открытых окон кристаллами:"; valText: Math.round(Atmosphere.snowWindowFrostIntensity * 100) + "%"; fromVal: 0.00; toVal: 1.00; stepVal: 0.01; curVal: Atmosphere.snowWindowFrostIntensity; onMovedVal: (v) => Atmosphere.snowWindowFrostIntensity = v }
                    RowLayout {
                        Layout.fillWidth: true; visible: mainCol.isSnowMode && Atmosphere.snowEnabled; spacing: 8
                        Text { text: Config.trAuto("Форма замерзания и инея:"); color: "#cbd5e1"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 190 }
                        SegmentedButton {
                            Layout.fillWidth: true
                            options: ["󰼶 Бархатный иней", "󰌪 Ледяные иглы", "✦ Звёздная корка"]
                            selectedIndex: Atmosphere.snowFrostStyle
                            onSelected: (idx) => Atmosphere.snowFrostStyle = idx
                        }
                    }
                    SwitchRow {
                        visible: mainCol.isSnowMode && Atmosphere.snowEnabled && Atmosphere.snowFrostStyle === 0
                        label: "Морозные снежинки местами в бархатном инее:"
                        checkedVal: Atmosphere.snowVelvetFlakesEnabled
                        onToggledVal: (v) => Atmosphere.snowVelvetFlakesEnabled = v
                    }
                    SliderRow { visible: mainCol.isSnowMode && Atmosphere.snowEnabled; label: "Плотность и яркость кристаллов льда:"; valText: Math.round(Atmosphere.snowFrostPatternStrength * 100) + "%"; fromVal: 0.00; toVal: 2.00; stepVal: 0.05; curVal: Atmosphere.snowFrostPatternStrength; onMovedVal: (v) => Atmosphere.snowFrostPatternStrength = v }

                    // === БЛОК ЛИСТЬЕВ (виден в режимах Дождь и Хэллоуин) ===
                    Rectangle { visible: mainCol.isRainMode; Layout.fillWidth: true; height: 1; color: "#22ffffff" }
                    SwitchRow { visible: mainCol.isRainMode; label: "Добавить падающие листья:"; checkedVal: Atmosphere.leavesEnabled; onToggledVal: (v) => Atmosphere.leavesEnabled = v }
                    SliderRow { visible: mainCol.isRainMode && Atmosphere.leavesEnabled; label: "Количество листьев (от 1 шт.):"; valText: Math.round(Atmosphere.leavesIntensity * 100) + "%"; fromVal: 0.01; toVal: 1.0; stepVal: 0.01; curVal: Atmosphere.leavesIntensity; onMovedVal: (v) => Atmosphere.leavesIntensity = v }
                    SliderRow { visible: mainCol.isRainMode && Atmosphere.leavesEnabled; label: "Размер падающих листьев:"; valText: Math.round(Atmosphere.leavesScale * 100) + "%"; fromVal: 0.20; toVal: 6.00; stepVal: 0.10; curVal: Atmosphere.leavesScale; onMovedVal: (v) => Atmosphere.leavesScale = v }
                    SliderRow { visible: mainCol.isRainMode && Atmosphere.leavesEnabled; label: "Детализация и 3D-изгиб листьев:"; valText: Math.round(Atmosphere.leavesDetail * 100) + "%"; fromVal: 0.0; toVal: 2.00; stepVal: 0.05; curVal: Atmosphere.leavesDetail; onMovedVal: (v) => Atmosphere.leavesDetail = v }
                    RowLayout {
                        Layout.fillWidth: true; visible: mainCol.isRainMode && Atmosphere.leavesEnabled; spacing: 8
                        Text { text: Config.trAuto("Палитра листьев:"); color: "#cbd5e1"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 160 }
                        SegmentedButton {
                            Layout.fillWidth: true
                            options: ["Багряная осень", "Золотой клен", "Сакура", "Лес"]
                            selectedIndex: Atmosphere.leavesPalette
                            onSelected: (idx) => Atmosphere.leavesPalette = idx
                        }
                    }
                }
            }

            GlassPanel {
                visible: mainCol.anyAtmoActive
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: physCol.implicitHeight + 28
                ColumnLayout {
                    id: physCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10
                    Text { text: Config.trAuto("Физика, ветер и реакция на бездействие"); color: Config.accentColor; font.pixelSize: Config.fontSize + 2; font.bold: true }
                    SliderRow { label: "Сила и направление ветра:"; valText: Math.round(Atmosphere.windStrength * 100) + "%"; fromVal: -1.0; toVal: 1.0; curVal: Atmosphere.windStrength; onMovedVal: (v) => Atmosphere.windStrength = v }
                    SliderRow { label: "Турбулентность воздуха:"; valText: Math.round(Atmosphere.windTurbulence * 100) + "%"; fromVal: 0.0; toVal: 1.0; curVal: Atmosphere.windTurbulence; onMovedVal: (v) => Atmosphere.windTurbulence = v }
                    SliderRow { label: "Гравитация (скорость падения):"; valText: Atmosphere.gravityScale.toFixed(2) + "x"; fromVal: 0.4; toVal: 4.0; stepVal: 0.10; curVal: Atmosphere.gravityScale; onMovedVal: (v) => Atmosphere.gravityScale = v }
                    SliderRow { label: "Размер частиц:"; valText: Atmosphere.particleScale.toFixed(2) + "x"; fromVal: 0.20; toVal: 1.8; stepVal: 0.05; curVal: Atmosphere.particleScale; onMovedVal: (v) => Atmosphere.particleScale = v }
                    Rectangle { Layout.fillWidth: true; height: 1; color: "#22ffffff" }
                    SwitchRow { label: "Коллизии со всеми элементами внутри WinWal:"; checkedVal: Atmosphere.uiCollisionsEnabled; onToggledVal: (v) => Atmosphere.uiCollisionsEnabled = v }
                    SwitchRow { visible: mainCol.isRainMode; label: "Эффект запотевания стекла и конденсата:"; checkedVal: Atmosphere.condensationEnabled; onToggledVal: (v) => Atmosphere.condensationEnabled = v }
                    SliderRow { visible: mainCol.isRainMode && Atmosphere.condensationEnabled; label: "Сила запотевания в окне WinWal и на карточках:"; valText: Math.round(Atmosphere.fogIntensity * 100) + "%"; fromVal: 0.0; toVal: 1.0; curVal: Atmosphere.fogIntensity; onMovedVal: (v) => Atmosphere.fogIntensity = v }
                    SliderRow { visible: mainCol.isRainMode && Atmosphere.condensationEnabled; label: "Глубина запотевания окна WinWal (от краёв к центру):"; valText: Math.round(Atmosphere.fogSpread * 100) + "%"; fromVal: 0.0; toVal: 1.0; curVal: Atmosphere.fogSpread; onMovedVal: (v) => Atmosphere.fogSpread = v }
                    SwitchRow {
                        visible: Atmosphere.condensationEnabled || mainCol.isSnowMode
                        label: mainCol.isSnowMode ? "Сильное промерзание экрана при бездействии:" : "Сильное запотевание при бездействии экрана:"
                        checkedVal: Atmosphere.idleFogEnabled
                        onToggledVal: (v) => Atmosphere.idleFogEnabled = v
                    }
                    SliderRow {
                        visible: (Atmosphere.condensationEnabled || mainCol.isSnowMode) && Atmosphere.idleFogEnabled
                        label: "Время неактивности до срабатывания:"
                        valText: Atmosphere.idleFogTimeout < 60
                            ? (Atmosphere.idleFogTimeout + " сек")
                            : (Math.floor(Atmosphere.idleFogTimeout / 60) + " мин" + (Atmosphere.idleFogTimeout % 60 > 0 ? (" " + (Atmosphere.idleFogTimeout % 60) + " с") : ""))
                        fromVal: 5; toVal: 1800; stepVal: 5
                        curVal: Atmosphere.idleFogTimeout
                        onMovedVal: (v) => Atmosphere.idleFogTimeout = Math.round(v)
                    }
                    SliderRow {
                        visible: (Atmosphere.condensationEnabled || mainCol.isSnowMode) && Atmosphere.idleFogEnabled
                        label: "Сила эффекта при бездействии:"
                        valText: Math.round(Atmosphere.idleFogStrength * 100) + "%"
                        fromVal: 0.40; toVal: 1.0; stepVal: 0.05
                        curVal: Atmosphere.idleFogStrength
                        onMovedVal: (v) => Atmosphere.idleFogStrength = v
                    }
                    SwitchRow { label: "Сдувание листьев и снежинок курсором мыши:"; checkedVal: Atmosphere.cursorWindEnabled; onToggledVal: (v) => Atmosphere.cursorWindEnabled = v }
                }
            }

            GlassPanel {
                visible: mainCol.anyAtmoActive
                objectName: "atmoObstacle"
                Layout.fillWidth: true
                implicitHeight: optCol.implicitHeight + 28
                ColumnLayout {
                    id: optCol
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10
                    Text { text: Config.trAuto("Оптимизация и частота кадров (FPS)"); color: Config.accentColor; font.pixelSize: Config.fontSize + 2; font.bold: true }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Text { text: Config.trAuto("Лимит FPS погоды:"); color: "#e2e8f0"; font.pixelSize: Config.fontSize; Layout.preferredWidth: 180 }
                        SegmentedButton {
                            Layout.fillWidth: true
                            options: ["30 FPS", "60 FPS", "90 FPS", "120 FPS", "160 FPS"]
                            property var fpsVals: [30, 60, 90, 120, 160]
                            selectedIndex: Math.max(0, fpsVals.indexOf(Atmosphere.targetFps))
                            onSelected: (idx) => Atmosphere.targetFps = fpsVals[idx]
                        }
                    }
                    SwitchRow { label: "Автопауза (0% GPU) при полноэкранных играх и видео:"; checkedVal: Atmosphere.pauseOnFullscreen; onToggledVal: (v) => Atmosphere.pauseOnFullscreen = v }
                    StyledButton { Layout.fillWidth: true; allowFullWidth: true; text: Config.trAuto("↺   Сбросить настройки атмосферы по умолчанию"); onClicked: Atmosphere.resetDefaults() }
                }
            }
        }
    }
}
