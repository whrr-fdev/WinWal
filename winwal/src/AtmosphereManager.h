#pragma once
#include <QObject>
#include <QQuickPaintedItem>
#include <QQuickItem>
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QSocketNotifier>
#include <QProcess>
#include <QVector>
#include <QHash>
#include <QRectF>
#include <QPointF>
#include <QColor>
#include <QVector3D>

class AtmosphereManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool inAppEnabled READ inAppEnabled WRITE setInAppEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool globalOverlayEnabled READ globalOverlayEnabled WRITE setGlobalOverlayEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool syncWithOverlay READ syncWithOverlay WRITE setSyncWithOverlay NOTIFY stateChanged)
    Q_PROPERTY(bool overlayScreenFogEnabled READ overlayScreenFogEnabled WRITE setOverlayScreenFogEnabled NOTIFY stateChanged)
    Q_PROPERTY(qreal overlayScreenFogIntensity READ overlayScreenFogIntensity WRITE setOverlayScreenFogIntensity NOTIFY stateChanged)
    Q_PROPERTY(qreal overlayFogSpread READ overlayFogSpread WRITE setOverlayFogSpread NOTIFY stateChanged)
    Q_PROPERTY(qreal overlayDropsIntensity READ overlayDropsIntensity WRITE setOverlayDropsIntensity NOTIFY stateChanged)
    Q_PROPERTY(qreal windowFogIntensity READ windowFogIntensity WRITE setWindowFogIntensity NOTIFY stateChanged)
    Q_PROPERTY(int trailStyle READ trailStyle WRITE setTrailStyle NOTIFY stateChanged)
    Q_PROPERTY(int presetProfile READ presetProfile WRITE setPresetProfile NOTIFY stateChanged)
    Q_PROPERTY(bool rainEnabled READ rainEnabled WRITE setRainEnabled NOTIFY stateChanged)
    Q_PROPERTY(qreal rainIntensity READ rainIntensity WRITE setRainIntensity NOTIFY stateChanged)
    Q_PROPERTY(bool wetGlassEnabled READ wetGlassEnabled WRITE setWetGlassEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool surfacePuddlesEnabled READ surfacePuddlesEnabled WRITE setSurfacePuddlesEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool snowEnabled READ snowEnabled WRITE setSnowEnabled NOTIFY stateChanged)
    Q_PROPERTY(qreal snowIntensity READ snowIntensity WRITE setSnowIntensity NOTIFY stateChanged)
    Q_PROPERTY(bool snowDriftEnabled READ snowDriftEnabled WRITE setSnowDriftEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool snowScreenFrostEnabled READ snowScreenFrostEnabled WRITE setSnowScreenFrostEnabled NOTIFY stateChanged)
    Q_PROPERTY(qreal snowScreenFrostIntensity READ snowScreenFrostIntensity WRITE setSnowScreenFrostIntensity NOTIFY stateChanged)
    Q_PROPERTY(qreal snowScreenFrostSpread READ snowScreenFrostSpread WRITE setSnowScreenFrostSpread NOTIFY stateChanged)
    Q_PROPERTY(qreal snowWindowFrostIntensity READ snowWindowFrostIntensity WRITE setSnowWindowFrostIntensity NOTIFY stateChanged)
    Q_PROPERTY(qreal snowFrostPatternStrength READ snowFrostPatternStrength WRITE setSnowFrostPatternStrength NOTIFY stateChanged)
    Q_PROPERTY(int snowFrostStyle READ snowFrostStyle WRITE setSnowFrostStyle NOTIFY stateChanged)
    Q_PROPERTY(bool snowVelvetFlakesEnabled READ snowVelvetFlakesEnabled WRITE setSnowVelvetFlakesEnabled NOTIFY stateChanged)
    Q_PROPERTY(qreal snowflakeScale READ snowflakeScale WRITE setSnowflakeScale NOTIFY stateChanged)
    Q_PROPERTY(bool snowStickMeltEnabled READ snowStickMeltEnabled WRITE setSnowStickMeltEnabled NOTIFY stateChanged)
    Q_PROPERTY(qreal snowMeltAmount READ snowMeltAmount WRITE setSnowMeltAmount NOTIFY stateChanged)
    Q_PROPERTY(qreal snowDriftScale READ snowDriftScale WRITE setSnowDriftScale NOTIFY stateChanged)
    Q_PROPERTY(bool leavesEnabled READ leavesEnabled WRITE setLeavesEnabled NOTIFY stateChanged)
    Q_PROPERTY(qreal leavesIntensity READ leavesIntensity WRITE setLeavesIntensity NOTIFY stateChanged)
    Q_PROPERTY(qreal leavesScale READ leavesScale WRITE setLeavesScale NOTIFY stateChanged)
    Q_PROPERTY(qreal leavesDetail READ leavesDetail WRITE setLeavesDetail NOTIFY stateChanged)
    Q_PROPERTY(int leavesPalette READ leavesPalette WRITE setLeavesPalette NOTIFY stateChanged)
    Q_PROPERTY(qreal windStrength READ windStrength WRITE setWindStrength NOTIFY stateChanged)
    Q_PROPERTY(qreal windTurbulence READ windTurbulence WRITE setWindTurbulence NOTIFY stateChanged)
    Q_PROPERTY(qreal gravityScale READ gravityScale WRITE setGravityScale NOTIFY stateChanged)
    Q_PROPERTY(qreal particleScale READ particleScale WRITE setParticleScale NOTIFY stateChanged)
    Q_PROPERTY(qreal dropScale READ dropScale WRITE setDropScale NOTIFY stateChanged)
    Q_PROPERTY(qreal dropSlideSpeed READ dropSlideSpeed WRITE setDropSlideSpeed NOTIFY stateChanged)
    Q_PROPERTY(int dropTintMode READ dropTintMode WRITE setDropTintMode NOTIFY stateChanged)
    Q_PROPERTY(qreal dropTintStrength READ dropTintStrength WRITE setDropTintStrength NOTIFY stateChanged)
    Q_PROPERTY(qreal dropHighlightStrength READ dropHighlightStrength WRITE setDropHighlightStrength NOTIFY stateChanged)
    Q_PROPERTY(qreal dropRefraction READ dropRefraction WRITE setDropRefraction NOTIFY stateChanged)
    Q_PROPERTY(qreal wipeTrailStrength READ wipeTrailStrength WRITE setWipeTrailStrength NOTIFY stateChanged)
    Q_PROPERTY(qreal dropTintR READ dropTintR WRITE setDropTintR NOTIFY stateChanged)
    Q_PROPERTY(qreal dropTintG READ dropTintG WRITE setDropTintG NOTIFY stateChanged)
    Q_PROPERTY(qreal dropTintB READ dropTintB WRITE setDropTintB NOTIFY stateChanged)
    Q_PROPERTY(qreal screenDropsAmount READ screenDropsAmount WRITE setScreenDropsAmount NOTIFY stateChanged)
    Q_PROPERTY(qreal windowDropsAmount READ windowDropsAmount WRITE setWindowDropsAmount NOTIFY stateChanged)
    Q_PROPERTY(qreal waybarDropsAmount READ waybarDropsAmount WRITE setWaybarDropsAmount NOTIFY stateChanged)
    Q_PROPERTY(QVector3D effectiveDropTintVec READ effectiveDropTintVec NOTIFY stateChanged)
    Q_PROPERTY(bool uiCollisionsEnabled READ uiCollisionsEnabled WRITE setUiCollisionsEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool waybarInteractEnabled READ waybarInteractEnabled WRITE setWaybarInteractEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool windowInteractEnabled READ windowInteractEnabled WRITE setWindowInteractEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool condensationEnabled READ condensationEnabled WRITE setCondensationEnabled NOTIFY stateChanged)
    Q_PROPERTY(qreal fogIntensity READ fogIntensity WRITE setFogIntensity NOTIFY stateChanged)
    Q_PROPERTY(qreal fogSpread READ fogSpread WRITE setFogSpread NOTIFY stateChanged)
    Q_PROPERTY(bool idleFogEnabled READ idleFogEnabled WRITE setIdleFogEnabled NOTIFY stateChanged)
    Q_PROPERTY(int idleFogTimeout READ idleFogTimeout WRITE setIdleFogTimeout NOTIFY stateChanged)
    Q_PROPERTY(qreal idleFogStrength READ idleFogStrength WRITE setIdleFogStrength NOTIFY stateChanged)
    Q_PROPERTY(bool thunderEnabled READ thunderEnabled WRITE setThunderEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool cursorWindEnabled READ cursorWindEnabled WRITE setCursorWindEnabled NOTIFY stateChanged)
    Q_PROPERTY(int targetFps READ targetFps WRITE setTargetFps NOTIFY stateChanged)
    Q_PROPERTY(bool pauseOnFullscreen READ pauseOnFullscreen WRITE setPauseOnFullscreen NOTIFY stateChanged)

public:
    explicit AtmosphereManager(bool isOverlayProcess = false, QObject *parent = nullptr);
    ~AtmosphereManager() override;
    static AtmosphereManager *instance();

    bool inAppEnabled() const { return m_inAppEnabled; }
    void setInAppEnabled(bool v);
    bool globalOverlayEnabled() const { return m_globalOverlayEnabled; }
    void setGlobalOverlayEnabled(bool v);
    bool syncWithOverlay() const { return m_syncWithOverlay; }
    void setSyncWithOverlay(bool v);
    bool overlayScreenFogEnabled() const { return m_overlayScreenFogEnabled; }
    void setOverlayScreenFogEnabled(bool v);
    qreal overlayScreenFogIntensity() const { return m_overlayScreenFogIntensity; }
    void setOverlayScreenFogIntensity(qreal v);
    qreal overlayFogSpread() const { return m_overlayFogSpread; }
    void setOverlayFogSpread(qreal v);
    qreal overlayDropsIntensity() const { return m_overlayDropsIntensity; }
    void setOverlayDropsIntensity(qreal v);
    qreal windowFogIntensity() const { return m_windowFogIntensity; }
    void setWindowFogIntensity(qreal v);
    int trailStyle() const { return m_trailStyle; }
    void setTrailStyle(int v);

    int presetProfile() const { return m_presetProfile; }
    void setPresetProfile(int v);
    bool rainEnabled() const { return m_rainEnabled; }
    void setRainEnabled(bool v);
    qreal rainIntensity() const { return m_rainIntensity; }
    void setRainIntensity(qreal v);
    bool wetGlassEnabled() const { return m_wetGlassEnabled; }
    void setWetGlassEnabled(bool v);
    bool surfacePuddlesEnabled() const { return m_surfacePuddlesEnabled; }
    void setSurfacePuddlesEnabled(bool v);
    bool snowEnabled() const { return m_snowEnabled; }
    void setSnowEnabled(bool v);
    qreal snowIntensity() const { return m_snowIntensity; }
    void setSnowIntensity(qreal v);
    bool snowDriftEnabled() const { return m_snowDriftEnabled; }
    void setSnowDriftEnabled(bool v);
    bool snowScreenFrostEnabled() const { return m_snowScreenFrostEnabled; }
    void setSnowScreenFrostEnabled(bool v);
    qreal snowScreenFrostIntensity() const { return m_snowScreenFrostIntensity; }
    void setSnowScreenFrostIntensity(qreal v);
    qreal snowScreenFrostSpread() const { return m_snowScreenFrostSpread; }
    void setSnowScreenFrostSpread(qreal v);
    qreal snowWindowFrostIntensity() const { return m_snowWindowFrostIntensity; }
    void setSnowWindowFrostIntensity(qreal v);
    qreal snowFrostPatternStrength() const { return m_snowFrostPatternStrength; }
    void setSnowFrostPatternStrength(qreal v);
    int snowFrostStyle() const { return m_snowFrostStyle; }
    void setSnowFrostStyle(int v);
    bool snowVelvetFlakesEnabled() const { return m_snowVelvetFlakesEnabled; }
    void setSnowVelvetFlakesEnabled(bool v);
    qreal snowflakeScale() const { return m_snowflakeScale; }
    void setSnowflakeScale(qreal v);
    bool snowStickMeltEnabled() const { return m_snowStickMeltEnabled; }
    void setSnowStickMeltEnabled(bool v);
    qreal snowMeltAmount() const { return m_snowMeltAmount; }
    void setSnowMeltAmount(qreal v);
    qreal snowDriftScale() const { return m_snowDriftScale; }
    void setSnowDriftScale(qreal v);
    bool leavesEnabled() const { return m_leavesEnabled; }
    void setLeavesEnabled(bool v);
    qreal leavesIntensity() const { return m_leavesIntensity; }
    void setLeavesIntensity(qreal v);
    qreal leavesScale() const { return m_leavesScale; }
    void setLeavesScale(qreal v);
    qreal leavesDetail() const { return m_leavesDetail; }
    void setLeavesDetail(qreal v);
    int leavesPalette() const { return m_leavesPalette; }
    void setLeavesPalette(int v);
    qreal windStrength() const { return m_windStrength; }
    void setWindStrength(qreal v);
    qreal windTurbulence() const { return m_windTurbulence; }
    void setWindTurbulence(qreal v);
    qreal gravityScale() const { return m_gravityScale; }
    void setGravityScale(qreal v);
    qreal particleScale() const { return m_particleScale; }
    void setParticleScale(qreal v);
    qreal dropScale() const { return m_dropScale; }
    void setDropScale(qreal v);
    qreal dropSlideSpeed() const { return m_dropSlideSpeed; }
    void setDropSlideSpeed(qreal v);
    int dropTintMode() const { return m_dropTintMode; }
    void setDropTintMode(int v);
    qreal dropTintStrength() const { return m_dropTintStrength; }
    void setDropTintStrength(qreal v);
    qreal dropHighlightStrength() const { return m_dropHighlightStrength; }
    void setDropHighlightStrength(qreal v);
    qreal dropRefraction() const { return m_dropRefraction; }
    void setDropRefraction(qreal v);
    qreal wipeTrailStrength() const { return m_wipeTrailStrength; }
    void setWipeTrailStrength(qreal v);
    qreal dropTintR() const { return m_dropTintR; }
    void setDropTintR(qreal v);
    qreal dropTintG() const { return m_dropTintG; }
    void setDropTintG(qreal v);
    qreal dropTintB() const { return m_dropTintB; }
    void setDropTintB(qreal v);
    qreal screenDropsAmount() const { return m_screenDropsAmount; }
    void setScreenDropsAmount(qreal v);
    qreal windowDropsAmount() const { return m_windowDropsAmount; }
    void setWindowDropsAmount(qreal v);
    qreal waybarDropsAmount() const { return m_waybarDropsAmount; }
    void setWaybarDropsAmount(qreal v);
    QVector3D effectiveDropTintVec() const;
    bool uiCollisionsEnabled() const { return m_uiCollisionsEnabled; }
    void setUiCollisionsEnabled(bool v);
    bool waybarInteractEnabled() const { return m_waybarInteractEnabled; }
    void setWaybarInteractEnabled(bool v);
    bool windowInteractEnabled() const { return m_windowInteractEnabled; }
    void setWindowInteractEnabled(bool v);
    bool condensationEnabled() const { return m_condensationEnabled; }
    void setCondensationEnabled(bool v);
    qreal fogIntensity() const { return m_fogIntensity; }
    void setFogIntensity(qreal v);
    qreal fogSpread() const { return m_fogSpread; }
    void setFogSpread(qreal v);
    bool idleFogEnabled() const { return m_idleFogEnabled; }
    void setIdleFogEnabled(bool v);
    int idleFogTimeout() const { return m_idleFogTimeout; }
    void setIdleFogTimeout(int v);
    qreal idleFogStrength() const { return m_idleFogStrength; }
    void setIdleFogStrength(qreal v);
    bool thunderEnabled() const { return m_thunderEnabled; }
    void setThunderEnabled(bool v);
    bool cursorWindEnabled() const { return m_cursorWindEnabled; }
    void setCursorWindEnabled(bool v);
    int targetFps() const { return m_targetFps; }
    void setTargetFps(int v);
    bool pauseOnFullscreen() const { return m_pauseOnFullscreen; }
    void setPauseOnFullscreen(bool v);

    Q_INVOKABLE void applySeasonalProfile(int profileIdx);
    Q_INVOKABLE void triggerWindGust();
    Q_INVOKABLE void triggerThunderstorm();
    Q_INVOKABLE void clearAccumulations();
    Q_INVOKABLE void resetDefaults();

    void load();
    void save();
    void syncOverlayProcess();

signals:
    void stateChanged();
    void actionTriggered(const QString &action);

private:
    QString configFilePath() const;
    void applyHyprOverlayRules() const;
    bool m_isOverlayProcess = false, m_ignoreFileWatch = false;
    QFileSystemWatcher m_watcher;
    QProcess *m_overlayProc = nullptr;
    bool m_inAppEnabled = true, m_globalOverlayEnabled = false;
    bool m_syncWithOverlay = true, m_overlayScreenFogEnabled = true;
    qreal m_overlayScreenFogIntensity = 0.60, m_overlayFogSpread = 0.45, m_overlayDropsIntensity = 1.00, m_windowFogIntensity = 0.50;
    int m_trailStyle = 0; // 0 = микро-бусинки (жемчуг), 1 = мягкий след, 2 = без следов
    int m_presetProfile = 1;
    bool m_rainEnabled = true, m_wetGlassEnabled = true, m_surfacePuddlesEnabled = true;
    qreal m_rainIntensity = 0.65;
    bool m_snowEnabled = false, m_snowDriftEnabled = true, m_snowScreenFrostEnabled = true, m_snowStickMeltEnabled = true;
    qreal m_snowIntensity = 0.60, m_snowScreenFrostIntensity = 0.65, m_snowScreenFrostSpread = 0.45, m_snowWindowFrostIntensity = 0.55;
    qreal m_snowFrostPatternStrength = 1.00, m_snowflakeScale = 1.00, m_snowMeltAmount = 1.00, m_snowDriftScale = 1.00;
    int m_snowFrostStyle = 0;
    bool m_snowVelvetFlakesEnabled = true;
    bool m_leavesEnabled = true;
    qreal m_leavesIntensity = 0.45, m_leavesScale = 1.00, m_leavesDetail = 1.00;
    int m_leavesPalette = 0;
    qreal m_fogIntensity = 0.65, m_fogSpread = 0.45, m_windStrength = 0.20, m_windTurbulence = 0.45, m_gravityScale = 1.00, m_particleScale = 1.00, m_dropScale = 1.00;
    int m_dropTintMode = 1;
    qreal m_dropSlideSpeed = 1.00;
    qreal m_dropTintStrength = 0.20, m_dropHighlightStrength = 1.00, m_dropRefraction = 1.00, m_wipeTrailStrength = 0.55;
    qreal m_dropTintR = 0.04, m_dropTintG = 0.04, m_dropTintB = 0.05;
    qreal m_screenDropsAmount = 1.00, m_windowDropsAmount = 1.00, m_waybarDropsAmount = 1.00;
    bool m_idleFogEnabled = true;
    int m_idleFogTimeout = 25;
    qreal m_idleFogStrength = 0.95;
    bool m_uiCollisionsEnabled = true, m_waybarInteractEnabled = true, m_windowInteractEnabled = true, m_condensationEnabled = true, m_thunderEnabled = true, m_cursorWindEnabled = true;
    int m_targetFps = 60;
    bool m_pauseOnFullscreen = true;
    int m_actionSerial = 0;
    QString m_lastAction;
};

class AtmosphereCanvas : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(bool globalMode READ globalMode WRITE setGlobalMode NOTIFY globalModeChanged)
    Q_PROPERTY(bool activeRunning READ activeRunning WRITE setActiveRunning NOTIFY activeRunningChanged)
    Q_PROPERTY(int renderPass READ renderPass WRITE setRenderPass NOTIFY renderPassChanged)
    Q_PROPERTY(AtmosphereCanvas* masterCanvas READ masterCanvas WRITE setMasterCanvas NOTIFY masterCanvasChanged)
    Q_PROPERTY(QPointF cursorPos READ cursorPos WRITE setCursorPos NOTIFY cursorPosChanged)
    Q_PROPERTY(bool cursorPressed READ cursorPressed WRITE setCursorPressed NOTIFY cursorPosChanged)
    Q_PROPERTY(qreal lightningFlash READ lightningFlash NOTIFY lightningFlashChanged)
    Q_PROPERTY(qreal lightningPosX READ lightningPosX NOTIFY lightningFlashChanged)
    Q_PROPERTY(qreal idleFogBoost READ idleFogBoost NOTIFY idleFogBoostChanged)
    Q_PROPERTY(bool roundedTheme READ roundedTheme WRITE setRoundedTheme NOTIFY roundedThemeChanged)

public:
    explicit AtmosphereCanvas(QQuickItem *parent = nullptr);
    ~AtmosphereCanvas() override;
    bool globalMode() const { return m_globalMode; }
    void setGlobalMode(bool v);
    bool activeRunning() const { return m_activeRunning; }
    void setActiveRunning(bool v);
    int renderPass() const { return m_renderPass; }
    void setRenderPass(int v);
    AtmosphereCanvas *masterCanvas() const { return m_masterCanvas; }
    void setMasterCanvas(AtmosphereCanvas *c);
    qreal lightningFlash() const { return m_lightningFlash; }
    qreal lightningPosX() const { return m_lightningPosX; }
    qreal idleFogBoost() const { return m_idleFogBoost; }
    bool roundedTheme() const { return m_roundedTheme; }
    void setRoundedTheme(bool v) { if (m_roundedTheme != v) { m_roundedTheme = v; emit roundedThemeChanged(); update(); } }
    QPointF cursorPos() const { return m_cursorPos; }
    void setCursorPos(const QPointF &p);
    bool cursorPressed() const { return m_cursorPressed; }
    void setCursorPressed(bool v);
    void paint(QPainter *painter) override;

signals:
    void globalModeChanged();
    void activeRunningChanged();
    void renderPassChanged();
    void masterCanvasChanged();
    void cursorPosChanged();
    void lightningFlashChanged();
    void idleFogBoostChanged();
    void roundedThemeChanged();
    void frameSimulated();

private slots:
    void onTick();
    void onConfigChanged();
    void onActionTriggered(const QString &act);
    void onHyprEventReady();

private:
    enum ParticleType { Rain = 0, Snow = 1, Leaf = 2, FallingDrop = 3 };
    struct Particle {
        ParticleType type = Rain;
        float x = 0, y = 0, vx = 0, vy = 0, size = 1, alpha = 0.8f, depth = 1;
        float angle = 0, angVel = 0, tiltPhase = 0, tiltSpeed = 0, swayPhase = 0;
        int leafKind = 0;
        QColor color;
        bool resting = false;
        float restTimer = 0, relX = 0, relY = 0, bendAmount = 0;
        float termV = 0.0f, drag = 3.0f;
        QString attachedObstacleId;
    };
    struct SplashDrop { float x = 0, y = 0, vx = 0, vy = 0, radius = 1.6f, life = 0.35f, maxLife = 0.35f, targetY = -1.0f; QString obsId; };
    struct TrailBead { float relX = 0, relY = 0, width = 1.0f, lenY = 1.2f, life = 3.2f, maxLife = 3.2f; };
    struct WipedTrack { float relX1 = 0, y1 = 0, relX2 = 0, y2 = 0, radius = 3.2f, life = 16.0f, maxLife = 16.0f; };
    struct SurfaceDrop {
        float relX = 0, relY = 0, prevRelX = 0;
        float radius = 2.1f, spreadX = 3.2f, mass = 0.55f;
        float vx = 0, vy = 0;
        float edgeAge = 0.0f, maxEdgeLife = 3.2f;
        float driftSlope = 0.0f;
        float tailDirX = 0.0f;
        float stretchFactor = 0.25f;
        float pinchDistAcc = 0.0f;
        float nextPinchTarget = 9.0f;
        float pauseTimer = 0.0f;
        float releaseMass = 2.2f;
        bool pinned = false;
        int sideWall = 0;
        float spreadProg = 0.0f;
        float detachProg = 0.0f;
    };
    struct ObstacleSurface {
        QString id; QRectF rect; QRectF clipRect; QPointer<QQuickItem> itemPtr; bool isWaybar = false, isWindow = false;
        qint64 createdMs = 0, lastSeenMs = 0;
        QVector<float> snowBins, wetFilmBins;
        QVector<SurfaceDrop> waterDrops;
        QVector<TrailBead> trailBeads;
        QVector<WipedTrack> wipedTracks;
        float fogLevel = 0.0f;
        float snowPhaseShift = 0.0f;
        float snowSlideRem = 0.0f;
        float snowmanProg = 0.0f;
    };

    void updateTimerState();
    void scanInAppObstacles(qint64 nowMs);
    void collectInAppItems(QQuickItem *item, const QRectF &clip, QHash<QString, QPair<QRectF, QQuickItem*>> &found);
    void pollHyprlandState(qint64 nowMs);
    void connectHyprEventSocket();
    QString queryHyprSocket(const QString &cmd) const;
    void releaseObstacleAccumulations(const ObstacleSurface &obs, bool violent);
    void spawnParticle(Particle &p, ParticleType type, bool randomY);
    QColor pickLeafColor(int palette) const;
    void paintWaterMap(QPainter *painter, const AtmosphereCanvas *src);
    void paintOverlayElements(QPainter *painter, const AtmosphereCanvas *src);

    bool m_roundedTheme = true, m_globalMode = false, m_activeRunning = true, m_fullscreenPaused = false;
    int m_renderPass = 0;
    AtmosphereCanvas *m_masterCanvas = nullptr;
    QTimer m_tickTimer;
    QElapsedTimer m_clock;
    qint64 m_lastTickMs = 0, m_lastObstacleScanMs = 0, m_lastHyprPollMs = 0;
    float m_idleSec = 0.0f, m_idleFogBoost = 0.0f;
    float m_timeSec = 0.0f, m_gustBoost = 0.0f, m_lightningFlash = 0.0f, m_lightningPosX = 0.5f, m_thunderTimer = 9.0f;
    float m_windDriftOffset = 0.0f, m_windTargetOffset = 0.0f, m_windShiftTimer = 6.0f, m_autoGustTimer = 11.0f;
    int m_lightningPulseStage = 0;
    float m_lightningStageTimer = 0.0f;
    QPointF m_cursorPos = QPointF(-500, -500), m_prevCursorPos = QPointF(-500, -500), m_cursorVel;
    bool m_cursorPressed = false;
    int m_draggedLeafIdx = -1;
    QVector<Particle> m_particles;
    QVector<SplashDrop> m_splashes;
    QHash<QString, ObstacleSurface> m_obstacles;
    int m_hyprEventFd = -1;
    QSocketNotifier *m_hyprNotifier = nullptr;
};
