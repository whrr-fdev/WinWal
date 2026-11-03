import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HySett

Rectangle {
    id: bar
    property int currentIndex: 0
    signal categorySelected(int index)
    signal returnToWelcome()

    color: Qt.rgba(0.04, 0.04, 0.04, Config ? Config.sidebarOpacity : 0.0)

    property Item activeBtnObj: {
        var _ = Config ? Config.hiddenSidebarTabs : ""
        if (bar.currentIndex === 17) return setBtn
        for (var i = 0; i < navRepeater.count; ++i) {
            var it = navRepeater.itemAt(i)
            if (it && it.tabIdx === bar.currentIndex) return it
        }
        return null
    }
    property Item hoveredBtnObj: null
    property Item lastHoveredBtnObj: null

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Qt.rgba(1, 1, 1, 0.08)
    }

    Rectangle {
        id: globalHoverPill
        x: 4
        width: bar.width - 8
        height: 24
        radius: (Theme && Theme.isRounded) ? 4 : 0
        color: Qt.rgba(1, 1, 1, 0.08)
        opacity: bar.hoveredBtnObj && bar.hoveredBtnObj !== bar.activeBtnObj ? 1.0 : 0.0
        y: {
            if (!bar.lastHoveredBtnObj) return 0
            if (bar.lastHoveredBtnObj === archHeaderItem || bar.lastHoveredBtnObj === setBtn || bar.lastHoveredBtnObj === trayBtn || bar.lastHoveredBtnObj === quitBtn) {
                return mainColLayout.y + bar.lastHoveredBtnObj.y
            }
            return mainColLayout.y + catScroll.y + bar.lastHoveredBtnObj.y - catScroll.contentItem.contentY
        }

        Behavior on y { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 90 } }
    }

    Rectangle {
        id: activeCategoryPill
        x: 4
        width: bar.width - 8
        height: 24
        radius: (Theme && Theme.isRounded) ? 4 : 0
        color: Config ? Config.accentColor : "#ffffff"
        visible: bar.currentIndex > 0 && bar.activeBtnObj !== null && bar.activeBtnObj.opacity > 0.5
        y: {
            if (!bar.activeBtnObj) return 34
            if (bar.activeBtnObj === setBtn) {
                return mainColLayout.y + setBtn.y
            }
            return mainColLayout.y + catScroll.y + bar.activeBtnObj.y - catScroll.contentItem.contentY
        }

        Behavior on y { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on radius { NumberAnimation { duration: 150 } }
    }

    ColumnLayout {
        id: mainColLayout
        anchors.fill: parent
        anchors.margins: 4
        spacing: 2

        Item {
            id: archHeaderItem
            Layout.fillWidth: true
            implicitHeight: 24

            Row {
                anchors.centerIn: parent
                spacing: 6
                Text { renderType: Text.QtRendering; text: "󰍲"; color: archMouse.containsMouse ? "#ffffff" : "#cccccc"; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                Text { renderType: Text.QtRendering; text: "WINWAL"; color: archMouse.containsMouse ? "#ffffff" : "#aaaaaa"; font.pixelSize: 10; font.letterSpacing: 1.2; anchors.verticalCenter: parent.verticalCenter }
            }

            MouseArea {
                id: archMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onHoveredChanged: {
                    if (containsMouse) { bar.lastHoveredBtnObj = archHeaderItem; bar.hoveredBtnObj = archHeaderItem }
                    else if (bar.hoveredBtnObj === archHeaderItem) bar.hoveredBtnObj = null
                }
                onClicked: bar.returnToWelcome()
            }
        }

        Rectangle {
            objectName: "sidebarSepLine"
            Layout.fillWidth: true
            height: 1
            color: Qt.rgba(1, 1, 1, 0.06)
            Layout.topMargin: 1
            Layout.bottomMargin: 2
        }

        ScrollView {
            id: catScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: navCol
                width: catScroll.availableWidth
                spacing: 0

                Repeater {
                    id: navRepeater
                    model: [
                        { title: "Окна", icon: "󰖲", idx: 1 },
                        { title: "Панель задач", icon: "󱓞", idx: 8 },
                    { icon: "󰖲", ic: "󰖲", glyph: "󰖲", title: "Тайлинг", label: "Тайлинг", name: "Тайлинг", desc: "Авто-тайлинг окон в стиле Hyprland", sub: "Hyprland Tiling WM", idx: 11, index: 11, page: 11, target: 11 },
                        { title: "Обои", icon: "󰸉", idx: 14 },
                        { title: "Виджеты", icon: "󰕰", idx: 15 },
                        { title: "Атмосфера", icon: "󰖗", idx: 19 }
                    ]

                    NavButton {
                        id: nb
                        tabIdx: modelData.idx
                        property bool isShown: {
                            var _ = Config ? Config.hiddenSidebarTabs : ""
                            return Config ? Config.isTabVisible(modelData.idx) : true
                        }
                        Layout.fillWidth: true
                        Layout.preferredHeight: isShown ? 25 : 0
                        Layout.maximumHeight: isShown ? 25 : 0
                        implicitHeight: isShown ? 25 : 0
                        opacity: isShown ? 1.0 : 0.0
                        visible: Layout.preferredHeight > 0.5 || opacity > 0.01
                        enabled: isShown
                        clip: true

                        text: Config ? (Config.language, Config.trAuto(modelData.title)) : modelData.title
                        iconSymbol: modelData.icon
                        isActive: bar.currentIndex === modelData.idx
                        background: Item {}
                        onClicked: bar.categorySelected(modelData.idx)
                        onHoveredChanged: {
                            if (hovered && isShown) { bar.lastHoveredBtnObj = nb; bar.hoveredBtnObj = nb }
                            else if (bar.hoveredBtnObj === nb) bar.hoveredBtnObj = null
                        }
                    }
                }
            }
        }

        Rectangle {
            objectName: "sidebarSepLine"
            Layout.fillWidth: true
            height: 1
            color: Qt.rgba(1, 1, 1, 0.06)
            Layout.topMargin: 2
            Layout.bottomMargin: 2
        }

        NavButton {
            id: setBtn
            tabIdx: 17
            Layout.fillWidth: true
            text: Config ? (Config.language, Config.trAuto("Настройки")) : "Настройки"
            iconSymbol: "󰒓"
            isActive: bar.currentIndex === 17
            background: Item {}
            onClicked: bar.categorySelected(17)
            onHoveredChanged: {
                if (hovered) { bar.lastHoveredBtnObj = setBtn; bar.hoveredBtnObj = setBtn }
                else if (bar.hoveredBtnObj === setBtn) bar.hoveredBtnObj = null
            }
        }

        NavButton {
            id: trayBtn
            Layout.fillWidth: true
            text: (Config && Config.language === "en") ? "To Tray" : "В трей"
            iconSymbol: "—"
            isActive: false
            background: Item {}
            onClicked: if (ApplicationWindow.window) ApplicationWindow.window.hide()
            onHoveredChanged: {
                if (hovered) { bar.lastHoveredBtnObj = trayBtn; bar.hoveredBtnObj = trayBtn }
                else if (bar.hoveredBtnObj === trayBtn) bar.hoveredBtnObj = null
            }
        }

        NavButton {
            id: quitBtn
            Layout.fillWidth: true
            text: (Config && Config.language === "en") ? "Quit" : "Выключить"
            iconSymbol: "✕"
            isActive: false
            background: Item {}
            onClicked: if (Hypr) Hypr.quitApplication(); else Qt.quit()
            onHoveredChanged: {
                if (hovered) { bar.lastHoveredBtnObj = quitBtn; bar.hoveredBtnObj = quitBtn }
                else if (bar.hoveredBtnObj === quitBtn) bar.hoveredBtnObj = null
            }
        }
    }
}