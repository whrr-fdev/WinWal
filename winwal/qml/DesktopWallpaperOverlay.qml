import QtQuick
import QtQuick.Window
import QtMultimedia

Window {
    id: deskWin
    objectName: "desktopWallWin"
    property bool isOverlayWindow: true
    visible: false
    x: Screen.virtualX
    y: Screen.virtualY
    width: Screen.width
    height: Screen.height
    title: "WinWalDesktopWallpaper"
    color: "#08080a"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.Tool | Qt.WindowDoesNotAcceptFocus | Qt.WindowTransparentForInput

    property string currentVideoSource: ""
    property string currentImageSource: ""
    property bool wasVideoBefore: false
    property bool videoCoveredPaused: false
    property real transProgress: 1.0
    property real transMode: 0.0
    readonly property bool inTransition: transProgress < 0.999 || swapTimer.running

    property string pendingUrl: ""
    property bool pendingIsVideo: false

    function setVideoPausedByMaximized(paused) {
        if (videoCoveredPaused === paused) return
        videoCoveredPaused = paused
        if (currentVideoSource !== "" && deskWin.visible) {
            if (paused) vidPlayer.pause()
            else vidPlayer.play()
        }
    }

    function initCurrentImage(fileUrl) {
        if (currentImageSource !== "" || currentVideoSource !== "" || fileUrl === "") return
        currentImageSource = fileUrl
        curImg.source = fileUrl
    }

    function playVideoWallpaper(fileUrl, animKind) {
        swapTimer.stop()
        transAnim.stop()
        transMode = animKind

        var hasOld = (currentVideoSource !== "" || currentImageSource !== "")
        if (!hasOld) {
            wasVideoBefore = false
            currentVideoSource = fileUrl
            currentImageSource = ""
            curImg.source = ""
            oldImg.source = ""
            deskWin.visible = true
            if (vidPlayer.source.toString() !== fileUrl) {
                vidPlayer.stop()
                vidPlayer.source = fileUrl
            }
            if (!videoCoveredPaused) vidPlayer.play()
            transProgress = 1.0
            return
        }

        if (currentVideoSource !== "") {
            wasVideoBefore = true
            vidFreezeSource.sourceItem = vidOut
            vidFreezeSource.scheduleUpdate()
        } else {
            wasVideoBefore = false
            oldImg.source = currentImageSource
        }

        prevSnapshot.sourceItem = oldScene
        prevSnapshot.scheduleUpdate()
        deskWin.visible = true
        transProgress = 0.0

        pendingUrl = fileUrl
        pendingIsVideo = true
        swapTimer.restart()
    }

    function playImageTransition(fileUrl, animKind) {
        swapTimer.stop()
        transAnim.stop()
        transMode = animKind

        var hasOld = (currentVideoSource !== "" || currentImageSource !== "")
        if (!hasOld) {
            wasVideoBefore = false
            oldImg.source = fileUrl
            currentImageSource = fileUrl
            curImg.source = fileUrl
            deskWin.visible = true
            transProgress = 0.0
            prevSnapshot.sourceItem = oldScene
            prevSnapshot.scheduleUpdate()
            transAnim.restart()
            return
        }

        if (currentVideoSource !== "") {
            wasVideoBefore = true
            vidFreezeSource.sourceItem = vidOut
            vidFreezeSource.scheduleUpdate()
        } else {
            wasVideoBefore = false
            oldImg.source = currentImageSource
        }

        prevSnapshot.sourceItem = oldScene
        prevSnapshot.scheduleUpdate()
        deskWin.visible = true
        transProgress = 0.0

        pendingUrl = fileUrl
        pendingIsVideo = false
        swapTimer.restart()
    }

    Timer {
        id: swapTimer
        interval: 25
        repeat: false
        onTriggered: {
            if (deskWin.pendingIsVideo) {
                deskWin.currentVideoSource = deskWin.pendingUrl
                deskWin.currentImageSource = ""
                curImg.source = ""
                if (vidPlayer.source.toString() !== deskWin.pendingUrl) {
                    vidPlayer.stop()
                    vidPlayer.source = deskWin.pendingUrl
                }
                if (!deskWin.videoCoveredPaused) vidPlayer.play()
            } else {
                vidPlayer.stop()
                vidPlayer.source = ""
                deskWin.currentVideoSource = ""
                deskWin.currentImageSource = deskWin.pendingUrl
                curImg.source = deskWin.pendingUrl
            }
            transAnim.restart()
        }
    }

    function stopAll() {
        swapTimer.stop()
        transAnim.stop()
        vidPlayer.stop()
        vidPlayer.source = ""
        currentVideoSource = ""
        currentImageSource = ""
        curImg.source = ""
        oldImg.source = ""
        vidFreezeSource.sourceItem = null
        prevSnapshot.sourceItem = null
        transProgress = 1.0
        deskWin.visible = false
    }

    NumberAnimation on transProgress {
        id: transAnim
        from: 0.0
        to: 1.0
        duration: 700
        running: false
        easing.type: Easing.InOutCubic
        onFinished: {
            // Полностью освобождаем FBO-текстуры старого кадра после завершения перехода (0 МБ лишней VRAM/ОЗУ и 0% нагрузки на GPU!)
            oldImg.source = ""
            vidFreezeSource.sourceItem = null
            prevSnapshot.sourceItem = null
        }
    }

    Item {
        id: oldScene
        anchors.fill: parent
        visible: deskWin.inTransition

        Image {
            id: oldImg
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
            asynchronous: false
            cache: false
            visible: !deskWin.wasVideoBefore
        }

        ShaderEffectSource {
            id: vidFreezeSource
            sourceItem: null
            anchors.fill: parent
            live: false
            visible: deskWin.wasVideoBefore
        }
    }

    Item {
        id: activeScene
        anchors.fill: parent
        visible: true

        Image {
            id: curImg
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
            asynchronous: false
            cache: false
            visible: deskWin.currentVideoSource === ""
        }

        AudioOutput {
            id: vidAudio
            muted: true
            volume: 0.0
        }

        MediaPlayer {
            id: vidPlayer
            loops: MediaPlayer.Infinite
            videoOutput: vidOut
            audioOutput: vidAudio
        }

        VideoOutput {
            id: vidOut
            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectCrop
            visible: deskWin.currentVideoSource !== ""
        }
    }

    ShaderEffectSource {
        id: prevSnapshot
        sourceItem: null
        anchors.fill: parent
        live: false
        hideSource: true
        visible: false
    }

    ShaderEffectSource {
        id: currLiveSource
        sourceItem: deskWin.inTransition ? activeScene : null
        anchors.fill: parent
        live: deskWin.inTransition
        hideSource: false
        visible: false
    }

    ShaderEffect {
        anchors.fill: parent
        visible: deskWin.inTransition
        blending: false
        property vector2d u_resolution: Qt.vector2d(width, height)
        property real u_progress: deskWin.transProgress
        property real u_mode: deskWin.transMode
        property variant u_prevTex: prevSnapshot
        property variant u_currTex: currLiveSource
        fragmentShader: "qrc:/shaders/qml/shaders/wallpaper_trans.frag.qsb"
    }
}