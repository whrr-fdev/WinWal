import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import HySett
import "components"
import "views"

ApplicationWindow {
    id: root
    visible: Config ? !Config.isDaemonStart() : true
    Component.onCompleted: { if (!visible) Hypr.trimMemory() }
    width: 1040
    minimumWidth: 780
    minimumHeight: 520
    property int startupHeight: Config ? Config.windowHeight : 720
    height: startupHeight
    objectName: "mainControlWindow"
    title: "WinWal"
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint

    property bool modalOpen: false
    property bool hiddenByWorkspace: false
    readonly property bool anyModalActive: modalOpen || (Monitors && Monitors.revertDialogActive)
    readonly property bool isWindowExpanded: root.width > 1150 || root.visibility === Window.FullScreen

    property real wallpapersLiveOpacity: Config ? Config.wallpapersOpacity : 0.59
    property real wallpapersLiveBlur: Config ? Config.wallpapersBlur : 0.15

    Behavior on wallpapersLiveOpacity { NumberAnimation { duration: 400; easing.type: Easing.OutQuint } }
    Behavior on wallpapersLiveBlur { NumberAnimation { duration: 400; easing.type: Easing.OutQuint } }

    function syncWallpapersModeState() {
        if (root.isWindowExpanded) {
            root.wallpapersLiveOpacity = 0.22
            root.wallpapersLiveBlur = 0.14
        } else {
            root.wallpapersLiveOpacity = Config ? Config.wallpapersOpacity : 0.59
            root.wallpapersLiveBlur = Config ? Config.wallpapersBlur : 0.15
        }
        root.syncTabBlur()
    }

    onIsWindowExpandedChanged: syncWallpapersModeState()
    onVisibilityChanged: syncWallpapersModeState()

    property point mousePos: Qt.point(-500, -500)
    property point trail1: Qt.point(-500, -500)
    property point trail2: Qt.point(-500, -500)
    property point trail3: Qt.point(-500, -500)
    property bool trailActive: false
    property point clickPos: Qt.point(-500, -500)
    property real liquidTime: 0.0

    function applyPendingLaunchCmd() {
        if (!Config) return
        var cmd = Config.consumeLaunchCmd()
        if (cmd === "wallpaper") {
            if (root.visibility === Window.FullScreen) root.showNormal()
            root.wallpapersLiveOpacity = NaN
            root.wallpapersLiveBlur = NaN
            pageController.switchPage(14)
            Hypr.dispatch("settiled class:com.whrr.HyprSettings")
        } else if (cmd !== "wallpaper") { Hypr.dispatch("setfloating class:com.whrr.HyprSettings") }
        if (cmd === "apps") {
            root.showNormal()
            pageController.jumpToPageInstant(12)
            root.syncWallpapersModeState()
        } else if (cmd === "search") {
            root.showNormal()
            pageController.jumpToPageInstant(16)
            root.syncWallpapersModeState()
        } else if (cmd === "default") {
            root.showNormal()
            root.syncWallpapersModeState()
        }
    }

    function syncTabBlur() {
        if (!Theme || !Config) return
        if (root.visible && pageController.currentIndex === 14 && Config.autoGlass) {
            var targetBlur = root.isWindowExpanded ? 0.14 : root.wallpapersLiveBlur
            Theme.applyEffectiveBlur(targetBlur)
        } else {
            Theme.applyEffectiveBlur(Theme.windowBlur)
        }
    }

    function triggerClickRipple(cx, cy) {
        if (Config && Config.clickFlashIntensity <= 0.01) return
        clickPos = Qt.point(cx, cy)
        clickAnim.restart()
    }

    NumberAnimation on liquidTime {
        from: 0.0
        to: 628.3185
        duration: 120000
        loops: Animation.Infinite
        running: root.visible && ((Config && Config.borderGlowEnabled && (Config.borderGlowAllPages || pageController.currentIndex === 0)) || (Atmosphere && Atmosphere.inAppEnabled))
    }

    Timer {
        id: trailTimer
        interval: 16
        running: root.visible && root.trailActive
        repeat: true
        onTriggered: {
            root.trail1 = Qt.point(root.trail1.x + (root.mousePos.x - root.trail1.x) * 0.38,
                                   root.trail1.y + (root.mousePos.y - root.trail1.y) * 0.38)
            root.trail2 = Qt.point(root.trail2.x + (root.trail1.x - root.trail2.x) * 0.30,
                                   root.trail2.y + (root.trail1.y - root.trail2.y) * 0.30)
            root.trail3 = Qt.point(root.trail3.x + (root.trail2.x - root.trail3.x) * 0.24,
                                   root.trail3.y + (root.trail2.y - root.trail3.y) * 0.24)
            var d = Math.abs(root.mousePos.x - root.trail3.x) + Math.abs(root.mousePos.y - root.trail3.y)
            if (d < 0.8) root.trailActive = false
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (!root.anyModalActive) root.hide()
        }
    }

    onClosing: (close) => {
        close.accepted = false
        if (!root.anyModalActive) root.hide()
    }

    onVisibleChanged: {
        if (root.hiddenByWorkspace) return
        if (!visible) {
            modalOpen = false
            trailActive = false
            if (Theme) Theme.applyEffectiveBlur(Theme.windowBlur)
            if (!Config.rememberLastTab) pageController.resetToWelcomeInstant()
            Hypr.trimMemory()
        } else {
            if (!Config.rememberLastTab && pageController.currentIndex !== 0) {
                pageController.resetToWelcomeInstant()
            }
            root.applyPendingLaunchCmd()
            root.syncTabBlur()
        }
    }

    Item {
        anchors.fill: parent

        HoverHandler {
            onPointChanged: {
                if (root.mousePos.x < -100) {
                    root.trail1 = point.position
                    root.trail2 = point.position
                    root.trail3 = point.position
                }
                root.mousePos = point.position
                root.trailActive = true
            }
        }

        MouseArea {
            anchors.fill: parent
            z: 1000
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            hoverEnabled: false
            onPressed: (mouse) => {
                root.triggerClickRipple(mouse.x, mouse.y)
                mouse.accepted = false
            }
        }

        Rectangle {
            id: bgRect
            anchors.fill: parent
            property real currentOpacity: (pageController.currentIndex === 14 && Config.autoGlass) ?
                                          root.wallpapersLiveOpacity : Config.opacity
            color: Qt.rgba(Config ? Config.windowBgColor.r : 0.04,
                           Config ? Config.windowBgColor.g : 0.04,
                           Config ? Config.windowBgColor.b : 0.04,
                           currentOpacity)
            radius: (Theme && Theme.isRounded) ? 10 : 0
            border.color: Qt.rgba(1, 1, 1, 0.12)
            border.width: 1
            clip: true

            Behavior on radius { NumberAnimation { duration: 200; easing.type: Easing.OutQuint } }
            Behavior on currentOpacity { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }

            Item {
                id: scaledRoot
                width: parent.width / (Config ? Config.uiScale : 1.0)
                height: parent.height / (Config ? Config.uiScale : 1.0)
                scale: Config ? Config.uiScale : 1.0
                transformOrigin: Item.TopLeft
                clip: true
                layer.enabled: false
                layer.smooth: true
                layer.effect: ShaderEffect {
                    property vector2d u_resolution: Qt.vector2d(scaledRoot.width, scaledRoot.height)
                    property real u_blur: Config ? Config.dialogBlur : 0.65
                    fragmentShader: "qrc:/shaders/qml/shaders/dialog_blur.frag.qsb"
                }

                Item {
                    id: pageController
                    anchors.fill: parent
                    anchors.topMargin: 28
                    clip: true

                    property int currentIndex: 0
                    property int previousIndex: 0
                    property bool inTransition: false
                    property int sidebarW: 136

                    onCurrentIndexChanged: {
                        root.syncWallpapersModeState()
                    }

                    function basePageX(idx) { return idx === 0 ? 0 : sidebarW }
                    function basePageW(idx) { return idx === 0 ? pageController.width : (pageController.width - sidebarW) }

                    function stopAllTransitions() {
                        pageSlideAnim.stop()
                        pageFadeAnim.stop()
                        depthTransition.stop()
                        bounceTransition.stop()
                        vortexTransition.stop()
                        blackholeTransition.stop()
                        portalTransition.stop()
                        shatterTransition.stop()
                    }

                    function jumpToPageInstant(targetIdx) {
                        stopAllTransitions()
                        inTransition = false
                        for (var i = 0; i < pageRepeater.count; ++i) {
                            var p = pageRepeater.itemAt(i)
                            if (p) {
                                p.visible = (i === targetIdx)
                                p.opacity = (i === targetIdx) ? 1.0 : 0.0
                                p.x = basePageX(i)
                                p.y = 0
                                p.scale = 1.0
                                p.rotation = 0
                                p.vortexProgress = 0.0
                            }
                        }
                        previousIndex = targetIdx
                        currentIndex = targetIdx
                        var tp = pageRepeater.itemAt(targetIdx)
                        if (tp && tp.item && tp.item.focusSearchInput) Qt.callLater(tp.item.focusSearchInput)
                    }

                    function resetToWelcomeInstant() {
                        jumpToPageInstant(0)
                    }

                    function runTransition(mode, oldPage, newPage, oldIdx, newIdx, toForward) {
                        stopAllTransitions()
                        var dur = Math.max(40, Config.animDuration)
                        var oldBaseX = basePageX(oldIdx)
                        var newBaseX = basePageX(newIdx)
                        var w = basePageW(newIdx)

                        oldPage.x = oldBaseX; oldPage.y = 0; oldPage.scale = 1.0; oldPage.rotation = 0; oldPage.opacity = 1.0; oldPage.vortexProgress = 0.0; oldPage.vortexDir = 1.0; oldPage.vortexMode = 0.0
                        newPage.x = newBaseX; newPage.y = 0; newPage.scale = 1.0; newPage.rotation = 0; newPage.opacity = 0.0; newPage.vortexProgress = 0.0; newPage.vortexDir = -1.0; newPage.vortexMode = 0.0

                        if (mode === "none" || dur <= 15) {
                            oldPage.visible = false; oldPage.opacity = 0.0
                            newPage.visible = true; newPage.opacity = 1.0
                            inTransition = false
                            return
                        }

                        inTransition = true

                        if (mode === "slide") {
                            var offset = toForward ? w * 0.45 : -w * 0.45
                            newPage.x = newBaseX + offset; newPage.opacity = 0.0; newPage.visible = true
                            slideOutAnim.target = oldPage; slideOutAnim.from = oldBaseX; slideOutAnim.to = oldBaseX - offset * 0.6; slideOutAnim.duration = dur
                            slideOutOp.target = oldPage; slideOutOp.duration = dur * 0.85
                            slideInAnim.target = newPage; slideInAnim.from = newPage.x; slideInAnim.to = newBaseX; slideInAnim.duration = dur
                            slideInOp.target = newPage; slideInOp.duration = dur
                            pageSlideAnim.start()
                        } else if (mode === "depth") {
                            newPage.scale = toForward ? 1.12 : 0.88; newPage.visible = true
                            depthOutOp.target = oldPage; depthOutOp.duration = dur
                            depthOutSc.target = oldPage; depthOutSc.to = toForward ? 0.86 : 1.12; depthOutSc.duration = dur
                            depthInOp.target = newPage; depthInOp.duration = dur
                            depthInSc.target = newPage; depthInSc.from = newPage.scale; depthInSc.duration = dur
                            depthTransition.start()
                        } else if (mode === "bounce") {
                            newPage.y = toForward ? 70 : -70; newPage.scale = 0.94; newPage.visible = true
                            bounceOutOp.target = oldPage; bounceOutOp.duration = dur * 0.75
                            bounceOutY.target = oldPage; bounceOutY.to = toForward ? -40 : 40; bounceOutY.duration = dur
                            bounceInOp.target = newPage; bounceInOp.duration = dur
                            bounceInY.target = newPage; bounceInY.from = newPage.y; bounceInY.duration = dur * 1.15
                            bounceInSc.target = newPage; bounceInSc.duration = dur * 1.15
                            bounceTransition.start()
                        } else if (mode === "blackhole") {
                            var dir = toForward ? 1.0 : -1.0
                            oldPage.vortexMode = 0.0
                            oldPage.vortexDir = dir
                            oldPage.vortexProgress = 0.0
                            oldPage.scale = 1.0
                            oldPage.rotation = 0
                            oldPage.opacity = 1.0
                            oldPage.visible = true

                            newPage.vortexMode = 0.0
                            newPage.vortexDir = -dir
                            newPage.vortexProgress = 0.65
                            newPage.scale = 0.04
                            newPage.rotation = -140 * dir
                            newPage.opacity = 0.0
                            newPage.visible = true

                            bhOutVortex.target = oldPage; bhOutVortex.duration = 380
                            bhOutSc.target = oldPage; bhOutSc.duration = 380
                            bhOutRot.target = oldPage; bhOutRot.to = 200 * dir; bhOutRot.duration = 380
                            bhOutOp.target = oldPage; bhOutOp.duration = 380

                            bhInDelay.duration = 270
                            bhInVortex.target = newPage; bhInVortex.duration = 370
                            bhInSc.target = newPage; bhInSc.duration = 370
                            bhInRot.target = newPage; bhInRot.from = -140 * dir; bhInRot.duration = 370
                            bhInOp.target = newPage; bhInOp.duration = 240

                            blackholeTransition.start()
                        } else if (mode === "portal") {
                            var pdir = toForward ? 1.0 : -1.0
                            oldPage.vortexMode = 1.0
                            oldPage.vortexProgress = 0.0
                            oldPage.scale = 1.0
                            oldPage.rotation = 0
                            oldPage.opacity = 1.0
                            oldPage.visible = true

                            newPage.vortexMode = 1.0
                            newPage.vortexProgress = 0.75
                            newPage.scale = 0.16
                            newPage.rotation = 35 * pdir
                            newPage.opacity = 0.0
                            newPage.visible = true

                            portalOutWave.target = oldPage; portalOutWave.duration = 340
                            portalOutSc.target = oldPage; portalOutSc.duration = 340
                            portalOutRot.target = oldPage; portalOutRot.to = -35 * pdir; portalOutRot.duration = 340
                            portalOutOp.target = oldPage; portalOutOp.duration = 320

                            portalInDelay.duration = 200
                            portalInWave.target = newPage; portalInWave.duration = 380
                            portalInSc.target = newPage; portalInSc.duration = 380
                            portalInRot.target = newPage; portalInRot.from = 35 * pdir; portalInRot.duration = 380
                            portalInOp.target = newPage; portalInOp.duration = 240

                            portalTransition.start()
                        } else if (mode === "shatter") {
                            var sdir = toForward ? 1.0 : -1.0
                            oldPage.vortexMode = 2.0
                            oldPage.vortexDir = sdir
                            oldPage.vortexProgress = 0.0
                            oldPage.scale = 1.0
                            oldPage.opacity = 1.0
                            oldPage.visible = true

                            newPage.vortexMode = 2.0
                            newPage.vortexDir = -sdir
                            newPage.vortexProgress = 0.85
                            newPage.scale = 1.16
                            newPage.x = newBaseX + 90 * sdir
                            newPage.opacity = 0.0
                            newPage.visible = true

                            shatterOutProg.target = oldPage; shatterOutProg.duration = 300
                            shatterOutSc.target = oldPage; shatterOutSc.duration = 300
                            shatterOutX.target = oldPage; shatterOutX.from = oldBaseX; shatterOutX.to = oldBaseX - 90 * sdir; shatterOutX.duration = 300
                            shatterOutOp.target = oldPage; shatterOutOp.duration = 280

                            shatterInDelay.duration = 140
                            shatterInProg.target = newPage; shatterInProg.duration = 320
                            shatterInSc.target = newPage; shatterInSc.duration = 320
                            shatterInX.target = newPage; shatterInX.from = newPage.x; shatterInX.to = newBaseX; shatterInX.duration = 320
                            shatterInOp.target = newPage; shatterInOp.duration = 220

                            shatterTransition.start()
                        } else if (mode === "vortex") {
                            newPage.x = newBaseX + (toForward ? 110 : -110); newPage.y = toForward ? 28 : -28; newPage.scale = 0.90; newPage.visible = true
                            vortexOutOp.target = oldPage; vortexOutOp.duration = dur * 0.8
                            vortexOutX.target = oldPage; vortexOutX.from = oldBaseX; vortexOutX.to = oldBaseX + (toForward ? -80 : 80); vortexOutX.duration = dur
                            vortexOutSc.target = oldPage; vortexOutSc.duration = dur
                            vortexInOp.target = newPage; vortexInOp.duration = dur
                            vortexInX.target = newPage; vortexInX.from = newPage.x; vortexInX.to = newBaseX; vortexInX.duration = dur
                            vortexInY.target = newPage; vortexInY.from = newPage.y; vortexInY.duration = dur
                            vortexInSc.target = newPage; vortexInSc.duration = dur
                            vortexTransition.start()
                        } else {
                            newPage.visible = true
                            fadeOldAnim.target = oldPage; fadeOldAnim.from = 1.0; fadeOldAnim.to = 0.0; fadeOldAnim.duration = dur
                            fadeNewAnim.target = newPage; fadeNewAnim.from = 0.0; fadeNewAnim.to = 1.0; fadeNewAnim.duration = dur
                            pageFadeAnim.start()
                        }
                    }

                    function switchPage(targetIdx) {
                        if (targetIdx === currentIndex) return
                        inTransition = true
                        previousIndex = currentIndex
                        currentIndex = targetIdx
                        var oldPage = pageRepeater.itemAt(previousIndex)
                        var newPage = pageRepeater.itemAt(targetIdx)
                        if (!oldPage || !newPage) {
                            jumpToPageInstant(targetIdx)
                            return
                        }
                        var isWelcomeTransition = (previousIndex === 0 || targetIdx === 0)
                        var mode = isWelcomeTransition ? Config.welcomeAnimType : Config.animType
                        runTransition(mode, oldPage, newPage, previousIndex, targetIdx, targetIdx > previousIndex)
                    }

                    function cleanupTransition() {
                        var p = pageRepeater.itemAt(pageController.previousIndex)
                        if (p && pageController.previousIndex !== pageController.currentIndex) {
                            p.visible = false
                            p.x = basePageX(pageController.previousIndex)
                            p.y = 0
                            p.scale = 1.0
                            p.rotation = 0
                            p.opacity = 0.0
                            p.vortexProgress = 0.0
                        }
                        var cur = pageRepeater.itemAt(pageController.currentIndex)
                        if (cur) {
                            cur.visible = true
                            cur.x = basePageX(pageController.currentIndex)
                            cur.y = 0
                            cur.scale = 1.0
                            cur.rotation = 0
                            cur.opacity = 1.0
                            cur.vortexProgress = 0.0
                        }
                        inTransition = false
                        if (cur && cur.item && cur.item.focusSearchInput) Qt.callLater(cur.item.focusSearchInput)
                    }

                    ParallelAnimation {
                        id: pageSlideAnim
                        NumberAnimation { id: slideOutAnim; property: "x"; easing.type: Easing.OutCubic }
                        NumberAnimation { id: slideOutOp; property: "opacity"; from: 1.0; to: 0.0 }
                        NumberAnimation { id: slideInAnim; property: "x"; easing.type: Easing.OutCubic }
                        NumberAnimation { id: slideInOp; property: "opacity"; from: 0.0; to: 1.0 }
                        onFinished: pageController.cleanupTransition()
                    }
                    ParallelAnimation {
                        id: pageFadeAnim
                        NumberAnimation { id: fadeOldAnim; property: "opacity" }
                        NumberAnimation { id: fadeNewAnim; property: "opacity" }
                        onFinished: pageController.cleanupTransition()
                    }
                    ParallelAnimation {
                        id: depthTransition
                        NumberAnimation { id: depthOutOp; property: "opacity"; from: 1.0; to: 0.0 }
                        NumberAnimation { id: depthOutSc; property: "scale"; from: 1.0; easing.type: Easing.OutCubic }
                        NumberAnimation { id: depthInOp; property: "opacity"; from: 0.0; to: 1.0 }
                        NumberAnimation { id: depthInSc; property: "scale"; to: 1.0; easing.type: Easing.OutCubic }
                        onFinished: pageController.cleanupTransition()
                    }
                    ParallelAnimation {
                        id: bounceTransition
                        NumberAnimation { id: bounceOutOp; property: "opacity"; from: 1.0; to: 0.0 }
                        NumberAnimation { id: bounceOutY; property: "y"; from: 0; easing.type: Easing.OutQuad }
                        NumberAnimation { id: bounceInOp; property: "opacity"; from: 0.0; to: 1.0 }
                        NumberAnimation { id: bounceInY; property: "y"; to: 0; easing.type: Easing.OutBack }
                        NumberAnimation { id: bounceInSc; property: "scale"; from: 0.94; to: 1.0; easing.type: Easing.OutBack }
                        onFinished: pageController.cleanupTransition()
                    }
                    ParallelAnimation {
                        id: vortexTransition
                        NumberAnimation { id: vortexOutOp; property: "opacity"; from: 1.0; to: 0.0 }
                        NumberAnimation { id: vortexOutX; property: "x"; easing.type: Easing.OutCubic }
                        NumberAnimation { id: vortexOutSc; property: "scale"; from: 1.0; to: 0.90; easing.type: Easing.OutCubic }
                        NumberAnimation { id: vortexInOp; property: "opacity"; from: 0.0; to: 1.0 }
                        NumberAnimation { id: vortexInX; property: "x"; easing.type: Easing.OutCubic }
                        NumberAnimation { id: vortexInY; property: "y"; to: 0; easing.type: Easing.OutCubic }
                        NumberAnimation { id: vortexInSc; property: "scale"; from: 0.90; to: 1.0; easing.type: Easing.OutCubic }
                        onFinished: pageController.cleanupTransition()
                    }
                    ParallelAnimation {
                        id: blackholeTransition
                        NumberAnimation { id: bhOutVortex; property: "vortexProgress"; from: 0.0; to: 1.0; easing.type: Easing.InCubic }
                        NumberAnimation { id: bhOutSc; property: "scale"; from: 1.0; to: 0.02; easing.type: Easing.InBack; easing.overshoot: 0.6 }
                        NumberAnimation { id: bhOutRot; property: "rotation"; from: 0; easing.type: Easing.InCubic }
                        NumberAnimation { id: bhOutOp; property: "opacity"; from: 1.0; to: 0.0; easing.type: Easing.InExpo }
                        SequentialAnimation {
                            PauseAnimation { id: bhInDelay }
                            ParallelAnimation {
                                NumberAnimation { id: bhInVortex; property: "vortexProgress"; from: 0.65; to: 0.0; easing.type: Easing.OutCubic }
                                NumberAnimation { id: bhInSc; property: "scale"; from: 0.04; to: 1.0; easing.type: Easing.OutBack; easing.overshoot: 0.85 }
                                NumberAnimation { id: bhInRot; property: "rotation"; to: 0; easing.type: Easing.OutCubic }
                                NumberAnimation { id: bhInOp; property: "opacity"; from: 0.0; to: 1.0; easing.type: Easing.OutCubic }
                            }
                        }
                        onFinished: pageController.cleanupTransition()
                    }
                    ParallelAnimation {
                        id: portalTransition
                        NumberAnimation { id: portalOutWave; property: "vortexProgress"; from: 0.0; to: 1.0; easing.type: Easing.InCubic }
                        NumberAnimation { id: portalOutSc; property: "scale"; from: 1.0; to: 1.85; easing.type: Easing.InCubic }
                        NumberAnimation { id: portalOutRot; property: "rotation"; from: 0; easing.type: Easing.InCubic }
                        NumberAnimation { id: portalOutOp; property: "opacity"; from: 1.0; to: 0.0; easing.type: Easing.InQuad }
                        SequentialAnimation {
                            PauseAnimation { id: portalInDelay }
                            ParallelAnimation {
                                NumberAnimation { id: portalInWave; property: "vortexProgress"; from: 0.75; to: 0.0; easing.type: Easing.OutCubic }
                                NumberAnimation { id: portalInSc; property: "scale"; from: 0.16; to: 1.0; easing.type: Easing.OutBack; easing.overshoot: 0.75 }
                                NumberAnimation { id: portalInRot; property: "rotation"; to: 0; easing.type: Easing.OutCubic }
                                NumberAnimation { id: portalInOp; property: "opacity"; from: 0.0; to: 1.0; easing.type: Easing.OutCubic }
                            }
                        }
                        onFinished: pageController.cleanupTransition()
                    }
                    ParallelAnimation {
                        id: shatterTransition
                        NumberAnimation { id: shatterOutProg; property: "vortexProgress"; from: 0.0; to: 1.0; easing.type: Easing.InCubic }
                        NumberAnimation { id: shatterOutSc; property: "scale"; from: 1.0; to: 0.82; easing.type: Easing.InCubic }
                        NumberAnimation { id: shatterOutX; property: "x"; easing.type: Easing.InBack }
                        NumberAnimation { id: shatterOutOp; property: "opacity"; from: 1.0; to: 0.0; easing.type: Easing.InQuad }
                        SequentialAnimation {
                            PauseAnimation { id: shatterInDelay }
                            ParallelAnimation {
                                NumberAnimation { id: shatterInProg; property: "vortexProgress"; from: 0.85; to: 0.0; easing.type: Easing.OutCubic }
                                NumberAnimation { id: shatterInSc; property: "scale"; from: 1.16; to: 1.0; easing.type: Easing.OutBack; easing.overshoot: 0.8 }
                                NumberAnimation { id: shatterInX; property: "x"; easing.type: Easing.OutBack; easing.overshoot: 0.7 }
                                NumberAnimation { id: shatterInOp; property: "opacity"; from: 0.0; to: 1.0; easing.type: Easing.OutCubic }
                            }
                        }
                        onFinished: pageController.cleanupTransition()
                    }

                    Repeater {
                        id: pageRepeater
                        model: [
                            "views/WelcomeView.qml",
                            "views/LookSettingsView.qml",
                            "views/AnimsSettingsView.qml",
                            "views/CursorSettingsView.qml",
                            "views/MonitorsSettingsView.qml",
                            "views/DevicesSettingsView.qml",
                            "views/NetworkSettingsView.qml",
                            "views/BluetoothSettingsView.qml",
                            "views/WaybarSettingsView.qml",
                            "views/AutostartSettingsView.qml",
                            "views/BindsSettingsView.qml",
                            "views/RulesSettingsView.qml",
                            "views/AppsSettingsView.qml",
                            "views/ShadersView.qml",
                            "views/WallpapersView.qml",
                            "views/WidgetsSettingsView.qml",
                            "views/SearchView.qml",
                            "views/AppSettingsView.qml",
                            "views/MakoSettingsView.qml",
                            "views/AtmosphereView.qml"
                        ]

                        Loader {
                            id: pageLoader
                            z: (pageController.inTransition && index === pageController.previousIndex) ? 2 : 1
                            x: index === 0 ? 0 : pageController.sidebarW
                            width: index === 0 ? pageController.width : (pageController.width - pageController.sidebarW)
                            height: pageController.height
                            active: root.visible && (
                                    index === pageController.currentIndex ||
                                    (pageController.inTransition && index === pageController.previousIndex))
                            source: modelData
                            visible: index === 0
                            opacity: index === 0 ? 1.0 : 0.0
                            transformOrigin: Item.Center
                            property real vortexProgress: 0.0
                            property real vortexDir: 1.0
                            property real vortexMode: 0.0
                            layer.enabled: pageController.inTransition && vortexProgress > 0.001
                            layer.smooth: true
                            layer.effect: ShaderEffect {
                                property vector2d u_resolution: Qt.vector2d(pageLoader.width, pageLoader.height)
                                property real u_progress: pageLoader.vortexProgress
                                property real u_direction: pageLoader.vortexDir
                                property real u_mode: pageLoader.vortexMode
                                fragmentShader: "qrc:/shaders/qml/shaders/vortex_suck.frag.qsb"
                            }

                            onLoaded: {
                                if ((index === 12 || index === 16) && item && item.focusSearchInput && index === pageController.currentIndex) Qt.callLater(item.focusSearchInput)
                                if (index === 0 && item) {
                                    item.navigateTo.connect((target) => pageController.switchPage(target))
                                }
                            }
                        }
                    }
                }

                Sidebar {
                    id: sidebar
                    width: 136
                    height: parent.height
                    property bool showBar: pageController.currentIndex > 0 && !(blackholeTransition.running && pageController.previousIndex === 0 && bhInOp.target && bhInOp.target.opacity < 0.05)
                    x: showBar ? 0 : -136
                    opacity: showBar ? 1.0 : 0.0
                    visible: opacity > 0.01
                    currentIndex: pageController.currentIndex
                    onCategorySelected: (idx) => pageController.switchPage(idx)
                    onReturnToWelcome: () => pageController.switchPage(0)

                    Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: 160 } }
                }

                ShaderEffectSource {
                    id: pageSceneSource
                    sourceItem: (root.visible && Atmosphere && Atmosphere.inAppEnabled && (Atmosphere.rainEnabled || Atmosphere.snowEnabled)) ? pageController : null
                    anchors.fill: pageController
                    visible: false
                    live: root.visible && Atmosphere && Atmosphere.inAppEnabled && (Atmosphere.rainEnabled || Atmosphere.snowEnabled)
                }

                AtmosphereCanvas {
                    id: inAppWaterMap
                    roundedTheme: (Theme && Theme.isRounded) ? true : false
                    anchors.fill: parent
                    visible: root.visible && Atmosphere && Atmosphere.inAppEnabled
                    enabled: false
                    globalMode: false
                    renderPass: 0
                    activeRunning: root.visible && Atmosphere && Atmosphere.inAppEnabled
                    cursorPos: Qt.point(root.mousePos.x / (Config ? Config.uiScale : 1.0),
                                        root.mousePos.y / (Config ? Config.uiScale : 1.0))
                }

                ShaderEffectSource {
                    id: inAppWaterMapSource
                    sourceItem: (root.visible && Atmosphere && Atmosphere.inAppEnabled) ? inAppWaterMap : null
                    anchors.fill: parent
                    visible: false
                    hideSource: true
                    smooth: true
                    live: root.visible && Atmosphere && Atmosphere.inAppEnabled
                }

                AtmosphereCanvas {
                    id: inAppOverlayMap
                    anchors.fill: parent
                    visible: root.visible && Atmosphere && Atmosphere.inAppEnabled
                    enabled: false
                    globalMode: false
                    renderPass: 1
                    masterCanvas: inAppWaterMap
                    activeRunning: root.visible && Atmosphere && Atmosphere.inAppEnabled
                }

                ShaderEffectSource {
                    id: inAppOverlayMapSource
                    sourceItem: (root.visible && Atmosphere && Atmosphere.inAppEnabled) ? inAppOverlayMap : null
                    anchors.fill: parent
                    visible: false
                    hideSource: true
                    smooth: true
                    live: root.visible && Atmosphere && Atmosphere.inAppEnabled
                }

                ShaderEffect {
                    id: inAppAtmosphereShader
                    anchors.fill: parent
                    z: 95
                    enabled: false
                    visible: root.visible && Atmosphere && Atmosphere.inAppEnabled
                    property vector2d u_resolution: Qt.vector2d(width, height)
                    property real u_has_scene: 0.0
                    property real u_refraction_strength: 1.15 * (Atmosphere ? Atmosphere.dropRefraction : 1.0)
                    property real u_time: root.liquidTime * (Config ? Config.borderGlowSpeed : 1.0)
                    property vector2d u_mouse: Qt.vector2d(root.mousePos.x / (Config ? Config.uiScale : 1.0), root.mousePos.y / (Config ? Config.uiScale : 1.0))
                    property real u_cursor_strength: Config ? Config.cursorGlowIntensity : 0.65
                    property vector3d u_cursor_color: Config ? Qt.vector3d(Config.cursorCustomColor.r, Config.cursorCustomColor.g, Config.cursorCustomColor.b) : Qt.vector3d(1, 1, 1)
                    property real u_border_strength: (Config && Config.borderGlowEnabled && (Config.borderGlowAllPages || pageController.currentIndex === 0)) ? Config.borderGlowIntensity : 0.0
                    property real u_border_spread: Config ? Config.borderGlowSpread : 0.45
                    property real u_border_color_mode: Config ? Config.borderGlowColorMode : 0
                    property vector3d u_border_color: Config ? Qt.vector3d(Config.borderGlowCustomColor.r, Config.borderGlowCustomColor.g, Config.borderGlowCustomColor.b) : Qt.vector3d(1, 1, 1)
                    property real idleBoost: (Atmosphere && Atmosphere.idleFogEnabled) ? (inAppWaterMap.idleFogBoost * Atmosphere.idleFogStrength) : 0.0
                    property real baseSpread: Atmosphere ? ((Atmosphere.snowEnabled && !Atmosphere.rainEnabled) ? Atmosphere.snowScreenFrostSpread : Atmosphere.fogSpread) : 0.45
                    property real baseInt: Atmosphere ? ((Atmosphere.snowEnabled && !Atmosphere.rainEnabled) ? Atmosphere.snowScreenFrostIntensity : Atmosphere.fogIntensity) : 0.65
                    property bool inAppMistOn: Atmosphere ? ((Atmosphere.snowEnabled && !Atmosphere.rainEnabled) ? Atmosphere.snowScreenFrostEnabled : Atmosphere.condensationEnabled) : false
                    property real effSpread: Math.min(1.0, baseSpread + idleBoost * (1.0 - baseSpread) * 0.78)
                    property real effInt: Math.min(0.995, baseInt + idleBoost * (0.995 - baseInt))
                    property real u_fog_strength: (inAppMistOn && !(Atmosphere.globalOverlayEnabled && Atmosphere.syncWithOverlay)) ? (Math.round(effSpread * 100) + effInt) : 0.0
                    property real u_lightning: inAppWaterMap.lightningFlash
                    property real u_lightning_x: inAppWaterMap.lightningPosX
                    property real u_drop_tint_strength: Atmosphere ? Atmosphere.dropTintStrength : 0.20
                    property real u_drop_highlight: Atmosphere ? Atmosphere.dropHighlightStrength : 1.0
                    property vector3d u_drop_tint_color: Atmosphere ? Atmosphere.effectiveDropTintVec : Qt.vector3d(0.015, 0.015, 0.018)
                    property real u_wipe_strength: Atmosphere ? Atmosphere.wipeTrailStrength : 0.55
                    property real u_frost_strength: (Atmosphere && Atmosphere.snowEnabled) ? (((Atmosphere.snowFrostStyle === 0 && Atmosphere.snowVelvetFlakesEnabled) ? 3 : Atmosphere.snowFrostStyle) * 10.0 + Atmosphere.snowFrostPatternStrength) : 0.0
                    property real u_idle_fog_boost: (Atmosphere && Atmosphere.condensationEnabled && Atmosphere.idleFogEnabled && !(Atmosphere.globalOverlayEnabled && Atmosphere.syncWithOverlay)) ? (inAppWaterMap.idleFogBoost * Atmosphere.idleFogStrength) : 0.0
                    property variant u_waterMap: inAppWaterMapSource
                    property variant u_overlayMap: inAppOverlayMapSource
                    property variant u_scene: pageSceneSource
                    fragmentShader: "qrc:/shaders/qml/shaders/atmosphere_fx.frag.qsb"
                }
            }

            ShaderEffect {
                id: cursorFollowItem
                width: 240
                height: 240
                x: root.mousePos.x - 120
                y: root.mousePos.y - 120
                z: 97
                enabled: false
                visible: root.visible && Config && Config.cursorGlowIntensity > 0.01 && root.mousePos.x > -100

                property vector2d u_size: Qt.vector2d(240, 240)
                property vector2d u_origin: Qt.vector2d(x, y)
                property vector2d u_mouse: Qt.vector2d(root.mousePos.x, root.mousePos.y)
                property vector2d u_trail1: Qt.vector2d(root.trail1.x, root.trail1.y)
                property vector2d u_trail2: Qt.vector2d(root.trail2.x, root.trail2.y)
                property vector2d u_trail3: Qt.vector2d(root.trail3.x, root.trail3.y)
                property real u_cursor_strength: Config ? Config.cursorGlowIntensity : 0.65
                property real u_liquid_enabled: (Theme && (Theme.controlsLiquidGlass || Theme.glassRefraction > 0.01)) ? 1.0 : 0.0
                property real u_refraction: Theme ? Math.max(Theme.glassRefraction, Theme.controlsLiquidGlass ? 0.55 : 0.0) : 0.55
                property vector3d u_custom_color: Config ? Qt.vector3d(Config.cursorCustomColor.r, Config.cursorCustomColor.g, Config.cursorCustomColor.b) : Qt.vector3d(1, 1, 1)
                fragmentShader: "qrc:/shaders/qml/shaders/cursor_fx.frag.qsb"
            }

            ShaderEffect {
                id: clickFxItem
                width: 280
                height: 280
                x: root.clickPos.x - 140
                y: root.clickPos.y - 140
                z: 98
                enabled: false
                visible: clickAnim.running

                property real u_progress: 0.0
                property real u_intensity: Config ? Config.clickFlashIntensity : 0.70
                property real u_shape: Config ? Config.clickFlashShape : 0
                property vector3d u_custom_color: Config ? Qt.vector3d(Config.cursorCustomColor.r, Config.cursorCustomColor.g, Config.cursorCustomColor.b) : Qt.vector3d(1, 1, 1)

                NumberAnimation on u_progress {
                    id: clickAnim
                    from: 0.001
                    to: 1.0
                    duration: 460
                    running: false
                    easing.type: Easing.OutCubic
                }

                fragmentShader: "qrc:/shaders/qml/shaders/click_fx.frag.qsb"
            }

            // Прямое изменение размера окна мышью за любые края и углы (работает 100% на безрамочных окнах!)
            Item {
                id: winResizeBorders
                anchors.fill: parent
                z: 3000
                visible: root.visibility === Window.Windowed

                property int activeEdges: 0
                property real startScreenX: 0
                property real startScreenY: 0
                property int origX: 0
                property int origY: 0
                property int origW: 0
                property int origH: 0

                function startResizeDrag(edges, mouseObj, areaItem) {
                    activeEdges = edges
                    var gp = areaItem.mapToGlobal(mouseObj.x, mouseObj.y)
                    startScreenX = gp.x
                    startScreenY = gp.y
                    origX = root.x
                    origY = root.y
                    origW = root.width
                    origH = root.height
                }

                function updateResizeDrag(mouseObj, areaItem) {
                    if (activeEdges === 0) return
                    var gp = areaItem.mapToGlobal(mouseObj.x, mouseObj.y)
                    var dx = Math.round(gp.x - startScreenX)
                    var dy = Math.round(gp.y - startScreenY)
                    var nx = origX, ny = origY, nw = origW, nh = origH

                    if (activeEdges & Qt.RightEdge) {
                        nw = Math.max(root.minimumWidth, origW + dx)
                    }
                    if (activeEdges & Qt.BottomEdge) {
                        nh = Math.max(root.minimumHeight, origH + dy)
                    }
                    if (activeEdges & Qt.LeftEdge) {
                        var cw = Math.max(root.minimumWidth, origW - dx)
                        nx = origX + (origW - cw)
                        nw = cw
                    }
                    if (activeEdges & Qt.TopEdge) {
                        var ch = Math.max(root.minimumHeight, origH - dy)
                        ny = origY + (origH - ch)
                        nh = ch
                    }
                    root.x = nx; root.y = ny; root.width = nw; root.height = nh
                }

                // Левая грань
                MouseArea {
                    id: maL; anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                    anchors.topMargin: 10; anchors.bottomMargin: 10; width: 7; cursorShape: Qt.SizeHorCursor
                    onPressed: (m) => winResizeBorders.startResizeDrag(Qt.LeftEdge, m, maL)
                    onPositionChanged: (m) => { if (pressed) winResizeBorders.updateResizeDrag(m, maL) }
                }
                // Правая грань
                MouseArea {
                    id: maR; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
                    anchors.topMargin: 10; anchors.bottomMargin: 10; width: 7; cursorShape: Qt.SizeHorCursor
                    onPressed: (m) => winResizeBorders.startResizeDrag(Qt.RightEdge, m, maR)
                    onPositionChanged: (m) => { if (pressed) winResizeBorders.updateResizeDrag(m, maR) }
                }
                // Верхняя грань
                MouseArea {
                    id: maT; anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
                    anchors.leftMargin: 10; anchors.rightMargin: 105; height: 6; cursorShape: Qt.SizeVerCursor
                    onPressed: (m) => winResizeBorders.startResizeDrag(Qt.TopEdge, m, maT)
                    onPositionChanged: (m) => { if (pressed) winResizeBorders.updateResizeDrag(m, maT) }
                }
                // Нижняя грань
                MouseArea {
                    id: maB; anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.right: parent.right
                    anchors.leftMargin: 10; anchors.rightMargin: 10; height: 7; cursorShape: Qt.SizeVerCursor
                    onPressed: (m) => winResizeBorders.startResizeDrag(Qt.BottomEdge, m, maB)
                    onPositionChanged: (m) => { if (pressed) winResizeBorders.updateResizeDrag(m, maB) }
                }
                // 4 угла
                MouseArea {
                    id: maTL; anchors.left: parent.left; anchors.top: parent.top; width: 12; height: 12; cursorShape: Qt.SizeFDiagCursor
                    onPressed: (m) => winResizeBorders.startResizeDrag(Qt.LeftEdge | Qt.TopEdge, m, maTL)
                    onPositionChanged: (m) => { if (pressed) winResizeBorders.updateResizeDrag(m, maTL) }
                }
                MouseArea {
                    id: maTR; anchors.right: parent.right; anchors.top: parent.top; width: 12; height: 12; cursorShape: Qt.SizeBDiagCursor
                    onPressed: (m) => winResizeBorders.startResizeDrag(Qt.RightEdge | Qt.TopEdge, m, maTR)
                    onPositionChanged: (m) => { if (pressed) winResizeBorders.updateResizeDrag(m, maTR) }
                }
                MouseArea {
                    id: maBL; anchors.left: parent.left; anchors.bottom: parent.bottom; width: 12; height: 12; cursorShape: Qt.SizeBDiagCursor
                    onPressed: (m) => winResizeBorders.startResizeDrag(Qt.LeftEdge | Qt.BottomEdge, m, maBL)
                    onPositionChanged: (m) => { if (pressed) winResizeBorders.updateResizeDrag(m, maBL) }
                }
                MouseArea {
                    id: maBR; anchors.right: parent.right; anchors.bottom: parent.bottom; width: 12; height: 12; cursorShape: Qt.SizeFDiagCursor
                    onPressed: (m) => winResizeBorders.startResizeDrag(Qt.RightEdge | Qt.BottomEdge, m, maBR)
                    onPositionChanged: (m) => { if (pressed) winResizeBorders.updateResizeDrag(m, maBR) }
                }
            }
            Item {
                id: winCaptionBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 32
                z: 1005

                MouseArea {
                    anchors.fill: parent
                    anchors.leftMargin: sidebar.showBar ? Math.round(136 * (Config ? Config.uiScale : 1.0)) : 0
                    anchors.rightMargin: 105
                    acceptedButtons: Qt.LeftButton
                    onPressed: root.startSystemMove()
                    onDoubleClicked: {
                        if (root.visibility === Window.Maximized) root.showNormal()
                        else root.showMaximized()
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.topMargin: 7; anchors.rightMargin: 12
                    spacing: 4

                    Button {
                        id: minWinBtn
                        width: 26; height: 22; flat: true
                        onClicked: root.showMinimized()
                        background: Rectangle {
                            radius: (Theme && Theme.isRounded) ? 4 : 0
                            color: minWinBtn.hovered ? Qt.rgba(1, 1, 1, 0.14) : "transparent"
                        }
                        contentItem: Text {
                            renderType: Text.QtRendering
                            text: "—"; color: minWinBtn.hovered ? "#ffffff" : "#999999"
                            font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Button {
                        id: maxWinBtn
                        width: 26; height: 22; flat: true
                        onClicked: {
                            if (root.visibility === Window.Maximized) root.showNormal()
                            else root.showMaximized()
                        }
                        background: Rectangle {
                            radius: (Theme && Theme.isRounded) ? 4 : 0
                            color: maxWinBtn.hovered ? Qt.rgba(1, 1, 1, 0.14) : "transparent"
                        }
                        contentItem: Text {
                            renderType: Text.QtRendering
                            text: root.visibility === Window.Maximized ? "❐" : "□"
                            color: maxWinBtn.hovered ? "#ffffff" : "#999999"
                            font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Button {
                        id: clsWinBtn
                        width: 26; height: 22; flat: true
                        onClicked: root.hide()
                        background: Rectangle {
                            radius: (Theme && Theme.isRounded) ? 4 : 0
                            color: clsWinBtn.hovered ? Qt.rgba(0.92, 0.22, 0.25, 0.85) : "transparent"
                        }
                        contentItem: Text {
                            renderType: Text.QtRendering
                            text: "✕"; color: clsWinBtn.hovered ? "#ffffff" : "#999999"
                            font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            ShaderEffect {
                id: borderGlowLayer
                anchors.fill: parent
                z: 99
                enabled: false
                property bool shouldShow: Config && Config.borderGlowEnabled &&
                                          (Config.borderGlowAllPages || pageController.currentIndex === 0)
                opacity: shouldShow ? 1.0 : 0.0
                visible: opacity > 0.005

                Behavior on opacity { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }

                property vector2d u_resolution: Qt.vector2d(width, height)
                property real u_radius: bgRect.radius
                property real u_time: root.liquidTime * (Config ? Config.borderGlowSpeed : 1.0)
                property real u_intensity: Config ? Config.borderGlowIntensity : 0.75
                property real u_spread: Config ? Config.borderGlowSpread : 0.45
                property real u_color_mode: Config ? Config.borderGlowColorMode : 0
                property real u_direction: Config ? Config.borderGlowDirection : 0
                property real u_cursor_strength: Config ? Config.cursorGlowIntensity : 0.65
                property vector2d u_mouse: Qt.vector2d(root.mousePos.x, root.mousePos.y)
                property vector3d u_custom_color: Config ? Qt.vector3d(Config.borderGlowCustomColor.r, Config.borderGlowCustomColor.g, Config.borderGlowCustomColor.b) : Qt.vector3d(1, 1, 1)
                fragmentShader: "qrc:/shaders/qml/shaders/neon_border.frag.qsb"
            }
        }
    }
}
