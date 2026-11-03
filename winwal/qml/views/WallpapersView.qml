import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HySett
import "../components"

Item {
    id: wallRoot

    property var winObj: ApplicationWindow.window
    readonly property real curOp: (winObj && !isNaN(winObj.wallpapersLiveOpacity)) ? winObj.wallpapersLiveOpacity : (Config ? Config.wallpapersOpacity : 0.59)
    readonly property real curBl: (winObj && !isNaN(winObj.wallpapersLiveBlur)) ? winObj.wallpapersLiveBlur : (Config ? Config.wallpapersBlur : 0.15)
    readonly property bool isExpanded: wallRoot.width > 1050
    property real targetScrollY: 0
    property real targetScrollX: 0
    property int activeCatIndex: 0
    property string activeCatName: Wallpapers.categories.length > 0 ? Wallpapers.categories[0].name : ""
    property bool isProgrammaticScroll: false

    // Точный пересчет горизонтальных позиций кнопок категорий (чтобы не налезали друг на друга и чтобы бегунок всегда знал координаты!)
    function recalcCatLayout() {
        var cnt = morphCatRepeater.count
        if (cnt === 0) return

        var spacing = 4
        var totalW = 0
        var widths = []
        for (var i = 0; i < cnt; ++i) {
            var item = morphCatRepeater.itemAt(i)
            var w = item ? Math.ceil(item.textImplicitW + 12) : 48
            widths.push(w)
            totalW += w + (i > 0 ? spacing : 0)
        }

        var startX = Math.round(sceneContainer.rightAreaX + Math.max(0, (sceneContainer.rightAreaW - totalW) / 2))
        var curX = startX
        for (var j = 0; j < cnt; ++j) {
            var btn = morphCatRepeater.itemAt(j)
            if (btn) {
                btn.calcHorizX = curX
                btn.calcHorizW = widths[j]
            }
            curX += widths[j] + spacing
        }

        morphCategoriesLayer.activeCatBtnObj = morphCatRepeater.itemAt(wallRoot.activeCatIndex)
    }

    onWidthChanged: recalcCatLayout()
    onHeightChanged: recalcCatLayout()
    onIsExpandedChanged: recalcCatLayout()
    onActiveCatIndexChanged: {
        morphCategoriesLayer.activeCatBtnObj = morphCatRepeater.itemAt(wallRoot.activeCatIndex)
    }
    Component.onCompleted: Qt.callLater(recalcCatLayout)

    NumberAnimation {
        id: fastWheelAnim
        target: wallListView
        property: "contentY"
        duration: 240
        easing.type: Easing.OutCubic
        onStopped: wallRoot.isProgrammaticScroll = false
    }

    NumberAnimation {
        id: fastHorizWheelAnim
        target: horizWallListView
        property: "contentX"
        duration: 240
        easing.type: Easing.OutCubic
        onStopped: wallRoot.isProgrammaticScroll = false
    }

    function scrollToCategory(catName, catIdx) {
        wallRoot.isProgrammaticScroll = true
        activeCatName = catName
        activeCatIndex = catIdx
        for (var i = 0; i < Wallpapers.wallpapers.length; ++i) {
            if (Wallpapers.wallpapers[i].category === catName) {
                if (wallRoot.isExpanded) {
                    var targetX = i * 222
                    var maxX = Math.max(0, horizWallListView.contentWidth - horizWallListView.width)
                    wallRoot.targetScrollX = Math.min(targetX, maxX)
                    fastHorizWheelAnim.stop()
                    fastHorizWheelAnim.duration = 320
                    fastHorizWheelAnim.from = horizWallListView.contentX
                    fastHorizWheelAnim.to = wallRoot.targetScrollX
                    fastHorizWheelAnim.start()
                } else {
                    var targetY = i * 130
                    var maxY = Math.max(0, wallListView.contentHeight - wallListView.height)
                    wallRoot.targetScrollY = Math.min(targetY, maxY)
                    fastWheelAnim.stop()
                    fastWheelAnim.duration = 340
                    fastWheelAnim.from = wallListView.contentY
                    fastWheelAnim.to = wallRoot.targetScrollY
                    fastWheelAnim.start()
                }
                break
            }
        }
    }

    Item {
        id: sceneContainer
        anchors.fill: parent

        readonly property real leftPanelW: wallRoot.isExpanded ? Math.max(250, Math.min(295, Math.round(wallRoot.width * 0.20))) : 250
        readonly property real leftPanelX: wallRoot.isExpanded ? 16 : Math.round(Math.max(18, (wallRoot.width - 668) / 2))
        readonly property real rightAreaX: leftPanelX + leftPanelW + 20
        readonly property real rightAreaW: Math.max(260, wallRoot.width - rightAreaX - 18)

        ScrollView {
            id: col1Scroll
            width: sceneContainer.leftPanelW
            height: parent.height
            x: sceneContainer.leftPanelX
            y: 0
            contentWidth: availableWidth
            clip: true

            Behavior on x { NumberAnimation { duration: 420; easing.type: Easing.OutQuint } }
            Behavior on width { NumberAnimation { duration: 420; easing.type: Easing.OutQuint } }

            Item {
                width: col1Scroll.availableWidth
                height: Math.max(col1Scroll.height, col1Content.implicitHeight + 36)

                ColumnLayout {
                    id: col1Content
                    width: parent.width - 12
                    x: 4
                    y: wallRoot.isExpanded
                       ? Math.round(Math.max(16, col1Scroll.height - col1Content.implicitHeight - 16))
                       : Math.round(Math.max(20, (col1Scroll.height - col1Content.implicitHeight) / 2))
                    spacing: 8

                    Behavior on y { NumberAnimation { duration: 440; easing.type: Easing.OutQuint } }

                    Text {
                        text: Config ? (Config.language, Config.trAuto("Обои рабочего стола")) : "Обои рабочего стола"
                        color: "#ffffff"
                        font.pixelSize: Config.fontSize + 3
                        font.bold: true
                        Layout.bottomMargin: 2
                    }

                    GlassPanel {
                        Layout.fillWidth: true
                        implicitHeight: colAuto.implicitHeight + 16

                        ColumnLayout {
                            id: colAuto
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 8

                            Text { text: Config ? (Config.language, Config.trAuto("Фоновый демон и режим")) : "Фоновый демон и режим"; color: "#ffffff"; font.pixelSize: Config.fontSize + 1; font.bold: true }

                            RowLayout {
                                Text { text: Config ? (Config.language, Config.trAuto("Автосмена обоев:")) : "Автосмена обоев:"; color: Qt.rgba(1, 1, 1, 0.92); font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                                StyledSwitch {
                                    checked: Wallpapers.daemonRunning
                                    onToggled: Wallpapers.daemonRunning = checked
                                }
                            }

                            ColumnLayout {
                                visible: Wallpapers.daemonRunning
                                Layout.fillWidth: true
                                spacing: 6

                                RowLayout {
                                    Text { text: Config ? (Config.language, Config.trAuto("Режим:")) : "Режим:"; color: Qt.rgba(1, 1, 1, 0.88); font.pixelSize: Config.fontSize - 1 }
                                    Item { Layout.fillWidth: true }
                                    SegmentedButton {
                                        options: ["Все", "Видео", "Фото"]
                                        selectedIndex: Wallpapers.autoMode === "video" ? 1 : (Wallpapers.autoMode === "image" ? 2 : 0)
                                        onSelected: (i) => Wallpapers.autoMode = ["all", "video", "image"][i]
                                    }
                                }

                                RowLayout {
                                    Text { text: Config ? (Config.language, Config.trAuto("Интервал:")) : "Интервал:"; color: Qt.rgba(1, 1, 1, 0.88); font.pixelSize: Config.fontSize - 1 }
                                    Item { Layout.fillWidth: true }
                                    SegmentedButton {
                                        options: ["1м", "2м", "5м", "10м", "30м"]
                                        selectedIndex: {
                                            var s = Wallpapers.autoInterval
                                            if (s === 60) return 0
                                            if (s === 300) return 2
                                            if (s === 600) return 3
                                            if (s === 1800) return 4
                                            return 1
                                        }
                                        onSelected: (i) => Wallpapers.autoInterval = [60, 120, 300, 600, 1800][i]
                                    }
                                }
                            }

                            RowLayout {
                                Text { text: Config ? (Config.language, Config.trAuto("Случайная анимация:")) : "Случайная анимация:"; color: Qt.rgba(1, 1, 1, 0.92); font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                                StyledSwitch { checked: Wallpapers.randomAnim; onToggled: Wallpapers.randomAnim = checked }
                            }

                            RowLayout {
                                Text { text: Config ? (Config.language, Config.trAuto("Скругления окон:")) : "Скругления окон:"; color: Qt.rgba(1, 1, 1, 0.92); font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                                StyledSwitch {
                                    checked: Theme.isRounded
                                    onToggled: Theme.setRoundedState(checked)
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                
                                RowLayout {
                                    Text { text: Config ? (Config.language, Config.trAuto("Прозрачность вкладки:")) : "Прозрачность вкладки:"; color: Qt.rgba(1, 1, 1, 0.92); font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                                    Text { text: Math.round(wallRoot.curOp * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize }
                                }
                                StyledSlider {
                                    Layout.fillWidth: true
                                    from: 0.0
                                    to: 1.0
                                    value: wallRoot.curOp
                                    onMoved: {
                                        if (wallRoot.winObj) wallRoot.winObj.wallpapersLiveOpacity = value
                                        if (!wallRoot.isExpanded) Config.wallpapersOpacity = value
                                    }
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                
                                RowLayout {
                                    Text { text: Config ? (Config.language, Config.trAuto("Блюр вкладки:")) : "Блюр вкладки:"; color: Qt.rgba(1, 1, 1, 0.92); font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                                    Text { text: wallRoot.curBl <= 0.01 ? "Выкл" : Math.round(wallRoot.curBl * 100) + "%"; color: "#ffffff"; font.pixelSize: Config.fontSize }
                                }
                                StyledSlider {
                                    Layout.fillWidth: true
                                    from: 0.0
                                    to: 1.0
                                    stepSize: 0.02
                                    value: wallRoot.curBl
                                    onMoved: {
                                        if (wallRoot.winObj) wallRoot.winObj.wallpapersLiveBlur = value
                                        if (Theme) Theme.applyEffectiveBlur(value)
                                        if (!wallRoot.isExpanded) Config.wallpapersBlur = value
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        StyledButton {
                            text: Config ? (Config.language, Config.trAuto("󰒝   Случайные")) : "󰒝   Случайные"
                            allowFullWidth: true
                            Layout.fillWidth: true
                            onClicked: Wallpapers.applyRandomWallpaper()
                        }
                        StyledButton {
                            text: (Config && Config.language === "en") ? "󰸉   No wallpaper" : "󰸉   Без обоев"
                            allowFullWidth: true
                            Layout.fillWidth: true
                            onClicked: Wallpapers.clearWallpaper()
                        }
                    }

                    StyledButton {
                        Layout.fillWidth: true
                        allowFullWidth: true
                        enabled: true
                        text: Wallpapers.downloading
                              ? Wallpapers.downloadStatus
                              : (Wallpapers.downloadStatus !== "" ? Wallpapers.downloadStatus : ((Config && Config.language === "en") ? "󰇚   Sync Wallpapers (GitHub)" : "󰇚   Скачать обои с GitHub"))
                        onClicked: Wallpapers.downloadGithubWallpapers()
                    }

                    GlassPanel {
                        Layout.fillWidth: true
                        implicitHeight: colPre.implicitHeight + 16

                        ColumnLayout {
                            id: colPre
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6

                            RowLayout {
                                Text { text: Config ? (Config.language, Config.trAuto("Пресеты")) : "Пресеты"; color: "#ffffff"; font.pixelSize: Config.fontSize + 1; font.bold: true; Layout.fillWidth: true }
                                StyledButton {
                                    text: Config ? (Config.language, Config.trAuto("+ Создать")) : "+ Создать"
                                    implicitHeight: 20
                                    onClicked: {
                                        presetModal.editIdx = -1
                                        presetModal.openModal("Новый пресет", "", 0.85, 0.75, true, 8.0)
                                    }
                                }
                            }

                            Repeater {
                                model: Wallpapers.presets

                                RowLayout {
                                    spacing: 4
                                    Layout.fillWidth: true
                                    property bool isPresetActive: Wallpapers.activePresetIndex === index

                                    Button {
                                        id: pBtn
                                        Layout.fillWidth: true
                                        implicitHeight: 22
                                        flat: true
                                        onClicked: Wallpapers.applyPreset(index)

                                        contentItem: Text {
                                            text: (isPresetActive ? "✓  " : "") + modelData.name
                                            color: isPresetActive ? "#000000" : (pBtn.hovered ? "#000000" : "#e0e0e0")
                                            font.pixelSize: Config.fontSize
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }

                                        background: Rectangle {
                                            radius: (Theme && Theme.isRounded) ? 3 : 0
                                            color: isPresetActive ? "#ffffff" : (pBtn.hovered ? "#ffffff" : Qt.rgba(0.05, 0.05, 0.07, 0.65))
                                            border.color: isPresetActive ? "#ffffff" : Qt.rgba(1, 1, 1, 0.18)
                                            border.width: 1
                                            Behavior on color { ColorAnimation { duration: 100 } }
                                        }
                                    }

                                    StyledButton {
                                        text: "✎"
                                        implicitWidth: 22
                                        onClicked: {
                                            presetModal.editIdx = index
                                            presetModal.openModal(modelData.name, modelData.wall, modelData.active_opacity, modelData.inactive_opacity, modelData.blur_enabled, modelData.blur_size)
                                        }
                                    }

                                    StyledButton {
                                        visible: modelData.canDelete === true
                                        text: "✕"
                                        implicitWidth: 22
                                        onClicked: Wallpapers.deletePreset(index)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // 2. ТРАНСФОРМИРУЕМЫЙ КОНТЕЙНЕР ГАЛЕРЕИ ОБОЕВ
        Item {
            id: morphGalleryBox
            x: wallRoot.isExpanded ? sceneContainer.rightAreaX : (sceneContainer.leftPanelX + 250 + 24 + 110 + 24)
            y: wallRoot.isExpanded ? (wallRoot.height - 174) : 6
            width: wallRoot.isExpanded ? sceneContainer.rightAreaW : 260
            height: wallRoot.isExpanded ? 130 : (wallRoot.height - 12)
            clip: true

            Behavior on x { NumberAnimation { duration: 440; easing.type: Easing.OutQuint } }
            Behavior on y { NumberAnimation { duration: 440; easing.type: Easing.OutQuint } }
            Behavior on width { NumberAnimation { duration: 440; easing.type: Easing.OutQuint } }
            Behavior on height { NumberAnimation { duration: 440; easing.type: Easing.OutQuint } }

            Item {
                anchors.fill: parent
                opacity: wallRoot.isExpanded ? 0.0 : 1.0
                visible: opacity > 0.01
                Behavior on opacity { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

                ListView {
                    id: wallListView
                    anchors.fill: parent
                    clip: true
                    spacing: 10
                    interactive: false
                    model: (!wallRoot.isExpanded && wallRoot.visible) ? Wallpapers.wallpapers : null
                    cacheBuffer: 360
                    reuseItems: true
                    pixelAligned: true

                    onContentYChanged: {
                        if (wallRoot.isExpanded || wallRoot.isProgrammaticScroll || Wallpapers.wallpapers.length === 0) return
                        var idx = Math.floor((contentY + height * 0.25) / 130)
                        idx = Math.max(0, Math.min(Wallpapers.wallpapers.length - 1, idx))
                        var cat = Wallpapers.wallpapers[idx].category
                        if (cat && wallRoot.activeCatName !== cat) {
                            wallRoot.activeCatName = cat
                            for (var c = 0; c < Wallpapers.categories.length; ++c) {
                                if (Wallpapers.categories[c].name === cat) {
                                    wallRoot.activeCatIndex = c
                                    break
                                }
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar {
                        id: vBar
                        policy: ScrollBar.AlwaysOn
                        width: 8
                        contentItem: Rectangle {
                            implicitWidth: 8
                            radius: (Theme && Theme.isRounded) ? 4 : 0
                            color: vBar.pressed ? "#ffffff" : Qt.rgba(1, 1, 1, 0.40)
                        }
                    }

                    delegate: Item {
                        width: wallListView.width
                        height: 120

                        Button {
                            id: wallBtn
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 210
                            height: 120
                            flat: true
                            onClicked: Wallpapers.applyWallpaper(modelData.path)

                            background: Rectangle {
                                radius: (Theme && Theme.isRounded) ? 6 : 0
                                color: Qt.rgba(0.04, 0.04, 0.04, 0.85)
                                border.color: wallBtn.hovered ? Qt.rgba(1, 1, 1, 0.85) : Qt.rgba(1, 1, 1, 0.16)
                                border.width: wallBtn.hovered ? 2 : 1
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    source: "image://wallthumb" + modelData.path
                                    sourceSize.width: 168
                                    sourceSize.height: 94
                                    asynchronous: true
                                    cache: true
                                    fillMode: Image.PreserveAspectCrop
                                    opacity: wallBtn.hovered ? 1.0 : 0.85
                                    Behavior on opacity { NumberAnimation { duration: 90 } }
                                }

                                Rectangle {
                        objectName: "atmoObstacle"
                                    visible: modelData.isVideo
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 6
                                    width: 18
                                    height: 14
                                    radius: 2
                                    color: Qt.rgba(0, 0, 0, 0.75)
                                    Text { anchors.centerIn: parent; text: "▶"; color: "#ffffff"; font.pixelSize: 8 }
                                }
                            }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: !wallRoot.isExpanded
                    acceptedButtons: Qt.NoButton
                    onWheel: (wheel) => {
                        wheel.accepted = true
                        wallRoot.isProgrammaticScroll = false
                        var maxY = Math.max(0, wallListView.contentHeight - wallListView.height)
                        if (maxY <= 0) return
                        if (!fastWheelAnim.running) wallRoot.targetScrollY = wallListView.contentY

                        var step = (wheel.angleDelta.y / 120.0) * 180.0
                        if (step < 0 && wallListView.contentY >= maxY - 2) wallRoot.targetScrollY = 0
                        else if (step > 0 && wallListView.contentY <= 2) wallRoot.targetScrollY = maxY
                        else wallRoot.targetScrollY = Math.max(0, Math.min(maxY, wallRoot.targetScrollY - step))

                        fastWheelAnim.stop()
                        fastWheelAnim.duration = 240
                        fastWheelAnim.from = wallListView.contentY
                        fastWheelAnim.to = wallRoot.targetScrollY
                        fastWheelAnim.start()
                    }
                }
            }

            Item {
                anchors.fill: parent
                opacity: wallRoot.isExpanded ? 1.0 : 0.0
                visible: opacity > 0.01
                Behavior on opacity { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

                ListView {
                    id: horizWallListView
                    anchors.fill: parent
                    orientation: ListView.Horizontal
                    spacing: 12
                    clip: true
                    interactive: false
                    model: (wallRoot.isExpanded && wallRoot.visible) ? Wallpapers.wallpapers : null
                    cacheBuffer: 360
                    reuseItems: true
                    pixelAligned: true

                    onContentXChanged: {
                        if (!wallRoot.isExpanded || wallRoot.isProgrammaticScroll || Wallpapers.wallpapers.length === 0) return
                        var idx = Math.floor((contentX + width * 0.25) / 222)
                        idx = Math.max(0, Math.min(Wallpapers.wallpapers.length - 1, idx))
                        var cat = Wallpapers.wallpapers[idx].category
                        if (cat && wallRoot.activeCatName !== cat) {
                            wallRoot.activeCatName = cat
                            for (var c = 0; c < Wallpapers.categories.length; ++c) {
                                if (Wallpapers.categories[c].name === cat) {
                                    wallRoot.activeCatIndex = c
                                    break
                                }
                            }
                        }
                    }

                    ScrollBar.horizontal: ScrollBar {
                        id: hBar
                        policy: ScrollBar.AlwaysOn
                        height: 5
                        anchors.bottom: parent.bottom
                        contentItem: Rectangle {
                            implicitHeight: 5
                            radius: (Theme && Theme.isRounded) ? 2.5 : 0
                            color: hBar.pressed ? "#ffffff" : Qt.rgba(1, 1, 1, 0.38)
                        }
                    }

                    delegate: Item {
                        width: 210
                        height: 120

                        Button {
                            id: hWallBtn
                            anchors.fill: parent
                            flat: true
                            onClicked: Wallpapers.applyWallpaper(modelData.path)

                            background: Rectangle {
                                radius: (Theme && Theme.isRounded) ? 6 : 0
                                color: Qt.rgba(0.04, 0.04, 0.04, 0.85)
                                border.color: hWallBtn.hovered ? Qt.rgba(1, 1, 1, 0.85) : Qt.rgba(1, 1, 1, 0.16)
                                border.width: hWallBtn.hovered ? 2 : 1
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    source: "image://wallthumb" + modelData.path
                                    sourceSize.width: 168
                                    sourceSize.height: 94
                                    asynchronous: true
                                    cache: true
                                    fillMode: Image.PreserveAspectCrop
                                    opacity: hWallBtn.hovered ? 1.0 : 0.85
                                    Behavior on opacity { NumberAnimation { duration: 90 } }
                                }

                                Rectangle {
                        objectName: "atmoObstacle"
                                    visible: modelData.isVideo
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 6
                                    width: 18
                                    height: 14
                                    radius: 2
                                    color: Qt.rgba(0, 0, 0, 0.75)
                                    Text { anchors.centerIn: parent; text: "▶"; color: "#ffffff"; font.pixelSize: 8 }
                                }
                            }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: wallRoot.isExpanded
                    acceptedButtons: Qt.NoButton
                    onWheel: (wheel) => {
                        wheel.accepted = true
                        wallRoot.isProgrammaticScroll = false
                        var maxX = Math.max(0, horizWallListView.contentWidth - horizWallListView.width)
                        if (maxX <= 0) return
                        if (!fastHorizWheelAnim.running) wallRoot.targetScrollX = horizWallListView.contentX

                        var delta = (wheel.angleDelta.y !== 0 ? wheel.angleDelta.y : wheel.angleDelta.x)
                        var step = (delta / 120.0) * 260.0
                        if (step < 0 && horizWallListView.contentX >= maxX - 2) wallRoot.targetScrollX = 0
                        else if (step > 0 && horizWallListView.contentX <= 2) wallRoot.targetScrollX = maxX
                        else wallRoot.targetScrollX = Math.max(0, Math.min(maxX, wallRoot.targetScrollX - step))

                        fastHorizWheelAnim.stop()
                        fastHorizWheelAnim.duration = 240
                        fastHorizWheelAnim.from = horizWallListView.contentX
                        fastHorizWheelAnim.to = wallRoot.targetScrollX
                        fastHorizWheelAnim.start()
                    }
                }
            }
        }

        // 3. ТРАНСФОРМИРУЕМЫЕ КНОПКИ КАТЕГОРИЙ И АКТИВНАЯ ПЛАШКА
        Item {
            id: morphCategoriesLayer
            objectName: "morphCategoriesLayer"
            anchors.fill: parent

            property Item activeCatBtnObj: null
            property Item hoveredCatBtnObj: null
            property Item lastHoveredCatBtnObj: null

            readonly property real vertX: Math.round(sceneContainer.leftPanelX + 250 + 24)
            readonly property real vertTotalH: Wallpapers.categories.length * 24 - 2
            readonly property real vertStartY: Math.round(Math.max(16, (wallRoot.height - vertTotalH) / 2))
            readonly property real horizY: Math.round(wallRoot.height - 36)

            Rectangle {
                id: morphHoverPill
                x: morphCategoriesLayer.lastHoveredCatBtnObj ? morphCategoriesLayer.lastHoveredCatBtnObj.x : morphCategoriesLayer.vertX
                y: morphCategoriesLayer.lastHoveredCatBtnObj ? morphCategoriesLayer.lastHoveredCatBtnObj.y : morphCategoriesLayer.vertStartY
                width: morphCategoriesLayer.lastHoveredCatBtnObj ? morphCategoriesLayer.lastHoveredCatBtnObj.width : 110
                height: morphCategoriesLayer.lastHoveredCatBtnObj ? morphCategoriesLayer.lastHoveredCatBtnObj.height : 22
                radius: (Theme && Theme.isRounded) ? 3 : 0
                color: Qt.rgba(1, 1, 1, 0.10)
                opacity: morphCategoriesLayer.hoveredCatBtnObj && morphCategoriesLayer.hoveredCatBtnObj !== morphCategoriesLayer.activeCatBtnObj ? 1.0 : 0.0

                Behavior on x { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                Behavior on y { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                Behavior on width { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                Behavior on height { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                Behavior on opacity { NumberAnimation { duration: 90 } }
            }

            Rectangle {
                id: morphActivePill
                x: morphCategoriesLayer.activeCatBtnObj ? morphCategoriesLayer.activeCatBtnObj.targetX : morphCategoriesLayer.vertX
                y: morphCategoriesLayer.activeCatBtnObj ? morphCategoriesLayer.activeCatBtnObj.targetY : morphCategoriesLayer.vertStartY
                width: morphCategoriesLayer.activeCatBtnObj ? morphCategoriesLayer.activeCatBtnObj.targetW : 110
                height: morphCategoriesLayer.activeCatBtnObj ? morphCategoriesLayer.activeCatBtnObj.targetH : 22
                radius: (Theme && Theme.isRounded) ? 3 : 0
                color: "#ffffff"

                Behavior on x { NumberAnimation { duration: 360; easing.type: Easing.OutQuint } }
                Behavior on y { NumberAnimation { duration: 360; easing.type: Easing.OutQuint } }
                Behavior on width { NumberAnimation { duration: 360; easing.type: Easing.OutQuint } }
                Behavior on height { NumberAnimation { duration: 360; easing.type: Easing.OutQuint } }
            }

            Repeater {
                id: morphCatRepeater
                model: Wallpapers.categories
                onCountChanged: Qt.callLater(wallRoot.recalcCatLayout)

                Button {
                    id: morphCatBtn
                    flat: true
                    property bool isSel: wallRoot.activeCatIndex === index
                    property real textImplicitW: mCatTxt.implicitWidth
                    property real calcHorizX: morphCategoriesLayer.vertX
                    property real calcHorizW: Math.ceil(mCatTxt.implicitWidth + 12)

                    onTextImplicitWChanged: Qt.callLater(wallRoot.recalcCatLayout)

                    readonly property real targetX: wallRoot.isExpanded ? calcHorizX : morphCategoriesLayer.vertX
                    readonly property real targetY: wallRoot.isExpanded ? morphCategoriesLayer.horizY : (morphCategoriesLayer.vertStartY + index * 24)
                    readonly property real targetW: wallRoot.isExpanded ? calcHorizW : 110
                    readonly property real targetH: wallRoot.isExpanded ? 20 : 22

                    x: targetX
                    y: targetY
                    width: targetW
                    height: targetH

                    Behavior on x { NumberAnimation { duration: 360; easing.type: Easing.OutQuint } }
                    Behavior on y { NumberAnimation { duration: 360; easing.type: Easing.OutQuint } }
                    Behavior on width { NumberAnimation { duration: 340; easing.type: Easing.OutQuint } }
                    Behavior on height { NumberAnimation { duration: 340; easing.type: Easing.OutQuint } }

                    contentItem: Text {
                        id: mCatTxt
                        text: modelData.name
                        color: morphCatBtn.isSel ? "#000000" : (morphCatBtn.hovered ? "#ffffff" : Qt.rgba(1, 1, 1, 0.92))
                        font.pixelSize: wallRoot.isExpanded ? Math.max(8, Config.fontSize - 2) : 10
                        font.bold: false
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    background: Rectangle {
                        radius: (Theme && Theme.isRounded) ? 3 : 0
                        color: morphCatBtn.isSel ? "transparent" : Qt.rgba(0.05, 0.05, 0.07, 0.65)
                        border.color: morphCatBtn.isSel ? "transparent" : Qt.rgba(1, 1, 1, 0.14)
                        border.width: 1
                    }

                    onHoveredChanged: {
                        if (hovered) {
                            morphCategoriesLayer.lastHoveredCatBtnObj = morphCatBtn
                            morphCategoriesLayer.hoveredCatBtnObj = morphCatBtn
                        } else if (morphCategoriesLayer.hoveredCatBtnObj === morphCatBtn) {
                            morphCategoriesLayer.hoveredCatBtnObj = null
                        }
                    }

                    onClicked: wallRoot.scrollToCategory(modelData.name, index)
                }
            }
        }
    }

    // МОДАЛЬНОЕ ОКНО ПРЕСЕТОВ (с динамической светящейся обводкой границ!)
    Item {
        id: presetModal
        parent: Overlay.overlay
        anchors.fill: parent
        visible: opacity > 0.01
        opacity: 0.0

        property int editIdx: -1
        function openModal(name, wall, act, inact, blEn, blSz) {
            edName.text = name
            edWall.text = wall
            edAct.value = act
            edInact.value = inact
            edBlur.checked = blEn
            edSz.value = blSz
            presetModal.opacity = 1.0
            modalCard.scale = 1.0
            if (ApplicationWindow.window) ApplicationWindow.window.modalOpen = true
        }
        function closeModal() {
            presetModal.opacity = 0.0
            modalCard.scale = 0.92
            if (ApplicationWindow.window) ApplicationWindow.window.modalOpen = false
        }

        Behavior on opacity { NumberAnimation { duration: 160 } }

        MouseArea {
            anchors.fill: parent
            onClicked: presetModal.closeModal()
        }

        GlassPanel {
            id: modalCard
            anchors.centerIn: parent
            width: 340
            implicitHeight: colDialog.implicitHeight + 24
            scale: 0.92
            radius: (Theme && Theme.isRounded) ? 8 : 0
            tintColor: Qt.rgba(0.04, 0.04, 0.05, Config ? Config.dialogOpacity : 0.45)

            MouseArea { anchors.fill: parent }

            Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutBack } }

            // Светящаяся обводка диалогового окна (как у главного окна)
            ShaderEffect {
                anchors.fill: parent
                z: 20
                enabled: false
                visible: Config ? Config.borderGlowEnabled : true
                property vector2d u_resolution: Qt.vector2d(width, height)
                property real u_radius: modalCard.radius
                property real u_time: (wallRoot.winObj ? wallRoot.winObj.liquidTime : 0.0) * (Config ? Config.borderGlowSpeed : 1.0)
                property real u_intensity: Config ? Config.borderGlowIntensity : 0.75
                property real u_spread: Config ? Config.borderGlowSpread * 0.7 : 0.35
                property real u_color_mode: Config ? Config.borderGlowColorMode : 0
                property real u_direction: Config ? Config.borderGlowDirection : 0
                property real u_cursor_strength: 0.0
                property vector2d u_mouse: Qt.vector2d(-999, -999)
                property vector3d u_custom_color: Config ? Qt.vector3d(Config.borderGlowCustomColor.r, Config.borderGlowCustomColor.g, Config.borderGlowCustomColor.b) : Qt.vector3d(1, 1, 1)
                fragmentShader: "qrc:/shaders/qml/shaders/neon_border.frag.qsb"
            }

            ColumnLayout {
                id: colDialog
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                RowLayout {
                    Text {
                        text: presetModal.editIdx === -1 ? "Создание нового пресета" : "Настройка пресета " + (presetModal.editIdx + 1)
                        color: "#ffffff"
                        font.bold: false
                        font.pixelSize: Config.fontSize + 1
                        Layout.fillWidth: true
                    }
                    StyledButton { text: "✕"; implicitWidth: 20; implicitHeight: 20; onClicked: presetModal.closeModal() }
                }

                Text { text: Config ? (Config.language, Config.trAuto("Название:")) : "Название:"; color: Qt.rgba(1, 1, 1, 0.88); font.pixelSize: Config.fontSize - 1 }
                TextField {
                    id: edName
                    implicitHeight: 24
                    Layout.fillWidth: true
                    color: "#ffffff"
                    font.pixelSize: Config.fontSize
                    background: Rectangle {
                        color: Qt.rgba(1, 1, 1, 0.08)
                        border.color: Qt.rgba(1, 1, 1, 0.16)
                        border.width: 1
                        radius: (Theme && Theme.isRounded) ? 4 : 0
                    }
                }

                Text { text: Config ? (Config.language, Config.trAuto("Путь к обоям:")) : "Путь к обоям:"; color: Qt.rgba(1, 1, 1, 0.88); font.pixelSize: Config.fontSize - 1 }
                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: edWall
                        implicitHeight: 24
                        Layout.fillWidth: true
                        color: "#ffffff"
                        font.pixelSize: Config.fontSize
                        background: Rectangle {
                            color: Qt.rgba(1, 1, 1, 0.08)
                            border.color: Qt.rgba(1, 1, 1, 0.16)
                            border.width: 1
                            radius: (Theme && Theme.isRounded) ? 4 : 0
                        }
                    }
                    StyledButton {
                        text: Config ? (Config.language, Config.trAuto("󰉋 Обзор...")) : "󰉋 Обзор..."
                        onClicked: {
                            Hypr.dispatch("exec /home/whrr/.config/hypr/scripts/pick_file.sh")
                        }
                    }
                }

                StyledButton {
                    text: Config ? (Config.language, Config.trAuto("✎  Записать текущие Обои + Окна")) : "✎  Записать текущие Обои + Окна"
                    allowFullWidth: true
                    Layout.fillWidth: true
                    onClicked: {
                        var s = Wallpapers.snapshotCurrentSettings()
                        if (s.wall) edWall.text = s.wall
                        edAct.value = Look.activeOpacity
                        edInact.value = Look.inactiveOpacity
                        edBlur.checked = Look.blurEnabled
                        edSz.value = 8.0
                    }
                }

                RowLayout {
                    Text { text: Config ? (Config.language, Config.trAuto("Прозрачность окон:")) : "Прозрачность окон:"; color: Qt.rgba(1, 1, 1, 0.92); font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                    StyledSlider { id: edAct; from: 0.1; to: 1.0 }
                }

                RowLayout {
                    Text { text: Config ? (Config.language, Config.trAuto("Неактивные окна:")) : "Неактивные окна:"; color: Qt.rgba(1, 1, 1, 0.92); font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                    StyledSlider { id: edInact; from: 0.1; to: 1.0 }
                }

                RowLayout {
                    Text { text: Config ? (Config.language, Config.trAuto("Блюр окон:")) : "Блюр окон:"; color: Qt.rgba(1, 1, 1, 0.92); font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                    StyledSwitch { id: edBlur }
                }

                RowLayout {
                    Text { text: Config ? (Config.language, Config.trAuto("Размер блюра:")) : "Размер блюра:"; color: Qt.rgba(1, 1, 1, 0.92); font.pixelSize: Config.fontSize; Layout.fillWidth: true }
                    StyledSlider { id: edSz; from: 1; to: 20; stepSize: 1 }
                }

                StyledButton {
                    text: Config ? (Config.language, Config.trAuto("Сохранить пресет")) : "Сохранить пресет"
                    allowFullWidth: true
                    Layout.fillWidth: true
                    implicitHeight: 24
                    onClicked: {
                        Wallpapers.savePreset(presetModal.editIdx, edName.text, edWall.text, edAct.value, edInact.value, edBlur.checked, edSz.value)
                        presetModal.closeModal()
                    }
                }
            }
        }
    }
}
