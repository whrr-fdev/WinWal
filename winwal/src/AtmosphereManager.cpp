#include "AtmosphereManager.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QQuickWindow>
#include <algorithm>
#include <cmath>
#include <QCursor>
#include <QDateTime>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#else
#include <QCursor>
#include <QDateTime>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#endif

static void syncWaybarWindowPillVisibility(bool showPill) {
    Q_UNUSED(showPill);
    static bool s_cleanedOldBlock = false;
    if (s_cleanedOldBlock) return;
    s_cleanedOldBlock = true;
    QString cssPath = QDir::homePath() + "/.config/waybar/style.css";
    QFile f(cssPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QString css = QString::fromUtf8(f.readAll());
    f.close();
    const QString startTag = "/* === ATMO_WIN_PILL_START === */";
    const QString endTag   = "/* === ATMO_WIN_PILL_END === */";
    int i1 = css.indexOf(startTag);
    int i2 = css.indexOf(endTag);
    if (i1 != -1 && i2 != -1 && i2 >= i1) {
        css.remove(i1, (i2 + endTag.length()) - i1);
        QFile tf(cssPath);
        if (tf.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            tf.write(css.toUtf8());
            tf.flush();
            tf.close();
        }
    }
}

static AtmosphereManager *s_atmoInstance = nullptr;
AtmosphereManager *AtmosphereManager::instance() { return s_atmoInstance; }

AtmosphereManager::AtmosphereManager(bool isOverlayProcess, QObject *parent)
    : QObject(parent), m_isOverlayProcess(isOverlayProcess) {
    s_atmoInstance = this;
    load();
    QFileInfo fi(configFilePath());
    QDir().mkpath(fi.absolutePath());
    if (!QFile::exists(configFilePath())) save();
    m_watcher.addPath(fi.absolutePath());
    if (QFile::exists(configFilePath())) m_watcher.addPath(configFilePath());

    auto reloadFn = [this](const QString &) {
        if (!m_watcher.files().contains(configFilePath()) && QFile::exists(configFilePath()))
            m_watcher.addPath(configFilePath());
        if (!m_ignoreFileWatch) load();
    };
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, reloadFn);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, reloadFn);
    if (!m_isOverlayProcess) QTimer::singleShot(400, this, &AtmosphereManager::syncOverlayProcess);
}

AtmosphereManager::~AtmosphereManager() {
    if (m_isOverlayProcess) syncWaybarWindowPillVisibility(true);
    if (!m_isOverlayProcess && m_overlayProc) { m_overlayProc->terminate(); m_overlayProc->waitForFinished(250); }
    if (s_atmoInstance == this) s_atmoInstance = nullptr;
}

QString AtmosphereManager::configFilePath() const { return QDir::homePath() + "/.config/hypr/scripts/atmosphere.conf"; }

void AtmosphereManager::load() {
    QFile file(configFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    int prevSerial = m_actionSerial;
    QString newAction = m_lastAction;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        int eq = line.indexOf('=');
        if (line.isEmpty() || line.startsWith('#') || eq == -1) continue;
        QString k = line.left(eq).trimmed(), v = line.mid(eq + 1).trimmed();
        if (k == "in_app_enabled") m_inAppEnabled = (v == "1" || v == "true");
        else if (k == "global_overlay_enabled") m_globalOverlayEnabled = (v == "1" || v == "true");
        else if (k == "sync_with_overlay") m_syncWithOverlay = (v == "1" || v == "true");
        else if (k == "overlay_screen_fog_enabled") m_overlayScreenFogEnabled = (v == "1" || v == "true");
        else if (k == "overlay_screen_fog_intensity") m_overlayScreenFogIntensity = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "overlay_fog_spread") m_overlayFogSpread = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "overlay_drops_intensity") m_overlayDropsIntensity = std::clamp(v.toDouble(), 0.05, 3.0);
        else if (k == "window_fog_intensity") m_windowFogIntensity = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "trail_style") m_trailStyle = std::clamp(v.toInt(), 0, 2);
        else if (k == "preset_profile") m_presetProfile = std::clamp(v.toInt(), 0, 4);
        else if (k == "rain_enabled") m_rainEnabled = (v == "1" || v == "true");
        else if (k == "rain_intensity") m_rainIntensity = std::clamp(v.toDouble(), 0.05, 1.0);
        else if (k == "wet_glass_enabled") m_wetGlassEnabled = (v == "1" || v == "true");
        else if (k == "surface_puddles_enabled") m_surfacePuddlesEnabled = (v == "1" || v == "true");
        else if (k == "snow_enabled") m_snowEnabled = (v == "1" || v == "true");
        else if (k == "snow_intensity") m_snowIntensity = std::clamp(v.toDouble(), 0.05, 1.0);
        else if (k == "snow_drift_enabled") m_snowDriftEnabled = (v == "1" || v == "true");
        else if (k == "snow_screen_frost_enabled") m_snowScreenFrostEnabled = (v == "1" || v == "true");
        else if (k == "snow_screen_frost_intensity") m_snowScreenFrostIntensity = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "snow_screen_frost_spread") m_snowScreenFrostSpread = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "snow_window_frost_intensity") m_snowWindowFrostIntensity = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "snow_frost_pattern_strength") m_snowFrostPatternStrength = std::clamp(v.toDouble(), 0.0, 2.0);
        else if (k == "snow_frost_style") m_snowFrostStyle = std::clamp(v.toInt(), 0, 2);
        else if (k == "snow_velvet_flakes_enabled") m_snowVelvetFlakesEnabled = (v == "1" || v == "true");
        else if (k == "snowflake_scale") m_snowflakeScale = std::clamp(v.toDouble(), 0.3, 6.0);
        else if (k == "snow_stick_melt_enabled") m_snowStickMeltEnabled = (v == "1" || v == "true");
        else if (k == "snow_melt_amount") m_snowMeltAmount = std::clamp(v.toDouble(), 0.05, 3.0);
        else if (k == "snow_drift_scale") m_snowDriftScale = std::clamp(v.toDouble(), 0.3, 2.5);
        else if (k == "leaves_enabled") m_leavesEnabled = (v == "1" || v == "true");
        else if (k == "leaves_intensity") m_leavesIntensity = std::clamp(v.toDouble(), 0.01, 1.0);
        else if (k == "leaves_scale") m_leavesScale = std::clamp(v.toDouble(), 0.2, 6.0);
        else if (k == "leaves_detail") m_leavesDetail = std::clamp(v.toDouble(), 0.0, 2.0);
        else if (k == "leaves_palette") m_leavesPalette = std::clamp(v.toInt(), 0, 3);
        else if (k == "wind_strength") m_windStrength = std::clamp(v.toDouble(), -1.0, 1.0);
        else if (k == "wind_turbulence") m_windTurbulence = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "gravity_scale") m_gravityScale = std::clamp(v.toDouble(), 0.4, 4.0);
        else if (k == "particle_scale") m_particleScale = std::clamp(v.toDouble(), 0.2, 1.8);
        else if (k == "drop_scale") m_dropScale = std::clamp(v.toDouble(), 0.4, 2.0);
        else if (k == "drop_slide_speed") m_dropSlideSpeed = std::clamp(v.toDouble(), 0.2, 6.0);
        else if (k == "drop_tint_mode") m_dropTintMode = std::clamp(v.toInt(), 0, 3);
        else if (k == "drop_tint_strength") m_dropTintStrength = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "drop_highlight_strength") m_dropHighlightStrength = std::clamp(v.toDouble(), 0.05, 3.0);
        else if (k == "drop_refraction") m_dropRefraction = std::clamp(v.toDouble(), 0.2, 2.0);
        else if (k == "wipe_trail_strength") m_wipeTrailStrength = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "drop_tint_r") m_dropTintR = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "drop_tint_g") m_dropTintG = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "drop_tint_b") m_dropTintB = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "screen_drops_amount") m_screenDropsAmount = std::clamp(v.toDouble(), 0.0, 2.0);
        else if (k == "window_drops_amount") m_windowDropsAmount = std::clamp(v.toDouble(), 0.0, 2.0);
        else if (k == "waybar_drops_amount") m_waybarDropsAmount = std::clamp(v.toDouble(), 0.0, 2.0);
        else if (k == "ui_collisions_enabled") m_uiCollisionsEnabled = (v == "1" || v == "true");
        else if (k == "waybar_interact_enabled") m_waybarInteractEnabled = (v == "1" || v == "true");
        else if (k == "window_interact_enabled") m_windowInteractEnabled = (v == "1" || v == "true");
        else if (k == "condensation_enabled") m_condensationEnabled = (v == "1" || v == "true");
        else if (k == "fog_intensity") m_fogIntensity = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "fog_spread") m_fogSpread = std::clamp(v.toDouble(), 0.0, 1.0);
        else if (k == "idle_fog_enabled") m_idleFogEnabled = (v == "1" || v == "true");
        else if (k == "idle_fog_timeout") m_idleFogTimeout = std::clamp(v.toInt(), 5, 1800);
        else if (k == "idle_fog_strength") m_idleFogStrength = std::clamp(v.toDouble(), 0.3, 1.0);
        else if (k == "thunder_enabled") m_thunderEnabled = (v == "1" || v == "true");
        else if (k == "cursor_wind_enabled") m_cursorWindEnabled = (v == "1" || v == "true");
        else if (k == "target_fps") m_targetFps = std::clamp(v.toInt(), 24, 165);
        else if (k == "pause_on_fullscreen") m_pauseOnFullscreen = (v == "1" || v == "true");
        else if (k == "action_serial") m_actionSerial = v.toInt();
        else if (k == "last_action") newAction = v;
    }
    file.close();
    emit stateChanged();
    if (m_actionSerial != prevSerial && !newAction.isEmpty()) { m_lastAction = newAction; emit actionTriggered(m_lastAction); }
    if (m_isOverlayProcess && !m_globalOverlayEnabled) QCoreApplication::quit();
}

void AtmosphereManager::save() {
    m_ignoreFileWatch = true;
    QFile file(configFilePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&file);
        out << "in_app_enabled=" << (m_inAppEnabled ? 1 : 0) << "\n"
            << "global_overlay_enabled=" << (m_globalOverlayEnabled ? 1 : 0) << "\n"
            << "sync_with_overlay=" << (m_syncWithOverlay ? 1 : 0) << "\n"
            << "overlay_screen_fog_enabled=" << (m_overlayScreenFogEnabled ? 1 : 0) << "\n"
            << "overlay_screen_fog_intensity=" << QString::number(m_overlayScreenFogIntensity, 'f', 2) << "\n"
            << "overlay_fog_spread=" << QString::number(m_overlayFogSpread, 'f', 2) << "\n"
            << "overlay_drops_intensity=" << QString::number(m_overlayDropsIntensity, 'f', 2) << "\n"
            << "window_fog_intensity=" << QString::number(m_windowFogIntensity, 'f', 2) << "\n"
            << "trail_style=" << m_trailStyle << "\n"
            << "preset_profile=" << m_presetProfile << "\n"
            << "rain_enabled=" << (m_rainEnabled ? 1 : 0) << "\n"
            << "rain_intensity=" << QString::number(m_rainIntensity, 'f', 2) << "\n"
            << "wet_glass_enabled=" << (m_wetGlassEnabled ? 1 : 0) << "\n"
            << "surface_puddles_enabled=" << (m_surfacePuddlesEnabled ? 1 : 0) << "\n"
            << "snow_enabled=" << (m_snowEnabled ? 1 : 0) << "\n"
            << "snow_intensity=" << QString::number(m_snowIntensity, 'f', 2) << "\n"
            << "snow_drift_enabled=" << (m_snowDriftEnabled ? 1 : 0) << "\n"
            << "snow_screen_frost_enabled=" << (m_snowScreenFrostEnabled ? 1 : 0) << "\n"
            << "snow_screen_frost_intensity=" << QString::number(m_snowScreenFrostIntensity, 'f', 2) << "\n"
            << "snow_screen_frost_spread=" << QString::number(m_snowScreenFrostSpread, 'f', 2) << "\n"
            << "snow_window_frost_intensity=" << QString::number(m_snowWindowFrostIntensity, 'f', 2) << "\n"
            << "snow_frost_pattern_strength=" << QString::number(m_snowFrostPatternStrength, 'f', 2) << "\n"
            << "snow_frost_style=" << m_snowFrostStyle << "\n"
            << "snow_velvet_flakes_enabled=" << (m_snowVelvetFlakesEnabled ? 1 : 0) << "\n"
            << "snowflake_scale=" << QString::number(m_snowflakeScale, 'f', 2) << "\n"
            << "snow_stick_melt_enabled=" << (m_snowStickMeltEnabled ? 1 : 0) << "\n"
            << "snow_melt_amount=" << QString::number(m_snowMeltAmount, 'f', 2) << "\n"
            << "snow_drift_scale=" << QString::number(m_snowDriftScale, 'f', 2) << "\n"
            << "leaves_enabled=" << (m_leavesEnabled ? 1 : 0) << "\n"
            << "leaves_intensity=" << QString::number(m_leavesIntensity, 'f', 2) << "\n"
            << "leaves_scale=" << QString::number(m_leavesScale, 'f', 2) << "\n"
            << "leaves_detail=" << QString::number(m_leavesDetail, 'f', 2) << "\n"
            << "leaves_palette=" << m_leavesPalette << "\n"
            << "wind_strength=" << QString::number(m_windStrength, 'f', 2) << "\n"
            << "wind_turbulence=" << QString::number(m_windTurbulence, 'f', 2) << "\n"
            << "gravity_scale=" << QString::number(m_gravityScale, 'f', 2) << "\n"
            << "particle_scale=" << QString::number(m_particleScale, 'f', 2) << "\n"
            << "drop_scale=" << QString::number(m_dropScale, 'f', 2) << "\n"
            << "drop_slide_speed=" << QString::number(m_dropSlideSpeed, 'f', 2) << "\n"
            << "drop_tint_mode=" << m_dropTintMode << "\n"
            << "drop_tint_strength=" << QString::number(m_dropTintStrength, 'f', 2) << "\n"
            << "drop_highlight_strength=" << QString::number(m_dropHighlightStrength, 'f', 2) << "\n"
            << "drop_refraction=" << QString::number(m_dropRefraction, 'f', 2) << "\n"
            << "wipe_trail_strength=" << QString::number(m_wipeTrailStrength, 'f', 2) << "\n"
            << "drop_tint_r=" << QString::number(m_dropTintR, 'f', 2) << "\n"
            << "drop_tint_g=" << QString::number(m_dropTintG, 'f', 2) << "\n"
            << "drop_tint_b=" << QString::number(m_dropTintB, 'f', 2) << "\n"
            << "screen_drops_amount=" << QString::number(m_screenDropsAmount, 'f', 2) << "\n"
            << "window_drops_amount=" << QString::number(m_windowDropsAmount, 'f', 2) << "\n"
            << "waybar_drops_amount=" << QString::number(m_waybarDropsAmount, 'f', 2) << "\n"
            << "ui_collisions_enabled=" << (m_uiCollisionsEnabled ? 1 : 0) << "\n"
            << "waybar_interact_enabled=" << (m_waybarInteractEnabled ? 1 : 0) << "\n"
            << "window_interact_enabled=" << (m_windowInteractEnabled ? 1 : 0) << "\n"
            << "condensation_enabled=" << (m_condensationEnabled ? 1 : 0) << "\n"
            << "fog_intensity=" << QString::number(m_fogIntensity, 'f', 2) << "\n"
            << "fog_spread=" << QString::number(m_fogSpread, 'f', 2) << "\n"
            << "idle_fog_enabled=" << (m_idleFogEnabled ? 1 : 0) << "\n"
            << "idle_fog_timeout=" << m_idleFogTimeout << "\n"
            << "idle_fog_strength=" << QString::number(m_idleFogStrength, 'f', 2) << "\n"
            << "thunder_enabled=" << (m_thunderEnabled ? 1 : 0) << "\n"
            << "cursor_wind_enabled=" << (m_cursorWindEnabled ? 1 : 0) << "\n"
            << "target_fps=" << m_targetFps << "\n"
            << "pause_on_fullscreen=" << (m_pauseOnFullscreen ? 1 : 0) << "\n"
            << "action_serial=" << m_actionSerial << "\n"
            << "last_action=" << m_lastAction << "\n";
    }
    QTimer::singleShot(60, this, [this]() { m_ignoreFileWatch = false; });
}

void AtmosphereManager::applyHyprOverlayRules() const {}
void AtmosphereManager::syncOverlayProcess() {}

void AtmosphereManager::setInAppEnabled(bool v) { if (m_inAppEnabled != v) { m_inAppEnabled = v; emit stateChanged(); save(); } }
void AtmosphereManager::setGlobalOverlayEnabled(bool v) { if (m_globalOverlayEnabled != v) { m_globalOverlayEnabled = v; emit stateChanged(); save(); syncOverlayProcess(); } }
void AtmosphereManager::setSyncWithOverlay(bool v) { if (m_syncWithOverlay != v) { m_syncWithOverlay = v; emit stateChanged(); save(); } }
void AtmosphereManager::setOverlayScreenFogEnabled(bool v) { if (m_overlayScreenFogEnabled != v) { m_overlayScreenFogEnabled = v; emit stateChanged(); save(); } }
void AtmosphereManager::setOverlayScreenFogIntensity(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_overlayScreenFogIntensity, v)) { m_overlayScreenFogIntensity = v; emit stateChanged(); save(); } }
void AtmosphereManager::setOverlayFogSpread(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_overlayFogSpread, v)) { m_overlayFogSpread = v; emit stateChanged(); save(); } }
void AtmosphereManager::setOverlayDropsIntensity(qreal v) { v = std::clamp(v, 0.05, 3.0); if (!qFuzzyCompare(m_overlayDropsIntensity, v)) { m_overlayDropsIntensity = v; emit stateChanged(); save(); } }
void AtmosphereManager::setWindowFogIntensity(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_windowFogIntensity, v)) { m_windowFogIntensity = v; emit stateChanged(); save(); } }
void AtmosphereManager::setTrailStyle(int v) { v = std::clamp(v, 0, 2); if (m_trailStyle != v) { m_trailStyle = v; emit stateChanged(); save(); } }
void AtmosphereManager::setPresetProfile(int v) { v = std::clamp(v, 0, 4); if (m_presetProfile != v) { m_presetProfile = v; emit stateChanged(); save(); } }

void AtmosphereManager::applySeasonalProfile(int idx) {
    m_presetProfile = std::clamp(idx, 0, 3);
    if (idx == 0) { // Дождь
        m_rainEnabled = true;
        m_wetGlassEnabled = true;
        m_snowEnabled = false;
    } else if (idx == 1) { // Снег
        m_rainEnabled = false;
        m_snowEnabled = true;
        m_snowDriftEnabled = true;
        m_leavesEnabled = false;
    } else if (idx == 2) { // Хэллоуин (Дождь + Листья + Гроза + Паутина/Мыши/Свечи/Тыквы/Скелет)
        m_rainEnabled = true;
        m_wetGlassEnabled = true;
        m_thunderEnabled = true;
        m_leavesEnabled = true;
        m_leavesPalette = 0;
        m_snowEnabled = false;
    } else if (idx == 3) { // Новый год / Рождество (Снег + Сугробы + Ёлочка/Гирлянда/Снеговик)
        m_rainEnabled = false;
        m_snowEnabled = true;
        m_snowDriftEnabled = true;
        m_leavesEnabled = false;
    }
    emit stateChanged(); save();
}

void AtmosphereManager::setRainEnabled(bool v) { if (m_rainEnabled != v) { m_rainEnabled = v;  emit stateChanged(); save(); } }
void AtmosphereManager::setRainIntensity(qreal v) { v = std::clamp(v, 0.05, 1.0); if (!qFuzzyCompare(m_rainIntensity, v)) { m_rainIntensity = v; emit stateChanged(); save(); } }
void AtmosphereManager::setWetGlassEnabled(bool v) { if (m_wetGlassEnabled != v) { m_wetGlassEnabled = v; emit stateChanged(); save(); } }
void AtmosphereManager::setSurfacePuddlesEnabled(bool v) { if (m_surfacePuddlesEnabled != v) { m_surfacePuddlesEnabled = v; emit stateChanged(); save(); } }
void AtmosphereManager::setSnowEnabled(bool v) { if (m_snowEnabled != v) { m_snowEnabled = v;  emit stateChanged(); save(); } }
void AtmosphereManager::setSnowIntensity(qreal v) { v = std::clamp(v, 0.05, 1.0); if (!qFuzzyCompare(m_snowIntensity, v)) { m_snowIntensity = v; emit stateChanged(); save(); } }
void AtmosphereManager::setSnowDriftEnabled(bool v) { if (m_snowDriftEnabled != v) { m_snowDriftEnabled = v; emit stateChanged(); save(); } }
void AtmosphereManager::setSnowScreenFrostEnabled(bool v) { if (m_snowScreenFrostEnabled != v) { m_snowScreenFrostEnabled = v; emit stateChanged(); save(); } }
void AtmosphereManager::setSnowScreenFrostIntensity(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_snowScreenFrostIntensity, v)) { m_snowScreenFrostIntensity = v; emit stateChanged(); save(); } }
void AtmosphereManager::setSnowScreenFrostSpread(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_snowScreenFrostSpread, v)) { m_snowScreenFrostSpread = v; emit stateChanged(); save(); } }
void AtmosphereManager::setSnowWindowFrostIntensity(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_snowWindowFrostIntensity, v)) { m_snowWindowFrostIntensity = v; emit stateChanged(); save(); } }
void AtmosphereManager::setSnowFrostPatternStrength(qreal v) { v = std::clamp(v, 0.0, 2.0); if (!qFuzzyCompare(m_snowFrostPatternStrength, v)) { m_snowFrostPatternStrength = v; emit stateChanged(); save(); } }
void AtmosphereManager::setSnowFrostStyle(int v) { v = std::clamp(v, 0, 2); if (m_snowFrostStyle != v) { m_snowFrostStyle = v; emit stateChanged(); save(); } }
void AtmosphereManager::setSnowVelvetFlakesEnabled(bool v) { if (m_snowVelvetFlakesEnabled != v) { m_snowVelvetFlakesEnabled = v; emit stateChanged(); save(); } }
void AtmosphereManager::setSnowflakeScale(qreal v) { v = std::clamp(v, 0.3, 6.0); if (!qFuzzyCompare(m_snowflakeScale, v)) { m_snowflakeScale = v; emit stateChanged(); save(); } }
void AtmosphereManager::setSnowStickMeltEnabled(bool v) { if (m_snowStickMeltEnabled != v) { m_snowStickMeltEnabled = v; emit stateChanged(); save(); } }
void AtmosphereManager::setSnowMeltAmount(qreal v) { v = std::clamp(v, 0.05, 3.0); if (!qFuzzyCompare(m_snowMeltAmount, v)) { m_snowMeltAmount = v; emit stateChanged(); save(); } }
void AtmosphereManager::setSnowDriftScale(qreal v) { v = std::clamp(v, 0.3, 2.5); if (!qFuzzyCompare(m_snowDriftScale, v)) { m_snowDriftScale = v; emit stateChanged(); save(); } }
void AtmosphereManager::setLeavesEnabled(bool v) { if (m_leavesEnabled != v) { m_leavesEnabled = v;  emit stateChanged(); save(); } }
void AtmosphereManager::setLeavesIntensity(qreal v) { v = std::clamp(v, 0.01, 1.0); if (!qFuzzyCompare(m_leavesIntensity, v)) { m_leavesIntensity = v; emit stateChanged(); save(); } }
void AtmosphereManager::setLeavesScale(qreal v) { v = std::clamp(v, 0.2, 6.0); if (!qFuzzyCompare(m_leavesScale, v)) { m_leavesScale = v; emit stateChanged(); save(); } }
void AtmosphereManager::setLeavesDetail(qreal v) { v = std::clamp(v, 0.0, 2.0); if (!qFuzzyCompare(m_leavesDetail, v)) { m_leavesDetail = v; emit stateChanged(); save(); } }
void AtmosphereManager::setLeavesPalette(int v) { v = std::clamp(v, 0, 3); if (m_leavesPalette != v) { m_leavesPalette = v; emit stateChanged(); save(); } }
void AtmosphereManager::setWindStrength(qreal v) { v = std::clamp(v, -1.0, 1.0); if (!qFuzzyCompare(m_windStrength, v)) { m_windStrength = v; emit stateChanged(); save(); } }
void AtmosphereManager::setWindTurbulence(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_windTurbulence, v)) { m_windTurbulence = v; emit stateChanged(); save(); } }
void AtmosphereManager::setGravityScale(qreal v) { v = std::clamp(v, 0.4, 4.0); if (!qFuzzyCompare(m_gravityScale, v)) { m_gravityScale = v; emit stateChanged(); save(); } }
void AtmosphereManager::setParticleScale(qreal v) { v = std::clamp(v, 0.2, 1.8); if (!qFuzzyCompare(m_particleScale, v)) { m_particleScale = v; emit stateChanged(); save(); } }
void AtmosphereManager::setDropScale(qreal v) { v = std::clamp(v, 0.4, 2.0); if (!qFuzzyCompare(m_dropScale, v)) { m_dropScale = v; emit stateChanged(); save(); } }
void AtmosphereManager::setDropSlideSpeed(qreal v) { v = std::clamp(v, 0.2, 6.0); if (!qFuzzyCompare(m_dropSlideSpeed, v)) { m_dropSlideSpeed = v; emit stateChanged(); save(); } }
void AtmosphereManager::setDropTintMode(int v) { v = std::clamp(v, 0, 3); if (m_dropTintMode != v) { m_dropTintMode = v; emit stateChanged(); save(); } }
void AtmosphereManager::setDropTintStrength(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_dropTintStrength, v)) { m_dropTintStrength = v; emit stateChanged(); save(); } }
void AtmosphereManager::setDropHighlightStrength(qreal v) { v = std::clamp(v, 0.05, 3.0); if (!qFuzzyCompare(m_dropHighlightStrength, v)) { m_dropHighlightStrength = v; emit stateChanged(); save(); } }
void AtmosphereManager::setDropRefraction(qreal v) { v = std::clamp(v, 0.2, 2.0); if (!qFuzzyCompare(m_dropRefraction, v)) { m_dropRefraction = v; emit stateChanged(); save(); } }
void AtmosphereManager::setWipeTrailStrength(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_wipeTrailStrength, v)) { m_wipeTrailStrength = v; emit stateChanged(); save(); } }
void AtmosphereManager::setDropTintR(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_dropTintR, v)) { m_dropTintR = v; emit stateChanged(); save(); } }
void AtmosphereManager::setDropTintG(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_dropTintG, v)) { m_dropTintG = v; emit stateChanged(); save(); } }
void AtmosphereManager::setDropTintB(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_dropTintB, v)) { m_dropTintB = v; emit stateChanged(); save(); } }
void AtmosphereManager::setScreenDropsAmount(qreal v) { v = std::clamp(v, 0.0, 2.0); if (!qFuzzyCompare(m_screenDropsAmount, v)) { m_screenDropsAmount = v; emit stateChanged(); save(); } }
void AtmosphereManager::setWindowDropsAmount(qreal v) { v = std::clamp(v, 0.0, 2.0); if (!qFuzzyCompare(m_windowDropsAmount, v)) { m_windowDropsAmount = v; emit stateChanged(); save(); } }
void AtmosphereManager::setWaybarDropsAmount(qreal v) { v = std::clamp(v, 0.0, 2.0); if (!qFuzzyCompare(m_waybarDropsAmount, v)) { m_waybarDropsAmount = v; emit stateChanged(); save(); } }
QVector3D AtmosphereManager::effectiveDropTintVec() const {
    if (m_dropTintMode == 0) return QVector3D(0.18f, 0.20f, 0.22f);
    if (m_dropTintMode == 1) return QVector3D(0.015f, 0.015f, 0.018f);
    if (m_dropTintMode == 2) return QVector3D(0.06f, 0.16f, 0.28f);
    return QVector3D(float(m_dropTintR), float(m_dropTintG), float(m_dropTintB));
}
void AtmosphereManager::setUiCollisionsEnabled(bool v) { if (m_uiCollisionsEnabled != v) { m_uiCollisionsEnabled = v; emit stateChanged(); save(); } }
void AtmosphereManager::setWaybarInteractEnabled(bool v) { if (m_waybarInteractEnabled != v) { m_waybarInteractEnabled = v; emit stateChanged(); save(); } }
void AtmosphereManager::setWindowInteractEnabled(bool v) { if (m_windowInteractEnabled != v) { m_windowInteractEnabled = v; emit stateChanged(); save(); } }
void AtmosphereManager::setCondensationEnabled(bool v) { if (m_condensationEnabled != v) { m_condensationEnabled = v; emit stateChanged(); save(); } }
void AtmosphereManager::setFogIntensity(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_fogIntensity, v)) { m_fogIntensity = v; emit stateChanged(); save(); } }
void AtmosphereManager::setFogSpread(qreal v) { v = std::clamp(v, 0.0, 1.0); if (!qFuzzyCompare(m_fogSpread, v)) { m_fogSpread = v; emit stateChanged(); save(); } }
void AtmosphereManager::setIdleFogEnabled(bool v) { if (m_idleFogEnabled != v) { m_idleFogEnabled = v; emit stateChanged(); save(); } }
void AtmosphereManager::setIdleFogTimeout(int v) { v = std::clamp(v, 5, 1800); if (m_idleFogTimeout != v) { m_idleFogTimeout = v; emit stateChanged(); save(); } }
void AtmosphereManager::setIdleFogStrength(qreal v) { v = std::clamp(v, 0.3, 1.0); if (!qFuzzyCompare(m_idleFogStrength, v)) { m_idleFogStrength = v; emit stateChanged(); save(); } }
void AtmosphereManager::setThunderEnabled(bool v) { if (m_thunderEnabled != v) { m_thunderEnabled = v; emit stateChanged(); save(); } }
void AtmosphereManager::triggerThunderstorm() { m_actionSerial++; m_lastAction = "thunder"; emit actionTriggered(m_lastAction); save(); }
void AtmosphereManager::setCursorWindEnabled(bool v) { if (m_cursorWindEnabled != v) { m_cursorWindEnabled = v; emit stateChanged(); save(); } }
void AtmosphereManager::setTargetFps(int v) { v = std::clamp(v, 24, 165); if (m_targetFps != v) { m_targetFps = v; emit stateChanged(); save(); } }
void AtmosphereManager::setPauseOnFullscreen(bool v) { if (m_pauseOnFullscreen != v) { m_pauseOnFullscreen = v; emit stateChanged(); save(); } }

void AtmosphereManager::triggerWindGust() { m_actionSerial++; m_lastAction = "gust"; emit actionTriggered(m_lastAction); save(); }
void AtmosphereManager::clearAccumulations() { m_actionSerial++; m_lastAction = "shake"; emit actionTriggered(m_lastAction); save(); }
void AtmosphereManager::resetDefaults() {
    m_inAppEnabled = true; m_globalOverlayEnabled = false;
    m_uiCollisionsEnabled = true; m_waybarInteractEnabled = true;
    m_windowInteractEnabled = true; m_condensationEnabled = true;
    m_cursorWindEnabled = true; m_targetFps = 60; m_pauseOnFullscreen = true;
    m_gravityScale = 1.0; m_particleScale = 1.0;
    applySeasonalProfile(1); syncOverlayProcess();
}

AtmosphereCanvas::AtmosphereCanvas(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    setOpaquePainting(false);
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);
    setAcceptTouchEvents(false);
    m_clock.start();
    m_lastTickMs = m_clock.elapsed();
    connect(&m_tickTimer, &QTimer::timeout, this, &AtmosphereCanvas::onTick);
    if (auto *mgr = AtmosphereManager::instance()) {
        connect(mgr, &AtmosphereManager::stateChanged, this, &AtmosphereCanvas::onConfigChanged);
        connect(mgr, &AtmosphereManager::actionTriggered, this, &AtmosphereCanvas::onActionTriggered);
    }
    updateTimerState();
}

AtmosphereCanvas::~AtmosphereCanvas() {
    if (m_hyprNotifier) { m_hyprNotifier->setEnabled(false); delete m_hyprNotifier; }
    #ifndef Q_OS_WIN
    if (m_hyprEventFd >= 0) ::close(m_hyprEventFd);
#endif
}

void AtmosphereCanvas::setGlobalMode(bool v) {
    if (m_globalMode != v) {
        m_globalMode = v;
        if (v && m_renderPass == 0) {
            connectHyprEventSocket();
            QTimer::singleShot(10, this, [this]() { pollHyprlandState(m_clock.elapsed()); });
            QTimer::singleShot(50, this, [this]() { pollHyprlandState(m_clock.elapsed()); });
            QTimer::singleShot(150, this, [this]() { pollHyprlandState(m_clock.elapsed()); });
        }
        emit globalModeChanged();
        updateTimerState();
    }
}
void AtmosphereCanvas::setActiveRunning(bool v) {
    if (m_activeRunning != v) {
        m_activeRunning = v;
        if (v) {
            qint64 nowMs = m_clock.elapsed();
            m_lastTickMs = nowMs;
            m_lastObstacleScanMs = 0;
            for (auto &obs : m_obstacles) {
                obs.lastSeenMs = nowMs;
                if (!m_globalMode) {
                    obs.snowBins.fill(0.0f);
                    obs.snowmanProg = 0.0f;
                }
            }
            setFlag(QQuickItem::ItemHasContents, false);
            setFlag(QQuickItem::ItemHasContents, true);
            update();
            emit frameSimulated();
            QTimer::singleShot(35, this, [this]() { update(); emit frameSimulated(); });
            QTimer::singleShot(140, this, [this]() { update(); emit frameSimulated(); });
        }
        emit activeRunningChanged();
        updateTimerState();
    }
}
void AtmosphereCanvas::setRenderPass(int v) {
    if (m_renderPass != v) { m_renderPass = v; emit renderPassChanged(); updateTimerState(); }
}
void AtmosphereCanvas::setMasterCanvas(AtmosphereCanvas *c) {
    if (m_masterCanvas == c) return;
    if (m_masterCanvas) {
        disconnect(m_masterCanvas, &AtmosphereCanvas::frameSimulated, this, nullptr);
        disconnect(m_masterCanvas, &AtmosphereCanvas::activeRunningChanged, this, nullptr);
    }
    m_masterCanvas = c;
    if (m_masterCanvas) {
        connect(m_masterCanvas, &AtmosphereCanvas::frameSimulated, this, [this]() {
            if (!flags().testFlag(QQuickItem::ItemHasContents)) setFlag(QQuickItem::ItemHasContents, true);
            update();
        });
        connect(m_masterCanvas, &AtmosphereCanvas::activeRunningChanged, this, [this]() {
            setFlag(QQuickItem::ItemHasContents, false);
            setFlag(QQuickItem::ItemHasContents, true);
            update();
        });
    }
    emit masterCanvasChanged();
    updateTimerState();
}
void AtmosphereCanvas::setCursorPos(const QPointF &p) {
    if (m_cursorPos != p) {
        if (m_cursorPos.x() > -100 && p.x() > -100 && std::hypot(p.x() - m_cursorPos.x(), p.y() - m_cursorPos.y()) > 2.5) {
            m_idleSec = 0.0f;
        }
        if (m_prevCursorPos.x() > -100 && p.x() > -100) m_cursorVel = p - m_prevCursorPos;
        m_prevCursorPos = m_cursorPos; m_cursorPos = p; emit cursorPosChanged();
    }
}
void AtmosphereCanvas::setCursorPressed(bool v) {
    if (m_cursorPressed != v) {
        m_cursorPressed = v;
        if (!v) m_draggedLeafIdx = -1;
        emit cursorPosChanged();
    }
}
void AtmosphereCanvas::onConfigChanged() { updateTimerState(); }

void AtmosphereCanvas::onActionTriggered(const QString &act) {
    if (m_masterCanvas) return;
    if (act == "gust") {
        m_gustBoost = 1.65f;
        for (auto &p : m_particles) if (p.resting) { p.resting = false; p.attachedObstacleId.clear(); p.vy = -45.0f; p.vx += 240.0f; }
    } else if (act == "thunder") {
        if (m_lightningPulseStage == 0) {
            int side = QRandomGenerator::global()->bounded(3);
            m_lightningPosX = (side == 0) ? 0.16f : ((side == 1) ? 0.50f : 0.84f);
            m_lightningPulseStage = 1;
            m_lightningStageTimer = 0.0f;
            m_lightningFlash = 0.92f;
            emit lightningFlashChanged();
        }
    } else if (act == "shake") {
        for (auto it = m_obstacles.begin(); it != m_obstacles.end(); ++it) {
            releaseObstacleAccumulations(it.value(), true);
            it.value().waterDrops.clear(); it.value().trailBeads.clear();
            it.value().wetFilmBins.fill(0.0f); it.value().snowBins.fill(0.0f);
        }
        for (auto &p : m_particles) if (p.resting) { p.resting = false; p.attachedObstacleId.clear(); p.vy = 45.0f; }
    }
}

void AtmosphereCanvas::updateTimerState() {
    if (m_masterCanvas) { m_tickTimer.stop(); update(); return; }
    auto *mgr = AtmosphereManager::instance();
    if (!mgr) return;
    bool modeOk = m_globalMode ? mgr->globalOverlayEnabled() : mgr->inAppEnabled();
    bool anyW = mgr->rainEnabled() || mgr->snowEnabled() || mgr->leavesEnabled();
    m_tickTimer.setInterval(std::max(6, qRound(1000.0 / std::clamp(mgr->targetFps(), 24, 165))));
    if (m_activeRunning && modeOk && anyW) {
        if (!m_tickTimer.isActive()) { m_lastTickMs = m_clock.elapsed(); m_tickTimer.start(); }
        update();
        emit frameSimulated();
    } else {
        m_tickTimer.stop();
        // Очищаем капли ТОЛЬКО если сама погода выключена, а при сворачивании в демон сохраняем состояние!
        if (!modeOk || !anyW) {
            m_particles.clear(); m_splashes.clear(); m_obstacles.clear();
        }
        update();
        emit frameSimulated();
    }
}

QString AtmosphereCanvas::queryHyprSocket(const QString &cmd) const {
#ifdef Q_OS_WIN
    if (cmd.contains("cursorpos")) {
        QPoint pt = QCursor::pos();
        if (cmd.startsWith("j/") || cmd.startsWith("/j/"))
            return QString("{\"x\":%1,\"y\":%2}").arg(pt.x()).arg(pt.y());
        return QString("%1, %2").arg(pt.x()).arg(pt.y());
    }
    if (cmd.contains("monitors")) {
        QScreen *scr = QGuiApplication::primaryScreen();
        QRect g = scr ? scr->geometry() : QRect(0, 0, 1920, 1080);
        return QString("[{\"x\":%1,\"y\":%2,\"width\":%3,\"height\":%4,\"scale\":1.0}]")
            .arg(g.x()).arg(g.y()).arg(g.width()).arg(g.height());
    }
    if (cmd.contains("activeworkspace")) {
        return QStringLiteral("{\"id\":1,\"hasfullscreen\":false}");
    }
    if (cmd.contains("clients")) {
        static qint64 s_lastCliMs = 0;
        static QString s_cachedCli = QStringLiteral("[]");
        qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - s_lastCliMs < 25) return s_cachedCli;
        s_lastCliMs = nowMs;

        QJsonArray arr;
        double dpr = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
        if (dpr <= 0.1) dpr = 1.0;
        struct EnumCtx { QJsonArray *arr; double dpr; };
        EnumCtx ctx{&arr, dpr};
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            auto *c = reinterpret_cast<EnumCtx*>(lParam);
            if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;
            INT cloaked = 0;
            if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0)
                return TRUE;
            LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
            if ((ex & WS_EX_TRANSPARENT) || (ex & WS_EX_TOOLWINDOW)) return TRUE;
            wchar_t cls[128] = {};
            GetClassNameW(hwnd, cls, 127);
            QString cName = QString::fromWCharArray(cls);
            if (cName == "Progman" || cName == "WorkerW" || cName == "Shell_TrayWnd" ||
                cName == "Windows.UI.Core.CoreWindow") return TRUE;
            wchar_t titleBuf[256] = {};
            GetWindowTextW(hwnd, titleBuf, 255);
            QString title = QString::fromWCharArray(titleBuf);
            if (title.isEmpty() || title == "WinWalAtmosphereOverlay" || title == "WinWalDesktopWallpaper") return TRUE;

            RECT r = {};
            if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r)))) {
                GetWindowRect(hwnd, &r);
            }
            int x = int(std::round(r.left / c->dpr));
            int y = int(std::round(r.top / c->dpr));
            int w = int(std::round((r.right - r.left) / c->dpr));
            int h = int(std::round((r.bottom - r.top) / c->dpr));
            if (w < 80 || h < 50) return TRUE;

            QJsonObject obj;
            obj["class"] = cName;
            obj["title"] = title;
            obj["address"] = QString("0x%1").arg(reinterpret_cast<quintptr>(hwnd), 0, 16);
            obj["mapped"] = true;
            obj["hidden"] = false;
            obj["floating"] = true;
            obj["pinned"] = false;
            bool isRealFs = false;
            QScreen *pScr = QGuiApplication::primaryScreen();
            QRect scrR = pScr ? pScr->geometry() : QRect(0, 0, 1920, 1080);
            LONG_PTR wStyle = GetWindowLongPtrW(hwnd, GWL_STYLE);
            if ((r.right - r.left) >= scrR.width() && (r.bottom - r.top) >= scrR.height() &&
                !(wStyle & WS_CAPTION) && title != "WinWal" && !cName.contains("Qt", Qt::CaseInsensitive)) {
                isRealFs = true;
            }
            obj["fullscreen"] = isRealFs ? 1 : 0;
            obj["zOrder"] = c->arr->size();
            QJsonObject ws; ws["id"] = 1;
            obj["workspace"] = ws;
            QJsonArray at; at.append(x); at.append(y);
            QJsonArray sz; sz.append(w); sz.append(h);
            obj["at"] = at;
            obj["size"] = sz;
            c->arr->append(obj);
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));
        s_cachedCli = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        return s_cachedCli;
    }
#endif
    Q_UNUSED(cmd);
    return {};
}

void AtmosphereCanvas::connectHyprEventSocket() {}
void AtmosphereCanvas::onHyprEventReady() {}

static bool isCardInActivePage(QQuickItem *item, QQuickItem *rootStop) {
    for (QQuickItem *p = item; p && p != rootStop; p = p->parentItem()) {
        if (!p->isVisible() || p->opacity() < 0.15 || std::abs(p->rotation()) > 0.5)
            return false;
        QVariant vp = p->property("vortexProgress");
        if (vp.isValid() && vp.toReal() > 0.001)
            return false;
    }
    return true;
}

void AtmosphereCanvas::collectInAppItems(QQuickItem *item, const QRectF &clip, QHash<QString, QPair<QRectF, QQuickItem*>> &found) {
    if (!item || item == this || !isCardInActivePage(item, this)) return;
    const char *cls = item->metaObject()->className();

    if (strstr(cls, "NavButton") || strstr(cls, "StyledSlider") || item->objectName() == "skipAtmoChildren") return;

    if (item->objectName() == "sidebarSepLine") {
        QPointF tl = item->mapToItem(this, QPointF(0, 0));
        QPointF br = item->mapToItem(this, QPointF(item->width(), item->height()));
        if (tl.y() > 8 && tl.y() < 140) {
            double panelH = std::max(24.0, (height() - 36.0) - tl.y());
            QRectF r(tl.x(), tl.y(), std::abs(br.x() - tl.x()), panelH);
            if (r.width() >= 40)
                found.insert(QStringLiteral("sep_%1").arg(reinterpret_cast<quintptr>(item), 0, 16), qMakePair(r, item));
        }
        return;
    }

    QRectF curClip = clip;
    if (item->clip() && item->width() > 0 && item->height() > 0) {
        QRectF mr(item->mapToItem(this, QPointF(0, 0)), item->mapToItem(this, QPointF(item->width(), item->height())));
        curClip = curClip.intersected(mr.normalized());
        if (curClip.isEmpty()) return;
    }

    if (item->objectName() == "unifiedAtmoCard") {
        QRectF r = QRectF(item->mapToItem(this, QPointF(0, 0)), item->mapToItem(this, QPointF(item->width(), item->height()))).normalized().intersected(curClip);
        if (r.width() >= 36 && r.height() >= 18 && r.top() > 6 && r.top() < height() - 8)
            found.insert(QStringLiteral("list_%1").arg(reinterpret_cast<quintptr>(item), 0, 16), qMakePair(r, item));
        return;
    }

    bool isExplicit = (item->objectName() == "atmoObstacle");

    if (!isExplicit && item->parentItem() && item->parentItem()->objectName() == "morphCategoriesLayer") {
        if (item->width() >= 18 && item->height() >= 14 && !strstr(cls, "Repeater")) {
            isExplicit = true;
        }
    }

    // Признак настоящей карточки: только explicit atmoObstacle или GlassPanel (исключаем плавающие QQuickRectangle подсветки hoverPill!)
    bool isGlassCard = strstr(cls, "GlassPanel") != nullptr;
    if (isGlassCard && item->parentItem()) {
        const char *pcls = item->parentItem()->metaObject()->className();
        // Если GlassPanel является внутренним фоном маленькой кнопки/поля ввода, не считаем его отдельной большой карточкой
        if (strstr(pcls, "TextField") || strstr(pcls, "StyledButton") || strstr(pcls, "StyledSwitch") || strstr(pcls, "StyledComboBox"))
            isGlassCard = false;
    }

    bool isTarget = isExplicit || isGlassCard;

    if (isTarget && item->width() >= 45 && item->height() >= 22 && item->width() < width() * 0.96) {
        QRectF rawR = QRectF(item->mapToItem(this, QPointF(0, 0)), item->mapToItem(this, QPointF(item->width(), item->height()))).normalized();
        QRectF visR = rawR.intersected(curClip);
        if (visR.width() >= 40 && visR.height() >= 8 && rawR.bottom() > 10.0 && rawR.top() < height() - 10.0) {
            found.insert(QStringLiteral("ui_%1").arg(reinterpret_cast<quintptr>(item), 0, 16), qMakePair(rawR, item));
        }
    }
    for (QQuickItem *ch : item->childItems()) collectInAppItems(ch, curClip, found);
}

void AtmosphereCanvas::scanInAppObstacles(qint64 nowMs) {
    auto *mgr = AtmosphereManager::instance();
    if (!mgr || !mgr->uiCollisionsEnabled() || !window() || !window()->contentItem()) {
        for (auto &obs : m_obstacles) releaseObstacleAccumulations(obs, false);
        m_obstacles.clear();
        return;
    }
    QHash<QString, QPair<QRectF, QQuickItem*>> rawFound;
    collectInAppItems(window()->contentItem(), QRectF(0, 0, width(), height()), rawFound);

    // Фильтруем вложенные/совпадающие прямоугольники (например, когда у карточки и обертка atmoObstacle, и внутри GlassPanel)
    QHash<QString, QPair<QRectF, QQuickItem*>> found;
    for (auto itA = rawFound.begin(); itA != rawFound.end(); ++itA) {
        if (itA.key().startsWith("sep_")) {
            found.insert(itA.key(), itA.value());
            continue;
        }
        const QRectF &rA = itA.value().first;
        bool insideAnother = false;
        for (auto itB = rawFound.begin(); itB != rawFound.end(); ++itB) {
            if (itA.key() == itB.key() || itB.key().startsWith("sep_")) continue;
            const QRectF &rB = itB.value().first;
            // Если rA лежит внутри rB (или почти совпадает, но rB — внешний контейнер большего/равного размера)
            if (rB.adjusted(-4, -4, 4, 4).contains(rA)) {
                double areaA = rA.width() * rA.height();
                double areaB = rB.width() * rB.height();
                if (areaB > areaA + 10.0 || (std::abs(areaB - areaA) <= 10.0 && itA.key() > itB.key())) {
                    insideAnother = true;
                    break;
                }
            }
        }
        if (!insideAnother) {
            found.insert(itA.key(), itA.value());
        }
    }

    if (width() > 100 && height() > 100) {
        found.insert(QStringLiteral("window_glass"), qMakePair(QRectF(2.0, 2.0, width() - 4.0, height() - 4.0), window()->contentItem()));
        // Верхняя кромка самого окна HySett: копит воду сверху и сбрасывает капли вниз на внутренние карточки и элементы!
        found.insert(QStringLiteral("window_top_ledge"), qMakePair(QRectF(8.0, 1.0, width() - 16.0, 18.0), window()->contentItem()));
    }
    for (auto it = found.begin(); it != found.end(); ++it) {
        const QRectF &newR = it.value().first;
        QQuickItem *ptr = it.value().second;
        if (!m_obstacles.contains(it.key())) {
            ObstacleSurface obs;
            obs.id = it.key();
            obs.rect = newR;
            obs.itemPtr = ptr;
            obs.createdMs = obs.lastSeenMs = nowMs;
            obs.snowBins.fill(0.0f, 36);
            obs.wetFilmBins.fill(0.0f, 48);
            obs.fogLevel = (mgr->condensationEnabled() && (mgr->rainEnabled() || mgr->snowEnabled()) && it.key() != "window_glass") ? 0.35f : 0.0f;
            m_obstacles.insert(it.key(), obs);
        } else {
            auto &obs = m_obstacles[it.key()];
            obs.itemPtr = ptr;
            // Сбрасываем только если карточка сменила колонку по X (при вертикальном скролле страницы туман и капли сохраняются!)
            if (it.key() != "window_glass" && (std::abs(obs.rect.left() - newR.left()) > 90 || std::abs(obs.rect.width() - newR.width()) > 90)) {
                releaseObstacleAccumulations(obs, false);
                obs.waterDrops.clear();
                obs.trailBeads.clear();
                obs.wipedTracks.clear();
                obs.fogLevel = 0.0f;
                obs.wetFilmBins.fill(0.0f, 48);
            }
            obs.rect = newR;
            obs.lastSeenMs = nowMs;
        }
    }
    for (auto it = m_obstacles.begin(); it != m_obstacles.end();) {
        if (it.value().lastSeenMs < nowMs) {
            releaseObstacleAccumulations(it.value(), false);
            it = m_obstacles.erase(it);
        } else ++it;
    }
}

static int s_prevWsId = -1;
static int s_wsSlideDir = 0;
static qint64 s_wsSwitchMs = -5000;

void AtmosphereCanvas::pollHyprlandState(qint64 nowMs) {
    auto *mgr = AtmosphereManager::instance();
    if (!mgr) return;
    static QRect s_cachedMonGeom(0, 0, 1920, 1080);
    static qint64 s_lastMonQueryMs = -5000;
    if (nowMs - s_lastMonQueryMs > 3000) {
        s_lastMonQueryMs = nowMs;
        QJsonDocument mdoc = QJsonDocument::fromJson(queryHyprSocket("j/monitors").toUtf8());
        if (mdoc.isArray() && !mdoc.array().isEmpty()) {
            QJsonObject mo = mdoc.array().first().toObject();
            double sc = std::max(0.5, mo["scale"].toDouble(1.0));
            s_cachedMonGeom = QRect(mo["x"].toInt(0), mo["y"].toInt(0),
                                    int(std::round(mo["width"].toInt(1920) / sc)),
                                    int(std::round(mo["height"].toInt(1080) / sc)));
        } else if (window() && window()->screen()) {
            s_cachedMonGeom = window()->screen()->geometry();
        }
    }
    QRect monGeom = s_cachedMonGeom;
    QPointF monOff = monGeom.topLeft();
    double scaleX = (monGeom.width() > 0 && width() > 100) ? (width() / double(monGeom.width())) : 1.0;
    double scaleY = (monGeom.height() > 0 && height() > 100) ? (height() / double(monGeom.height())) : 1.0;

    // Позиция курсора централизованно опрашивается в onTick()

    static int s_cachedWsId = -1;
    static bool s_cachedWsFullscreen = false;
    static qint64 s_lastWsQueryMs = -5000;
    if (nowMs - s_lastWsQueryMs > 120) {
        s_lastWsQueryMs = nowMs;
        QJsonDocument wsDoc = QJsonDocument::fromJson(queryHyprSocket("j/activeworkspace").toUtf8());
        if (wsDoc.isObject()) {
            s_cachedWsId = wsDoc.object()["id"].toInt(-1);
            s_cachedWsFullscreen = wsDoc.object()["hasfullscreen"].toBool(false);
            if (s_cachedWsId != -1 && s_prevWsId != -1 && s_cachedWsId != s_prevWsId) {
                s_wsSlideDir = (s_cachedWsId > s_prevWsId) ? 1 : -1;
                s_wsSwitchMs = nowMs;
            }
            if (s_cachedWsId != -1) s_prevWsId = s_cachedWsId;
        }
    }
    int activeWsId = s_cachedWsId;
    if (mgr->pauseOnFullscreen() && s_cachedWsFullscreen) { m_fullscreenPaused = true; return; }
    m_fullscreenPaused = false;
    QHash<QString, ObstacleSurface> targets;

    if (width() > 100 && height() > 100) {
        ObstacleSurface scrObs;
        scrObs.id = "window_glass";
        scrObs.rect = QRectF(2.0, 2.0, width() - 4.0, height() - 4.0);
        targets.insert("window_glass", scrObs);
    }

    bool hasRealWindowOnWs = false;
    QJsonDocument cdoc = QJsonDocument::fromJson(queryHyprSocket("j/clients").toUtf8());
    if (cdoc.isArray()) {
        for (const auto &v : cdoc.array()) {
            QJsonObject co = v.toObject();
            QString cls = co["class"].toString();
            QString title = co["title"].toString();
            QString addr = co["address"].toString();

            if (cls == "hypratmooverlay" || cls == "com.whrr.HyprAtmosphere" || title == "HySettAtmosphereOverlay") {
                bool isFloat = co["floating"].toBool(false);
                bool isPin = co["pinned"].toBool(false);
                QJsonArray at = co["at"].toArray(), sz = co["size"].toArray();
                int curX = at.size() == 2 ? at[0].toInt() : -1;
                int curY = at.size() == 2 ? at[1].toInt() : -1;
                int curW = sz.size() == 2 ? sz[0].toInt() : 0;
                int curH = sz.size() == 2 ? sz[1].toInt() : 0;
                if (!isFloat || !isPin || curX != monGeom.x() || curY != monGeom.y() ||
                    std::abs(curW - monGeom.width()) > 4 || std::abs(curH - monGeom.height()) > 4) {
                    if (!isFloat) queryHyprSocket(QString("/dispatch setfloating address:%1").arg(addr));
                    if (!isPin)   queryHyprSocket(QString("/dispatch pin address:%1").arg(addr));
                    queryHyprSocket(QString("/dispatch movewindowpixel exact %1 %2,address:%3").arg(monGeom.x()).arg(monGeom.y()).arg(addr));
                    queryHyprSocket(QString("/dispatch resizewindowpixel exact %1 %2,address:%3").arg(monGeom.width()).arg(monGeom.height()).arg(addr));
                }
                // Если оверлей случайно получил фокус — сбрасываем фокус, чтобы Waybar считал пустой стол .empty!
                if (co["focusHistoryID"].toInt(99) == 0) {
                    queryHyprSocket("/dispatch focuswindow address:0x0");
                }
                continue;
            }

            if (!co["mapped"].toBool(false) || co["hidden"].toBool(false)) continue;
            if (cls.contains("overlay", Qt::CaseInsensitive) || cls.contains("widget", Qt::CaseInsensitive) ||
                title.contains("overlay", Qt::CaseInsensitive) || (co["pinned"].toBool(false) && co["nofocus"].toBool(false))) continue;
            if (activeWsId != -1 && co["workspace"].toObject()["id"].toInt(-1) != activeWsId) continue;
            hasRealWindowOnWs = true;

            // учитываем и тайловое, и плавающее окно HySett как открытое окно
            if (!mgr->windowInteractEnabled()) continue;
            if (mgr->pauseOnFullscreen() && co["fullscreen"].toInt(0) != 0) { m_fullscreenPaused = true; return; }
            QJsonArray at = co["at"].toArray(), sz = co["size"].toArray();
            if (at.size() == 2 && sz.size() == 2) {
                QRectF r((at[0].toDouble() - monOff.x()) * scaleX,
                         (at[1].toDouble() - monOff.y()) * scaleY,
                         sz[0].toDouble() * scaleX,
                         sz[1].toDouble() * scaleY);
                if (r.width() >= 80 && r.height() >= 50) {
                    ObstacleSurface wobs; wobs.id = "win_" + addr; wobs.rect = r; wobs.isWindow = true; wobs.clipRect = QRectF(co["zOrder"].toInt(0), 0, 0, 0);
                    targets.insert(wobs.id, wobs);
                }
            }
        }
    }

    // Островки модулей Waybar (всегда отслеживаем их геометрию с кэшем 2 сек, чтобы сугробы окон рендерились строго ПОД Waybar!)
    if (true) {
        static bool s_posBottom = false;
        static double s_barH = 30, s_mTop = 8, s_mBot = 8, s_mLeft = 10, s_mRight = 10;
        static qint64 s_lastWbConfMs = -5000;
        if (nowMs - s_lastWbConfMs > 2000) {
            s_lastWbConfMs = nowMs;
            QFile wf(QDir::homePath() + "/.config/hypr/scripts/waybar.conf");
            if (wf.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&wf);
                while (!in.atEnd()) {
                    QString l = in.readLine().trimmed(); int eq = l.indexOf('='); if (eq == -1) continue;
                    QString k = l.left(eq).trimmed(), v = l.mid(eq + 1).trimmed();
                    if (k == "pos_bottom") s_posBottom = (v == "1" || v == "true");
                    else if (k == "bar_height") s_barH = v.toDouble();
                    else if (k == "margin_top") s_mTop = v.toDouble();
                    else if (k == "margin_bottom") s_mBot = v.toDouble();
                    else if (k == "margin_left") s_mLeft = v.toDouble();
                    else if (k == "margin_right") s_mRight = v.toDouble();
                }
            }
        }
        bool posBottom = s_posBottom; double barH = s_barH, mTop = s_mTop, mBot = s_mBot, mLeft = s_mLeft, mRight = s_mRight;
        double wy = posBottom ? (height() - barH - mBot) : mTop;

        // 1. Левый островок Waybar (иконка Arch, воркспейсы, погода)
        ObstacleSurface wbLeft; wbLeft.id = "waybar_left"; wbLeft.isWaybar = true;
        wbLeft.rect = QRectF(mLeft, wy, std::min(285.0, width() * 0.22), barH);
        targets.insert(wbLeft.id, wbLeft);

        // 2. Правый островок Waybar (ресурсы, трей, звук, сеть, часы)
        double rightW = std::min(510.0, width() * 0.30);
        ObstacleSurface wbRight; wbRight.id = "waybar_right"; wbRight.isWaybar = true;
        wbRight.rect = QRectF(std::max(300.0, width() - mRight - rightW), wy, rightW, barH);
        targets.insert(wbRight.id, wbRight);

        // Синхронизируем видимость блока #window в Waybar (скрываем пустой блок, когда открыт только оверлей)
        syncWaybarWindowPillVisibility(hasRealWindowOnWs);
        // 3. Центральный островок Waybar (название активного окна — только если на рабочем столе открыто окно!)
                if (mgr->waybarInteractEnabled()) {
            ObstacleSurface tbObs;
            tbObs.id = "win10_taskbar";
            tbObs.isWaybar = true;
            tbObs.rect = QRectF(0.0, height() - 40.0, width(), 40.0);
            targets.insert(tbObs.id, tbObs);
        }
        if (hasRealWindowOnWs) {
            double centerW = 190.0;
            ObstacleSurface wbCenter; wbCenter.id = "waybar_center"; wbCenter.isWaybar = true;
            wbCenter.rect = QRectF((width() - centerW) * 0.5, wy, centerW, barH);
            targets.insert(wbCenter.id, wbCenter);
        }
    }

    for (auto it = targets.begin(); it != targets.end(); ++it) {
        if (!m_obstacles.contains(it.key())) {
            ObstacleSurface obs = it.value(); obs.createdMs = obs.lastSeenMs = nowMs;
            obs.snowBins.fill(0.0f, 36); obs.wetFilmBins.fill(0.0f, 48);
            obs.fogLevel = (mgr->condensationEnabled() && (mgr->rainEnabled() || mgr->snowEnabled()) && it.key() != "window_glass" && !it.value().isWindow) ? 0.35f : 0.0f;
            m_obstacles.insert(it.key(), obs);
        } else {
            auto &obs = m_obstacles[it.key()];
            double dxWin = it.value().rect.left() - obs.rect.left();
            double dyWin = it.value().rect.top() - obs.rect.top();
            double dSize = std::abs(obs.rect.width() - it.value().rect.width())
                         + std::abs(obs.rect.height() - it.value().rect.height());
            double dMove = std::abs(dxWin) + std::abs(dyWin) + dSize;
            if (it.key() != "window_glass" && dMove > 0.4) {
                // Снег на верхней грани окна мгновенно реагирует на перетаскивание и потряхивание!
                if (!obs.snowBins.isEmpty() && (std::abs(dxWin) > 0.4 || std::abs(dyWin) > 0.4)) {
                    int n = obs.snowBins.size();
                    double binW = std::max(4.0, obs.rect.width() / double(n));
                    // Накапливаем дробный сдвиг ячеек, чтобы даже мелкие движения и тряска окна реально передвигали кучки снега!
                    obs.snowSlideRem += float(-dxWin / binW * 1.45);
                    obs.snowPhaseShift += float(-dxWin * 0.025);
                    int intShift = int(std::trunc(obs.snowSlideRem));
                    double shakeMag = std::hypot(dxWin, dyWin);

                    if (intShift != 0 || shakeMag > 4.0) {
                        obs.snowSlideRem -= float(intShift);
                        int stepShift = std::clamp(intShift, -5, 5);
                        QVector<float> oldBins = obs.snowBins;
                        QVector<float> shifted(n, 0.0f);
                        float shakeDamp = std::clamp(1.0f - float(shakeMag) * 0.0045f, 0.82f, 0.995f);

                        for (int b = 0; b < n; ++b) {
                            int src = b - stepShift;
                            float val = (src >= 0 && src < n) ? oldBins[src] : 0.0f;
                            // При тряске верхушки кучек расползаются и перекатываются в соседние ячейки
                            if (shakeMag > 5.0 && b > 0 && b < n - 1) {
                                int sL = std::clamp(src - 1, 0, n - 1);
                                int sR = std::clamp(src + 1, 0, n - 1);
                                val = val * 0.78f + (oldBins[sL] + oldBins[sR]) * 0.11f;
                            }
                            shifted[b] = val * shakeDamp;
                        }
                        obs.snowBins = shifted;

                        // При потряхивании окна с кучек снега взлетают и осыпаются снежинки!
                        auto *rng = QRandomGenerator::global();
                        if (shakeMag > 6.5 && m_particles.size() < 460) {
                            int burst = std::clamp(int(shakeMag / 8.0), 1, 4);
                            for (int k = 0; k < burst; ++k) {
                                int pickB = rng->bounded(n);
                                if (oldBins[pickB] > 2.2f) {
                                    obs.snowBins[pickB] = std::max(0.0f, obs.snowBins[pickB] - 0.65f);
                                    Particle sp;
                                    sp.type = Snow;
                                    sp.x = float(it.value().rect.left() + (pickB + 0.5) / double(n) * it.value().rect.width());
                                    sp.y = float(it.value().rect.top() - oldBins[pickB] * 0.5f);
                                    sp.vx = float(-dxWin * 3.2 + (rng->bounded(70) - 35));
                                    sp.vy = float(std::min(-15.0, -dyWin * 2.0) + (rng->bounded(50) - 10));
                                    sp.size = 3.4f + (rng->bounded(20) * 0.1f);
                                    sp.alpha = 0.90f; sp.depth = 0.85f;
                                    sp.angle = rng->bounded(360); sp.angVel = rng->bounded(110) - 55;
                                    sp.leafKind = rng->bounded(8); sp.color = QColor(244, 250, 255);
                                    m_particles.append(sp);
                                }
                            }
                        }
                    }
                }
                if (!mgr->snowEnabled() || dSize > 10.0) {
                    obs.fogLevel = 0.0f;
                    obs.createdMs = nowMs;
                    obs.wipedTracks.clear();
                }
                if (dMove > 14.0 && !mgr->snowEnabled()) {
                    releaseObstacleAccumulations(obs, true);
                    obs.waterDrops.clear();
                    obs.trailBeads.clear();
                    obs.wetFilmBins.fill(0.0f, 48);
                }
            }
            obs.rect = it.value().rect; obs.clipRect = it.value().clipRect; obs.lastSeenMs = nowMs;
        }
    }
    for (auto it = m_obstacles.begin(); it != m_obstacles.end();) {
        if (it.value().lastSeenMs < nowMs) { releaseObstacleAccumulations(it.value(), true); it = m_obstacles.erase(it); }
        else ++it;
    }
}

void AtmosphereCanvas::releaseObstacleAccumulations(const ObstacleSurface &obs, bool violent) {
    auto *rng = QRandomGenerator::global();
    for (const auto &wd : obs.waterDrops) {
        if (wd.pinned && !violent) continue;
        float px = obs.rect.left() + wd.relX * obs.rect.width(), py = obs.rect.top() + wd.relY;
        Particle p;
        p.type = FallingDrop; // Сорвавшаяся капля летит именно каплей (💧), а не полосой дождя!
        p.x = px; p.y = py;
        p.vx = (rng->bounded(36) - 18) * 1.0f;
        p.vy = violent ? (65.0f + rng->bounded(75)) : (40.0f + rng->bounded(50));
        p.size = std::clamp(wd.radius * 0.85f, 2.8f, 5.0f);
        p.alpha = 0.90f; p.depth = 0.95f;
        if (m_particles.size() < 460) m_particles.append(p);
        if (violent && m_splashes.size() < 180) {
            SplashDrop sp; sp.x = px; sp.y = py; sp.vx = (rng->bounded(110) - 55); sp.vy = -(30.0f + rng->bounded(65));
            sp.radius = 1.8f; sp.life = sp.maxLife = 0.28f; m_splashes.append(sp);
        }
    }
    int bins = obs.snowBins.size();
    for (int i = 0; i < bins; ++i) {
        if (obs.snowBins[i] > 1.2f && m_particles.size() < 460) {
            int flakesFromBin = (obs.snowBins[i] > 6.5f) ? 2 : 1;
            for (int k = 0; k < flakesFromBin && m_particles.size() < 460; ++k) {
                Particle p;
                p.type = Snow;
                p.depth = 0.35f + (rng->bounded(65) * 0.01f);
                p.x = float(obs.rect.left() + (i + 0.5) / double(bins) * obs.rect.width() + (rng->bounded(25) - 12));
                p.y = float(obs.rect.top() - rng->bounded(int(std::max(4.0f, obs.snowBins[i] * 1.4f))) + (rng->bounded(19) - 9));
                p.vx = float(rng->bounded(130) - 65);
                // Широкий разброс начальных и установившихся скоростей падения (0.45x .. 1.85x), чтобы ряд сразу распадался в облако!
                p.termV = 0.45f + (rng->bounded(140) * 0.01f);
                p.vy = (25.0f + rng->bounded(110)) * p.termV;
                p.size = 2.8f + (rng->bounded(35) * 0.1f);
                p.alpha = 0.65f + p.depth * 0.32f;
                p.angle = rng->bounded(360);
                p.angVel = (rng->bounded(110) - 55) * 1.0f;
                p.tiltPhase = rng->bounded(628) * 0.01f;
                p.tiltSpeed = 1.1f + rng->bounded(18) * 0.1f;
                p.swayPhase = rng->bounded(628) * 0.01f;
                p.leafKind = rng->bounded(8);
                p.color = QColor(245, 250, 255);
                m_particles.append(p);
            }
        }
    }
    for (auto &p : m_particles) if (p.resting && p.attachedObstacleId == obs.id) {
        p.resting = false; p.attachedObstacleId.clear(); p.vy = 45.0f;
    }
}

QColor AtmosphereCanvas::pickLeafColor(int palette) const {
    auto *rng = QRandomGenerator::global();
    int idx = rng->bounded(4);
    QColor base;
    if (palette == 1) {
        // Старое золото / сухая охра / пожухлый янтарь
        static const QColor c[4] = {QColor("#8c5818"), QColor("#784712"), QColor("#9a6b24"), QColor("#633910")};
        base = c[idx];
    } else if (palette == 2) {
        // Припылённая увядшая сакура / пепельно-розовый
        static const QColor c[4] = {QColor("#8f5b66"), QColor("#7a4954"), QColor("#9e6b75"), QColor("#693d47")};
        base = c[idx];
    } else if (palette == 3) {
        // Тёмный лесной мох / оливково-бурый
        static const QColor c[4] = {QColor("#43542a"), QColor("#364521"), QColor("#525e34"), QColor("#2d3b1e")};
        base = c[idx];
    } else {
        // Пожухлая багряная осень / ржавая умбра / тёмный терракотовый
        static const QColor c[4] = {QColor("#78261e"), QColor("#6b341a"), QColor("#85381c"), QColor("#572318")};
        base = c[idx];
    }
    // Небольшой индивидуальный разброс тона и затемнения для каждого листа
    int darken = 92 + rng->bounded(22);
    return base.darker(darken);
}

void AtmosphereCanvas::spawnParticle(Particle &p, ParticleType type, bool randomY) {
    auto *mgr = AtmosphereManager::instance();
    auto *rng = QRandomGenerator::global();
    float w = std::max(100.0, width()), h = std::max(100.0, height());
    float sm = mgr ? float(mgr->particleScale()) : 1.0f, gm = mgr ? float(mgr->gravityScale()) : 1.0f, wind = mgr ? float(mgr->windStrength()) : 0.2f;
    p.type = type; p.resting = false; p.attachedObstacleId.clear();
    p.depth = 0.25f + (rng->bounded(75) * 0.01f);
    p.x = -w * 0.15f + rng->bounded(int(w * 1.30f));
    p.y = randomY ? rng->bounded(int(h)) : -(12.0f + rng->bounded(65));
    if (type == Rain) {
        // Широкий разброс размеров капель дождя: от мелкой мороси (0.48x) до крупных тяжелых капель (1.85x)
        float randFactor = (rng->bounded(1000) * 0.001f);
        float sizeVar = 0.95f + std::pow(randFactor, 1.30f) * 1.95f;
        p.size = sizeVar * sm;
        float par = 0.62f + 0.38f * p.depth;
        p.termV = (340.0f + sizeVar * 210.0f) * par;
        p.drag = 2.6f + (1.9f - sizeVar) * 1.3f;
        p.vy = p.termV * gm * (0.85f + rng->bounded(15) * 0.01f);
        float wEff = std::clamp(wind + m_windDriftOffset + m_gustBoost, -1.35f, 1.35f);
        p.vx = wEff * 195.0f * par;
        float drift = p.vx / std::max(260.0f, p.vy) * h;
        p.x = -w * 0.15f - (drift > 0.0f ? drift : 0.0f) + rng->bounded(int(w * 1.30f + std::abs(drift)));
        p.alpha = std::clamp(0.30f + sizeVar * 0.32f + p.depth * 0.25f, 0.28f, 0.92f);
    } else if (type == Snow) {
        int curBokehCount = 0;
        for (const auto &otherP : m_particles) {
            if (otherP.type == Snow && otherP.leafKind == 8 && (&otherP != &p)) ++curBokehCount;
        }
        bool isBgBokehCircle = (curBokehCount < 3 && rng->bounded(100) < 4);
        if (isBgBokehCircle) {
            // Крупные белые мягкие круги на заднем плане, падающие медленно!
            p.leafKind = 8;
            p.depth = 0.18f + (rng->bounded(32) * 0.01f);
            p.size = (6.8f + (rng->bounded(55) * 0.1f)) * sm;
            p.termV = 0.34f + (rng->bounded(32) * 0.01f); // Падают в ~2.5 раза медленнее
            p.vy = (22.0f + p.depth * 28.0f) * p.termV * gm;
            p.vx = wind * 48.0f;
            p.alpha = 0.24f + (rng->bounded(26) * 0.01f);
        } else {
            p.leafKind = rng->bounded(8); // 8 видов 6-лучевых кристаллических снежинок
            p.size = (3.4f + p.depth * 4.8f) * sm;
            // Индивидуальный разброс скорости падения каждой снежинки (0.55x .. 1.65x)
            p.termV = 0.55f + (rng->bounded(110) * 0.01f);
            p.vy = (42.0f + p.depth * 56.0f) * p.termV * gm;
            p.vx = wind * 80.0f;
            p.alpha = 0.62f + p.depth * 0.36f;
        }
        p.angle = rng->bounded(360);
        p.angVel = (rng->bounded(70) - 35) * 1.0f;
        p.tiltPhase = rng->bounded(628) * 0.01f;
        p.tiltSpeed = 1.1f + rng->bounded(16) * 0.1f;
        p.swayPhase = rng->bounded(628) * 0.01f;
        p.bendAmount = 0.0f;
        p.color = QColor(248, 252, 255);
        // Чтобы при старте (randomY = true) снежинки не зачислялись мгновенно в сугробы на первом кадре, сдвигаем их от граней:
        if (randomY) {
            for (const auto &obs : m_obstacles) {
                if (std::abs(p.y - obs.rect.top()) < 22.0) {
                    p.y = float(obs.rect.top() - 38.0);
                }
            }
        }
    } else {
        bool spawnInBg = (rng->bounded(100) < 45);
        p.depth = spawnInBg ? (0.26f + rng->bounded(20) * 0.01f) : (0.58f + rng->bounded(38) * 0.01f);
        if (spawnInBg && !randomY && rng->bounded(100) < 40) {
            // Фоновый лист может изначально появиться за открытым окном в верхней/средней части экрана и вылететь из-за его края!
            p.y = rng->bounded(int(h * 0.55f));
        }
        p.size = 9.6f;
        p.vy = (38.0f + p.depth * 36.0f) * gm;
        p.vx = wind * 105.0f;
        p.angle = rng->bounded(360) - 180;
        p.angVel = rng->bounded(90) - 45;
        p.tiltPhase = rng->bounded(628) * 0.01f;
        p.tiltSpeed = 1.6f + rng->bounded(22) * 0.1f;
        p.swayPhase = rng->bounded(628) * 0.01f;
        p.leafKind = (rng->bounded(100) < 75) ? 0 : (1 + rng->bounded(2)); // 75% = Клён, 25% = Дуб/Берёза
        p.x = -w * 0.05f + rng->bounded(int(w * 1.10f));
        p.relX = 0.0f; p.relY = 0.0f; p.bendAmount = 0.0f;
        p.alpha = 0.84f + p.depth * 0.15f;
        p.color = pickLeafColor(mgr ? mgr->leavesPalette() : 0);
    }
}


struct LeafImprint {
    int ownerTag = -1;
    int kind = 0, var = 0;
    float x1 = 0, y1 = 0, angle1 = 0;
    float x2 = 0, y2 = 0, angle2 = 0;
    float drawSz = 42.0f;
    float life = 14.0f, maxLife = 14.0f;
    bool released = false;
};
static QVector<LeafImprint> s_leafImprints[2]; // 0 = in-app, 1 = global overlay
static QVector<QImage> s_leafTextures;
static QVector<QImage> s_leafImprintMasks;


static QVector<QImage> s_snowflakeTextures;
static QVector<QImage> s_frostCornerTextures;

static void ensureSnowAssetsGenerated() {
    if (!s_snowflakeTextures.isEmpty() && !s_frostCornerTextures.isEmpty()) return;
    s_snowflakeTextures.resize(8);
    s_frostCornerTextures.resize(4);

    // 1. Генерация 8 видов 6-лучевых кристаллических снежинок (дендриты, звёздчатые пластинки, морозные перья)
    const int sSz = 160;
    const double cx = sSz * 0.5, cy = sSz * 0.5, maxR = sSz * 0.44;
    for (int k = 0; k < 8; ++k) {
        QImage img(sSz, sSz, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        for (int py = 0; py < sSz; ++py) {
            QRgb *scan = reinterpret_cast<QRgb *>(img.scanLine(py));
            for (int px = 0; px < sSz; ++px) {
                double dx = (px - cx) / maxR;
                double dy = (py - cy) / maxR;
                double r  = std::hypot(dx, dy);
                if (r > 1.05) continue;

                // Складываем угол в 6-кратную симметрию (сектор 0 .. pi/6)
                double ang = std::atan2(dy, dx);
                if (ang < 0.0) ang += 6.2831853;
                double sector = std::fmod(ang, 1.04719755); // pi / 3
                if (sector > 0.52359878) sector = 1.04719755 - sector; // 0 .. pi / 6

                // Координаты вдоль главного луча (uArm) и перпендикулярно ему (vArm)
                double uArm = r * std::cos(sector);
                double vArm = r * std::sin(sector);

                double density = 0.0;
                // Центральное гексагональное ледяное ядро
                double hexDist = uArm + vArm * 0.57735;
                double coreR = (k % 3 == 0) ? 0.18 : ((k % 3 == 1) ? 0.25 : 0.14);
                density = std::max(density, std::clamp(1.0 - (hexDist - coreR) * 18.0, 0.0, 1.0) * 0.85);
                if (hexDist < coreR * 0.72) density *= 0.65; // Прозрачное окошко внутри гексагона

                // Главный луч снежинки, сужающийся к острию
                double armW = (0.042 - 0.022 * uArm) * (k >= 6 ? 1.25 : 1.0);
                if (uArm <= 0.96) {
                    density = std::max(density, std::clamp(1.0 - vArm / std::max(0.008, armW), 0.0, 1.0));
                }

                // Боковые веточки-дендриты под углом 60 градусов от главного луча
                int numBranches = (k % 2 == 0) ? 3 : 4;
                for (int b = 1; b <= numBranches; ++b) {
                    double bPos = 0.22 + b * (0.64 / numBranches) + (k % 3) * 0.03;
                    double bLen = (0.34 - b * 0.055) * ((k % 4 == 2) ? 0.75 : 1.05);
                    // Проекция на веточку под углом 60 градусов: (u - bPos, v) вдоль (0.5, 0.866)
                    double du = uArm - bPos;
                    double alongB = du * 0.50 + vArm * 0.866025;
                    double perpB  = std::abs(-du * 0.866025 + vArm * 0.50);
                    if (alongB >= 0.0 && alongB <= bLen) {
                        double bw = 0.032 * (1.0 - alongB / bLen * 0.65);
                        double bMask = std::clamp(1.0 - perpB / std::max(0.007, bw), 0.0, 1.0);
                        density = std::max(density, bMask * 0.92);
                        // Микро-зубчики 3-го порядка на веточках
                        if (k < 5 && alongB > 0.06 && alongB < bLen * 0.85) {
                            double subWave = std::sin(alongB * 48.0) * 0.5 + 0.5;
                            double subMask = std::clamp(1.0 - perpB / (bw * 2.1), 0.0, 1.0) * subWave * 0.55;
                            density = std::max(density, subMask);
                        }
                    }
                }

                // Мягкое морозное свечение вокруг центра и лучей
                double halo = std::exp(-r * r * 5.5) * 0.32 + std::exp(-vArm * vArm * 180.0) * std::max(0.0, 1.0 - r) * 0.28;
                double totalA = std::clamp(density + halo, 0.0, 1.0) * std::clamp((1.02 - r) * 5.0, 0.0, 1.0);
                if (totalA <= 0.005) continue;

                // Ледяной 3D-блик внутри кристалла
                double iceBright = std::clamp(0.88 + 0.12 * density - 0.10 * dx - 0.10 * dy, 0.75, 1.0);
                int rC = std::clamp(int(236.0 * iceBright * totalA), 0, int(255 * totalA));
                int gC = std::clamp(int(246.0 * iceBright * totalA), 0, int(255 * totalA));
                int bC = std::clamp(int(255.0 * totalA), 0, int(255 * totalA));
                int aC = std::clamp(int(255.0 * totalA), 0, 255);
                scan[px] = qRgba(rC, gC, bC, aC);
            }
        }
        s_snowflakeTextures[k] = img;
    }

    // 2. Генерация 4 текстур угловых морозных узоров (220x220) с ветвистыми папоротниковыми дендритами льда
    const int fSz = 220;
    for (int corner = 0; corner < 4; ++corner) {
        QImage fImg(fSz, fSz, QImage::Format_ARGB32_Premultiplied);
        fImg.fill(Qt::transparent);
        for (int py = 0; py < fSz; ++py) {
            QRgb *scan = reinterpret_cast<QRgb *>(fImg.scanLine(py));
            for (int px = 0; px < fSz; ++px) {
                double u = double((corner & 1) ? (fSz - 1 - px) : px) / double(fSz);
                double v = double((corner & 2) ? (fSz - 1 - py) : py) / double(fSz);

                double dCorner = std::hypot(u, v);
                double dBorder = std::min(u, v);
                if (dCorner > 0.98) continue;

                double theta = std::atan2(v, u); // 0 .. pi/2
                // Изогнутые главные стебли морозного папоротника + боковые ледяные перья под углом 60°
                double curl = std::sin(dCorner * 11.5 + corner * 1.3) * 0.16 + std::cos(dCorner * 23.0 - theta * 4.0) * 0.06;
                double stemPhase = (theta + curl) * 7.5;
                double stemDist = std::abs(stemPhase - std::round(stemPhase));
                double mainStem = std::clamp(1.0 - stemDist * 3.8, 0.0, 1.0);

                // Боковые ответвления-иголочки от каждого стебля
                double sideCoord = dCorner * 28.0 - stemDist * 4.2;
                double sideTooth = std::abs(std::sin(sideCoord * 3.14159));
                double subNeedles = std::pow(std::clamp(1.0 - stemDist * 1.85, 0.0, 1.0), 1.4)
                                  * std::pow(sideTooth, 1.6)
                                  * (0.55 + 0.45 * std::sin(dCorner * 42.0 + theta * 19.0));

                // Вторичная тонкая кристаллическая сетка вдоль самих граней окна
                double borderNeedles = std::max(0.0, 1.0 - dBorder * 5.2)
                                     * (0.45 + 0.55 * std::pow(std::abs(std::sin((u + v) * 38.0 + curl * 6.0)), 1.8));

                double jaggedReach = 0.88 - 0.24 * stemDist + 0.14 * std::sin(theta * 14.0 + corner);
                double envCorner = std::clamp((jaggedReach - dCorner) * 2.8, 0.0, 1.0);
                double envBorder = std::clamp((1.0 - dBorder * 4.8) * (1.0 - dCorner * 0.92), 0.0, 1.0);
                double env = std::max(envCorner, envBorder);

                // Прозрачные промежутки между ветвями, чтобы узор выглядел именно кристаллами льда, а не туманом!
                double crystal = std::clamp(mainStem * 0.85 + subNeedles * 0.95 + borderNeedles * 0.70, 0.0, 1.0);
                double frostAlpha = std::clamp(env * crystal * (0.25 + 0.75 * crystal), 0.0, 0.92);
                if (frostAlpha <= 0.012) continue;

                double glint = (crystal > 0.62) ? 1.0 : 0.84;
                int rC = std::clamp(int(210.0 * glint * frostAlpha), 0, int(255 * frostAlpha));
                int gC = std::clamp(int(236.0 * glint * frostAlpha), 0, int(255 * frostAlpha));
                int bC = std::clamp(int(255.0 * frostAlpha), 0, int(255 * frostAlpha));
                int aC = std::clamp(int(255.0 * frostAlpha), 0, 255);
                scan[px] = qRgba(rC, gC, bC, aC);
            }
        }
        s_frostCornerTextures[corner] = fImg;
    }
}

static void ensureLeafAssetsGenerated() {
    if (!s_leafTextures.isEmpty() && !s_leafImprintMasks.isEmpty()) return;
    s_leafTextures.resize(48);
    s_leafImprintMasks.resize(12);

    auto hash2D = [](int x, int y, int seed) -> double {
        uint32_t u = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + uint32_t(seed) * 1442695041u; int n = int(u);
        n = (n ^ (n >> 13)) * 1274126177;
        return ((n ^ (n >> 16)) & 0x7fffffff) / double(0x7fffffff);
    };
    auto smoothNoise = [&](double x, double y, int seed) -> double {
        int ix = int(std::floor(x)), iy = int(std::floor(y));
        double fx = x - ix, fy = y - iy;
        double u = fx * fx * (3.0 - 2.0 * fx), v = fy * fy * (3.0 - 2.0 * fy);
        double a = hash2D(ix, iy, seed), b = hash2D(ix + 1, iy, seed);
        double c = hash2D(ix, iy + 1, seed), d = hash2D(ix + 1, iy + 1, seed);
        return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
    };
    auto fbm = [&](double x, double y, int seed) -> double {
        double val = 0.0, amp = 0.5, freq = 1.0;
        for (int o = 0; o < 4; ++o) {
            val += amp * smoothNoise(x * freq, y * freq, seed + o * 31);
            freq *= 2.05; amp *= 0.5;
        }
        return val;
    };
    auto voronoiEdge = [&](double x, double y, int seed) -> double {
        int ix = int(std::floor(x)), iy = int(std::floor(y));
        double fx = x - ix, fy = y - iy;
        double d1 = 8.0, d2 = 8.0;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                double px = dx + hash2D(ix + dx, iy + dy, seed) * 0.82 + 0.09 - fx;
                double py = dy + hash2D(ix + dx, iy + dy, seed + 79) * 0.82 + 0.09 - fy;
                double d = std::hypot(px, py);
                if (d < d1) { d2 = d1; d1 = d; }
                else if (d < d2) { d2 = d; }
            }
        }
        return std::clamp(1.0 - (d2 - d1) * 4.6, 0.0, 1.0);
    };

    const int texSz = 256;
    for (int pal = 0; pal < 4; ++pal) {
        for (int kind = 0; kind < 3; ++kind) {
            for (int var = 0; var < 4; ++var) {
                int idx = pal * 12 + kind * 4 + var;
                int seed = idx * 97 + 13;
                QImage img(texSz, texSz, QImage::Format_ARGB32_Premultiplied);
                img.fill(Qt::transparent);

                QImage impImg;
                if (pal == 0) {
                    impImg = QImage(texSz, texSz, QImage::Format_ARGB32_Premultiplied);
                    impImg.fill(Qt::transparent);
                }

                QColor cMain, cSec, cVein, cDry;
                if (pal == 0) {
                    const QColor m[4] = {QColor(162, 38, 22), QColor(142, 54, 20), QColor(176, 46, 26), QColor(122, 34, 18)};
                    cMain = m[var]; cSec = QColor(202, 114, 28); cVein = QColor(192, 126, 46); cDry = QColor(48, 22, 14);
                } else if (pal == 1) {
                    const QColor m[4] = {QColor(204, 136, 26), QColor(184, 112, 20), QColor(218, 152, 34), QColor(162, 92, 18)};
                    cMain = m[var]; cSec = QColor(146, 70, 18); cVein = QColor(225, 175, 64); cDry = QColor(60, 36, 14);
                } else if (pal == 2) {
                    const QColor m[4] = {QColor(180, 102, 116), QColor(158, 86, 98), QColor(196, 118, 130), QColor(140, 76, 88)};
                    cMain = m[var]; cSec = QColor(138, 90, 78); cVein = QColor(212, 148, 158); cDry = QColor(58, 32, 38);
                } else {
                    const QColor m[4] = {QColor(88, 116, 44), QColor(74, 100, 36), QColor(106, 126, 54), QColor(64, 86, 32)};
                    cMain = m[var]; cSec = QColor(152, 116, 40); cVein = QColor(138, 158, 72); cDry = QColor(38, 32, 16);
                }

                QVector<float> hMap(texSz * texSz, 0.0f);
                QVector<float> aMap(texSz * texSz, 0.0f);
                QVector<float> v1Map(texSz * texSz, 0.0f);
                QVector<float> v2Map(texSz * texSz, 0.0f);
                QVector<float> edgeMap(texSz * texSz, 0.0f);

                for (int py = 0; py < texSz; ++py) {
                    for (int px = 0; px < texSz; ++px) {
                        int pIdx = py * texSz + px;
                        double u = (px - texSz * 0.5) / (texSz * 0.42);
                        double v = (py - texSz * 0.46) / (texSz * 0.42);

                        double baseY = (kind == 0) ? 0.38 : 0.46;
                        double dy = v - baseY;
                        double r = std::hypot(u, dy);
                        double theta = std::atan2(u, -dy);
                        double nEdge = fbm(u * 5.0 + 10.0, v * 5.0 + 10.0, seed);
                        double targetR = 0.0;
                        double v1 = 0.0, v2 = 0.0;

                        if (kind == 0) {
                            static const double rAng[5] = {0.0, -0.72, 0.72, -1.40, 1.40};
                            static const double rLen[5] = {0.96, 0.84, 0.84, 0.52, 0.52};
                            static const double rWid[5] = {0.39, 0.36, 0.36, 0.30, 0.30};

                            int bestL = -1;
                            for (int L = 0; L < 5; ++L) {
                                double w = std::abs(theta - rAng[L]) / rWid[L];
                                if (w < 1.15) {
                                    double mainShape = std::pow(std::max(0.0, 1.0 - w * 0.84), 0.58);
                                    double centerTooth = 0.14 * std::exp(-w * w * 22.0);
                                    double sideTooth   = 0.085 * std::exp(-std::pow(w - 0.48, 2.0) * 42.0);
                                    double serrate     = 0.016 * std::sin(w * 26.0) * std::max(0.0, 1.0 - w);
                                    double candR = rLen[L] * (mainShape + centerTooth + sideTooth + serrate) * (0.97 + 0.06 * (nEdge - 0.5));
                                    if (candR > targetR) {
                                        targetR = candR;
                                        bestL = L;
                                    }
                                }
                                double da = theta - rAng[L];
                                double ca = std::cos(da), sa = std::sin(da);
                                if (ca > 0.0 && r * ca < rLen[L] * 0.96) {
                                    double along = r * ca;
                                    double dRib  = std::abs(r * sa);
                                    double prog  = along / rLen[L];
                                    double ribW  = 0.018 * (1.0 - prog * 0.75);
                                    v1 = std::max(v1, std::clamp(1.0 - dRib / std::max(0.003, ribW), 0.0, 1.0));
                                }
                            }
                            if (bestL >= 0) {
                                double da = theta - rAng[bestL];
                                double along = r * std::cos(da);
                                double dRib  = std::abs(r * std::sin(da));
                                double prog  = std::clamp(along / rLen[bestL], 0.0, 1.0);
                                double branchCoord = (along - dRib * 0.62) * 15.0;
                                double bDist = std::abs(branchCoord - std::round(branchCoord));
                                v2 = std::clamp(1.0 - bDist / 0.13, 0.0, 1.0) * (1.0 - prog * 0.35) * std::clamp(1.0 - dRib * 2.8, 0.0, 1.0);
                            }
                        } else if (kind == 1) {
                            if (std::abs(theta) < 2.40) {
                                double env = std::pow(std::cos(theta * 0.60), 0.68) * (0.86 + 0.36 * std::cos(theta));
                                double lobes = 0.17 * std::sin(std::abs(theta) * 9.5 - 0.5) * std::sin(std::abs(theta) * 1.35);
                                targetR = (env + lobes) * (0.97 + 0.06 * (nEdge - 0.5));
                            }
                            double dMain = std::abs(u);
                            double prog = std::clamp((baseY - v) / 1.32, 0.0, 1.0);
                            double ribW = 0.020 * (1.0 - prog * 0.75);
                            if (v < baseY && v > -0.90) {
                                v1 = std::clamp(1.0 - dMain / std::max(0.003, ribW), 0.0, 1.0);
                                double sideC = (v - dMain * 0.62) * 13.0;
                                double dSide = std::abs(sideC - std::round(sideC));
                                v2 = std::clamp(1.0 - dSide / 0.13, 0.0, 1.0) * std::clamp(1.0 - dMain * 1.5, 0.0, 1.0);
                            }
                        } else {
                            if (std::abs(theta) < 2.42) {
                                double heart = (0.78 + 0.38 * std::cos(theta)) * std::pow(std::cos(theta * 0.58), 0.62);
                                double serrate = 0.036 * std::sin(std::abs(theta) * 34.0) + 0.018 * std::sin(std::abs(theta) * 68.0);
                                targetR = (heart + serrate) * (0.97 + 0.06 * (nEdge - 0.5));
                            }
                            double dMain = std::abs(u);
                            double prog = std::clamp((baseY - v) / 1.30, 0.0, 1.0);
                            double ribW = 0.018 * (1.0 - prog * 0.78);
                            if (v < baseY && v > -0.88) {
                                v1 = std::clamp(1.0 - dMain / std::max(0.003, ribW), 0.0, 1.0);
                                double sideC = (v - dMain * 0.68) * 18.0;
                                double dSide = std::abs(sideC - std::round(sideC));
                                v2 = std::clamp(1.0 - dSide / 0.14, 0.0, 1.0) * std::clamp(1.0 - dMain * 1.4, 0.0, 1.0);
                            }
                        }

                        if (r <= targetR && targetR > 0.02) {
                            double edgeDist = (targetR - r) / std::max(0.05, targetR);
                            double alpha = std::clamp((targetR - r) * 36.0, 0.0, 1.0);
                            double v3 = voronoiEdge(u * 26.0, v * 26.0, seed + 55) * 0.32;
                            double pillow = (1.0 - v1) * (1.0 - v2 * 0.65) * 0.24 + fbm(u * 8.5, v * 8.5, seed + 77) * 0.14;
                            double height = pillow + v1 * 0.78 + v2 * 0.46 + v3 * 0.22;

                            aMap[pIdx]    = float(alpha);
                            hMap[pIdx]    = float(height);
                            v1Map[pIdx]   = float(v1);
                            v2Map[pIdx]   = float(std::max(v2, v3));
                            edgeMap[pIdx] = float(edgeDist);
                        }
                    }
                }

                for (int py = 0; py < texSz; ++py) {
                    QRgb *scan = reinterpret_cast<QRgb *>(img.scanLine(py));
                    QRgb *impScan = (pal == 0) ? reinterpret_cast<QRgb *>(impImg.scanLine(py)) : nullptr;
                    for (int px = 0; px < texSz; ++px) {
                        int pIdx = py * texSz + px;
                        double u = (px - texSz * 0.5) / (texSz * 0.42);
                        double v = (py - texSz * 0.46) / (texSz * 0.42);
                        double baseY = (kind == 0) ? 0.38 : 0.46;

                        double maxStemV = baseY + 0.16;
                        double stemProg = std::clamp((v - baseY) / 0.16, 0.0, 1.0);
                        double stemCurve = std::sin((v - baseY) * 4.0) * 0.010;
                        double stemHalfW = 0.016 * (1.05 - stemProg * 0.45);
                        if (v >= baseY - 0.02 && v < maxStemV && std::abs(u - stemCurve) < stemHalfW && aMap[pIdx] < 0.4f) {
                            double nx = (u - stemCurve) / std::max(0.004, stemHalfW);
                            double nz = std::sqrt(std::max(0.0, 1.0 - nx * nx));
                            double stemLight = std::clamp(0.50 + 0.50 * (-nx * 0.6 + nz * 0.8), 0.30, 1.15);
                            int sr = std::clamp(int(cDry.red() * 1.3 * stemLight), 0, 255);
                            int sg = std::clamp(int(cDry.green() * 1.3 * stemLight), 0, 255);
                            int sb = std::clamp(int(cDry.blue() * 1.3 * stemLight), 0, 255);
                            scan[px] = qRgba(sr, sg, sb, 255);
                            continue;
                        }

                        double alpha = aMap[pIdx];
                        if (alpha <= 0.004) continue;

                        float hL = (px > 0) ? hMap[pIdx - 1] : hMap[pIdx];
                        float hR = (px + 1 < texSz) ? hMap[pIdx + 1] : hMap[pIdx];
                        float hU = (py > 0) ? hMap[pIdx - texSz] : hMap[pIdx];
                        float hD = (py + 1 < texSz) ? hMap[pIdx + texSz] : hMap[pIdx];
                        double nx = -(hR - hL) * 3.8 + u * 0.22;
                        double ny = -(hD - hU) * 3.8 + v * 0.22;
                        double nz = 1.0;
                        double nLen = std::sqrt(nx * nx + ny * ny + nz * nz);
                        nx /= nLen; ny /= nLen; nz /= nLen;

                        double lx = -0.46, ly = -0.56, lz = 0.68;
                        double ndotl = std::max(0.0, nx * lx + ny * ly + nz * lz);
                        double hx = lx, hy = ly, hz = lz + 1.0;
                        double hLen = std::sqrt(hx * hx + hy * hy + hz * hz);
                        double ndoth = std::max(0.0, (nx * hx + ny * hy + nz * hz) / hLen);
                        double spec = std::pow(ndoth, 24.0) * 0.22;
                        double bumpLight = 0.62 + 0.52 * ndotl;

                        double edgeDist = edgeMap[pIdx];
                        double v1 = v1Map[pIdx];
                        double v2 = v2Map[pIdx];
                        double nPigment = fbm(u * 3.6 + 4.0, v * 3.6 + 4.0, seed + 101);
                        double nSpots   = fbm(u * 12.0 + 2.0, v * 12.0 + 2.0, seed + 202);

                        double mixSec = std::clamp((nPigment - 0.36) * 2.0 + (1.0 - edgeDist) * 0.32, 0.0, 1.0);
                        double rCol = cMain.red()   * (1.0 - mixSec) + cSec.red()   * mixSec;
                        double gCol = cMain.green() * (1.0 - mixSec) + cSec.green() * mixSec;
                        double bCol = cMain.blue()  * (1.0 - mixSec) + cSec.blue()  * mixSec;

                        double veinMix = std::clamp(v1 * 0.70 + v2 * 0.34, 0.0, 0.82);
                        rCol = rCol * (1.0 - veinMix) + cVein.red()   * veinMix;
                        gCol = gCol * (1.0 - veinMix) + cVein.green() * veinMix;
                        bCol = bCol * (1.0 - veinMix) + cVein.blue()  * veinMix;

                        double dryEdge = std::pow(std::clamp(1.0 - edgeDist * 6.5, 0.0, 1.0), 1.6);
                        double spots   = std::pow(std::clamp((nSpots - 0.66) * 3.2, 0.0, 1.0), 1.6) * 0.75;
                        double darkMix = std::clamp(dryEdge * 0.72 + spots, 0.0, 0.85);
                        rCol = rCol * (1.0 - darkMix) + cDry.red()   * darkMix;
                        gCol = gCol * (1.0 - darkMix) + cDry.green() * darkMix;
                        bCol = bCol * (1.0 - darkMix) + cDry.blue()  * darkMix;

                        int outR = std::clamp(int((rCol * bumpLight + 240.0 * spec) * alpha), 0, int(255 * alpha));
                        int outG = std::clamp(int((gCol * bumpLight + 230.0 * spec) * alpha), 0, int(255 * alpha));
                        int outB = std::clamp(int((bCol * bumpLight + 210.0 * spec) * alpha), 0, int(255 * alpha));
                        int outA = std::clamp(int(255 * alpha), 0, 255);
                        scan[px] = qRgba(outR, outG, outB, outA);

                        // Маска отпечатка листа на запотевшем стекле (равномерный тонкий контур без скопления воды в верхней части)
                        if (impScan) {
                            // Истинная 2D-кромка листа одинаковой толщины со всех сторон
                            float aL = (px > 2) ? aMap[pIdx - 3] : 0.0f;
                            float aR = (px + 3 < texSz) ? aMap[pIdx + 3] : 0.0f;
                            float aU = (py > 2) ? aMap[pIdx - 3 * texSz] : 0.0f;
                            float aD = (py + 3 < texSz) ? aMap[pIdx + 3 * texSz] : 0.0f;
                            double minNeighborA = std::min({aL, aR, aU, aD});
                            double trueRim = alpha * std::clamp(1.0 - minNeighborA, 0.0, 1.0);

                            // Влага естественно уходит из верхней части листа (v < 0) чуть ниже (verticalDrain)
                            double verticalDrain = std::clamp(0.35 + 0.65 * ((v + 0.70) / 1.35), 0.30, 1.0);
                            double veinWet = (v1 * 0.70 + v2 * 0.35) * verticalDrain;
                            double wipeAlpha = alpha * (0.48 + 0.42 * std::max(trueRim * 0.8, veinWet)) * (0.78 + 0.22 * nPigment);
                            int iA = std::clamp(int(235.0 * wipeAlpha), 0, 235);
                            int iG = std::clamp(int(228.0 * wipeAlpha), 0, iA);
                            // Канал B держим деликатным (<= 36), чтобы в верхней части не образовывались выпуклые лужи воды
                            int iB = std::clamp(int((34.0 * trueRim + 26.0 * v1) * verticalDrain * wipeAlpha), 0, std::min(iA, 38));
                            impScan[px] = qRgba(0, iG, iB, iA);
                        }
                    }
                }
                s_leafTextures[idx] = img;
                if (pal == 0) s_leafImprintMasks[kind * 4 + var] = impImg;
            }
        }
    }
}

void AtmosphereCanvas::onTick() {
    if (m_masterCanvas) return;
    auto *mgr = AtmosphereManager::instance();
    if (!mgr || width() <= 10 || height() <= 10) return;
    qint64 nowMs = m_clock.elapsed();
    float dt = std::clamp(float(nowMs - m_lastTickMs) * 0.001f, 0.004f, 0.050f);
    m_lastTickMs = nowMs; m_timeSec += dt;
    if ((QGuiApplication::mouseButtons() != Qt::NoButton) || m_cursorPressed) m_idleSec = 0.0f;
    if (mgr->idleFogEnabled() && mgr->condensationEnabled()) {
        m_idleSec += dt;
        float timeoutSec = std::max(5.0f, float(mgr->idleFogTimeout()));
        float targetBoost = (m_idleSec >= timeoutSec) ? 1.0f : 0.0f;
        float rate = (targetBoost > m_idleFogBoost) ? (dt * 0.30f) : (dt * 0.85f);
        float nextBoost = (targetBoost > m_idleFogBoost)
            ? std::min(1.0f, m_idleFogBoost + rate)
            : std::max(0.0f, m_idleFogBoost - rate);
        if (std::abs(nextBoost - m_idleFogBoost) > 0.002f) {
            m_idleFogBoost = nextBoost;
            emit idleFogBoostChanged();
        }
    } else if (m_idleFogBoost > 0.001f) {
        m_idleSec = 0.0f;
        m_idleFogBoost = 0.0f;
        emit idleFogBoostChanged();
    }

    if (m_globalMode) {
        // Мгновенное обновление позиции курсора на каждом кадре (0 мс задержки отражения света!)
        static qint64 s_lastCurPollMs = 0;
        if (!m_cursorPressed && (mgr->snowEnabled() || mgr->leavesEnabled() || mgr->cursorWindEnabled() || mgr->idleFogEnabled()) && !m_fullscreenPaused && (nowMs - s_lastCurPollMs > 16)) {
            s_lastCurPollMs = nowMs;
            QJsonDocument cdoc = QJsonDocument::fromJson(queryHyprSocket("j/cursorpos").toUtf8());
            if (cdoc.isObject()) {
                QPointF monOff(0, 0);
                if (window() && window()->screen()) monOff = window()->screen()->geometry().topLeft();
                setCursorPos(QPointF(cdoc.object()["x"].toDouble() - monOff.x(),
                                     cdoc.object()["y"].toDouble() - monOff.y()));
            }
        }
        m_lastHyprPollMs = nowMs; pollHyprlandState(nowMs);
        if (m_fullscreenPaused) { update(); emit frameSimulated(); return; }
    } else {
        if (nowMs - m_lastObstacleScanMs > 160) {
            m_lastObstacleScanMs = nowMs;
            scanInAppObstacles(nowMs);
        } else {
            // Мгновенная синхронизация координат карточек на каждом кадре (0 мс отставания от анимаций!)
            for (auto it = m_obstacles.begin(); it != m_obstacles.end();) {
                QQuickItem *item = it.value().itemPtr.data();
                if (!item || !isCardInActivePage(item, this)) {
                    releaseObstacleAccumulations(it.value(), false);
                    it = m_obstacles.erase(it);
                } else {
                    if (it.key() == "window_glass") { it.value().rect = QRectF(2.0, 2.0, width() - 4.0, height() - 4.0); } else if (it.key() == "window_top_ledge") { it.value().rect = QRectF(8.0, 1.0, width() - 16.0, 18.0); } else if (!it.key().startsWith("sep_")) {
                        QRectF rawR = QRectF(item->mapToItem(this, QPointF(0, 0)), item->mapToItem(this, QPointF(item->width(), item->height()))).normalized();
                        // Сохраняем точное совпадение ширины и позиции с scanInAppObstacles (0.0px дрожания!)
                        it.value().rect = rawR;
                    }
                    ++it;
                }
            }
        }
    }

    // Плавное затухание порыва ветра (поддерживает и положительные, и отрицательные порывы!)
    if (m_gustBoost > 0.0f) m_gustBoost = std::max(0.0f, m_gustBoost - dt * 0.95f);
    else if (m_gustBoost < 0.0f) m_gustBoost = std::min(0.0f, m_gustBoost + dt * 0.95f);
    m_cursorVel *= 0.82;

    auto *rng = QRandomGenerator::global();

    // 1. ВРЕМЯ ОТ ВРЕМЕНИ ПОРЫВЫ ВЕТРА И ПЛАВНАЯ СМЕНА ЕГО НАПРАВЛЕНИЯ:
    m_windShiftTimer -= dt;
    if (m_windShiftTimer <= 0.0f) {
        m_windShiftTimer = 7.0f + (rng->bounded(110) * 0.1f); // Каждые 7..18 сек ветер меняет направление
        m_windTargetOffset = (rng->bounded(90) - 45) * 0.01f; // Сдвиг направления -0.45 .. +0.45
    }
    m_windDriftOffset += (m_windTargetOffset - m_windDriftOffset) * std::min(1.0f, dt * 0.65f);

    m_autoGustTimer -= dt;
    if (m_autoGustTimer <= 0.0f) {
        m_autoGustTimer = 9.0f + (rng->bounded(130) * 0.1f); // Каждые 9..22 сек спонтанный порыв ветра
        float gustDir = (m_windDriftOffset + float(mgr->windStrength()) >= 0.0f) ? 1.0f : -1.0f;
        if (rng->bounded(100) < 35) gustDir = -gustDir; // Иногда встречный порыв!
        m_gustBoost = gustDir * (0.42f + (rng->bounded(45) * 0.01f));
    }

    // 2. СИМУЛЯЦИЯ ГРОЗЫ И МОЛНИИ С РАЗНЫХ СТОРОН СВЕРХУ (слева, посередине, справа):
    if (mgr->rainEnabled() && mgr->thunderEnabled() && m_lightningPulseStage == 0) {
        m_thunderTimer -= dt;
        if (m_thunderTimer <= 0.0f) {
            m_thunderTimer = 14.0f + (rng->bounded(160) * 0.1f);
            int side = rng->bounded(3); // 0 = сверху-слева, 1 = сверху-посередине, 2 = сверху-справа
            m_lightningPosX = (side == 0) ? (0.14f + rng->bounded(10) * 0.01f)
                            : ((side == 1) ? (0.45f + rng->bounded(10) * 0.01f)
                                           : (0.76f + rng->bounded(12) * 0.01f));
            m_lightningPulseStage = 1;
            m_lightningStageTimer = 0.0f;
            m_lightningFlash = 0.85f;
            emit lightningFlashChanged();
        }
    }
    if (m_lightningPulseStage > 0) {
        m_lightningStageTimer += dt;
        if (m_lightningPulseStage == 1) {
            m_lightningFlash = std::max(0.0f, 0.88f - m_lightningStageTimer * 8.5f);
            if (m_lightningStageTimer >= 0.11f) {
                m_lightningPulseStage = 2;
                m_lightningStageTimer = 0.0f;
                // Вторая вспышка может чуть сместиться!
                m_lightningPosX = std::clamp(m_lightningPosX + (rng->bounded(30) - 15) * 0.01f, 0.12f, 0.88f);
                m_lightningFlash = 1.0f;
            }
            emit lightningFlashChanged();
        } else if (m_lightningPulseStage == 2) {
            m_lightningFlash = std::max(0.0f, 1.0f - m_lightningStageTimer * 2.6f);
            if (m_lightningStageTimer >= 0.16f) {
                m_lightningPulseStage = 3;
                for (auto &obs : m_obstacles) {
                    for (int d = obs.waterDrops.size() - 1; d >= 0; --d) {
                        auto &wd = obs.waterDrops[d];
                        if (wd.relY <= 1.2f && wd.sideWall == 0 && wd.mass > 1.15f && rng->bounded(100) < 35) {
                            wd.relY = 3.8f;
                            wd.vy = 20.0f + rng->bounded(18);
                            wd.driftSlope = ((rng->bounded(36) - 18) * 0.01f);
                            wd.tailDirX = wd.driftSlope;
                            wd.stretchFactor = 0.22f;
                            wd.pinchDistAcc = 0.0f;
                            wd.nextPinchTarget = 8.0f + (rng->bounded(60) * 0.1f);
                            wd.pauseTimer = 0.0f;
                        }
                    }
                }
            }
            emit lightningFlashChanged();
        } else if (m_lightningPulseStage == 3) {
            m_lightningFlash = std::max(0.0f, m_lightningFlash - dt * 2.4f);
            emit lightningFlashChanged();
            if (m_lightningFlash <= 0.005f) {
                m_lightningFlash = 0.0f;
                m_lightningPulseStage = 0;
            }
        }
    }

    auto countRunners = [](const QVector<SurfaceDrop> &v) { int n = 0; for (const auto &e : v) if (!e.pinned) ++n; return n; };
    auto countBeads   = [](const QVector<SurfaceDrop> &v) { int n = 0; for (const auto &e : v) if (e.pinned) ++n; return n; };
    auto spawnBead = [&](ObstacleSurface &obs, float rx, float ry, float dropSize, float rsc) {
        SurfaceDrop wd;
        wd.relX = wd.prevRelX = rx;
        wd.relY = std::max(6.0f, ry);
        wd.sideWall = 0;
        wd.spreadProg = 1.0f;
        wd.detachProg = 1.0f;
        wd.pinned = true;
        wd.mass = 0.65f + std::pow(rng->bounded(1000) * 0.001f, 1.4f) * 0.75f + dropSize * 0.10f;
        wd.releaseMass = 0.82f + rng->bounded(30) * 0.01f;
        wd.radius = std::clamp((2.90f + wd.mass * 1.65f) * rsc, 3.0f * rsc, 5.4f * rsc);
        wd.spreadX = wd.radius;
        wd.vy = 0.0f;
        wd.edgeAge = 0.0f;
        wd.maxEdgeLife = 0.22f + rng->bounded(65) * 0.01f; // Быстро переходит в стекание (0.22..0.87 сек)!
        wd.stretchFactor = 0.20f;
        wd.tailDirX = 0.0f;
        wd.driftSlope = 0.0f;
        obs.waterDrops.append(wd);
    };
    float af = std::clamp(float((width() * height()) / (1040.0 * 720.0)), 0.6f, 2.6f);
    bool allowRainAndWet = mgr->rainEnabled() || (mgr->wetGlassEnabled() && !mgr->snowEnabled()); int tRain = allowRainAndWet ? int((mgr->rainEnabled() ? mgr->rainIntensity() : std::max(0.55, mgr->rainIntensity())) * 140.0f * af) : 0;
    int tSnow = mgr->snowEnabled() ? int(mgr->snowIntensity() * 130.0f * af) : 0;
    int tLeaf = mgr->leavesEnabled() ? std::max(1, int(std::round(std::pow(float(mgr->leavesIntensity()), 1.45f) * 42.0f * std::min(1.3f, af)))) : 0;

    int cR = 0, cS = 0, cL = 0;
    for (int i = m_particles.size() - 1; i >= 0; --i) {
        auto &p = m_particles[i];
        if (p.type == FallingDrop) {
            if (!allowRainAndWet) m_particles.removeAt(i);
            continue;
        }
        if (p.type == Rain) { if (!allowRainAndWet || cR >= tRain + 12) { m_particles.removeAt(i); continue; } cR++; }
        else if (p.type == Snow) { if (!mgr->snowEnabled() || cS >= tSnow + 12) { m_particles.removeAt(i); continue; } cS++; }
        else { if (!mgr->leavesEnabled() || cL >= tLeaf) { if (m_draggedLeafIdx == i) m_draggedLeafIdx = -1; m_particles.removeAt(i); continue; } cL++; }
    }
    while (cR < tRain) { Particle p; spawnParticle(p, Rain, true); m_particles.append(p); cR++; }
    while (cS < tSnow) { Particle p; spawnParticle(p, Snow, true); m_particles.append(p); cS++; }
    while (cL < tLeaf) { Particle p; spawnParticle(p, Leaf, true); m_particles.append(p); cL++; }

    float wind = std::clamp(float(mgr->windStrength()) + m_windDriftOffset + m_gustBoost, -1.35f, 1.35f);
    float turb = float(mgr->windTurbulence()), grav = float(mgr->gravityScale());
    bool useCur = mgr->cursorWindEnabled() && m_cursorPos.x() > -50;

    bool qtLmb = ((QGuiApplication::mouseButtons() & Qt::LeftButton) != 0);
    bool lmbDown = m_cursorPressed || qtLmb;

    double detVal = mgr ? std::clamp(double(mgr->leavesDetail()), 0.0, 2.0) : 1.0;
    double rareBoostVal = (mgr && mgr->leavesIntensity() < 0.25) ? (1.0 + (0.25 - mgr->leavesIntensity()) * 1.4 * std::min(1.0, detVal)) : 1.0;
    float leafScaleMul = float((mgr ? mgr->leavesScale() : 1.0) * rareBoostVal);

    // Проверяем, есть ли уже захваченный мышкой лист
    int curDraggedIdx = -1;
    for (int i = 0; i < m_particles.size(); ++i) {
        if (m_particles[i].type == Leaf && m_particles[i].attachedObstacleId == QStringLiteral("__dragged__")) {
            curDraggedIdx = i;
            break;
        }
    }

    static float s_grabOffX = 0.0f, s_grabOffY = 0.0f;
    if (!lmbDown) {
        if (curDraggedIdx >= 0) {
            auto &dp = m_particles[curDraggedIdx];
            dp.attachedObstacleId.clear();
            dp.vx = std::clamp(dp.vx * 0.45f, -380.0f, 380.0f);
            dp.vy = std::clamp(dp.vy * 0.45f, -320.0f, 380.0f);
            dp.restTimer = -1.5f;
            curDraggedIdx = -1;
        }
        m_draggedLeafIdx = -1;
    } else if (curDraggedIdx < 0 && m_cursorPos.x() > -40) {
        float bestD2 = 1e9f;
        int bestIdx = -1;
        for (int i = 0; i < m_particles.size(); ++i) {
            auto &lp = m_particles[i];
            if (lp.type != Leaf) continue;
            // Захватывать можно только листья на переднем плане (depth >= 0.52) или прилипшие к окнам/экрану!
            if (lp.depth < 0.52f && !lp.resting) continue;
            float hitR = std::max(64.0f, lp.size * 2.10f * leafScaleMul);
            float dx = lp.x - float(m_cursorPos.x());
            float dy = lp.y - float(m_cursorPos.y());
            float d2 = dx * dx + dy * dy;
            if (d2 <= hitR * hitR && d2 < bestD2) {
                bestD2 = d2;
                bestIdx = i;
            }
        }
        if (bestIdx >= 0) {
            curDraggedIdx = bestIdx;
            auto &grabbed = m_particles[bestIdx];
            s_grabOffX = grabbed.x - float(m_cursorPos.x());
            s_grabOffY = grabbed.y - float(m_cursorPos.y());
            // Если лист был прилипшим к окну или экрану — он сразу отлипает и смазывает свой отпечаток!
            if (grabbed.resting) {
                grabbed.resting = false;
                grabbed.restTimer = -2.2f;
                int impListIdx = m_globalMode ? 1 : 0;
                int myTag = bestIdx + 1000;
                for (auto &imp : s_leafImprints[impListIdx]) {
                    if (!imp.released && imp.ownerTag == myTag) {
                        imp.x2 = imp.x1 + 12.0f;
                        imp.y2 = imp.y1 + 14.0f;
                        imp.released = true;
                        imp.ownerTag = -1;
                    }
                }
            }
            grabbed.attachedObstacleId = QStringLiteral("__dragged__");
            grabbed.depth = std::max(0.70f, grabbed.depth);
        }
    }
    m_draggedLeafIdx = curDraggedIdx;

    // В глобальном оверлее держим маску ввода вокруг каждого переднего/прилипшего листа (0 мс задержки при клике!),
    // а во время перетаскивания — на весь экран!
    if (false && m_globalMode && window()) {
        if (curDraggedIdx >= 0) {
            window()->setMask(QRegion(0, 0, int(width()), int(height())));
        } else if (mgr->leavesEnabled() && m_cursorPos.x() > -40) {
            QRegion leafMaskReg;
            for (const auto &lp : m_particles) {
                if (lp.type == Leaf && (lp.depth >= 0.52f || lp.resting)) {
                    int r = std::max(44, int(lp.size * 1.65f * leafScaleMul));
                    float dx = lp.x - float(m_cursorPos.x());
                    float dy = lp.y - float(m_cursorPos.y());
                    if (dx * dx + dy * dy <= float(r + 28) * float(r + 28)) {
                        leafMaskReg += QRegion(int(lp.x) - r, int(lp.y) - r, r * 2, r * 2);
                    }
                }
            }
            if (leafMaskReg.isEmpty()) leafMaskReg = QRegion(-100, -100, 1, 1);
            window()->setMask(leafMaskReg);
        } else {
            window()->setMask(QRegion(-100, -100, 1, 1));
        }
    }

    for (int i = m_particles.size() - 1; i >= 0; --i) {
        auto &p = m_particles[i];

        // Если этот лист сейчас перетаскивается мышкой — плавно ведём его за курсором с 3D-наклоном!
        if (p.type == Leaf && p.attachedObstacleId == QStringLiteral("__dragged__")) {
            p.resting = false;
            p.restTimer = -2.0f;
            p.depth = std::max(0.70f, p.depth);
            float targetX = float(m_cursorPos.x());
            float targetY = float(m_cursorPos.y());
            float newVx = (targetX - p.x) / std::max(0.005f, dt);
            float newVy = (targetY - p.y) / std::max(0.005f, dt);
            p.vx = p.vx * 0.35f + newVx * 0.65f;
            p.vy = p.vy * 0.35f + newVy * 0.65f;
            p.x = targetX;
            p.y = targetY;
            float dragTilt = std::clamp(p.vx * 0.14f, -55.0f, 55.0f);
            p.angle += (dragTilt - p.angle) * std::min(1.0f, 10.0f * dt);
            p.tiltPhase += dt * (1.2f + std::hypot(p.vx, p.vy) * 0.004f);
            float dragBend = std::clamp(-p.vx * 0.0016f, -0.55f, 0.55f);
            p.bendAmount += (dragBend - p.bendAmount) * std::min(1.0f, 10.0f * dt);
            continue;
        }

        // Таймер иммунитета после отлипания (чтобы лист не застревал повторно на том же самом Waybar/блоке!)
        if (!p.resting && p.restTimer < 0.0f) {
            p.restTimer = std::min(0.0f, p.restTimer + dt);
        }

        if (p.resting) {
            if (!m_obstacles.contains(p.attachedObstacleId) || m_obstacles[p.attachedObstacleId].isWaybar) { p.resting = false; p.attachedObstacleId.clear(); continue; }
            const auto &obs = m_obstacles[p.attachedObstacleId];
            p.restTimer -= dt;

            if (p.type == Snow) {
                if (!mgr->snowStickMeltEnabled() || !mgr->snowEnabled()) {
                    p.resting = false;
                    p.attachedObstacleId.clear();
                    continue;
                }
                // Прилипшая к окну или экрану снежинка тает и переходит в каплю, которая чуть стекает, подсыхает и оставляет след!
                float meltFrac = 1.0f - std::clamp(p.restTimer / std::max(0.5f, p.bendAmount), 0.0f, 1.0f);
                p.x = obs.rect.left() + p.relX * obs.rect.width();
                p.y = obs.rect.top() + p.relY + meltFrac * 3.2f;
                if (p.restTimer <= 0.0f) {
                    auto &mutObs = m_obstacles[p.attachedObstacleId];
                    int maxFrozen = (mutObs.id == "window_glass") ? 4 : (mutObs.isWindow ? 3 : 1);
                    int frozenDropsCount = 0;
                    int oldestFrozenIdx = -1;
                    for (int k = 0; k < mutObs.waterDrops.size(); ++k) {
                        if (mutObs.waterDrops[k].pinned && mutObs.waterDrops[k].releaseMass > 500.0f) {
                            if (oldestFrozenIdx < 0) oldestFrozenIdx = k;
                            ++frozenDropsCount;
                        }
                    }
                    if (frozenDropsCount >= maxFrozen && oldestFrozenIdx >= 0) {
                        mutObs.waterDrops.removeAt(oldestFrozenIdx);
                    }
                    if (mutObs.waterDrops.size() < 10) {
                        SurfaceDrop wd;
                        wd.relX = wd.prevRelX = std::clamp(p.relX, 0.03f, 0.97f);
                        wd.relY = std::max(5.0f, p.relY + 2.4f);
                        wd.sideWall = 0;
                        wd.spreadProg = 1.0f;
                        wd.detachProg = 1.0f;
                        wd.pinned = false;
                        wd.mass = 0.45f + (rng->bounded(20) * 0.01f);
                        wd.releaseMass = 0.40f;
                        wd.radius = std::clamp(1.85f + wd.mass * 0.95f, 1.95f, 2.65f);
                        wd.spreadX = wd.radius;
                        wd.vy = (11.0f + rng->bounded(7)) * float(mgr->dropSlideSpeed());
                        wd.driftSlope = ((rng->bounded(16) - 8) * 0.01f) + wind * 0.02f;
                        wd.tailDirX = wd.driftSlope;
                        wd.stretchFactor = 0.18f;
                        wd.pinchDistAcc = 0.0f;
                        wd.nextPinchTarget = 3.2f + (rng->bounded(15) * 0.1f);
                        wd.pauseTimer = 0.0f;
                        wd.edgeAge = 0.0f;
                        wd.maxEdgeLife = -1.0f; // Стекает совсем чуть-чуть (6..14px) и замерзает на месте!
                        mutObs.waterDrops.append(wd);
                    }
                    spawnParticle(p, Snow, false);
                }
                continue;
            }



            if (p.relY > 0.5f) {
                // Лист ПРИЛИП К ЭКРАНУ / ОКНУ (без покачивания и дрожания перед отрывом!)
                int impListIdx = m_globalMode ? 1 : 0;
                int myTag = i + 1000;
                if (mgr->rainEnabled()) p.relY += (1.2f + p.depth * 1.0f) * dt;

                // Сначала (при restTimer < 0.85) лист отгибается сверху от стекла, а за последние 0.35 сек срывается вниз, смазывая отпечаток!
                if (p.restTimer < 0.35f) {
                    float slideDir = (wind >= 0.0f) ? 1.0f : -1.0f;
                    p.relX += (slideDir * 32.0f + wind * 45.0f) * dt / std::max(100.0, obs.rect.width());
                    p.relY += 42.0f * dt;
                }
                p.x = obs.rect.left() + p.relX * obs.rect.width();
                p.y = obs.rect.top() + p.relY;

                p.tiltPhase *= std::pow(0.15f, dt);
                p.bendAmount *= std::pow(0.20f, dt);

                double det = mgr ? std::clamp(double(mgr->leavesDetail()), 0.0, 2.0) : 1.0;
                double rareBoost = (mgr && mgr->leavesIntensity() < 0.25) ? (1.0 + (0.25 - mgr->leavesIntensity()) * 1.4 * std::min(1.0, det)) : 1.0;
                float curDrawSz = float(p.size * 2.35 * (mgr ? mgr->leavesScale() : 1.0) * rareBoost);
                int varIdx = int(std::abs(p.tiltSpeed * 10.0f)) % 4;

                bool foundImp = false;
                for (auto &imp : s_leafImprints[impListIdx]) {
                    if (!imp.released && imp.ownerTag == myTag) {
                        imp.x2 = p.x; imp.y2 = p.y; imp.angle2 = p.angle;
                        imp.drawSz = curDrawSz;
                        imp.life = imp.maxLife;
                        foundImp = true;
                        break;
                    }
                }
                if (!foundImp && mgr->condensationEnabled()) {
                    if (s_leafImprints[impListIdx].size() >= 24) s_leafImprints[impListIdx].removeFirst();
                    LeafImprint imp;
                    imp.ownerTag = myTag;
                    imp.kind = std::clamp(p.leafKind, 0, 2);
                    imp.var  = varIdx;
                    imp.x1 = imp.x2 = p.x;
                    imp.y1 = imp.y2 = p.y;
                    imp.angle1 = imp.angle2 = p.angle;
                    imp.drawSz = curDrawSz;
                    imp.life = imp.maxLife = 15.0f;
                    imp.released = false;
                    s_leafImprints[impListIdx].append(imp);
                }

                bool gustPeel = (std::abs(m_gustBoost) > 0.55f && rng->bounded(100) < 8);
                if (p.restTimer <= 0.0f || gustPeel || p.relY >= obs.rect.height() - 4.0f) {
                    p.resting = false;
                    p.attachedObstacleId.clear();
                    p.restTimer = -2.2f;
                    p.vx = wind * 95.0f + (rng->bounded(60) - 30);
                    p.vy = 48.0f;
                    p.tiltPhase = 1.15f; // Продолжает наклон после верхнего отгиба в плавное пикирование вниз
                    p.bendAmount = 0.35f;
                    p.swayPhase = (p.vx >= 0.0f) ? 0.35f : 2.79f;
                    // Фиксируем финальный смаз отпечатка в сторону отрыва листа
                    for (auto &imp : s_leafImprints[impListIdx]) {
                        if (!imp.released && imp.ownerTag == myTag) {
                            if (std::hypot(imp.x2 - imp.x1, imp.y2 - imp.y1) < 8.0f) {
                                imp.x2 = imp.x1 + std::clamp(p.vx * 0.14f, -22.0f, 22.0f);
                                imp.y2 = imp.y1 + 14.0f;
                                imp.angle2 = imp.angle1 + (p.vx >= 0.0f ? 8.0f : -8.0f);
                            }
                            imp.released = true;
                            imp.ownerTag = -1;
                        }
                    }
                }
            } else {
                // Лист падает на грань или угол окна/блока и плавно перегибается через ребро (bendAmount: 0 -> 1)
                bool onLeftCorner  = (p.relX < 0.12f);
                bool onRightCorner = (p.relX > 0.88f);
                float cornerSlide  = onLeftCorner ? -0.028f : (onRightCorner ? 0.028f : 0.0f);
                p.relX += (wind * 0.015f + cornerSlide) * dt;
                p.x = obs.rect.left() + p.relX * obs.rect.width();
                p.y = obs.rect.top() + 0.5f;

                // Лист на грани не реагирует на наведение мыши (только на зажатие ЛКМ)

                float targetEdgeBend = 0.88f + std::sin(m_timeSec * 2.6f + p.relX * 15.0f) * 0.12f;
                p.bendAmount += (targetEdgeBend - p.bendAmount) * std::min(1.0f, 5.2f * dt);

                float cornerAngle = onLeftCorner ? -38.0f : (onRightCorner ? 38.0f : 0.0f);
                p.angle += (cornerAngle - p.angle) * std::min(1.0f, 6.0f * dt);

                if (p.restTimer <= 0.0f || p.relX < 0.01f || p.relX > 0.99f) {
                    p.resting = false;
                    p.attachedObstacleId.clear();
                    p.restTimer = -2.2f;
                    p.vx = (onLeftCorner ? -65.0f : (onRightCorner ? 65.0f : wind * 110.0f)) + (rng->bounded(60) - 30);
                    p.vy = 38.0f;
                }
            }
            continue;
        }
        float prevY = p.y;
        if (p.type == Rain) {
            float par = 0.62f + 0.38f * p.depth;
            float steadyWind = wind + (p.depth - 0.6f) * 0.08f * turb;
            p.vx += (steadyWind * 185.0f * par - p.vx) * std::min(1.0f, 2.2f * dt);
            float termTarget = (p.termV > 1.0f ? p.termV : 560.0f) * grav;
            p.vy += (termTarget - p.vy) * std::min(1.0f, 4.5f * dt);
        } else if (p.type == FallingDrop) {
            p.vx = p.vx * std::pow(0.90f, dt * 60.0f);
            p.vy = std::min(520.0f * grav, p.vy + 420.0f * grav * dt);
        } else if (p.type == Snow) {
            float spdMul = (p.termV > 0.1f) ? p.termV : 1.0f;
            p.swayPhase += dt * (1.4f + 0.9f * spdMul);
            p.tiltPhase += dt * p.tiltSpeed;
            p.angle += (p.angVel + std::sin(p.swayPhase) * 25.0f) * dt;
            float swayAmp = (p.leafKind == 8) ? (18.0f + 22.0f * turb) : (34.0f + 45.0f * turb);
            float targetVx = wind * ((p.leafKind == 8) ? 55.0f : 95.0f) + std::sin(p.swayPhase) * swayAmp;
            float targetVy = (44.0f + p.depth * 56.0f) * spdMul * grav + std::cos(p.swayPhase * 1.3f) * 8.0f;
            p.vx += (targetVx - p.vx) * std::min(1.0f, 3.8f * dt);
            p.vy += (targetVy - p.vy) * std::min(1.0f, 3.2f * dt);
        } else {
            // Плавное планирование листа («маятник») без высокочастотной тряски
            p.swayPhase += dt * (1.35f + 0.35f * p.depth);
            float swingPos = std::sin(p.swayPhase);
            float swingVel = std::cos(p.swayPhase);

            float glideAmp = (115.0f + 80.0f * turb) * (0.75f + 0.30f * p.depth);
            float targetVx = wind * 130.0f + swingVel * glideAmp;
            p.vx += (targetVx - p.vx) * std::min(1.0f, 4.2f * dt);

            float baseVy = (36.0f + p.depth * 28.0f) * std::clamp(grav, 0.4f, 2.0f);
            float swoopArc = (1.0f - swingPos * swingPos) * 38.0f - (swingPos * swingPos) * 18.0f;
            float targetVy = std::max(10.0f, baseVy + swoopArc);
            p.vy += (targetVy - p.vy) * std::min(1.0f, 4.5f * dt);

            float targetAngle = -swingPos * 42.0f + swingVel * 16.0f + wind * 24.0f;
            p.angle += ((targetAngle - p.angle) * 3.0f + p.angVel * 0.18f) * dt;
            p.tiltPhase += dt * p.tiltSpeed * 0.55f;

            // Плавный упругий изгиб листа вдоль траектории качели
            float aeroBend = std::clamp(-swingVel * 0.45f, -0.55f, 0.55f);
            p.bendAmount += (aeroBend - p.bendAmount) * std::min(1.0f, 4.0f * dt);

            // Плавный перелёт листа между передним (depth > 0.52) и задним (depth < 0.52) планом:
            // Сквозь окно пролететь нельзя (окно на плоскости depth = 0.52 держит лист спереди или сзади),
            // а в свободном месте между окнами лист свободно уходит на задний план и возвращается на передний!
            float vz = std::sin(p.swayPhase * 0.38f + p.tiltSpeed * 2.5f) * 0.14f;
            float nextDepth = std::clamp(p.depth + vz * dt, 0.26f, 0.96f);
            float leafRad = p.size * 1.15f * float(mgr->leavesScale());
            bool overSolidWindow = false;
            for (const auto &obs : m_obstacles) {
                if (obs.id == "window_glass" || obs.id == "window_top_ledge" || obs.isWaybar) continue;
                if (obs.rect.adjusted(-leafRad * 0.45, -leafRad * 0.45, leafRad * 0.45, leafRad * 0.45).contains(QPointF(p.x, p.y))) {
                    overSolidWindow = true;
                    break;
                }
            }
            if (overSolidWindow) {
                if (p.depth >= 0.52f) nextDepth = std::max(0.55f, nextDepth);
                else                  nextDepth = std::min(0.49f, nextDepth);
            }
            p.depth = nextDepth;
        }

        if (useCur && p.type == Snow) {
            float dx = p.x - float(m_cursorPos.x()), dy = p.y - float(m_cursorPos.y()), d2 = dx * dx + dy * dy;
            if (d2 < 9500.0f && d2 > 4.0f) {
                float d = std::sqrt(d2), atten = (1.0f - d / 97.5f);
                float push = atten * 280.0f;
                p.vx += (dx / d) * push * dt * 8.0f;
                p.vy += (dy / d) * push * dt * 4.0f;
            }
        }
        p.x += p.vx * dt; p.y += p.vy * dt;

        if (p.depth >= 0.52f && !m_obstacles.isEmpty()) {
            bool hit = false;
            bool removed = false;
            bool syncSkipWindowGlass = false;
            float ovDropMul = float(mgr->overlayDropsIntensity());

            auto countActiveSlides = [](const QVector<SurfaceDrop> &v) {
                int n = 0;
                for (const auto &e : v) {
                    if (!e.pinned && (e.relY > 1.2f || e.sideWall != 0 || e.detachProg > 0.0f)) ++n;
                }
                return n;
            };
            auto countEdgeDrops = [](const QVector<SurfaceDrop> &v) {
                int n = 0;
                for (const auto &e : v) {
                    if (!e.pinned && e.relY <= 1.2f && e.sideWall == 0 && e.detachProg <= 0.0f) ++n;
                }
                return n;
            };

            for (auto &obs : m_obstacles) {
                const QRectF &r = obs.rect;
                if (obs.id == "window_glass" && m_globalMode) {
                    if (syncSkipWindowGlass) continue;
                    // Прилипание снежинок к боковым третям экрана с плавным таянием в маленькую замерзающую капельку
                    float meltMul = mgr->snowStickMeltEnabled() ? float(mgr->snowMeltAmount()) : 0.0f;
                    if (p.type == Snow && meltMul > 0.02f && p.depth >= 0.58f && (p.x <= r.left() + r.width() * 0.32 || p.x >= r.right() - r.width() * 0.32) &&
                        p.y > 40.0f && p.y < height() - 50.0f && rng->bounded(2500) < int(4.0f * meltMul)) {
                        int stuckSnow = 0;
                        for (const auto &other : m_particles) {
                            if (other.type == Snow && other.resting && other.attachedObstacleId == "window_glass") ++stuckSnow;
                        }
                        if (stuckSnow < std::clamp(int(3.0f * meltMul), 1, 6)) {
                            p.resting = true;
                            p.attachedObstacleId = "window_glass";
                            p.relX = std::clamp(float((p.x - r.left()) / r.width()), 0.02f, 0.98f);
                            p.relY = float(p.y - r.top());
                            p.restTimer = 2.6f + rng->bounded(28) * 0.1f;
                            p.bendAmount = p.restTimer; // Запоминаем начальное время для расчёта фазы таяния (0 -> 1)
                            hit = true; break;
                        }
                    }
                    // Прилипание листа к самому экрану (оставляет отпечаток на запотевшем стекле и смазывает его при взлёте)
                    int stickChance = (tLeaf <= 6) ? 14 : 5;
                    if (p.type == Leaf && p.restTimer >= 0.0f && p.depth >= 0.52f && p.y > 45.0f && p.y < height() - 65.0f && rng->bounded(1000) < stickChance) {
                        int scrLeaves = 0;
                        for (const auto &other : m_particles) {
                            if (other.type == Leaf && other.resting && other.attachedObstacleId == "window_glass") ++scrLeaves;
                        }
                        if (scrLeaves < 4) {
                            p.resting = true;
                            p.attachedObstacleId = "window_glass";
                            p.relX = std::clamp(float((p.x - r.left()) / r.width()), 0.04f, 0.96f);
                            p.relY = float(p.y - r.top());
                            p.restTimer = 2.6f + rng->bounded(28) * 0.1f;
                            hit = true; break;
                        }
                    }
                    float scrMul = ovDropMul * float(mgr->screenDropsAmount());
                    if (scrMul <= 0.02f) continue;
                    int maxScrSlides = std::clamp(int(6 * scrMul), 1, 16);
                    int maxScrBeads  = std::clamp(int(8 * scrMul), 1, 20);
                    float nx = (p.x / std::max(1.0, width())) * 2.0f - 1.0f;
                    float ny = (p.y / std::max(1.0, height())) * 2.0f - 1.0f;
                    float cornerFactor = std::clamp(std::sqrt((nx * nx + ny * ny) * 0.5f), 0.0f, 1.0f);
                    int scrChance = int(((!m_globalMode ? 48.0f : 14.0f) + cornerFactor * 28.0f) * scrMul);
                    bool inSideThirds = !m_globalMode || (p.x <= r.left() + r.width() * 0.333) || (p.x >= r.right() - r.width() * 0.333);
                    if (inSideThirds && (p.type == Rain || p.type == FallingDrop) && mgr->wetGlassEnabled() &&
                        (countActiveSlides(obs.waterDrops) < maxScrSlides || countBeads(obs.waterDrops) < maxScrBeads) &&
                        rng->bounded(10000) < scrChance && p.y > 10.0f && p.y < height() * 0.84f &&
                        p.x > r.left() + 24.0 && p.x < r.right() - 24.0) {
                        float landRx = std::clamp(float((p.x - r.left()) / r.width()), 0.025f, 0.975f);
                        bool winBead = countBeads(obs.waterDrops) < maxScrBeads && (countActiveSlides(obs.waterDrops) >= maxScrSlides || rng->bounded(100) < 24);
                        if (winBead) { spawnBead(obs, landRx, p.y, p.size, 1.10f); continue; }
                        if (countActiveSlides(obs.waterDrops) >= maxScrSlides) continue;
                        SurfaceDrop wd;
                        wd.relX = wd.prevRelX = landRx;
                        wd.relY = std::max(4.0f, p.y);
                        wd.sideWall = 0; wd.spreadProg = 1.0f; wd.detachProg = 1.0f;
                        wd.mass = 1.18f + (rng->bounded(120) * 0.01f) * p.size;
                        wd.radius = std::clamp(3.80f + wd.mass * 1.85f, 4.2f, 7.6f);
                        wd.spreadX = wd.radius;
                        wd.vy = (18.0f + wd.mass * 6.0f) * float(mgr->dropSlideSpeed());
                        wd.driftSlope = ((rng->bounded(32) - 16) * 0.01f) + wind * 0.05f;
                        wd.tailDirX = wd.driftSlope; wd.stretchFactor = 0.25f;
                        wd.pinchDistAcc = 0.0f; wd.nextPinchTarget = 11.0f + (rng->bounded(90) * 0.1f);
                        wd.pauseTimer = 0.0f;
                        obs.waterDrops.append(wd);
                    }
                    if (p.type == Leaf && p.restTimer >= 0.0f && p.depth >= 0.65f && p.y > 40.0f && p.y < height() - 60.0f && rng->bounded(1000) < 3) {
                        int scrLeaves = 0;
                        for (const auto &other : m_particles) {
                            if (other.type == Leaf && other.resting && other.attachedObstacleId == "window_glass") ++scrLeaves;
                        }
                        if (scrLeaves < 4) {
                            p.resting = true;
                            p.attachedObstacleId = "window_glass";
                            p.relX = std::clamp(float((p.x - r.left()) / r.width()), 0.04f, 0.96f);
                            p.relY = float(p.y - r.top());
                            p.restTimer = 4.5f + rng->bounded(70) * 0.1f;
                            hit = true; break;
                        }
                    }
                    continue;
                }

                // 1) Удар о верхнюю горизонтальную плоскость карточки или открытого окна:
                // Снег НЕ копится на Waybar, а снежинки с depth > 0.72 свободно пролетают ПЕРЕД окнами!
                bool inCollisionLayer = (p.type == Snow)
                    ? (p.leafKind < 8 && p.depth >= 0.52f && p.depth <= 0.72f)
                    : ((p.depth >= 0.52f && p.depth <= 0.78f) || (p.type == FallingDrop) || (p.type == Leaf && p.depth >= 0.52f));
                bool passWaybar = obs.isWaybar && (((p.type == Snow) && (((int(std::abs(p.x) * 17) + int(p.depth * 100)) % 100) < 62)) || (p.type == Rain && (((int(std::abs(p.x) * 13) + int(p.depth * 100)) % 100) < 78)));
                bool hitTopEdge = inCollisionLayer && !passWaybar && (p.x >= r.left() - 1 && p.x <= r.right() + 1 && prevY <= r.top() + 8 && p.y >= r.top() - 2 && p.y <= r.top() + 26);

                if (hitTopEdge) {
                    float rx = std::clamp(float((p.x - r.left()) / r.width()), 0.0f, 1.0f);
                    if (p.type == Rain || p.type == FallingDrop) {
                        int splashCount = 2 + rng->bounded(2);
                        if (m_splashes.size() < 140) {
                            for (int s = 0; s < splashCount; ++s) {
                                SplashDrop sp;
                                sp.x = p.x;
                                sp.y = float(r.top()) - 0.5f;
                                float dirSign = (s % 2 == 0) ? 1.0f : -1.0f;
                                float randSize = 0.85f + std::pow(rng->bounded(1000) * 0.001f, 1.4f) * 1.15f;
                                sp.radius = randSize;
                                sp.vx = dirSign * (20.0f + rng->bounded(65)) + p.vx * 0.05f;
                                sp.vy = -(28.0f + rng->bounded(58));
                                sp.life = sp.maxLife = 0.22f + (rng->bounded(12) * 0.01f);
                                sp.targetY = -1.0f;
                                m_splashes.append(sp);
                            }
                        }

                        if (mgr->wetGlassEnabled() && r.height() >= 18.0) {
                            float clampedRx = std::clamp(rx, 0.03f, 0.97f);
                            if (!obs.wetFilmBins.isEmpty()) {
                                int bc = obs.wetFilmBins.size();
                                int bi = std::clamp(int(clampedRx * bc), 0, bc - 1);
                                float addFilm = 0.052f * p.size;
                                obs.wetFilmBins[bi] = std::min(0.68f, obs.wetFilmBins[bi] + addFilm);
                                if (bi > 0) obs.wetFilmBins[bi - 1] = std::min(0.68f, obs.wetFilmBins[bi - 1] + addFilm * 0.72f);
                                if (bi + 1 < bc) obs.wetFilmBins[bi + 1] = std::min(0.68f, obs.wetFilmBins[bi + 1] + addFilm * 0.72f);
                                if (bi > 1) obs.wetFilmBins[bi - 2] = std::min(0.68f, obs.wetFilmBins[bi - 2] + addFilm * 0.38f);
                                if (bi + 2 < bc) obs.wetFilmBins[bi + 2] = std::min(0.68f, obs.wetFilmBins[bi + 2] + addFilm * 0.38f);
                            }

                            bool absorbedByEdge = false;
                            for (auto &existing : obs.waterDrops) {
                                if (!existing.pinned && existing.relY <= 1.2f && existing.sideWall == 0 && existing.detachProg <= 0.0f) {
                                    if (std::abs(existing.relX - clampedRx) * r.width() < 26.0) {
                                        existing.mass = std::min(3.2f, existing.mass + (0.18f + 0.14f * p.size) * ovDropMul);
                                        existing.radius = std::clamp(2.40f + existing.mass * 1.20f, 2.50f, 4.80f);
                                        existing.spreadX = existing.radius;
                                        existing.spreadProg = std::min(1.0f, existing.spreadProg + 0.25f);
                                        absorbedByEdge = true;
                                        break;
                                    }
                                }
                            }

                            int maxEdgeCount = obs.isWaybar ? std::clamp(int(r.width() / 220.0) + 1, 1, 2) : (obs.isWindow ? std::clamp(int(r.width() / 280.0) + 1, 1, 3) : std::clamp(int(r.width() / 150.0) + 1, 2, 5));
                            if (!absorbedByEdge && countEdgeDrops(obs.waterDrops) < maxEdgeCount && rng->bounded(100) < (obs.isWaybar ? 18 : 28)) {
                                SurfaceDrop wd;
                                wd.relX = wd.prevRelX = clampedRx;
                                wd.relY = obs.isWaybar ? 3.5f : 0.35f;
                                wd.sideWall = 0;
                                wd.spreadProg = 1.0f;
                                wd.detachProg = obs.isWaybar ? 1.0f : 0.0f;
                                wd.pinned = false;
                                wd.mass = 0.75f + (rng->bounded(45) * 0.01f) * p.size;
                                wd.releaseMass = 0.92f + (rng->bounded(35) * 0.01f);
                                wd.radius = obs.isWaybar ? std::clamp(3.10f + wd.mass * 1.35f, 3.2f, 5.4f) : (2.40f + wd.mass * 1.15f);
                                wd.spreadX = wd.radius;
                                wd.vy = obs.isWaybar ? (6.5f + wd.mass * 2.0f) : 0.0f;
                                wd.driftSlope = obs.isWaybar ? (((rng->bounded(24) - 12) * 0.01f) + wind * 0.04f) : 0.0f;
                                wd.tailDirX = wd.driftSlope;
                                wd.stretchFactor = 0.25f;
                                wd.nextPinchTarget = 6.0f + rng->bounded(40) * 0.1f;
                                wd.edgeAge = 0.0f;
                                wd.maxEdgeLife = 0.45f + (rng->bounded(80) * 0.01f);
                                obs.waterDrops.append(wd);
                            }
                        }

                        if (p.type == FallingDrop) {
                            m_particles.removeAt(i);
                            removed = true;
                        } else {
                            spawnParticle(p, Rain, false);
                        }
                        hit = true; break;
                    } else if (p.type == Snow) {
                        if (mgr->snowDriftEnabled() && !obs.snowBins.isEmpty()) {
                            int bc = obs.snowBins.size(), idx = std::clamp(int(rx * bc), 0, bc - 1);
                            float dScale = float(mgr->snowDriftScale());
                            uint hSeed = qHash(obs.id);
                            float phase1 = float(hSeed & 0xFF) * 0.024f + obs.snowPhaseShift;
                            float phase2 = float((hSeed >> 8) & 0xFF) * 0.024f - obs.snowPhaseShift * 0.7f;
                            float wbMaxH = std::min(4.6f * std::min(1.25f, dScale), float(std::max(1.5, obs.rect.top() - 3.2)));
                            float baseMax = obs.isWaybar ? wbMaxH : ((obs.isWindow ? 20.0f : 15.0f) * dScale);
                            for (int k = -2; k <= 2; ++k) {
                                int bi = idx + k;
                                if (bi < 0 || bi >= bc) continue;
                                float uBin = (bi + 0.5f) / float(bc);
                                float edgeTaper = std::pow(std::sin(std::clamp(uBin, 0.0f, 1.0f) * 3.14159f), 0.70f);
                                // Отдельные кучки снега (между кучками высота стремится к 0, чтобы не было сплошной полосы!)
                                float w1 = std::sin(uBin * 14.0f + phase1);
                                float w2 = std::cos(uBin * 8.5f - phase2);
                                float moundMask = std::clamp(w1 * 0.60f + w2 * 0.50f + 0.12f, 0.0f, 1.0f);
                                float hillWave = std::pow(moundMask, 1.75f) * 1.40f;
                                float localCap = baseMax * edgeTaper * hillWave;
                                // Заметное, но плавное накопление кучек от упавших снежинок
                                float kernel = (k == 0) ? 0.65f : ((std::abs(k) == 1) ? 0.42f : 0.18f);
                                obs.snowBins[bi] = std::min(localCap, obs.snowBins[bi] + kernel * dScale * (0.35f + 0.75f * hillWave));
                            }
                        }
                        spawnParticle(p, Snow, false); hit = true; break;
                    } else if (p.type == Leaf && !obs.isWaybar && p.restTimer >= 0.0f && rng->bounded(100) < 62) {
                        p.resting = true; p.attachedObstacleId = obs.id;
                        p.relX = rx; p.relY = 0.0f;
                        p.bendAmount = 0.0f;
                        p.y = float(r.top()) - 0.5f;
                        p.restTimer = 3.8f + rng->bounded(50) * 0.1f;
                        hit = true; break;
                    }
                }

                // Прилипание снежинок к поверхности открытых окон и карточек с последующим таянием!
                float winMeltMul = mgr->snowStickMeltEnabled() ? float(mgr->snowMeltAmount()) : 0.0f;
                if (!hit && p.type == Snow && winMeltMul > 0.02f && !obs.isWaybar && p.depth >= 0.58f &&
                    p.x >= r.left() + 16 && p.x <= r.right() - 16 &&
                    p.y >= r.top() + 16 && p.y <= r.bottom() - 18 && rng->bounded(2500) < int(5.0f * winMeltMul)) {
                    int stuckWinSnow = 0;
                    for (const auto &other : m_particles) {
                        if (other.type == Snow && other.resting && other.attachedObstacleId == obs.id) ++stuckWinSnow;
                    }
                    int maxWinSnow = std::clamp(int((obs.isWindow ? 3 : 1) * winMeltMul), 1, 5);
                    if (stuckWinSnow < maxWinSnow) {
                        p.resting = true;
                        p.attachedObstacleId = obs.id;
                        p.relX = std::clamp(float((p.x - r.left()) / r.width()), 0.03f, 0.97f);
                        p.relY = float(p.y - r.top());
                        p.restTimer = 2.5f + rng->bounded(30) * 0.1f;
                        p.bendAmount = p.restTimer;
                        hit = true; break;
                    }
                }
                // Прилипание летящего листа к самой лицевой поверхности открытого окна, карточки или мокрого экрана!
                if (!hit && p.type == Leaf && p.restTimer >= 0.0f && !obs.isWaybar && p.depth >= 0.56f &&
                    p.x >= r.left() + 14 && p.x <= r.right() - 14 &&
                    p.y >= r.top() + 14 && p.y <= r.bottom() - 18) {
                    // Считаем, сколько листьев уже прилипло к этому окну/блоку, чтобы не залеплять его целиком
                    int stuckLeaves = 0;
                    for (const auto &other : m_particles) {
                        if (other.type == Leaf && other.resting && other.attachedObstacleId == obs.id && other.relY > 0.5f)
                            ++stuckLeaves;
                    }
                    int maxStuck = obs.isWindow ? 4 : 2;
                    int stickChance = (mgr->rainEnabled() && mgr->wetGlassEnabled()) ? 7 : 4; // На мокрое стекло листья липнут охотнее!
                    if (stuckLeaves < maxStuck && rng->bounded(1000) < stickChance) {
                        p.resting = true;
                        p.attachedObstacleId = obs.id;
                        p.relX = std::clamp(float((p.x - r.left()) / r.width()), 0.05f, 0.95f);
                        p.relY = float(p.y - r.top());
                        p.restTimer = 4.0f + rng->bounded(75) * 0.1f; // Держится на окне 4..11.5 сек, затем отлипает!
                        hit = true; break;
                    }
                }

                // 2) Попадание капель на саму поверхность открытых окон (obs.isWindow), Waybar (obs.isWaybar) и карточек:
                float topPad = obs.isWaybar ? 2.0f : 12.0f;
                float botPad = obs.isWaybar ? 5.0f : 14.0f;
                float minDepthForFace = (obs.isWindow || obs.isWaybar) ? 0.58f : 0.76f;
                if (!hit && p.depth >= minDepthForFace && (p.type == Rain || p.type == FallingDrop) && mgr->wetGlassEnabled() &&
                    p.x >= r.left() + 8 && p.x <= r.right() - 8 && p.y > r.top() + topPad && p.y < r.bottom() - botPad) {
                    float landRx = std::clamp(float((p.x - r.left()) / r.width()), 0.03f, 0.97f);
                    float landRy = obs.isWaybar
                        ? std::clamp(float(p.y - r.top()), 2.2f, float(r.height() * 0.45))
                        : float(p.y - r.top());

                    float wMul = ovDropMul * float(mgr->windowDropsAmount());
                    float bMul = ovDropMul * float(mgr->waybarDropsAmount());
                    int maxFaceRunners = obs.isWindow
                        ? (wMul <= 0.02f ? 0 : std::clamp(int(2 * wMul), 1, 6))
                        : (obs.isWaybar ? (bMul <= 0.02f ? 0 : std::clamp(int(2 * bMul), 1, 6)) : 1);
                    int maxFaceBeads = obs.isWindow
                        ? (wMul <= 0.02f ? 0 : std::clamp(int(3 * wMul), 1, 8))
                        : (obs.isWaybar ? (bMul <= 0.02f ? 0 : std::clamp(int(2 * bMul), 1, 6)) : 3);
                    int faceChance = obs.isWindow
                        ? int(5 * wMul)
                        : (obs.isWaybar ? int(13 * bMul) : 3);

                    if ((countActiveSlides(obs.waterDrops) < maxFaceRunners || countBeads(obs.waterDrops) < maxFaceBeads) &&
                        rng->bounded(1000) < faceChance) {
                        bool wantBead = (countBeads(obs.waterDrops) < maxFaceBeads) && (rng->bounded(100) < (obs.isWaybar ? 22 : 28));
                        if (wantBead) {
                            spawnBead(obs, landRx, landRy, p.size, obs.isWindow ? 1.10f : (obs.isWaybar ? 0.88f : 1.0f));
                        } else if (countActiveSlides(obs.waterDrops) < maxFaceRunners) {
                            SurfaceDrop wd;
                            wd.relX = wd.prevRelX = landRx;
                            wd.relY = landRy;
                            wd.sideWall = 0; wd.spreadProg = 1.0f; wd.detachProg = 1.0f;
                            wd.mass = 1.15f + (rng->bounded(110) * 0.01f) * p.size;
                            wd.radius = obs.isWaybar
                                ? std::clamp(3.10f + wd.mass * 1.35f, 3.2f, 5.6f)
                                : std::clamp(3.80f + wd.mass * 1.85f, 4.2f, 7.4f);
                            wd.spreadX = wd.radius;
                            wd.vy = (obs.isWaybar ? (8.5f + wd.mass * 3.0f) : (16.0f + wd.mass * 5.5f)) * float(mgr->dropSlideSpeed());
                            wd.driftSlope = ((rng->bounded(30) - 15) * 0.01f) + wind * 0.05f;
                            wd.tailDirX = wd.driftSlope; wd.stretchFactor = 0.25f;
                            wd.pinchDistAcc = 0.0f; wd.nextPinchTarget = obs.isWaybar ? (5.5f + rng->bounded(40) * 0.1f) : (10.0f + rng->bounded(75) * 0.1f);
                            wd.pauseTimer = 0.0f;
                            obs.waterDrops.append(wd);
                        }
                    }
                }
            }
            if (removed || hit) continue;
        }
        const double xMargin = height() * 0.8 + 200.0;
        if (p.y > height() + 24 || p.x < -width() * 0.25 - xMargin || p.x > width() * 1.25 + xMargin) {
            if (p.type == FallingDrop) m_particles.removeAt(i);
            else spawnParticle(p, p.type, false);
        }
    }

    for (auto &obs : m_obstacles) {
        if (!obs.snowBins.isEmpty()) {
            int n = obs.snowBins.size();
            if (!mgr->snowEnabled() || !mgr->snowDriftEnabled() || obs.id == "window_glass" || obs.id == "window_top_ledge") {
                for (int b = 0; b < n; ++b) obs.snowBins[b] = std::max(0.0f, obs.snowBins[b] - dt * 6.5f);
                obs.snowmanProg = std::max(0.0f, obs.snowmanProg - dt * 2.5f);
            } else {
                float dScale = float(mgr->snowDriftScale());

                // ПРОСТОЕ И АККУРАТНОЕ СМАХИВАНИЕ СНЕГА КУРСОРОМ МЫШИ (при движении курсора по шапке снега он мягко счищается и осыпается вниз):
                double curSpd = std::hypot(m_cursorVel.x(), m_cursorVel.y());
                if (m_cursorPos.x() > -40 && curSpd > 0.35 &&
                    m_cursorPos.x() >= obs.rect.left() - 28.0 && m_cursorPos.x() <= obs.rect.right() + 28.0 &&
                    m_cursorPos.y() >= obs.rect.top() - 36.0  && m_cursorPos.y() <= obs.rect.top() + 24.0) {
                    double brushRadius = 38.0;
                    for (int b = 0; b < n; ++b) {
                        if (obs.snowBins[b] <= 0.15f) continue;
                        double binX = obs.rect.left() + (b + 0.5) / double(n) * obs.rect.width();
                        double binY = obs.rect.top() - obs.snowBins[b] * 0.45;
                        double dx = binX - m_cursorPos.x();
                        double dy = binY - m_cursorPos.y();
                        double dist = std::hypot(dx, dy);
                        if (dist < brushRadius) {
                            float smoothBell = std::pow(float(1.0 - dist / brushRadius), 1.4f);
                            float wipeAmount = std::min(obs.snowBins[b], (8.0f + float(curSpd) * 1.4f) * smoothBell * dt * 8.5f);
                            obs.snowBins[b] -= wipeAmount;

                            // Смахнутый снег естественно осыпается вниз снежинками!
                            if (wipeAmount > 0.30f && m_particles.size() < 460 && rng->bounded(100) < 48) {
                                Particle sp;
                                sp.type = Snow;
                                sp.x = float(binX + (rng->bounded(13) - 6));
                                sp.y = float(obs.rect.top() + 1.0);
                                sp.vx = std::clamp(float(m_cursorVel.x() * 3.2 + (rng->bounded(60) - 30)), -160.0f, 160.0f);
                                sp.vy = float(25.0 + rng->bounded(55));
                                sp.termV = 0.65f + (rng->bounded(85) * 0.01f);
                                sp.size = (3.0f + rng->bounded(20) * 0.1f) * float(mgr->particleScale());
                                sp.alpha = 0.88f;
                                sp.depth = 0.85f;
                                sp.angle = rng->bounded(360);
                                sp.angVel = rng->bounded(110) - 55;
                                sp.tiltPhase = rng->bounded(628) * 0.01f;
                                sp.tiltSpeed = 1.3f + rng->bounded(14) * 0.1f;
                                sp.swayPhase = rng->bounded(628) * 0.01f;
                                sp.leafKind = rng->bounded(8);
                                sp.color = QColor(246, 251, 255);
                                m_particles.append(sp);
                            }
                        }
                    }
                }

                // Сглаживаем только слишком крутые пики, сохраняя раздельные кучки
                QVector<float> cp = obs.snowBins;
                float maxSlope = 2.6f * dScale;
                for (int b = 1; b < n - 1; ++b) {
                    float avgN = (cp[b - 1] + cp[b + 1]) * 0.5f;
                    if (std::abs(cp[b] - avgN) > maxSlope) {
                        obs.snowBins[b] = cp[b] + (avgN - cp[b]) * std::min(1.0f, dt * 3.5f);
                    }
                }
                obs.snowBins[0] *= std::pow(0.4f, dt);
                obs.snowBins[n - 1] *= std::pow(0.4f, dt);

            }
        }

        if (!obs.wetFilmBins.isEmpty()) {
            int n = obs.wetFilmBins.size();
            if (!mgr->rainEnabled() || !mgr->wetGlassEnabled()) {
                for (int b = 0; b < n; ++b) obs.wetFilmBins[b] = std::max(0.0f, obs.wetFilmBins[b] - dt * 0.6f);
            } else {
                QVector<float> cp = obs.wetFilmBins;
                for (int b = 1; b < n - 1; ++b) {
                    float neighborAvg = (cp[b - 1] + cp[b + 1]) * 0.5f;
                    float targetVal = cp[b] * 0.80f + neighborAvg * 0.20f;
                    obs.wetFilmBins[b] = std::max(0.0f, targetVal - dt * 0.038f);
                }
            }
        }

        bool winAnimDone = (!obs.isWindow || (nowMs - obs.createdMs > 450));
        bool rainFogs = mgr->rainEnabled() && mgr->condensationEnabled();
        bool snowFogs = mgr->snowEnabled() && (mgr->condensationEnabled() || mgr->snowScreenFrostEnabled() || mgr->snowWindowFrostIntensity() > 0.01);
        bool weatherFogs = rainFogs || snowFogs;
        float weatherInt = mgr->rainEnabled() ? float(mgr->rainIntensity()) : float(mgr->snowIntensity());
        if (weatherFogs && obs.rect.height() >= 28.0 && winAnimDone) {
            obs.fogLevel = std::min(1.0f, obs.fogLevel + dt * 0.065f * (0.45f + weatherInt));
        } else if (!weatherFogs) {
            obs.fogLevel = std::max(0.0f, obs.fogLevel - dt * 0.35f);
        }

        bool freezeTracks = (mgr->snowEnabled() && mgr->snowStickMeltEnabled());
        for (int w = obs.wipedTracks.size() - 1; w >= 0; --w) {
            if (!freezeTracks) obs.wipedTracks[w].life -= dt;
            if (obs.wipedTracks[w].life <= 0.0f) obs.wipedTracks.removeAt(w);
        }
        for (int t = obs.trailBeads.size() - 1; t >= 0; --t) {
            if (!freezeTracks) obs.trailBeads[t].life -= dt;
            if (obs.trailBeads[t].life <= 0.0f) obs.trailBeads.removeAt(t);
        }

        // Плавное сближение и слияние капель на грани
        float obsW = std::max(30.0, obs.rect.width());
        for (int i = 0; i < obs.waterDrops.size(); ++i) {
            for (int j = i + 1; j < obs.waterDrops.size(); ++j) {
                auto &d1 = obs.waterDrops[i];
                auto &d2 = obs.waterDrops[j];
                bool e1 = (d1.relY <= 1.2f && d1.sideWall == 0 && d1.detachProg <= 0.0f);
                bool e2 = (d2.relY <= 1.2f && d2.sideWall == 0 && d2.detachProg <= 0.0f);
                if (e1 != e2 || d1.sideWall != d2.sideWall) continue;

                float dx = (d2.relX - d1.relX) * float(obsW);
                float dy = d2.relY - d1.relY;
                float distSq = dx * dx + dy * dy;
                float touchRange = e1 ? 22.0f : 11.0f;
                if (distSq < touchRange * touchRange && distSq > 0.001f) {
                    float dist = std::sqrt(distSq);
                    if (dist < 1.5f) {
                        float m1 = d1.mass, m2 = d2.mass;
                        d1.relX = (d1.relX * m1 + d2.relX * m2) / (m1 + m2);
                        d1.relY = (d1.relY * m1 + d2.relY * m2) / (m1 + m2);
                        // При слиянии двух капелек общая площадь растет плавно (без скачка в огромную лужу):
                        float bigM = std::max(m1, m2), smallM = std::min(m1, m2);
                        d1.mass = std::min(3.0f, bigM + smallM * 0.48f);
                        d1.radius = std::min(2.45f, 0.95f + d1.mass * 0.34f);
                        d1.spreadX = d1.radius;
                        d1.spreadProg = std::max(d1.spreadProg, d2.spreadProg);
                        d1.releaseMass = std::max(d1.releaseMass, d2.releaseMass);
                        obs.waterDrops.removeAt(j);
                        --j;
                    } else {
                        // В 2.2 раза медленнее стягивание мелких капелек в большую лужу
                        float pull = std::pow(1.0f - dist / touchRange, 1.8f) * 3.6f * dt;
                        float totalM = d1.mass + d2.mass;
                        d1.relX += (dx / dist) * (pull * (d2.mass / totalM)) / float(obsW);
                        d2.relX -= (dx / dist) * (pull * (d1.mass / totalM)) / float(obsW);
                    }
                }
            }
        }

        int activeRunners = 0;
        int sideRunners = 0;
        for (const auto &wcheck : obs.waterDrops) {
            if (!wcheck.pinned && (wcheck.relY > 1.2f || wcheck.sideWall != 0 || wcheck.detachProg > 0.0f)) activeRunners++;
            if (wcheck.sideWall != 0) sideRunners++;
        }
        float ovMul = m_globalMode ? float(mgr->overlayDropsIntensity()) : 1.0f;
        int maxRunners = (obs.id == "window_glass")
            ? std::clamp(int(5 * ovMul), 2, 9)
            : (obs.isWindow
                ? std::clamp(int(obs.rect.width() / 420.0 * ovMul) + 1, 1, 3)
                : (obs.isWaybar
                    ? std::clamp(int(obs.rect.width() / 240.0 * ovMul) + 1, 1, 3)
                    : ((obs.rect.width() > 400.0) ? 2 : 1)));

        for (int d = obs.waterDrops.size() - 1; d >= 0; --d) {
            auto &wd = obs.waterDrops[d];
            if ((mgr->snowEnabled() && !mgr->rainEnabled() && wd.maxEdgeLife >= 0.0f && wd.releaseMass < 500.0f) || (!mgr->wetGlassEnabled() && !mgr->condensationEnabled() && !(mgr->snowEnabled() && mgr->snowStickMeltEnabled()))) {
                wd.mass -= dt * 1.4f;
                if (wd.mass <= 0.1f) obs.waterDrops.removeAt(d);
                continue;
            }
            if (wd.relY <= 1.2f && wd.sideWall == 0) {
                wd.edgeAge += dt;
                if (wd.spreadProg < 1.0f) {
                    wd.spreadProg = std::min(1.0f, wd.spreadProg + dt * 0.75f);
                }
                wd.prevRelX = wd.relX;

                // ФАЗА АНИМАЦИИ ВЫТЯГИВАНИЯ ВОДЫ ВНИЗ И ОТДЕЛЕНИЯ КАПЛИ (detachProg: 0.0 -> 1.0):
                if (wd.detachProg > 0.0f) {
                    wd.detachProg += dt * 2.65f * float(mgr->dropSlideSpeed()); // ~0.65 сек плавного вытягивания вниз и отрыва!

                    // ПОКА КАПЛЯ ВЫТЯГИВАЕТСЯ И НАЧИНАЕТ ТЕЧЬ, ВОДА НА ГОРИЗОНТАЛЬНОЙ ПЛОСКОСТИ УБЫВАЕТ!
                    if (!obs.wetFilmBins.isEmpty()) {
                        int bc = obs.wetFilmBins.size();
                        int cIdx = std::clamp(int(wd.relX * bc), 0, bc - 1);
                        for (int k = -9; k <= 9; ++k) {
                            int bi = std::clamp(cIdx + k, 0, bc - 1);
                            float drainRate = (1.0f - std::abs(k) / 10.0f) * 4.2f * dt;
                            obs.wetFilmBins[bi] = std::max(0.0f, obs.wetFilmBins[bi] - drainRate);
                        }
                    }
                    // Соседние мелкие капельки рядом с точкой вытягивания втягиваются в формирующуюся каплю (вода на грани убывает!)
                    for (int j = obs.waterDrops.size() - 1; j >= 0; --j) {
                        if (j == d) continue;
                        auto &other = obs.waterDrops[j];
                        if (other.relY <= 1.2f && other.sideWall == 0 && other.detachProg <= 0.0f) {
                            float dxPx = (wd.relX - other.relX) * float(obsW);
                            if (std::abs(dxPx) < 20.0f) {
                                other.relX += (dxPx > 0 ? 1.0f : -1.0f) * (16.0f * dt) / float(obsW);
                                float drained = other.mass * dt * 1.8f;
                                other.mass -= drained;
                                other.radius = std::max(0.6f, 0.95f + other.mass * 0.40f);
                                wd.mass = std::min(3.2f, wd.mass + drained * 0.7f);
                                if (other.mass < 0.16f) {
                                    obs.waterDrops.removeAt(j);
                                    if (j < d) --d;
                                }
                            }
                        }
                    }

                    if (wd.detachProg >= 1.0f) {
                        wd.detachProg = 1.0f;
                        wd.relY = obs.isWaybar ? 2.2f : 7.5f;
                        wd.vy = (obs.isWaybar ? (8.5f + wd.mass * 3.0f) : (16.0f + wd.mass * 5.0f)) * float(mgr->dropSlideSpeed());
                        wd.radius = obs.isWaybar
                            ? std::clamp(3.10f + wd.mass * 1.35f, 3.20f, 5.60f)
                            : std::clamp(3.80f + wd.mass * 1.85f, 4.20f, 7.60f);
                        wd.spreadX = wd.radius;
                        wd.driftSlope = ((rng->bounded(36) - 18) * 0.01f) + wind * 0.06f;
                        wd.tailDirX = wd.driftSlope;
                        wd.stretchFactor = 0.32f;
                        wd.pinchDistAcc = 0.0f;
                        wd.nextPinchTarget = 6.5f + (rng->bounded(65) * 0.1f);
                        wd.pauseTimer = 0.0f;
                    }
                    continue;
                }

                if (!obs.wetFilmBins.isEmpty()) {
                    int bc = obs.wetFilmBins.size();
                    int bi = std::clamp(int(wd.relX * bc), 0, bc - 1);
                    obs.wetFilmBins[bi] = std::max(obs.wetFilmBins[bi], std::min(1.10f, wd.mass * 0.26f * wd.spreadProg));
                }

                double distLeftPx = wd.relX * obsW;
                double distRightPx = (1.0 - wd.relX) * obsW;
                if (false && sideRunners == 0 && wd.mass >= 1.95f && obs.rect.height() >= 28.0) {
                    if (distLeftPx < 2.8 && rng->bounded(100) < 20) {
                        wd.sideWall = -1; wd.relX = wd.prevRelX = 0.0f; wd.relY = 3.2f; wd.vy = 16.0f + wd.mass * 4.0f;
                        wd.driftSlope = 0.0f; wd.tailDirX = 0.0f; wd.stretchFactor = 0.25f; wd.pinchDistAcc = 0.0f; wd.nextPinchTarget = 7.0f;
                        sideRunners++;
                        continue;
                    } else if (distRightPx < 2.8 && rng->bounded(100) < 20) {
                        wd.sideWall = 1; wd.relX = wd.prevRelX = 1.0f; wd.relY = 3.2f; wd.vy = 16.0f + wd.mass * 4.0f;
                        wd.driftSlope = 0.0f; wd.tailDirX = 0.0f; wd.stretchFactor = 0.25f; wd.pinchDistAcc = 0.0f; wd.nextPinchTarget = 7.0f;
                        sideRunners++;
                        continue;
                    }
                }

                bool readyToDrain = (wd.mass >= wd.releaseMass) || (wd.edgeAge > wd.maxEdgeLife);
                if (readyToDrain) {
                    if (activeRunners < maxRunners) {
                        wd.mass = std::max(wd.mass, 1.15f);
                        wd.detachProg = 0.02f;
                        activeRunners++;
                    } else if (wd.edgeAge > wd.maxEdgeLife + 2.5f) {
                        // Если лимит бегущих капель занят слишком долго — капля плавно уходит в плёнку, не зависая точкой
                        wd.mass -= dt * 0.45f;
                        if (wd.mass <= 0.18f) { obs.waterDrops.removeAt(d); continue; }
                    }
                }
            } else {
                if (wd.pinned) {
                    wd.prevRelX = wd.relX;
                    wd.vy = 0.0f;
                    wd.edgeAge += dt;
                    if (wd.mass >= wd.releaseMass || wd.edgeAge >= wd.maxEdgeLife) {
                        wd.pinned = false;
                        wd.mass = std::max(wd.mass, 1.10f);
                        wd.vy = (14.0f + wd.mass * 5.5f) * float(mgr->dropSlideSpeed());
                        wd.driftSlope = ((rng->bounded(30) - 15) * 0.01f) + wind * 0.05f;
                        wd.tailDirX = wd.driftSlope;
                        wd.stretchFactor = 0.22f;
                        wd.pinchDistAcc = 0.0f;
                        wd.nextPinchTarget = 5.0f + rng->bounded(50) * 0.1f;
                        wd.pauseTimer = 0.0f;
                    }
                    continue;
                }
                // СТЕКАЮЩАЯ КАПЛЯ:
                float slideMul = float(mgr->dropSlideSpeed());
                if (wd.pauseTimer > 0.0f) {
                    wd.pauseTimer -= dt * slideMul;
                    wd.vy = std::max(9.0f * slideMul, wd.vy * 0.92f);
                } else {
                    float speedCap = ((obs.isWaybar ? 18.0f : 68.0f) + wd.mass * (obs.isWaybar ? 8.0f : 30.0f)) * grav * slideMul;
                    wd.vy = std::min(speedCap, wd.vy + (58.0f + wd.mass * 42.0f) * grav * slideMul * dt);
                }
                float pinchProg = std::clamp(wd.pinchDistAcc / std::max(4.0f, wd.nextPinchTarget), 0.0f, 1.0f);
                wd.stretchFactor = 0.38f + 1.35f * (0.5f - 0.5f * std::cos(pinchProg * 6.283185f)) + std::clamp(wd.vy * 0.008f, 0.0f, 0.65f);

                float dyStep = wd.vy * dt;
                float prevY = wd.relY;
                wd.prevRelX = wd.relX;
                wd.relY += dyStep;

                if (wd.sideWall != 0) {
                    wd.relX = (wd.sideWall == -1) ? 0.0f : 1.0f;
                    wd.tailDirX = 0.0f;
                } else {
                    float dxStepPx = dyStep * wd.driftSlope;
                    float nextRelX = wd.relX + dxStepPx / float(obsW);
                    if (nextRelX < 0.02f) { nextRelX = 0.02f + (0.02f - nextRelX); wd.driftSlope = std::abs(wd.driftSlope) * 0.5f + 0.04f; }
                    else if (nextRelX > 0.98f) { nextRelX = 0.98f - (nextRelX - 0.98f); wd.driftSlope = -std::abs(wd.driftSlope) * 0.5f - 0.04f; }
                    if (obs.id == "window_glass" && m_globalMode) {
                        if (wd.relX <= 0.5f && nextRelX > 0.333f) {
                            nextRelX = 0.333f - (nextRelX - 0.333f);
                            wd.driftSlope = -std::abs(wd.driftSlope) * 0.5f - 0.03f;
                        } else if (wd.relX > 0.5f && nextRelX < 0.667f) {
                            nextRelX = 0.667f + (0.667f - nextRelX);
                            wd.driftSlope = std::abs(wd.driftSlope) * 0.5f + 0.03f;
                        }
                    }
                    wd.relX = std::clamp(nextRelX, 0.01f, 0.99f);
                    wd.tailDirX = wd.tailDirX * 0.86f + wd.driftSlope * 0.14f;
                }

                bool isSnowMeltDrop = (wd.maxEdgeLife < 0.0f);
                if (isSnowMeltDrop) {
                    wd.edgeAge += dt;
                    // Капля стекает совсем чуть-чуть (6..14px за ~0.8 сек) и замерзает на месте вместе с коротким следом!
                    wd.vy *= std::pow(0.06f, dt);
                    wd.mass = std::max(0.42f, wd.mass - dyStep * 0.003f);
                    wd.radius = std::clamp(1.75f + wd.mass * 0.95f, 1.85f, 2.60f);
                    if (wd.edgeAge >= 0.85f || wd.vy <= 1.4f) {
                        wd.pinned = true;
                        wd.vy = 0.0f;
                        wd.edgeAge = 0.0f;
                        wd.maxEdgeLife = 3600.0f;
                        wd.releaseMass = 999.0f;
                        continue;
                    }
                } else {
                    float massLossRate = 0.00042f;
                    wd.mass -= dyStep * massLossRate;
                    wd.radius = std::clamp(3.60f + wd.mass * 1.85f, 4.00f, 7.60f);
                    if (wd.mass <= 0.20f) {
                        if (obs.trailBeads.size() < 95) {
                            TrailBead tb; tb.relX = wd.relX; tb.relY = wd.relY;
                            tb.width = 0.50f; tb.lenY = 1.2f; tb.life = tb.maxLife = 9.0f;
                            obs.trailBeads.append(tb);
                        }
                        obs.waterDrops.removeAt(d);
                        continue;
                    }
                }

                if ((mgr->condensationEnabled() || mgr->snowEnabled()) && dyStep > 0.04f && wd.sideWall == 0) {
                    int mergedIdx = -1;
                    int searchStop = std::max(0, int(obs.wipedTracks.size()) - 40);
                    for (int w = int(obs.wipedTracks.size()) - 1; w >= searchStop; --w) {
                        const auto &cand = obs.wipedTracks[w];
                        if (std::abs(cand.relX2 - wd.prevRelX) * obsW < 4.0 &&
                            std::abs(cand.y2 - prevY) < 4.5f &&
                            std::abs(cand.y2 - cand.y1) < 15.0f) {
                            mergedIdx = w;
                            break;
                        }
                    }
                    if (mergedIdx >= 0) {
                        obs.wipedTracks[mergedIdx].relX2 = wd.relX;
                        obs.wipedTracks[mergedIdx].y2 = wd.relY;
                        obs.wipedTracks[mergedIdx].radius = std::max(obs.wipedTracks[mergedIdx].radius, wd.radius * 0.96f);
                        obs.wipedTracks[mergedIdx].life = obs.wipedTracks[mergedIdx].maxLife;
                    } else {
                        if (obs.wipedTracks.size() >= 650) obs.wipedTracks.removeFirst();
                        WipedTrack wt;
                        wt.relX1 = wd.prevRelX; wt.y1 = prevY;
                        wt.relX2 = wd.relX;     wt.y2 = wd.relY;
                        wt.radius = wd.radius * 0.96f;
                        wt.life = wt.maxLife = 11.5f;
                        obs.wipedTracks.append(wt);
                    }
                }

                for (int t = obs.trailBeads.size() - 1; t >= 0; --t) {
                    float tdx = (obs.trailBeads[t].relX - wd.relX) * float(obsW);
                    float tdy = obs.trailBeads[t].relY - wd.relY;
                    if (tdx * tdx + tdy * tdy < 18.0f && tdy > 1.5f) {
                        wd.mass = std::min(3.2f, wd.mass + 0.04f);
                        obs.trailBeads.removeAt(t);
                    }
                }

                wd.pinchDistAcc += dyStep;
                if (wd.pinchDistAcc >= wd.nextPinchTarget && wd.relY > 5.0f) {
                    wd.pinchDistAcc = 0.0f;
                    wd.nextPinchTarget = 4.2f + (rng->bounded(45) * 0.1f);
                    wd.pauseTimer = 0.05f + (rng->bounded(7) * 0.01f);
                    wd.mass = std::max(0.24f, wd.mass - 0.030f);

                    if (mgr->trailStyle() != 2 && obs.trailBeads.size() < 110) {
                        int beadCount = (mgr->trailStyle() == 0 && rng->bounded(100) < 42) ? 2 : 1;
                        for (int bc = 0; bc < beadCount; ++bc) {
                            TrailBead tb;
                            if (isSnowMeltDrop) {
                                float rBead = 0.68f + (rng->bounded(25) * 0.01f);
                                tb.width = rBead;
                                tb.lenY  = rBead * 1.18f;
                                tb.life  = tb.maxLife = 3600.0f;
                                if (obs.trailBeads.size() >= 18) obs.trailBeads.removeFirst();
                            } else if (mgr->trailStyle() == 0) {
                                float rBead = 0.88f + std::pow(rng->bounded(1000) * 0.001f, 1.3f) * 0.52f;
                                tb.width = rBead;
                                tb.lenY  = rBead * 1.12f;
                                tb.life  = tb.maxLife = 5.2f + (rng->bounded(35) * 0.1f);
                            } else {
                                tb.width = 0.80f;
                                tb.lenY  = 1.20f;
                                tb.life  = tb.maxLife = 4.0f + (rng->bounded(30) * 0.1f);
                            }
                            float tailOffsetPx = wd.radius * (1.35f + wd.stretchFactor * 0.75f) + bc * 2.4f;
                            float jitterPx = (rng->bounded(21) - 10) * 0.045f;
                            tb.relY = std::max(1.4f + tb.lenY, wd.relY - tailOffsetPx);
                            tb.relX = (wd.sideWall != 0) ? wd.relX : std::clamp(wd.relX - (tailOffsetPx * wd.tailDirX - jitterPx) / float(obsW), 0.0f, 1.0f);
                            obs.trailBeads.append(tb);
                        }
                    }

                    wd.stretchFactor = 0.22f;
                    if (wd.sideWall == 0) {
                        float angleNudge = (rng->bounded(44) - 22) * 0.01f;
                        wd.driftSlope = std::clamp(wd.driftSlope * 0.4f + angleNudge + wind * 0.08f, -0.26f, 0.26f);
                    }
                }

                if (wd.relY >= obs.rect.height() - 1.5 || (obs.id == "window_top_ledge" && wd.relY >= 4.5f)) {
                    if (obs.id == "window_glass" && m_globalMode) { obs.waterDrops.removeAt(d); continue; }
                    Particle drip; drip.type = FallingDrop;
                    drip.x = float(obs.rect.left() + wd.relX * obs.rect.width());
                    drip.y = float(obs.rect.bottom() + 1.0);
                    drip.vx = 0.0f; drip.vy = 65.0f;
                    drip.size = std::clamp(wd.radius * 0.85f, 2.8f, 4.8f);
                    drip.alpha = 0.90f; drip.depth = 0.95f;
                    if (m_particles.size() < 460) m_particles.append(drip);
                    obs.waterDrops.removeAt(d);
                }
            }
        }
    }

    // СТРОГО 1 СНЕГОВИЧОК НА ЭКРАНЕ: ищем единственную самую высокую кучку снега среди всех окон/карточек
    if (mgr->snowEnabled() && mgr->snowDriftEnabled()) {
        QString bestObsId;
        float bestPeak = 0.0f;
        // Если уже есть начавший формироваться снеговичок и его сугроб цел — держим его, иначе выбираем самый высокий сугроб
        for (auto it = m_obstacles.begin(); it != m_obstacles.end(); ++it) {
            const auto &obs = it.value();
            if (obs.isWaybar || obs.id == "window_glass" || obs.id == "window_top_ledge" || obs.snowBins.isEmpty()) continue;
            float pk = 0.0f;
            for (int b = 3; b < obs.snowBins.size() - 3; ++b) pk = std::max(pk, obs.snowBins[b]);
            if (obs.snowmanProg > 0.05f && pk >= 7.0f * std::min(1.0f, float(mgr->snowDriftScale()))) {
                bestObsId = it.key();
                bestPeak = pk;
                break;
            }
            if (pk > bestPeak) {
                bestPeak = pk;
                bestObsId = it.key();
            }
        }
        float reqH = 9.5f * std::min(1.0f, float(mgr->snowDriftScale()));
        for (auto it = m_obstacles.begin(); it != m_obstacles.end(); ++it) {
            auto &obs = it.value();
            if (it.key() == bestObsId && bestPeak >= reqH) {
                // Медленно (за ~24 секунды) вылепливается из сугроба, не появляясь сразу!
                obs.snowmanProg = std::min(1.0f, obs.snowmanProg + dt * 0.042f);
            } else if (it.key() == bestObsId && bestPeak >= reqH * 0.70f && obs.snowmanProg > 0.0f) {
                // Держит форму, пока сугроб не смахнули
            } else {
                obs.snowmanProg = std::max(0.0f, obs.snowmanProg - dt * 1.8f);
            }
        }
    }

    for (int s = m_splashes.size() - 1; s >= 0; --s) {
        auto &sp = m_splashes[s];
        sp.vy += 360.0f * grav * dt;
        sp.x += sp.vx * dt;
        sp.y += sp.vy * dt;
        sp.life -= dt;
        if (sp.vy > 0.0f && sp.targetY > 0.0f && sp.y >= sp.targetY && m_obstacles.contains(sp.obsId)) {
            auto &obs = m_obstacles[sp.obsId];
            if (sp.x >= obs.rect.left() + 2 && sp.x <= obs.rect.right() - 2) {
                double rw = std::max(24.0, obs.rect.width());
                float sRx = std::clamp(float((sp.x - obs.rect.left()) / rw), 0.01f, 0.99f);
                if (!obs.wetFilmBins.isEmpty()) {
                    int bc = obs.wetFilmBins.size();
                    int bi = std::clamp(int(sRx * bc), 0, bc - 1);
                    obs.wetFilmBins[bi] = std::min(1.15f, obs.wetFilmBins[bi] + 0.032f * sp.radius);
                }
                // В месте прилета мелкой брызги создается крошечная лужица, но существующие крупные лужи растут от них в 3.5 раза медленнее:
                float puddleMass = 0.08f + sp.radius * 0.10f;
                bool mergedLanding = false;
                for (auto &wd : obs.waterDrops) {
                    if (wd.relY <= 1.2f && wd.sideWall == 0 && wd.detachProg <= 0.0f && std::abs(wd.relX - sRx) * rw < 4.5) {
                        float growthDamp = 1.0f / (1.0f + wd.mass * 1.6f);
                        wd.mass = std::min(3.0f, wd.mass + puddleMass * 0.35f * growthDamp);
                        wd.radius = std::min(2.45f, 0.82f + wd.mass * 0.34f);
                        wd.spreadX = wd.radius;
                        mergedLanding = true;
                        break;
                    }
                }
                if (!mergedLanding && obs.waterDrops.size() < 42 && rng->bounded(100) < 62) {
                    SurfaceDrop wd;
                    wd.relX = wd.prevRelX = sRx;
                    wd.relY = 0.25f;
                    wd.sideWall = 0;
                    wd.spreadProg = 0.18f; // Плавно растекается в маленькую лужицу на месте падения!
                    wd.detachProg = 0.0f;
                    wd.mass = puddleMass;
                    wd.radius = 0.82f + wd.mass * 0.42f; // Маленькая аккуратная лужица (0.9..1.2 px)
                    wd.spreadX = wd.radius;
                    wd.edgeAge = 0.0f;
                    wd.maxEdgeLife = 8.5f + (rng->bounded(55) * 0.1f);
                    wd.releaseMass = 1.45f + (rng->bounded(75) * 0.01f);
                    obs.waterDrops.append(wd);
                }
            }
            m_splashes.removeAt(s);
            continue;
        }
        if (sp.life <= 0) m_splashes.removeAt(s);
    }
    int impListIdx = m_globalMode ? 1 : 0;
    if (m_particles.isEmpty()) s_leafImprints[impListIdx].clear();
    for (int m = s_leafImprints[impListIdx].size() - 1; m >= 0; --m) {
        auto &imp = s_leafImprints[impListIdx][m];
        imp.life -= dt * (imp.released ? 1.8f : 1.0f);
        if (imp.life <= 0.0f || !mgr->leavesEnabled() || !mgr->condensationEnabled()) {
            s_leafImprints[impListIdx].removeAt(m);
        }
    }
    update();
    emit frameSimulated();
}

static double cornerArcOffset(double, double, bool, double = 6.5) {
    return 0.0;
}


void AtmosphereCanvas::paintWaterMap(QPainter *painter, const AtmosphereCanvas *src) {
    painter->setPen(Qt::NoPen);
    auto *mgr = AtmosphereManager::instance();
    double dScale = mgr ? std::clamp(double(mgr->dropScale()), 0.4, 2.0) : 1.0;

    auto drawDome = [&](double cx, double cy, double rx, double ry, int gPeak, int bPeak, int alphaPeak) {
        double maxR = std::max({rx, ry, 0.8});
        QPointF center(cx, cy);
        QPointF focal(cx, cy + ry * 0.18);
        QRadialGradient rg(center, maxR * 1.15, focal);
        rg.setColorAt(0.00, QColor(0, gPeak, bPeak, alphaPeak));
        rg.setColorAt(0.35, QColor(0, int(gPeak * 0.92), int(bPeak * 0.94), int(alphaPeak * 0.96)));
        rg.setColorAt(0.65, QColor(0, int(gPeak * 0.70), int(bPeak * 0.74), int(alphaPeak * 0.85)));
        rg.setColorAt(0.86, QColor(0, int(gPeak * 0.32), int(bPeak * 0.36), int(alphaPeak * 0.48)));
        rg.setColorAt(1.00, QColor(0, 0, 0, 0));
        painter->setBrush(rg);
        painter->drawEllipse(center, rx * 1.12, ry * 1.12);
    };

    // ПРОХОД 1: Базовая подложка тумана / морозной изморози открытых окон
    double effWinFogInt = 0.0;
    if (mgr) {
        if (mgr->snowEnabled() && !mgr->rainEnabled()) effWinFogInt = mgr->snowWindowFrostIntensity();
        else if (mgr->snowEnabled() && mgr->rainEnabled()) effWinFogInt = std::max(mgr->windowFogIntensity(), mgr->snowWindowFrostIntensity());
        else if (mgr->condensationEnabled()) effWinFogInt = mgr->windowFogIntensity();
    }
    if (mgr && effWinFogInt > 0.01) {
        bool snowPatternMode = mgr->snowEnabled();
        for (const auto &obs : src->m_obstacles) {
            if (obs.isWindow && obs.fogLevel > 0.02f) {
                const QRectF &r = obs.rect;
                int fogG = std::clamp(int(obs.fogLevel * effWinFogInt * 102.0f), 0, 105);
                QRectF innerR = r.adjusted(1.0, 1.0, -1.0, -1.0);
                painter->setPen(Qt::NoPen);
                if (snowPatternMode) {
                    // В режиме снега окно замерзает от краёв и углов к центру, образуя контур для морозных узоров!
                    painter->save();
                    QPainterPath clipP;
                    if (src->m_roundedTheme) clipP.addRoundedRect(innerR, 6.0, 6.0);
                    else clipP.addRect(innerR);
                    painter->setClipPath(clipP);

                    // 1) Тонкое настраиваемое промерзание вдоль 4 рамок окна (от 6px при малом ползунке до глубокого при 100%)
                    double scaleReach = std::pow(std::clamp(effWinFogInt, 0.0, 1.0), 1.15) * double(obs.fogLevel);
                    double reachX = std::clamp(innerR.width() * (0.02 + 0.30 * scaleReach), 6.0, 220.0);
                    double reachY = std::clamp(innerR.height() * (0.02 + 0.30 * scaleReach), 6.0, 200.0);

                    QLinearGradient lgL(innerR.left(), 0, innerR.left() + reachX, 0);
                    lgL.setColorAt(0.0, QColor(0, fogG, 0, 255));
                    lgL.setColorAt(0.55, QColor(0, int(fogG * 0.58), 0, 180));
                    lgL.setColorAt(1.0, QColor(0, 0, 0, 0));
                    painter->fillRect(QRectF(innerR.left(), innerR.top(), reachX, innerR.height()), lgL);

                    QLinearGradient lgR(innerR.right(), 0, innerR.right() - reachX, 0);
                    lgR.setColorAt(0.0, QColor(0, fogG, 0, 255));
                    lgR.setColorAt(0.55, QColor(0, int(fogG * 0.58), 0, 180));
                    lgR.setColorAt(1.0, QColor(0, 0, 0, 0));
                    painter->fillRect(QRectF(innerR.right() - reachX, innerR.top(), reachX, innerR.height()), lgR);

                    painter->save();
                    painter->setCompositionMode(QPainter::CompositionMode_Lighten);
                    QLinearGradient lgT(0, innerR.top(), 0, innerR.top() + reachY);
                    lgT.setColorAt(0.0, QColor(0, fogG, 0, 255));
                    lgT.setColorAt(0.55, QColor(0, int(fogG * 0.58), 0, 180));
                    lgT.setColorAt(1.0, QColor(0, 0, 0, 0));
                    painter->fillRect(QRectF(innerR.left(), innerR.top(), innerR.width(), reachY), lgT);

                    QLinearGradient lgB(0, innerR.bottom(), 0, innerR.bottom() - reachY);
                    lgB.setColorAt(0.0, QColor(0, fogG, 0, 255));
                    lgB.setColorAt(0.55, QColor(0, int(fogG * 0.58), 0, 180));
                    lgB.setColorAt(1.0, QColor(0, 0, 0, 0));
                    painter->fillRect(QRectF(innerR.left(), innerR.bottom() - reachY, innerR.width(), reachY), lgB);

                    // 2) Угловые очаги кристаллизации (масштабируются от маленьких уголков до крупных)
                    double cRad = std::clamp(std::min(innerR.width(), innerR.height()) * (0.05 + 0.48 * scaleReach), 12.0, 280.0);
                    const QPointF corners[4] = {innerR.topLeft(), innerR.topRight(), innerR.bottomLeft(), innerR.bottomRight()};
                    for (const QPointF &cpt : corners) {
                        QRadialGradient cg(cpt, cRad);
                        cg.setColorAt(0.0, QColor(0, std::min(108, int(fogG * 1.12)), 0, 255));
                        cg.setColorAt(0.5, QColor(0, int(fogG * 0.72), 0, 210));
                        cg.setColorAt(1.0, QColor(0, 0, 0, 0));
                        painter->setBrush(cg);
                        painter->drawEllipse(cpt, cRad, cRad);
                    }
                    painter->restore();
                    painter->restore();
                } else {
                    QLinearGradient fg(0, r.top(), 0, r.bottom());
                    fg.setColorAt(0.0,  QColor(0, fogG, 0, 255));
                    fg.setColorAt(0.55, QColor(0, int(fogG * 0.82), 0, 255));
                    fg.setColorAt(1.0,  QColor(0, int(fogG * 0.62), 0, 255));
                    painter->setBrush(fg);
                    if (src->m_roundedTheme) painter->drawRoundedRect(innerR, 6.0, 6.0);
                    else painter->fillRect(innerR, fg);
                }
            }
        }
    }

    // ПРОХОД 2 и 3 (из PRIORITY_RAIN_WORKING): Протёртые дорожки (CompositionMode_Source) и аккуратные капли
    for (const auto &obs : src->m_obstacles) {
        const QRectF &r = obs.rect;

        bool needScrollClip = (!src->m_globalMode && obs.id.startsWith("ui_"));
                if (obs.isWindow) {
            QRegion winVisReg(QRect(0, 0, int(width()), int(height())));
            int myZ = int(obs.clipRect.x());
            for (const auto &otherObs : src->m_obstacles) {
                if (otherObs.isWindow && otherObs.id != obs.id && int(otherObs.clipRect.x()) < myZ) {
                    if (otherObs.rect.width() < width() * 0.94 || otherObs.rect.height() < height() * 0.92) {
                        winVisReg -= otherObs.rect.toRect().adjusted(2, 2, -2, -2);
                    }
                }
            }
            painter->save();
            painter->setClipRegion(winVisReg, Qt::ReplaceClip);
            needScrollClip = true;
        } else if (needScrollClip) {
            painter->save();
            painter->setClipRect(QRectF(0, 14, src->width(), src->height() - 24));
        }

        if (!obs.wipedTracks.isEmpty()) {
            painter->save();
            painter->setCompositionMode(QPainter::CompositionMode_Source);
            for (const auto &wt : obs.wipedTracks) {
                float wRatio = std::pow(std::clamp(wt.life / wt.maxLife, 0.0f, 1.0f), 0.55f);
                if (wRatio < 0.05f) continue;
                double wx1 = r.left() + wt.relX1 * r.width();
                double wx2 = r.left() + wt.relX2 * r.width();
                painter->setPen(QPen(QColor(0, 240, 0, 255), std::max(0.8, wt.radius * 0.95 * dScale * wRatio), Qt::SolidLine, Qt::RoundCap));
                painter->drawLine(QPointF(wx1, r.top() + wt.y1), QPointF(wx2, r.top() + wt.y2));
            }
            painter->restore();
            painter->setPen(Qt::NoPen);
        }

        painter->setPen(Qt::NoPen);
        bool isWinGlass = (obs.id == "window_glass" || obs.id == "window_top_ledge" || src->m_globalMode);
        int dropAlpha = isWinGlass ? 255 : 195;

        painter->save();
        painter->setCompositionMode(QPainter::CompositionMode_Lighten);

        // Бусинки в следе за каплей (эталонные размеры из PRIORITY_RAIN_WORKING без клякс)
        for (const auto &tb : obs.trailBeads) {
            double cx = r.left() + tb.relX * r.width();
            double cy = std::max(r.top() + double(tb.lenY) + 1.0, r.top() + tb.relY);
            float fadeOut = std::pow(std::clamp(tb.life / tb.maxLife, 0.0f, 1.0f), 0.55f);
            float fadeIn  = std::clamp((tb.maxLife - tb.life) / 0.18f, 0.35f, 1.0f);
            float ratio   = fadeOut * fadeIn;
            int bStrength = std::clamp(int(228.0f * ratio), 145, 236);
            drawDome(cx, cy, tb.width * 0.82 * dScale, tb.lenY * 0.90 * dScale, 62, bStrength, dropAlpha);
        }

        // 3. Ровная, гладкая плёнка воды на горизонтальной плоскости (с широким перекрытием ячеек без бугорков)
        bool showSurfacePuddles = (!mgr || mgr->surfacePuddlesEnabled());
        if (showSurfacePuddles && !obs.isWaybar && !obs.wetFilmBins.isEmpty()) {
            int n = obs.wetFilmBins.size();
            double stepX = r.width() / double(n);
            for (int i = 1; i < n - 1; ++i) {
                float h = (obs.wetFilmBins[i - 1] + obs.wetFilmBins[i] * 2.0f + obs.wetFilmBins[i + 1]) * 0.25f;
                if (h > 0.18f) {
                    double cx = r.left() + (i + 0.5) / double(n) * r.width();
                    double rx = std::max(stepX * 1.28, (2.2 + h * 1.6) * dScale);
                    double ry = std::clamp((0.40 + h * 0.20) * dScale, 0.40, 0.80);
                    int bVal = std::clamp(int(80.0f + h * 45.0f), 80, 132);
                    drawDome(cx, r.top() + 0.38, rx, ry, 46, bVal, dropAlpha);
                }
            }
        }

        // 4. Капли с учётом ползунка размера dScale
        for (const auto &wd : obs.waterDrops) {
            double cx = r.left() + wd.relX * r.width(), cy = r.top() + wd.relY;
            bool onEdge = (wd.relY <= 1.2f && wd.sideWall == 0 && !wd.pinned);

            if (onEdge) {
                if (wd.detachProg > 0.005f) {
                    double dp = std::clamp(double(wd.detachProg), 0.0, 1.0);
                    double baseTopY = r.top() + 0.42;
                    double puddleScale = std::max(0.18, 1.0 - dp * 0.82);
                    double pR = std::clamp(double(wd.radius * 0.52 * puddleScale), 0.7, 1.7) * dScale;
                    drawDome(cx, baseTopY, pR * 2.15, pR * 0.42, 48, int(148 * puddleScale), dropAlpha);

                    double hangDist = (0.8 + std::pow(dp, 1.25) * 5.2) * dScale;
                    double headY = baseTopY + hangDist;
                    double headR = std::clamp(double(1.80 + wd.mass * 0.85) * (0.70 + 0.30 * dp), 1.80, 3.80) * dScale;
                    for (int seg = 0; seg < 6; ++seg) {
                        double t = seg / 5.0;
                        double sy = headY - hangDist * 0.85 * t;
                        double sr = headR * (1.0 - 0.68 * std::pow(t, 0.75));
                        int bVal = std::clamp(int(235.0 * (1.0 - 0.25 * t)), 120, 235);
                        drawDome(cx, sy, sr * 0.94, sr * 1.12, 70, bVal, dropAlpha);
                    }
                } else if (showSurfacePuddles) {
                    // Ровно растёкшаяся плоская капелька вдоль горизонтальной грани
                    double appear = std::clamp(double(wd.spreadProg), 0.35, 1.0);
                    double rBase = std::clamp(double(wd.radius * 0.50 * appear), 0.85, 1.70) * dScale;
                    int bPeak = std::clamp(int(148.0 * appear), 95, 155);
                    drawDome(cx, r.top() + 0.42, rBase * 2.15, rBase * 0.42, 48, bPeak, dropAlpha);
                }
            } else if (wd.pinned) {
                // Неидеальная, чуть асимметричная сидящая капля (с боковым наплывом и изогнутым верхом)
                double br = std::clamp(double(wd.radius) * 0.78, 2.0, 4.2) * dScale;
                double seed = wd.relX * 37.0 + wd.relY * 0.19;
                double skewX = std::sin(seed) * br * 0.18;
                double stretchSide = 1.0 + std::cos(seed * 1.7) * 0.14;
                // Основное брюшко + боковой асимметричный наплыв
                drawDome(cx, cy + br * 0.12, br * stretchSide, br * 1.06, 70, 236, dropAlpha);
                drawDome(cx + skewX, cy + br * 0.04, br * 0.82, br * 0.95, 68, 220, dropAlpha);
                for (int seg = 1; seg < 6; ++seg) {
                    double t = seg / 5.0;
                    double sr = br * (1.0 - 0.70 * std::pow(t, 0.85));
                    double curveX = std::sin(t * 3.14 + seed) * br * 0.22 * t;
                    double sy = cy + br * 0.12 - (br * 1.10) * t;
                    int bVal = std::clamp(int(232.0 * (1.0 - 0.28 * t)), 120, 232);
                    drawDome(cx + curveX, sy, sr * 0.94, sr * 1.10, 68, bVal, dropAlpha);
                }
            } else {
                // Живая, слегка искажающаяся при скольжении 3D-капля:
                // 1) Асимметричная головка, которая чуть колышется и наклоняется при движении
                double headR = std::clamp(double(wd.radius) * 0.78 * (1.05 - 0.08 * wd.stretchFactor), 2.4, 5.4) * dScale;
                double phase = wd.relY * 0.12 + wd.relX * 41.0;
                double wobbleX = std::sin(phase) * 0.14;
                double lobeShiftX = (wd.tailDirX * 0.8 + std::cos(phase * 1.3) * 0.20) * headR;
                drawDome(cx + lobeShiftX * 0.35, cy + headR * 0.06, headR * (0.90 + wobbleX * 0.5), headR * 1.04, 70, 235, dropAlpha);

                double maxAllowedTail = std::max(0.8, double(wd.relY) - 1.0);
                double desiredTail = headR * (2.60 + 3.20 * std::clamp(double(wd.stretchFactor), 0.15, 2.2));
                double tailLen = std::min(maxAllowedTail, desiredTail);
                double tailShiftX = -tailLen * wd.tailDirX;

                for (int seg = 0; seg < 14; ++seg) {
                    double t = seg / 13.0;
                    double pinchWave = 1.0 + 0.15 * std::sin(t * 5.2 - wd.relY * 0.15) * t;
                    double sr = headR * (1.0 - 0.80 * std::pow(t, 0.70)) * pinchWave;
                    double meanderX = std::sin(t * 4.2 + phase) * headR * 0.30 * t * (1.0 - t * 0.35);
                    double sx = cx + tailShiftX * t + meanderX;
                    double sy = std::max(r.top() + sr * 0.5, cy - tailLen * t);
                    int bHeight = std::clamp(int(242.0 * (1.0 - 0.30 * t)), 115, 242);
                    double aspectX = 0.94 + ((seg == 0) ? wobbleX : (-wobbleX * 0.5));
                    drawDome(sx, sy, sr * aspectX, sr * (seg == 0 ? 1.08 : 1.16), 72, bHeight, dropAlpha);
                }
            }
        }

        painter->restore();
        if (needScrollClip) painter->restore();
    }

    // 5. Брызги в воздухе (gPeak = 0 -> без волнового искажения стекла)
    painter->save();
    painter->setCompositionMode(QPainter::CompositionMode_Lighten);
    for (const auto &sp : src->m_splashes) {
        float ratio = std::clamp(sp.life / sp.maxLife, 0.0f, 1.0f);
        double sr = std::clamp(sp.radius * (0.55 + 0.35 * ratio), 0.8, 1.8) * dScale;
        int bVal = std::clamp(int(225.0 * ratio), 80, 225);
        drawDome(sp.x, sp.y, sr, sr * 1.15, 0, bVal, 245);
    }

    // 6. Падающие струи дождя и сорвавшиеся в полёт капли (FallingDrop: gPeak = 0 -> падают строго прямо, без «змейки»!)
    bool suppressInAppRainStreaks = (mgr && !mgr->rainEnabled());
    for (const auto &p : src->m_particles) {
        if (suppressInAppRainStreaks && p.type == Rain) continue;
        if (p.type == FallingDrop) {
            double rDrop = std::clamp(double(p.size) * 0.82, 2.1, 4.0) * dScale;
            double stretchMul = std::clamp(double(p.vy) / 180.0, 1.3, 3.4);
            for (int fd = 0; fd < 7; ++fd) {
                double u = fd / 6.0;
                double sr = rDrop * (1.0 - 0.72 * u);
                drawDome(p.x - p.vx * 0.025 * u, p.y - rDrop * 3.2 * stretchMul * u, sr * 0.82, sr * 1.45, 0, int(242 - 95 * u), 255);
            }
        } else if (p.type == Rain) {
            if (p.depth < 0.52f) {
                bool behindObstacle = false;
                for (const auto &obs : src->m_obstacles) {
                    if (obs.id == "window_glass" || obs.id == "window_top_ledge" || obs.isWaybar) continue;
                    if (obs.isWindow && (src->m_lastTickMs - obs.createdMs <= 420)) continue;
                    if (obs.rect.contains(QPointF(p.x, p.y))) {
                        behindObstacle = true;
                        break;
                    }
                }
                if (behindObstacle) continue;
            }
            // Увеличенные, более длинные и толстые струи дождя!
            double dy = p.vy * 0.095, dx = p.vx * 0.095;
            int rInt = std::clamp(int(p.alpha * 185.0f), 42, 205), bInt = std::clamp(int(p.depth * 150.0f), 35, 175);
            int tiltEnc = std::clamp(int(128.0 + (dx / std::max(6.0, dy)) * 180.0), 10, 245);
            double penW = std::clamp(1.15 + p.size * 0.85, 1.25, 2.95);
            painter->setPen(QPen(QColor(int(rInt * 0.52), 45, int(bInt * 0.60), tiltEnc), penW * 0.72, Qt::SolidLine, Qt::FlatCap));
            painter->drawLine(QPointF(p.x - dx, p.y - dy), QPointF(p.x - dx * 0.42, p.y - dy * 0.42));
            painter->setPen(QPen(QColor(rInt, 55, bInt, tiltEnc), penW, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(QPointF(p.x - dx * 0.42, p.y - dy * 0.42), QPointF(p.x, p.y));
            painter->setPen(Qt::NoPen);
        }
    }
    // 6b. Водяные капельки и проталинки от тающих на окнах и экране снежинок
    if (mgr && mgr->snowStickMeltEnabled()) {
        double sScale = std::clamp(double(mgr->snowflakeScale()), 0.3, 6.0);
        for (const auto &p : src->m_particles) {
            if (p.type == Snow && p.resting && p.bendAmount > 0.2f) {
                float meltProg = 1.0f - std::clamp(p.restTimer / p.bendAmount, 0.0f, 1.0f);
                float waterPhase = std::sin(meltProg * 3.14159f);
                if (waterPhase > 0.08f) {
                    double meltR = std::clamp(double(p.size) * sScale * 0.36 * (0.65 + 0.45 * waterPhase), 1.1, 3.6) * dScale;
                    int bVal = std::clamp(int(195.0f * waterPhase), 60, 210);
                    drawDome(p.x, p.y + meltProg * 1.2, meltR * 0.92, meltR * 1.08, 62, bVal, 235);
                }
            }
        }
    }

    // 7. Отпечатки прилипших к экрану листьев и их смазанный след при взлёте
    int impIdx = src->m_globalMode ? 1 : 0;
    if (mgr && mgr->condensationEnabled() && !s_leafImprints[impIdx].isEmpty()) {
        ensureLeafAssetsGenerated();
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        for (const auto &imp : s_leafImprints[impIdx]) {
            int maskIdx = std::clamp(imp.kind, 0, 2) * 4 + std::clamp(imp.var, 0, 3);
            if (maskIdx < 0 || maskIdx >= s_leafImprintMasks.size()) continue;
            const QImage &maskTex = s_leafImprintMasks[maskIdx];
            float lifeRatio = std::pow(std::clamp(imp.life / imp.maxLife, 0.0f, 1.0f), 0.65f);
            float distSmudge = std::hypot(imp.x2 - imp.x1, imp.y2 - imp.y1);
            int smudgeSteps = (distSmudge > 2.0f) ? 6 : 1;
            for (int s = 0; s < smudgeSteps; ++s) {
                float u = (smudgeSteps > 1) ? (float(s) / float(smudgeSteps - 1)) : 0.0f;
                float cx = imp.x1 + (imp.x2 - imp.x1) * u;
                float cy = imp.y1 + (imp.y2 - imp.y1) * u;
                float ang = imp.angle1 + (imp.angle2 - imp.angle1) * u;
                // Основной отпечаток (s == 0) чёткий, а шлейф смазывания (s > 0) мягко тянется за улетающим листом
                float stepWeight = (s == 0) ? 0.72f : (0.34f * (1.0f - u * 0.72f));
                painter->save();
                painter->translate(cx, cy);
                painter->rotate(ang);
                // Слегка растягиваем смазанные слои вдоль движения срыва
                double stretchSmudge = (s > 0) ? (1.0 + 0.12 * u) : 1.0;
                painter->scale(1.0, stretchSmudge);
                painter->setOpacity(std::clamp(double(lifeRatio * stepWeight), 0.0, 0.95));
                QRectF dstR(-imp.drawSz * 0.5, -imp.drawSz * 0.5, imp.drawSz, imp.drawSz);
                painter->drawImage(dstR, maskTex);
                painter->restore();
            }
        }
    }

    painter->restore();
}

void AtmosphereCanvas::paintOverlayElements(QPainter *painter, const AtmosphereCanvas *src) {
    auto *mgr = AtmosphereManager::instance();
    if (!mgr) return;
    if (mgr->snowDriftEnabled() && mgr->snowEnabled()) {
        ensureSnowAssetsGenerated();
        double sfScale = std::clamp(double(mgr->snowflakeScale()), 0.3, 6.0);
        for (const auto &obs : src->m_obstacles) {
            const QRectF &r = obs.rect;
            if (obs.snowBins.isEmpty() || obs.id == "window_glass" || obs.id == "window_top_ledge") continue;
            float maxH = 0.0f;
            for (float v : obs.snowBins) maxH = std::max(maxH, v);
            if (maxH <= 0.20f) continue;

            int n = obs.snowBins.size();
            double leftX = r.left() + 6.0;
            double spanW = std::max(10.0, r.width() - 12.0);
            double stepX = spanW / double(n - 1);
            double baseY = obs.isWaybar ? (r.top() + 1.0) : (r.top() + 0.4);
            // На Waybar ограничиваем максимальную высоту кучки (не более 4.6px и минимум в 3.2px от верхнего края монитора y=0!)
            double maxWbCap = std::min(4.6, std::max(1.2, baseY - 3.2));

            uint obsSeed = qHash(obs.id);
            double seedPh1 = double(obsSeed & 0xFF) * 0.031 + double(obs.snowPhaseShift);
            double seedPh2 = double((obsSeed >> 8) & 0xFF) * 0.027;

            QVector<double> heights(n, 0.0);
            QVector<double> overhangs(n, 0.0);
            int peakIdx = -1;
            double peakVal = 0.0;
            for (int i = 0; i < n; ++i) {
                double u = double(i) / double(n - 1);
                double taper = std::sin(u * 3.14159);
                double h = double(obs.snowBins[i]) * std::pow(taper, 0.55);
                if (obs.isWaybar) h = std::min(h, maxWbCap);
                heights[i] = (h > 0.18) ? h : 0.0;
                if (heights[i] > 1.2) {
                    double ovWave = std::sin(u * 19.0 + seedPh1) * 0.55 + std::cos(u * 11.0 - seedPh2) * 0.45;
                    double ovMask = std::clamp(ovWave + 0.22, 0.0, 1.0);
                    double maxOv = obs.isWaybar ? 2.6 : 10.5;
                    overhangs[i] = std::min(maxOv, heights[i] * 0.62 * std::pow(ovMask, 1.35)) * taper;
                }
                if (!obs.isWaybar && i >= 3 && i <= n - 4 && heights[i] > peakVal) {
                    peakVal = heights[i];
                    peakIdx = i;
                }
            }

            painter->save();
            painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter->setPen(Qt::NoPen);

            // Разбиваем массив на отдельные независимые кучки (островки снега), где heights[i] > 0
            int idxStart = 0;
            while (idxStart < n) {
                while (idxStart < n && heights[idxStart] <= 0.01) ++idxStart;
                if (idxStart >= n) break;
                int idxEnd = idxStart;
                double segMaxH = 0.0;
                double segMaxOv = 0.0;
                while (idxEnd < n && heights[idxEnd] > 0.01) {
                    segMaxH  = std::max(segMaxH, heights[idxEnd]);
                    segMaxOv = std::max(segMaxOv, overhangs[idxEnd]);
                    ++idxEnd;
                }
                --idxEnd;

                double fadeAlpha = std::clamp((segMaxH - 0.18) / 2.8, 0.05, 1.0);
                double xStart = leftX + std::max(0.0, (double(idxStart) - 0.65) * stepX);
                double xEnd   = leftX + std::min(spanW, (double(idxEnd) + 0.65) * stepX);

                // 0) 🧊 ЛЕДЯНЫЕ СОСУЛЬКИ, ОБРАЗУЮЩИЕСЯ ПОД КУЧКАМИ И НАПЛЫВАМИ СНЕГА НА ПОВЕРХНОСТИ ОКНА:
                for (int i = idxStart; i <= idxEnd; ++i) {
                    double h = heights[i];
                    if (h < 3.4) continue;
                    uint icHash = obsSeed ^ uint(i * 463 + 137);
                    // Сосульки растут группами в местах, где накопился снег (~42% ячеек сугроба)
                    if ((icHash % 100) >= 44) continue;

                    double icGrow = std::clamp((h - 3.2) / 9.5, 0.0, 1.0);
                    int subCount = ((icHash >> 8) % 3 == 0 && h > 6.0) ? 2 : 1;
                    for (int s = 0; s < subCount; ++s) {
                        uint sh = icHash ^ uint(s * 719);
                        double icX = leftX + double(i) * stepX + (int(sh % 9) - 4) * (stepX * 0.09) + s * 4.2;
                        if (icX <= r.left() + 8.0 || icX >= r.right() - 8.0) continue;
                        double icRootY = baseY + overhangs[i] * 0.65;
                        double icLen = (5.5 + double(sh % 17) * 1.15) * icGrow * (s == 0 ? 1.0 : 0.62);
                        double icHalfW = std::clamp(1.25 + icLen * 0.085, 1.2, 3.2);
                        if (icLen < 2.2) continue;

                        // Ребристый контур сосульки, сужающийся к острому ледяному кончику
                        QPainterPath icPath;
                        icPath.moveTo(icX - icHalfW, icRootY);
                        icPath.quadTo(icX - icHalfW * 0.55, icRootY + icLen * 0.48, icX, icRootY + icLen);
                        icPath.quadTo(icX + icHalfW * 0.55, icRootY + icLen * 0.48, icX + icHalfW, icRootY);
                        icPath.closeSubpath();

                        QLinearGradient icGrad(icX - icHalfW, icRootY, icX + icHalfW, icRootY);
                        icGrad.setColorAt(0.00, QColor(240, 250, 255, int(225 * fadeAlpha)));
                        icGrad.setColorAt(0.45, QColor(185, 222, 252, int(190 * fadeAlpha)));
                        icGrad.setColorAt(1.00, QColor(130, 178, 228, int(210 * fadeAlpha)));

                        painter->setPen(QPen(QColor(215, 238, 255, int(165 * fadeAlpha)), 0.55));
                        painter->setBrush(icGrad);
                        painter->drawPath(icPath);

                        // Продольный ледяной блик по левой грани сосульки
                        painter->setPen(QPen(QColor(255, 255, 255, int(210 * fadeAlpha)), 0.65, Qt::SolidLine, Qt::RoundCap));
                        painter->drawLine(QPointF(icX - icHalfW * 0.42, icRootY + 1.0),
                                          QPointF(icX - icHalfW * 0.12, icRootY + icLen * 0.78));
                    }
                }

                // 1) Торчащие части снежинок на верхушках кучки и на снежных наплывах
                for (int i = idxStart; i <= idxEnd; ++i) {
                    double h = heights[i];
                    if (h < 2.2) continue;
                    uint hsh = obsSeed ^ uint(i * 197 + 911);
                    const QImage &sTex = s_snowflakeTextures[int(hsh % 8)];
                    double flakeR = std::clamp(h * 0.48 * (0.80 + 0.20 * std::min(2.5, sfScale)), 1.6, obs.isWaybar ? 2.6 : 7.2);
                    double cx = leftX + double(i) * stepX + (int(hsh >> 4) % 7 - 3) * (stepX * 0.10);
                    double cy = baseY - h * 0.52 + overhangs[i] * 0.22;

                    painter->save();
                    painter->translate(cx, cy);
                    painter->rotate(double(hsh % 360));
                    painter->setOpacity(0.88 * fadeAlpha);
                    painter->drawImage(QRectF(-flakeR, -flakeR, flakeR * 2.0, flakeR * 2.0), sTex);
                    painter->restore();
                }

                // 2) Контур снежной кучки: сверху пушистый гребень, а снизу в некоторых местах мягкими языками ЗАХОДИТ НА ПОВЕРХНОСТЬ ОКНА!
                QPainterPath moundPath;
                moundPath.moveTo(xStart, baseY);
                QPointF prevPt(xStart, baseY);
                for (int i = idxStart; i <= idxEnd; ++i) {
                    QPointF curPt(leftX + double(i) * stepX, baseY - heights[i]);
                    QPointF mid = (prevPt + curPt) * 0.5;
                    moundPath.quadTo(prevPt, mid);
                    prevPt = curPt;
                }
                QPointF endPt(xEnd, baseY);
                moundPath.quadTo(prevPt, (prevPt + endPt) * 0.5);
                moundPath.lineTo(endPt);

                // Нижняя кромка сугроба, свисающая в некоторых местах на поверхность окна (overhangs[i] > 0)
                QPointF prevBotPt = endPt;
                for (int i = idxEnd; i >= idxStart; --i) {
                    QPointF curBotPt(leftX + double(i) * stepX, baseY + overhangs[i]);
                    QPointF midBot = (prevBotPt + curBotPt) * 0.5;
                    moundPath.quadTo(prevBotPt, midBot);
                    prevBotPt = curBotPt;
                }
                QPointF startPt(xStart, baseY);
                moundPath.quadTo(prevBotPt, (prevBotPt + startPt) * 0.5);
                moundPath.lineTo(startPt);
                moundPath.closeSubpath();

                QLinearGradient sg(0, baseY - std::max(3.0, segMaxH), 0, baseY + std::max(2.0, segMaxOv));
                sg.setColorAt(0.00, QColor(252, 254, 255, int(242 * fadeAlpha)));
                sg.setColorAt(0.55, QColor(224, 239, 254, int(225 * fadeAlpha)));
                sg.setColorAt(1.00, QColor(174, 204, 238, int(198 * fadeAlpha)));

                painter->setPen(Qt::NoPen);
                painter->setBrush(sg);
                painter->drawPath(moundPath);

                idxStart = idxEnd + 1;
            }

            // 3) Единственный на экране маленький снеговичок, который МЕДЛЕННО формируется прямо из снежной кучки (с 0% размера и прозрачности)!
            bool skipInAppSnowman = (!src->m_globalMode && mgr->globalOverlayEnabled() && mgr->syncWithOverlay());
            if (!skipInAppSnowman && obs.snowmanProg > 0.01f && peakIdx >= 3 && peakVal > 5.0) {
                double p = std::clamp(double(obs.snowmanProg), 0.0, 1.0);
                double smScale = std::clamp(peakVal / 14.5, 0.70, 1.15);
                double smX = leftX + double(peakIdx) * stepX;
                double smBaseY = baseY - peakVal * 0.48;

                // Фаза 1 (p: 0.0 -> 0.42): нижний ком медленно вырастает с 0 из вершины сугроба
                double p1 = std::clamp(p / 0.42, 0.0, 1.0);
                double ease1 = p1 * p1 * (3.0 - 2.0 * p1);
                double r1 = 5.4 * smScale * ease1;
                double y1 = smBaseY - r1 * 0.62 * ease1;
                painter->setPen(QPen(QColor(175, 202, 232, int(195 * ease1 * ease1)), 0.80));
                painter->setBrush(QColor(246, 251, 255, int(236 * ease1)));
                painter->drawEllipse(QPointF(smX, y1), r1 * 1.08, r1 * 0.94);

                // Фаза 2 (p: 0.38 -> 0.72): из нижнего кома плавно вырастает средний ком, затем голова
                if (p > 0.38) {
                    double p2 = std::clamp((p - 0.38) / 0.34, 0.0, 1.0);
                    double ease2 = p2 * p2 * (3.0 - 2.0 * p2);
                    double r2 = 3.9 * smScale * ease2;
                    double y2 = y1 - r1 * 0.76 - r2 * 0.62 * ease2;
                    painter->setPen(QPen(QColor(175, 202, 232, int(205 * ease2 * ease2)), 0.75));
                    painter->setBrush(QColor(248, 252, 255, int(240 * ease2)));
                    painter->drawEllipse(QPointF(smX, y2), r2 * 1.04, r2 * 0.94);

                    if (p > 0.62) {
                        double p3 = std::clamp((p - 0.62) / 0.24, 0.0, 1.0);
                        double ease3 = p3 * p3 * (3.0 - 2.0 * p3);
                        double r3 = 2.85 * smScale * ease3;
                        double y3 = y2 - r2 * 0.78 - r3 * 0.65 * ease3;
                        painter->setPen(QPen(QColor(175, 202, 232, int(215 * ease3 * ease3)), 0.72));
                        painter->setBrush(QColor(250, 253, 255, int(244 * ease3)));
                        painter->drawEllipse(QPointF(smX, y3), r3, r3 * 0.96);

                        // Фаза 3 (p: 0.80 -> 1.00): медленно прорисовываются веточки, угольки, морковка и шарфик
                        if (p > 0.80) {
                            double pd = std::clamp((p - 0.80) / 0.20, 0.0, 1.0);
                            pd = pd * pd;
                            // Веточки-ручки (растут из среднего кома)
                            painter->setPen(QPen(QColor(92, 62, 38, int(230 * pd)), 0.95 * smScale, Qt::SolidLine, Qt::RoundCap));
                            painter->drawLine(QPointF(smX - r2 * 0.85, y2 - r2 * 0.1),
                                              QPointF(smX - r2 * (0.85 + 0.95 * pd), y2 - r2 * (0.1 + 0.75 * pd)));
                            painter->drawLine(QPointF(smX + r2 * 0.85, y2 - r2 * 0.1),
                                              QPointF(smX + r2 * (0.85 + 0.95 * pd), y2 - r2 * (0.1 + 0.65 * pd)));

                            // Глазки-угольки и пуговки
                            painter->setPen(Qt::NoPen);
                            painter->setBrush(QColor(28, 32, 40, int(245 * pd)));
                            double eyeR = std::max(0.55, 0.68 * smScale);
                            painter->drawEllipse(QPointF(smX - r3 * 0.36, y3 - r3 * 0.18), eyeR, eyeR);
                            painter->drawEllipse(QPointF(smX + r3 * 0.36, y3 - r3 * 0.18), eyeR, eyeR);
                            painter->drawEllipse(QPointF(smX, y2 - r2 * 0.22), eyeR * 0.85, eyeR * 0.85);
                            painter->drawEllipse(QPointF(smX, y2 + r2 * 0.28), eyeR * 0.85, eyeR * 0.85);

                            // Носик-морковка
                            QPolygonF nose;
                            nose << QPointF(smX, y3 - r3 * 0.04)
                                 << QPointF(smX + r3 * 1.15 * pd, y3 + r3 * 0.12)
                                 << QPointF(smX, y3 + r3 * 0.22);
                            painter->setBrush(QColor(245, 118, 26, int(248 * pd)));
                            painter->drawPolygon(nose);

                            // Маленький красный шарфик
                            painter->setBrush(QColor(210, 42, 46, int(235 * pd)));
                            painter->drawRoundedRect(QRectF(smX - r3 * 0.82 * pd, y3 + r3 * 0.68, r3 * 1.64 * pd, 1.85 * smScale), 0.9, 0.9);
                        }
                    }
                }
            }
            painter->restore();
        }
    }

    bool suppressInAppOverlayParticles = false;
    for (const auto &p : src->m_particles) {
        if (suppressInAppOverlayParticles) continue;
        bool behindTranslucentWin = false;
        if (p.type != Leaf && p.depth < ((p.type == Snow) ? 0.34f : 0.52f) && !p.resting) {
            for (const auto &obs : src->m_obstacles) {
                if (obs.id != "window_glass" && obs.id != "window_top_ledge" && !obs.isWaybar &&
                    (!obs.isWindow || (src->m_lastTickMs - obs.createdMs > 420)) &&
                    obs.rect.contains(QPointF(p.x, p.y))) {
                    behindTranslucentWin = true;
                    break;
                }
            }
        }
        if (p.type == Rain) {
            if (!mgr->rainEnabled()) continue;
            double dy = p.vy * 0.088, dx = p.vx * 0.088;
            QLinearGradient rg(QPointF(p.x - dx, p.y - dy), QPointF(p.x, p.y));
            int aTop = std::clamp(int(p.alpha * 115.0f), 25, 135);
            rg.setColorAt(0.0, QColor(200, 225, 255, 0));
            rg.setColorAt(0.45, QColor(210, 232, 255, aTop / 2));
            rg.setColorAt(1.0, QColor(235, 246, 255, aTop));
            painter->setPen(QPen(QBrush(rg), std::clamp(1.0 + p.size * 0.45, 1.1, 2.3), Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(QPointF(p.x - dx, p.y - dy), QPointF(p.x, p.y));
            painter->setPen(Qt::NoPen);
            continue;
        }
        if (p.type == Snow) {
            ensureSnowAssetsGenerated();
            double sfScale = std::clamp(double(mgr->snowflakeScale()), 0.3, 6.0);
            if (p.leafKind == 8) {
                // Крупные белые мягкие круги (боке) на заднем плане, падающие медленно
                double bokehR = p.size * 1.65 * (0.70 + 0.30 * sfScale);
                float visAlpha = behindTranslucentWin ? (p.alpha * 0.35f) : p.alpha;
                QRadialGradient bgrad(QPointF(p.x, p.y), bokehR);
                bgrad.setColorAt(0.00, QColor(255, 255, 255, std::clamp(int(visAlpha * 165.0f), 0, 180)));
                bgrad.setColorAt(0.52, QColor(246, 251, 255, std::clamp(int(visAlpha * 115.0f), 0, 140)));
                bgrad.setColorAt(0.82, QColor(238, 247, 255, std::clamp(int(visAlpha * 42.0f), 0, 70)));
                bgrad.setColorAt(1.00, QColor(235, 245, 255, 0));
                painter->setPen(Qt::NoPen);
                painter->setBrush(bgrad);
                painter->drawEllipse(QPointF(p.x, p.y), bokehR, bokehR);
            } else if (behindTranslucentWin) {
                QColor sc = p.color;
                sc.setAlphaF(p.alpha * 0.26f);
                double snowR = p.size * 1.35 * sfScale;
                QRadialGradient sgrad(p.x, p.y, snowR);
                sgrad.setColorAt(0.0, sc); QColor edge = sc; edge.setAlphaF(0.0f); sgrad.setColorAt(1.0, edge);
                painter->setPen(Qt::NoPen); painter->setBrush(sgrad); painter->drawEllipse(QPointF(p.x, p.y), snowR, snowR);
            } else {
                int sKind = std::clamp(p.leafKind, 0, 7);
                const QImage &sTex = s_snowflakeTextures[sKind];
                float meltProg = (p.resting && p.bendAmount > 0.2f)
                    ? (1.0f - std::clamp(p.restTimer / p.bendAmount, 0.0f, 1.0f))
                    : 0.0f;
                // При таянии кристалл снежинки плавно уменьшается и растворяется, уступая место водяной капельке
                double meltScale = p.resting ? (1.0 - 0.52 * meltProg) : 1.0;
                double meltAlpha = p.resting ? (p.alpha * std::pow(1.0 - meltProg, 1.35)) : p.alpha;
                double drawR = p.size * 1.55 * sfScale * meltScale;

                painter->save();
                painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
                painter->translate(p.x, p.y);
                painter->rotate(p.angle);
                if (!p.resting) {
                    double tiltS = 0.58 + 0.42 * std::abs(std::cos(double(p.tiltPhase)));
                    painter->scale(1.0, tiltS);
                }
                painter->setOpacity(std::clamp(meltAlpha, 0.0, 1.0));
                painter->drawImage(QRectF(-drawR, -drawR, drawR * 2.0, drawR * 2.0), sTex);
                painter->restore();
            }
        } else if (p.type == Leaf) {
            ensureLeafAssetsGenerated();
            int pal = std::clamp(mgr->leavesPalette(), 0, 3);
            int kind = std::clamp(p.leafKind, 0, 2);
            int var = int(std::abs(p.tiltSpeed * 10.0f)) % 4;
            const QImage &tex = s_leafTextures[pal * 12 + kind * 4 + var];

            // Честный 3D-растеризатор изометрической сетки листа (16x16) с Z-буфером, спиральным закручиванием и загибом лопастей в 3D
            double det = mgr ? std::clamp(double(mgr->leavesDetail()), 0.0, 2.0) : 1.0;
            double rareBoost = (mgr && mgr->leavesIntensity() < 0.25) ? (1.0 + (0.25 - mgr->leavesIntensity()) * 1.4 * std::min(1.0, det)) : 1.0;
            double depthScale = p.resting ? 1.0 : (0.56 + 0.50 * double(p.depth));
            double drawSz = p.size * 2.35 * (mgr ? mgr->leavesScale() : 1.0) * rareBoost * depthScale;

            const int G = 16; // 17x17 вершин = 512 3D-треугольников
            struct Vtx3D { float x, y, z, u, v, shade; int backFace; };
            Vtx3D vtx[(G + 1) * (G + 1)];

            bool onLedge   = (p.resting && p.relY <= 0.5f);
            bool stuckFlat = (p.resting && p.relY > 0.5f);
            // Прогресс отклеивания верхней части листа перед взлётом (0.0 пока сидит, 0.0 -> 1.0 за последние 0.85 сек)
            double peelRaw    = stuckFlat ? std::clamp((0.85 - double(p.restTimer)) / 0.85, 0.0, 1.0) : 0.0;
            double peelSmooth = peelRaw * peelRaw * (3.0 - 2.0 * peelRaw);
            double dMul       = stuckFlat ? 0.14 : (0.35 + 0.65 * det);

            double curlPitch = onLedge ? 0.0 : ((std::sin(double(p.swayPhase) * 1.3 + 0.4) * 0.75 + double(p.bendAmount) * 0.85) * dMul);
            double tipCurl   = onLedge ? 0.0 : (std::sin(double(p.tiltPhase) * 0.9 - 0.6) * 1.15 * dMul);
            double twistAmp  = onLedge ? 0.0 : (stuckFlat ? (peelSmooth * 0.35) : (std::cos(double(p.swayPhase) * 1.1 + double(p.tiltPhase) * 0.7) * 0.95 * dMul));
            double cupAmp    = onLedge ? (0.25 * p.bendAmount) : (stuckFlat ? (0.10 + peelSmooth * 0.42) : (std::sin(double(p.swayPhase) * 1.5) * 0.70 * dMul));
            double lobeCurl  = onLedge ? 0.15 : (stuckFlat ? (0.12 + peelSmooth * 0.35) : (0.55 * dMul));

            // При прилипании к экрану ориентируем лист верхушкой вверх (yawRad -> 0 при отклеивании), чтобы он отгибался именно СВЕРХУ вниз!
            double baseYawDeg = double(p.angle);
            if (stuckFlat) {
                double normAng = std::fmod(baseYawDeg, 360.0);
                if (normAng > 180.0) normAng -= 360.0;
                if (normAng < -180.0) normAng += 360.0;
                baseYawDeg = normAng * (1.0 - 0.65 * peelSmooth);
            }
            double yawRad   = (onLedge ? (p.angle * 0.35) : baseYawDeg) * (3.14159265 / 180.0);
            double pitchRad = onLedge ? 0.0 : (stuckFlat ? (0.08 + peelSmooth * 0.32) : (std::sin(double(p.tiltPhase) * 0.85) * 0.82));
            double rollRad  = onLedge ? 0.0 : (stuckFlat ? 0.06 : (std::cos(double(p.tiltPhase) * 0.65 + double(p.swayPhase) * 0.5) * 0.88));

            double cy = std::cos(yawRad),   sy = std::sin(yawRad);
            double cp = std::cos(pitchRad), sp = std::sin(pitchRad);
            double cr = std::cos(rollRad),  sr = std::sin(rollRad);

            auto rot3D = [&](double x, double y, double z, double &ox, double &oy, double &oz) {
                double x1 =  x * cr + z * sr;
                double y1 =  y;
                double z1 = -x * sr + z * cr;
                double x2 =  x1;
                double y2 =  y1 * cp - z1 * sp;
                double z2 =  y1 * sp + z1 * cp;
                ox = x2 * cy - y2 * sy;
                oy = x2 * sy + y2 * cy;
                oz = z2;
            };

            // Шаг 1: Изометрическое интегрирование центральной жилки (при отклеивании точка опоры внизу anchorJ = 12, а верх отгибается вперёд-вниз!)
            double spineX[G + 1], spineY[G + 1], spineZ[G + 1], spineSlope[G + 1];
            int midJ = stuckFlat ? 11 : (G / 2);
            double anchorY = (double(midJ) / double(G)) - 0.5;
            spineX[midJ] = 0.0; spineY[midJ] = anchorY; spineZ[midJ] = 0.0; spineSlope[midJ] = 0.0;
            double stepLen = 1.0 / double(G);
            double fold = std::clamp(double(p.bendAmount), 0.0, 1.0);

            auto calcSpineAngles = [&](double vn, double &aPitch, double &aYaw) {
                if (onLedge) {
                    double edgeT = std::clamp((vn + 0.15) * 4.5, -1.0, 1.0);
                    aPitch = (edgeT < 0.0) ? (-1.10 * fold * (-edgeT)) : (0.25 * fold * edgeT);
                    aYaw   = (p.relX < 0.14f ? -0.35 : (p.relX > 0.86f ? 0.35 : 0.12)) * fold * vn;
                } else if (stuckFlat) {
                    // Отклеивание сверху: нижняя часть (vn > 0.35) ещё держится за стекло, а верхняя (vn < 0.35) отгибается от стекла (+Z) и заваливается вниз!
                    double topWeight = std::clamp((0.42 - vn) / 1.42, 0.0, 1.0);
                    aPitch = -peelSmooth * 2.15 * std::pow(topWeight, 1.15);
                    aYaw   = peelSmooth * 0.18 * topWeight;
                } else {
                    double tipF = std::pow(std::max(0.0, -vn), 1.6);
                    aPitch = curlPitch * vn + tipCurl * tipF;
                    aYaw   = double(p.bendAmount) * 0.42 * vn * dMul;
                }
            };

            for (int j = midJ + 1; j <= G; ++j) {
                double vn = (double(j) / G) * 2.0 - 1.0;
                double aP, aY; calcSpineAngles(vn, aP, aY);
                spineX[j] = spineX[j - 1] + stepLen * std::sin(aY) * std::cos(aP);
                spineY[j] = spineY[j - 1] + stepLen * std::cos(aY) * std::cos(aP);
                spineZ[j] = spineZ[j - 1] + stepLen * std::sin(aP);
                spineSlope[j] = aP;
            }
            for (int j = midJ - 1; j >= 0; --j) {
                double vn = (double(j) / G) * 2.0 - 1.0;
                double aP, aY; calcSpineAngles(vn, aP, aY);
                spineX[j] = spineX[j + 1] - stepLen * std::sin(aY) * std::cos(aP);
                spineY[j] = spineY[j + 1] - stepLen * std::cos(aY) * std::cos(aP);
                spineZ[j] = spineZ[j + 1] - stepLen * std::sin(aP);
                spineSlope[j] = aP;
            }

            // Шаг 2: Изометрическое интегрирование крыльев вдоль u (спиральный твист + закручивание краёв лопастей без сжатия!)
            for (int j = 0; j <= G; ++j) {
                double vn = (double(j) / G) * 2.0 - 1.0;
                double localTwist = twistAmp * vn * (1.0 - 0.25 * vn * vn);
                int midI = G / 2;

                auto setVertex = [&](int i, double lx, double ly, double lz, double wingAng) {
                    double rx, ry, rz;
                    rot3D(lx * drawSz, ly * drawSz, lz * drawSz, rx, ry, rz);
                    // Мягкая 3D-перспектива
                    double persp = 1.0 / std::clamp(1.0 - rz / (drawSz * 3.2), 0.65, 1.45);
                    double sx = rx * persp;
                    double sy = ry * persp;

                    // Вычисляем 3D-нормаль поверхности листа для освещения и определения изнанки (backFace)
                    double nx0 = -std::sin(wingAng), ny0 = -std::sin(spineSlope[j]) * std::cos(wingAng), nz0 = std::cos(wingAng) * std::cos(spineSlope[j]);
                    double rnx, rny, rnz;
                    rot3D(nx0, ny0, nz0, rnx, rny, rnz);
                    int isBack = (rnz < 0.0) ? 1 : 0;
                    double absNz = std::abs(rnz);
                    double light = 0.72 + 0.34 * std::max(0.0, -rnx * 0.45 - rny * 0.55 + absNz * 0.70);
                    if (isBack) light *= 0.82; // Изнаночная сторона загнутого кончика чуть матовее и темнее

                    Vtx3D &vOut = vtx[j * (G + 1) + i];
                    vOut.x = float(sx);
                    vOut.y = float(sy);
                    vOut.z = float(rz);
                    vOut.u = float(double(i) / G * (tex.width() - 1));
                    vOut.v = float(double(j) / G * (tex.height() - 1));
                    vOut.shade = float(std::clamp(light, 0.52, 1.15));
                    vOut.backFace = isBack;
                };

                setVertex(midI, spineX[j], spineY[j], spineZ[j], localTwist);

                // Правое крыло (i > midI) и левое крыло (i < midI) с сохранением длины дуги шаг за шагом
                double curX = spineX[j], curY = spineY[j], curZ = spineZ[j];
                for (int i = midI + 1; i <= G; ++i) {
                    double un = (double(i) / G) * 2.0 - 1.0;
                    double wAng = localTwist + cupAmp * un + lobeCurl * un * std::abs(un);
                    curX += stepLen * std::cos(wAng);
                    curZ += stepLen * std::sin(wAng);
                    setVertex(i, curX, curY, curZ, wAng);
                }
                curX = spineX[j]; curY = spineY[j]; curZ = spineZ[j];
                for (int i = midI - 1; i >= 0; --i) {
                    double un = (double(i) / G) * 2.0 - 1.0;
                    double wAng = localTwist + cupAmp * un + lobeCurl * un * std::abs(un);
                    curX -= stepLen * std::cos(wAng);
                    curZ -= stepLen * std::sin(wAng);
                    setVertex(i, curX, curY, curZ, wAng);
                }
            }

            // Шаг 3: Быстрая растеризация 3D-треугольников с Z-буфером в локальный буфер листа
            int bufSz = std::clamp(int(std::ceil(drawSz * 1.45)) + 6, 24, 520);
            int halfBuf = bufSz / 2;
            static QImage s_leafBuf;
            static QVector<float> s_zBuf;
            if (s_leafBuf.width() < bufSz || s_leafBuf.height() < bufSz) {
                int allocSz = std::max(bufSz, 256);
                s_leafBuf = QImage(allocSz, allocSz, QImage::Format_ARGB32_Premultiplied);
                s_zBuf.resize(allocSz * allocSz);
            }
            int stride = s_leafBuf.width();
            for (int y = 0; y < bufSz; ++y) {
                memset(s_leafBuf.scanLine(y), 0, bufSz * sizeof(QRgb));
                std::fill_n(s_zBuf.data() + y * stride, bufSz, -1e9f);
            }

            const QRgb *texBits = reinterpret_cast<const QRgb *>(tex.constBits());
            int texW = tex.width(), texH = tex.height();
            QRgb *dstBits = reinterpret_cast<QRgb *>(s_leafBuf.bits());
            float *zBits  = s_zBuf.data();

            auto rasterTri = [&](const Vtx3D &A, const Vtx3D &B, const Vtx3D &C) {
                float ax = A.x + halfBuf, ay = A.y + halfBuf;
                float bx = B.x + halfBuf, by = B.y + halfBuf;
                float cx = C.x + halfBuf, cy = C.y + halfBuf;
                float area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
                if (std::abs(area) < 0.05f) return;
                float invArea = 1.0f / area;

                int minX = std::max(0, int(std::floor(std::min({ax, bx, cx}))));
                int maxX = std::min(bufSz - 1, int(std::ceil(std::max({ax, bx, cx}))));
                int minY = std::max(0, int(std::floor(std::min({ay, by, cy}))));
                int maxY = std::min(bufSz - 1, int(std::ceil(std::max({ay, by, cy}))));

                for (int py = minY; py <= maxY; ++py) {
                    float fy = py + 0.5f;
                    QRgb *rowDst = dstBits + py * stride;
                    float *rowZ  = zBits + py * stride;
                    for (int px = minX; px <= maxX; ++px) {
                        float fx = px + 0.5f;
                        float w0 = ((bx - fx) * (cy - fy) - (by - fy) * (cx - fx)) * invArea;
                        float w1 = ((cx - fx) * (ay - fy) - (cy - fy) * (ax - fx)) * invArea;
                        float w2 = 1.0f - w0 - w1;
                        if (w0 < -0.002f || w1 < -0.002f || w2 < -0.002f) continue;

                        float z = w0 * A.z + w1 * B.z + w2 * C.z;
                        if (z < rowZ[px] - 1.5f) continue;

                        int tx = std::clamp(int(w0 * A.u + w1 * B.u + w2 * C.u + 0.5f), 0, texW - 1);
                        int ty = std::clamp(int(w0 * A.v + w1 * B.v + w2 * C.v + 0.5f), 0, texH - 1);
                        QRgb srcP = texBits[ty * texW + tx];
                        int sa = qAlpha(srcP);
                        if (sa <= 4) continue;

                        float sh = w0 * A.shade + w1 * B.shade + w2 * C.shade;
                        int sr = std::min(sa, int(qRed(srcP)   * sh));
                        int sg = std::min(sa, int(qGreen(srcP) * sh));
                        int sb = std::min(sa, int(qBlue(srcP)  * sh));
                        // Если обратная сторона загнутого кончика перекрывает лицевую — слегка теплим/приглушаем тон изнанки
                        if (A.backFace + B.backFace + C.backFace >= 2) {
                            sr = int(sr * 0.90 + sa * 0.08);
                            sg = int(sg * 0.88 + sa * 0.07);
                            sb = int(sb * 0.82 + sa * 0.05);
                        }

                        if (sa >= 250 || rowDst[px] == 0 || z > rowZ[px] + 0.8f) {
                            rowDst[px] = qRgba(sr, sg, sb, sa);
                            rowZ[px]   = z;
                        } else {
                            QRgb dP = rowDst[px];
                            int invA = 255 - sa;
                            rowDst[px] = qRgba(
                                std::min(255, sr + (qRed(dP)   * invA) / 255),
                                std::min(255, sg + (qGreen(dP) * invA) / 255),
                                std::min(255, sb + (qBlue(dP)  * invA) / 255),
                                std::min(255, sa + (qAlpha(dP) * invA) / 255)
                            );
                            rowZ[px] = std::max(rowZ[px], z);
                        }
                    }
                }
            };

            for (int j = 0; j < G; ++j) {
                for (int i = 0; i < G; ++i) {
                    const Vtx3D &v00 = vtx[j * (G + 1) + i];
                    const Vtx3D &v10 = vtx[j * (G + 1) + i + 1];
                    const Vtx3D &v01 = vtx[(j + 1) * (G + 1) + i];
                    const Vtx3D &v11 = vtx[(j + 1) * (G + 1) + i + 1];
                    rasterTri(v00, v10, v01);
                    rasterTri(v10, v11, v01);
                }
            }

            painter->save();
            painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
            double depthAlpha = p.resting ? p.alpha : (p.alpha * (0.78 + 0.22 * p.depth));
            QRect leafScreenRect(int(p.x - halfBuf - 4), int(p.y - halfBuf - 4), bufSz + 8, bufSz + 8);

            if (p.depth < 0.52f && !p.resting) {
                QRegion winRegion;
                for (const auto &obs : src->m_obstacles) {
                    if (obs.id == "window_glass" || obs.id == "window_top_ledge" || obs.isWaybar) continue;
                    QRectF wr = obs.rect;
                    qint64 winAge = src->m_lastTickMs - obs.createdMs;
                    if (obs.isWindow && winAge < 360) {
                        double prog = std::clamp(double(winAge) / 360.0, 0.0, 1.0);
                        double ease = 1.0 - std::pow(1.0 - prog, 3.0);
                        if (src->m_lastTickMs - s_wsSwitchMs < 420 && s_wsSlideDir != 0) {
                            double shiftX = (1.0 - ease) * s_wsSlideDir * src->width();
                            wr.translate(shiftX, 0.0);
                        } else {
                            double sc = 0.75 + 0.25 * ease;
                            QPointF c = wr.center();
                            wr = QRectF(c.x() - wr.width() * 0.5 * sc, c.y() - wr.height() * 0.5 * sc, wr.width() * sc, wr.height() * sc);
                        }
                    }
                    if (wr.toRect().intersects(leafScreenRect)) {
                        winRegion += QRegion(wr.toRect());
                    }
                }

                if (winRegion.isEmpty()) {
                    // Лист целиком в свободном пространстве
                    painter->setOpacity(depthAlpha);
                    painter->drawImage(QPointF(p.x - halfBuf, p.y - halfBuf), s_leafBuf, QRectF(0, 0, bufSz, bufSz));
                } else {
                    // 1) Часть листа снаружи окна — 100% чёткая и яркая
                    QRegion openRegion = QRegion(leafScreenRect) - winRegion;
                    if (!openRegion.isEmpty()) {
                        painter->save();
                        painter->setClipRegion(openRegion);
                        painter->setOpacity(depthAlpha);
                        painter->drawImage(QPointF(p.x - halfBuf, p.y - halfBuf), s_leafBuf, QRectF(0, 0, bufSz, bufSz));
                        painter->restore();
                    }
                    // 2) Часть листа ЗА окном — видна сквозь полупрозрачное размытое стекло окна (blurred + приглушённая)!
                    painter->save();
                    painter->setClipRegion(winRegion);
                    int blurSz = std::max(8, bufSz / 5);
                    QImage blurredLeaf = s_leafBuf.copy(0, 0, bufSz, bufSz).scaled(blurSz, blurSz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                    painter->setOpacity(depthAlpha * 0.28);
                    painter->drawImage(QRectF(p.x - halfBuf - 2.0, p.y - halfBuf - 2.0, bufSz + 4.0, bufSz + 4.0), blurredLeaf);
                    painter->setOpacity(depthAlpha * 0.16);
                    painter->drawImage(QRectF(p.x - halfBuf + 2.0, p.y - halfBuf + 2.0, bufSz, bufSz), blurredLeaf);
                    painter->restore();
                }
            } else {
                if (onLedge || (stuckFlat && peelSmooth > 0.05)) {
                    double shOp = onLedge ? (0.32 * fold) : (0.36 * peelSmooth);
                    double shOff = onLedge ? 2.2 : (1.5 + peelSmooth * 4.5);
                    painter->setOpacity(p.alpha * shOp);
                    painter->drawImage(QPointF(p.x - halfBuf + shOff * 0.6, p.y - halfBuf + shOff), s_leafBuf, QRectF(0, 0, bufSz, bufSz));
                }
                painter->setOpacity(depthAlpha);
                painter->drawImage(QPointF(p.x - halfBuf, p.y - halfBuf), s_leafBuf, QRectF(0, 0, bufSz, bufSz));
            }
            painter->restore();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════════
    // ПРАЗДНИЧНЫЕ АТМОСФЕРНЫЕ ТЕМЫ: 🎃 ХЭЛЛОУИН (profile == 2) и 🎄 НОВЫЙ ГОД (profile == 3)
    // ═══════════════════════════════════════════════════════════════════════════════
    int prof = mgr->presetProfile();
    double W = src->width(), H = src->height();
    double t = src->m_timeSec;
    double windVal = mgr->windStrength() + src->m_windDriftOffset + src->m_gustBoost * 0.5;

    // Не дублируем угловые предметы и декор внутри окна HySett, если включён глобальный оверлей и синхронизация (Пункт 6)
    if (suppressInAppOverlayParticles) return;

    // 🎅 СИЛУЭТ САНТЫ НА САНЯХ С ОЛЕНЯМИ, ПРОЛЕТАЮЩИЙ ПО ЧАСТИ ЭКРАНА КАЖДЫЕ 5 МИНУТ (300 СЕК)
    if (mgr->snowEnabled() && W > 300 && H > 250) {
        double santaPeriod = 300.0; // Каждые 5 минут (300 секунд)
        double santaDur    = 6.8;   // Длительность пролёта по дуге части экрана
        // Первый пролёт через ~15 секунд после запуска, далее каждые 5 минут
        double santaPhase  = std::fmod(t + 285.0, santaPeriod);
        if (santaPhase < santaDur) {
            double u = santaPhase / santaDur; // 0.0 .. 1.0
            // Плавное появление из морозной дымки в начале дуги и плавное растворение в конце
            double fadeEnv = std::sin(u * 3.14159265);
            fadeEnv = std::pow( std::clamp(fadeEnv * 1.35, 0.0, 1.0), 1.4 );

            // Пролёт по красивой дуге в верхней части экрана (от 18% до 72% ширины экрана)
            double startX = W * 0.16, endX = W * 0.74;
            double sx = startX + (endX - startX) * u;
            double baseArcY = H * 0.17 - std::sin(u * 3.14159265) * (H * 0.065);
            double sy = baseArcY + std::cos(u * 12.5) * 2.5;
            double dyDu = -std::cos(u * 3.14159265) * (H * 0.065) * 3.14159265 / (endX - startX);
            double pitchDeg = std::clamp(std::atan(dyDu) * (180.0 / 3.14159265), -16.0, 16.0);

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setOpacity(fadeEnv * 0.86);
            painter->translate(sx, sy);
            painter->rotate(pitchDeg);
            double sc = std::clamp(W / 1920.0, 0.65, 1.25) * 0.92;
            painter->scale(sc, sc);

            QColor silCol(12, 16, 24, 242);
            QColor rimCol(185, 215, 245, 85);

            // 1. Лёгкий волшебный шлейф снежной пыльцы позади полозьев саней
            painter->setPen(Qt::NoPen);
            for (int sp = 0; sp < 8; ++sp) {
                double trailX = -38.0 - sp * 11.0 - std::fmod(t * 28.0 + sp * 7.0, 12.0);
                double trailY = 8.0 + std::sin(t * 5.0 + sp * 1.7) * 3.5 + sp * 0.8;
                double rSpark = (1.8 - sp * 0.18);
                if (rSpark > 0.3) {
                    painter->setBrush(QColor(225, 240, 255, std::max(0, 145 - sp * 17)));
                    painter->drawEllipse(QPointF(trailX, trailY), rSpark, rSpark);
                }
            }

            // 2. Сани Санты (резной корпус + изогнутые полозья + мешок с подарками + силуэт Санты в шапке)
            QPainterPath sleigh;
            // Полозья саней с загнутым передком
            sleigh.moveTo(-36.0, 9.5);
            sleigh.lineTo(6.0, 9.5);
            sleigh.quadTo(15.5, 9.5, 18.0, 2.0);
            sleigh.quadTo(16.2, 8.0, 5.5, 8.0);
            sleigh.lineTo(-36.0, 8.0);
            sleigh.closeSubpath();
            // Стойки полозьев
            sleigh.addRect(QRectF(-26.0, 3.5, 2.2, 5.0));
            sleigh.addRect(QRectF(-6.0,  3.5, 2.2, 5.0));
            // Корпус саней
            sleigh.moveTo(-33.0, 4.0);
            sleigh.quadTo(-37.0, -6.0, -31.0, -13.5);
            sleigh.quadTo(-24.0, -9.0, -18.0, -4.5);
            sleigh.lineTo(-2.0, -4.5);
            sleigh.quadTo(6.0, -5.0, 12.5, -11.5);
            sleigh.quadTo(14.5, -3.0, 7.0, 4.0);
            sleigh.closeSubpath();

            painter->setPen(QPen(rimCol, 0.7));
            painter->setBrush(silCol);
            painter->drawPath(sleigh);

            // Мешок с подарками позади Санты и сам силуэт Санты
            painter->setPen(Qt::NoPen);
            painter->setBrush(silCol);
            painter->drawEllipse(QPointF(-22.5, -9.5), 7.2, 6.2); // Мешок
            painter->drawEllipse(QPointF(-10.5, -9.0), 6.2, 6.8); // Туловище Санты
            painter->drawEllipse(QPointF(-9.5, -16.8), 4.0, 4.0); // Голова + борода
            painter->drawEllipse(QPointF(-6.8, -15.5), 2.5, 2.2); // Окладистая борода
            // Колпак Санты с помпоном, развевающимся назад
            QPainterPath hat;
            hat.moveTo(-13.0, -18.0);
            hat.quadTo(-11.0, -24.0, -18.5, -21.0);
            hat.lineTo(-8.0, -18.5);
            hat.closeSubpath();
            painter->drawPath(hat);
            painter->drawEllipse(QPointF(-19.2, -20.8), 1.5, 1.5);

            // 3. Упряжка из 3 летящих оленей с ветвистыми рогами и анимированным бегом
            const double deerX[3] = {36.0, 68.0, 100.0};
            const double deerY[3] = {-1.5, -4.5, -7.5};

            // Вожжи от рук Санты к оленям
            QPainterPath reins;
            reins.moveTo(-5.0, -11.0);
            for (int d = 0; d < 3; ++d) {
                double bob = std::sin(t * 7.2 + d * 1.5) * 2.0;
                reins.quadTo(deerX[d] - 16.0, deerY[d] + 3.0 + bob, deerX[d] + 3.0, deerY[d] - 4.0 + bob);
            }
            painter->setPen(QPen(QColor(180, 200, 225, 110), 0.75));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(reins);

            for (int d = 0; d < 3; ++d) {
                double gallop = std::sin(t * 7.2 + d * 1.5);
                double gallopCos = std::cos(t * 7.2 + d * 1.5);
                double dx = deerX[d];
                double dy = deerY[d] + gallop * 2.0;

                painter->save();
                painter->translate(dx, dy);
                painter->rotate(gallopCos * 5.5);

                // Стройное туловище, шея и голова оленя
                QPainterPath deerBody;
                deerBody.addRoundedRect(QRectF(-8.5, -3.2, 15.5, 5.6), 2.6, 2.6);
                // Шея и голова
                QPolygonF neckHead;
                neckHead << QPointF(4.0, -2.0) << QPointF(8.2, -8.8) << QPointF(13.2, -7.2)
                         << QPointF(12.5, -5.0) << QPointF(7.5, -4.2) << QPointF(6.2, 1.2);
                deerBody.addPolygon(neckHead);
                // Хвостик
                deerBody.addEllipse(QPointF(-9.5, -3.5), 1.8, 1.1);

                painter->setPen(QPen(rimCol, 0.55));
                painter->setBrush(silCol);
                painter->drawPath(deerBody);

                // Анимированные бегущие ноги оленя (передняя и задняя пары)
                painter->setPen(QPen(silCol, 1.55, Qt::SolidLine, Qt::RoundCap));
                double fAng1 = 0.45 + gallop * 0.65;
                double fAng2 = 0.20 - gallop * 0.55;
                double bAng1 = -0.45 - gallop * 0.65;
                double bAng2 = -0.20 + gallop * 0.55;
                painter->drawLine(QPointF(5.0, 1.5),  QPointF(5.0 + std::sin(fAng1) * 8.5,  1.5 + std::cos(fAng1) * 8.0));
                painter->drawLine(QPointF(3.2, 1.5),  QPointF(3.2 + std::sin(fAng2) * 8.0,  1.5 + std::cos(fAng2) * 7.5));
                painter->drawLine(QPointF(-6.5, 1.5), QPointF(-6.5 + std::sin(bAng1) * 8.5, 1.5 + std::cos(bAng1) * 8.0));
                painter->drawLine(QPointF(-4.5, 1.5), QPointF(-4.5 + std::sin(bAng2) * 8.0, 1.5 + std::cos(bAng2) * 7.5));

                // Ветвистые оленьи рога
                painter->setPen(QPen(silCol, 1.05, Qt::SolidLine, Qt::RoundCap));
                painter->drawLine(QPointF(8.5, -8.5), QPointF(6.2, -14.8));
                painter->drawLine(QPointF(7.2, -11.5), QPointF(4.2, -13.0));
                painter->drawLine(QPointF(6.8, -13.2), QPointF(8.8, -15.2));

                // У переднего оленя (Рудольфа) — маленький светящийся красный нос!
                if (d == 2) {
                    painter->setPen(Qt::NoPen);
                    QRadialGradient noseGlow(QPointF(13.6, -6.4), 4.2);
                    noseGlow.setColorAt(0.0, QColor(255, 65, 55, 230));
                    noseGlow.setColorAt(0.45, QColor(255, 40, 30, 110));
                    noseGlow.setColorAt(1.0, QColor(255, 20, 20, 0));
                    painter->setBrush(noseGlow);
                    painter->drawEllipse(QPointF(13.6, -6.4), 4.2, 4.2);
                }
                painter->restore();
            }
            painter->restore();
        }
    }

    // Вычисляем точный отступ под Waybar, чтобы паутина в левом верхнем углу была строго под ним (Пункт 3)
    double topBarOffset = 4.0;
    if (src->m_globalMode && mgr->waybarInteractEnabled()) {
        topBarOffset = 38.0;
        if (src->m_obstacles.contains("waybar_left")) {
            const QRectF &wbr = src->m_obstacles["waybar_left"].rect;
            if (wbr.top() < H * 0.5) topBarOffset = std::max(4.0, wbr.bottom() + 2.0);
        }
    }

    if (prof == 2 && W > 200 && H > 200) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        // 0. 🌑 ЛЁГКОЕ АТМОСФЕРНОЕ ЗАТЕМНЕНИЕ ЭКРАНА ДЛЯ ХЭЛЛОУИНА (Пункт 8)
        QRadialGradient gloomGrad(QPointF(W * 0.5, H * 0.45), std::hypot(W, H) * 0.62);
        gloomGrad.setColorAt(0.0, QColor(8, 6, 14, 34));
        gloomGrad.setColorAt(0.65, QColor(6, 5, 12, 54));
        gloomGrad.setColorAt(1.0, QColor(4, 3, 9, 82));
        painter->setPen(Qt::NoPen);
        painter->setBrush(gloomGrad);
        painter->drawRect(QRectF(0, 0, W, H));

        // 1. 🕸 МАЛЕНЬКАЯ РЕАЛИСТИЧНАЯ ПАУТИНА ТОЛЬКО В ЛЕВОМ ВЕРХНЕМ УГЛУ ПОД WAYBAR (Пункт 3)
        double cornerX = 0.0;
        double cornerY = 0.0;
        double maxR = 92.0; // Паутина перекрывает область Waybar
        const int rays = 7;
        QVector<QPointF> rayDirs;
        for (int r = 0; r < rays; ++r) {
            double ang = (double(r) / (rays - 1)) * (3.14159 * 0.48) + 0.03;
            double sway = std::sin(t * 1.4 + r * 0.7) * 0.012 + windVal * 0.018;
            rayDirs.append(QPointF(std::cos(ang + sway), std::sin(ang + sway)));
        }

        // Тонкие шёлковые радиальные лучи с градиентным затуханием к концам
        for (int r = 0; r < rays; ++r) {
            const auto &d = rayDirs[r];
            QPointF endPt(cornerX + d.x() * maxR, cornerY + d.y() * maxR);
            QLinearGradient silkG(QPointF(cornerX, cornerY), endPt);
            silkG.setColorAt(0.0, QColor(235, 242, 255, 130));
            silkG.setColorAt(0.7, QColor(215, 228, 245, 92));
            silkG.setColorAt(1.0, QColor(200, 215, 235, 25));
            painter->setPen(QPen(QBrush(silkG), 0.75));
            painter->drawLine(QPointF(cornerX, cornerY), endPt);
        }

        // Провисающие спиральные нити с микро-капельками росы
        painter->setBrush(Qt::NoBrush);
        for (int ring = 1; ring <= 5; ++ring) {
            double rad = (ring / 5.0) * maxR * 0.88;
            QPainterPath webRing;
            for (int r = 0; r < rays; ++r) {
                QPointF pPt(cornerX + rayDirs[r].x() * rad, cornerY + rayDirs[r].y() * rad);
                if (r == 0) webRing.moveTo(pPt);
                else {
                    QPointF prevPt(cornerX + rayDirs[r - 1].x() * rad, cornerY + rayDirs[r - 1].y() * rad);
                    QPointF mid = (prevPt + pPt) * 0.5 * 0.90 + QPointF(cornerX, cornerY) * 0.10;
                    webRing.quadTo(mid, pPt);
                }
            }
            painter->setPen(QPen(QColor(225, 235, 250, 88 - ring * 6), 0.68));
            painter->drawPath(webRing);
        }

        // 🕷 РЕАЛИСТИЧНЫЙ ПАУК НА ПАУТИНКЕ (Чёрная вдова с 3-суставчатыми лапками и 3D-бликами)
        int anchorRay = 3;
        double anchorDist = maxR * 0.42;
        double spiderAnchorX = cornerX + rayDirs[anchorRay].x() * anchorDist;
        double spiderAnchorY = cornerY + rayDirs[anchorRay].y() * anchorDist;
        double climbCycle = std::sin(t * 0.32);
        double spiderDrop = 20.0 + 18.0 * climbCycle;
        double spiderSwayX = std::sin(t * 0.95) * 2.2 + windVal * 6.0;
        QPointF spiderPos(spiderAnchorX + spiderSwayX, spiderAnchorY + spiderDrop);

        // Шелковая нить паука с лёгким натяжением
        QLinearGradient threadG(QPointF(spiderAnchorX, spiderAnchorY), spiderPos);
        threadG.setColorAt(0.0, QColor(220, 230, 245, 95));
        threadG.setColorAt(1.0, QColor(240, 245, 255, 165));
        painter->setPen(QPen(QBrush(threadG), 0.72));
        painter->drawLine(QPointF(spiderAnchorX, spiderAnchorY), spiderPos);

        painter->save();
        painter->translate(spiderPos);
        painter->rotate(-spiderSwayX * 1.15);
        double spScale = 0.86;
        painter->scale(spScale, spScale);

        // Тень паука для объёма
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 75));
        painter->drawEllipse(QPointF(1.2, 1.5), 7.5, 8.5);

        // 8 реалистичных 3-сегментных лапок (тазик -> бедро -> голень -> лапка)
        static const double femurAngles[4] = {-0.95, -0.38, 0.28, 0.88};
        static const double femurLen[4]    = {6.8,   6.0,   5.8,  6.6};
        static const double tibiaLen[4]    = {7.2,   6.2,   6.0,  7.0};
        static const double tarsusLen[4]   = {4.2,   3.6,   3.5,  4.4};

        for (int side : {-1, 1}) {
            for (int leg = 0; leg < 4; ++leg) {
                double stepPhase = climbCycle * 3.6 + t * 0.70 + leg * 1.57 + (side > 0 ? 0.0 : 3.14159);
                double swing = std::sin(stepPhase) * 0.085;
                double a1 = femurAngles[leg] + swing;

                // Точка крепления к головогруди
                QPointF coxa(side * 2.2, 2.2 + (leg - 1.5) * 1.05);
                // Коленный сустав (возвышается дугой)
                QPointF knee(coxa.x() + side * std::cos(a1) * femurLen[leg],
                             coxa.y() + std::sin(a1) * femurLen[leg] - 1.6);
                // Голеностопный сустав
                double a2 = a1 + (leg < 2 ? -0.32 : 0.36) + swing * 0.6;
                QPointF ankle(knee.x() + side * std::cos(a2) * tibiaLen[leg],
                              knee.y() + std::sin(a2) * tibiaLen[leg] + 0.9);
                // Острый кончик лапки
                double a3 = a2 + (leg < 2 ? -0.25 : 0.42);
                QPointF tip(ankle.x() + side * std::cos(a3) * tarsusLen[leg],
                            ankle.y() + std::sin(a3) * tarsusLen[leg] + 1.2);

                // Бедро (толще, с хитиновым бликом)
                painter->setPen(QPen(QColor(14, 14, 18, 250), 1.55, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter->drawLine(coxa, knee);
                // Голень (средняя толщина)
                painter->setPen(QPen(QColor(20, 20, 26, 245), 1.15, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter->drawLine(knee, ankle);
                // Лапка (тонкий острый кончик)
                painter->setPen(QPen(QColor(28, 28, 36, 235), 0.78, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter->drawLine(ankle, tip);

                // Крошечный глянцевый блик на коленном суставе
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor(115, 125, 145, 135));
                painter->drawEllipse(knee, 0.65, 0.65);
            }
        }

        // Педипальпы (передние щупальца) и хелицеры
        for (int side : {-1, 1}) {
            double feelerWiggle = std::sin(t * 1.6 + side) * 0.35;
            QPainterPath palp;
            palp.moveTo(side * 1.4, 4.8);
            palp.quadTo(side * 3.4, 6.6 + feelerWiggle, side * 2.2, 8.2 + feelerWiggle);
            painter->setPen(QPen(QColor(18, 18, 24, 240), 1.0, Qt::SolidLine, Qt::RoundCap));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(palp);
        }

        // Брюшко (опистосома) с глубоким 3D хитиновым градиентом
        QRadialGradient abdGrad(QPointF(-1.6, -5.2), 6.8);
        abdGrad.setColorAt(0.0, QColor(68, 72, 86, 252));
        abdGrad.setColorAt(0.32, QColor(24, 25, 32, 252));
        abdGrad.setColorAt(1.0, QColor(8, 8, 12, 254));
        painter->setPen(QPen(QColor(5, 5, 8, 230), 0.65));
        painter->setBrush(abdGrad);
        painter->drawEllipse(QPointF(0, -3.8), 4.6, 5.5);

        // Красный узор «песочные часы» на спинке/брюшке
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(215, 28, 28, 215));
        QPolygonF hourglass;
        hourglass << QPointF(-1.3, -5.4) << QPointF(1.3, -5.4) << QPointF(0.3, -3.8)
                  << QPointF(1.2, -2.2) << QPointF(-1.2, -2.2) << QPointF(-0.3, -3.8);
        painter->drawPolygon(hourglass);

        // Головогрудь (просома)
        QRadialGradient cephGrad(QPointF(-0.9, 2.2), 3.8);
        cephGrad.setColorAt(0.0, QColor(62, 66, 78, 252));
        cephGrad.setColorAt(0.45, QColor(20, 21, 28, 252));
        cephGrad.setColorAt(1.0, QColor(10, 10, 14, 254));
        painter->setPen(QPen(QColor(6, 6, 10, 230), 0.6));
        painter->setBrush(cephGrad);
        painter->drawEllipse(QPointF(0, 2.8), 3.0, 2.9);

        // Кластер из 6 блестящих рубиновых глаз
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 48, 38, 240));
        painter->drawEllipse(QPointF(-0.85, 4.7), 0.68, 0.68);
        painter->drawEllipse(QPointF( 0.85, 4.7), 0.68, 0.68);
        painter->setBrush(QColor(220, 35, 30, 200));
        painter->drawEllipse(QPointF(-1.65, 4.3), 0.45, 0.45);
        painter->drawEllipse(QPointF( 1.65, 4.3), 0.45, 0.45);
        painter->restore();

        // 2. 🦇 РЕАЛИСТИЧНЫЕ ЛЕТУЧИЕ МЫШИ — СТРЕМИТЕЛЬНЫЙ ПРОЛЁТ РАЗ В МИНУТУ (Пункт 4)
        // Период 60 секунд: пролетают быстро за 2.4 секунды, остальные 57.6 секунды их нет
        double batPeriod = 60.0;
        double batFlightDur = 2.4;
        double batPhase = std::fmod(t + 48.0, batPeriod); // Первый пролёт через ~12 сек после запуска, далее раз в 60 сек
        if (batPhase < batFlightDur) {
            int flockCount = 4;
            for (int b = 0; b < flockCount; ++b) {
                double individualDelay = b * 0.16;
                double localT = (batPhase - individualDelay) / (batFlightDur - 0.65);
                if (localT < 0.0 || localT > 1.0) continue;

                // Стремительный диагонально-волновой пролёт через верхнюю треть экрана
                double bx = -90.0 + localT * (W + 180.0);
                double swoopY = std::sin(localT * 3.14159) * 42.0 + std::sin(localT * 10.5 + b * 1.9) * 14.0;
                double by = topBarOffset + H * (0.09 + (b % 3) * 0.045) + swoopY + (b * 11.0);

                double vyApprox = std::cos(localT * 10.5 + b * 1.9) * 10.0;
                double pitchDeg = std::clamp(vyApprox * 1.1 - 4.0, -18.0, 18.0);

                // Быстрый реалистичный взмах крыльев (18 Гц) с изгибом суставов
                double wingPhase = t * (18.5 + b * 1.2) + b * 2.1;
                double flap = std::sin(wingPhase);
                double wristFold = std::cos(wingPhase) * 0.35;
                double bScale = 0.72 + (b % 3) * 0.14;

                painter->save();
                painter->translate(bx, by);
                painter->rotate(pitchDeg);
                painter->scale(bScale, bScale);

                double shoulderY = -flap * 4.5;
                double wristY    = -flap * 13.5;
                double tipY      = -flap * 19.0 + wristFold * 5.0;
                double span      = 27.0 - std::abs(flap) * 3.2;

                // Кожистая перепонка крыльев с 4 пальцевыми фалангами и фестонами по нижнему краю
                QPainterPath membrane;
                membrane.moveTo(0, -4.5);
                // Голова и острые ушки
                membrane.lineTo(-1.4, -5.2);
                membrane.lineTo(-2.6, -9.2); // Левое ухо
                membrane.lineTo(-3.8, -5.0);
                // Левая передняя кромка крыла (плечо -> кисть -> кончик)
                membrane.quadTo(-10.5, shoulderY - 5.5, -16.0, wristY - 4.5);
                membrane.quadTo(-22.0, (wristY + tipY) * 0.5 - 2.0, -span, tipY);
                // 3 дугообразных выреза перепонки между пальцами левого крыла
                membrane.quadTo(-span * 0.78, tipY + 2.5, -span * 0.66, wristY + 7.5);
                membrane.quadTo(-span * 0.50, wristY + 3.2, -span * 0.38, shoulderY + 8.0);
                membrane.quadTo(-span * 0.22, shoulderY + 4.0, -3.2, 6.2);
                // Хвостовая перепонка (уропатагиум)
                membrane.lineTo(0, 8.2);
                membrane.lineTo(3.2, 6.2);
                // Правое крыло (симметрично с фалангами)
                membrane.quadTo(span * 0.22, shoulderY + 4.0, span * 0.38, shoulderY + 8.0);
                membrane.quadTo(span * 0.50, wristY + 3.2, span * 0.66, wristY + 7.5);
                membrane.quadTo(span * 0.78, tipY + 2.5, span, tipY);
                membrane.quadTo(22.0, (wristY + tipY) * 0.5 - 2.0, 16.0, wristY - 4.5);
                membrane.quadTo(10.5, shoulderY - 5.5, 3.8, -5.0);
                membrane.lineTo(2.6, -9.2); // Правое ухо
                membrane.lineTo(1.4, -5.2);
                membrane.closeSubpath();

                QLinearGradient batGrad(0, -10, 0, 10);
                batGrad.setColorAt(0.0, QColor(24, 20, 30, 242));
                batGrad.setColorAt(0.6, QColor(14, 12, 18, 235));
                batGrad.setColorAt(1.0, QColor(32, 26, 40, 210));
                painter->setPen(QPen(QColor(8, 6, 12, 230), 0.75));
                painter->setBrush(batGrad);
                painter->drawPath(membrane);

                // Кости крыла (лучи пальцев от кистевого сустава к фестонам перепонки)
                painter->setPen(QPen(QColor(38, 32, 48, 155), 0.65));
                for (int s : {-1, 1}) {
                    QPointF wristPt(s * 15.5, wristY - 3.8);
                    painter->drawLine(QPointF(s * 3.0, -3.0), wristPt);
                    painter->drawLine(wristPt, QPointF(s * span * 0.66, wristY + 7.2));
                    painter->drawLine(wristPt, QPointF(s * span * 0.38, shoulderY + 7.8));
                }

                // Пушистое тельце
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor(12, 10, 16, 250));
                painter->drawEllipse(QPointF(0, 0.5), 2.8, 5.2);
                painter->restore();
            }
        }

        // 3. 🕯 3 РЕАЛИСТИЧНЫЕ СВЕЧИ ЧУТЬ НИЖЕ СЕРЕДИНЫ ЛЕВОЙ ПОЛОВИНЫ WAYBAR
        const double candleOffsetX[3] = {-16.0, 0.0, 16.0};
        const double candleBaseY[3]   = {24.0, 34.0, 27.0};
        for (int c = 0; c < 3; ++c) {
            double floatSwayX = std::sin(t * 0.85 + c * 2.1) * 2.4 + windVal * 2.0;
            double floatBobY  = std::sin(t * 1.35 + c * 1.6) * 3.5;
            double cx = W * 0.26 + candleOffsetX[c] + floatSwayX;
            double cy = topBarOffset + candleBaseY[c] + floatBobY;

            // В 2 раза меньше прежних (было cW = 7.5, cH = 26..38 -> стало cW = 3.8, cH = 13.5..17.5)
            double cW = 3.8;
            double cH = 13.5 + (c % 3) * 2.0;
            double halfW = cW * 0.5;

            // Мягкая тень/затемнение снизу парящей свечи
            QRadialGradient botShadow(cx, cy + cH + 1.5, cW * 1.2);
            botShadow.setColorAt(0.0, QColor(0, 0, 0, 95));
            botShadow.setColorAt(1.0, QColor(0, 0, 0, 0));
            painter->setPen(Qt::NoPen);
            painter->setBrush(botShadow);
            painter->drawEllipse(QPointF(cx, cy + cH + 1.2), cW * 1.1, 2.2);

            // Цилиндрический восковой корпус с 3D-светотенью и тёплым подсвечиванием сверху (subsurface scattering)
            QLinearGradient waxBody(cx - halfW, cy, cx + halfW, cy);
            waxBody.setColorAt(0.00, QColor(185, 165, 132, 242));
            waxBody.setColorAt(0.28, QColor(246, 236, 208, 248));
            waxBody.setColorAt(0.62, QColor(232, 215, 182, 246));
            waxBody.setColorAt(1.00, QColor(142, 122, 95, 242));

            QPainterPath bodyPath;
            bodyPath.moveTo(cx - halfW, cy);
            bodyPath.lineTo(cx - halfW, cy + cH - 1.0);
            bodyPath.quadTo(cx, cy + cH + 1.4, cx + halfW, cy + cH - 1.0);
            bodyPath.lineTo(cx + halfW, cy);
            bodyPath.closeSubpath();

            painter->setPen(QPen(QColor(110, 92, 68, 175), 0.45));
            painter->setBrush(waxBody);
            painter->drawPath(bodyPath);

            // Тёплое внутреннее просвечивание воска у самой верхушки свечи от пламени
            double flicker = 0.90 + 0.10 * std::sin(t * 11.5 + c * 3.1) * std::cos(t * 7.7 + c);
            QLinearGradient sssGrad(cx, cy, cx, cy + cH * 0.48);
            sssGrad.setColorAt(0.0, QColor(255, 185, 75, int(115 * flicker)));
            sssGrad.setColorAt(1.0, QColor(255, 160, 50, 0));
            painter->setPen(Qt::NoPen);
            painter->setBrush(sssGrad);
            painter->drawPath(bodyPath);

            // Реалистичные потёки застывшего воска со скруглённой капелькой на конце
            double dripX1 = cx - halfW * 0.52;
            double dripH1 = cH * (0.42 + 0.08 * (c % 2));
            painter->setBrush(QColor(242, 228, 198, 235));
            painter->drawRoundedRect(QRectF(dripX1, cy + 0.5, 1.05, dripH1), 0.5, 0.5);
            painter->drawEllipse(QPointF(dripX1 + 0.52, cy + 0.5 + dripH1), 0.72, 0.85);

            double dripX2 = cx + halfW * 0.25;
            double dripH2 = cH * 0.26;
            painter->setBrush(QColor(225, 208, 176, 225));
            painter->drawRoundedRect(QRectF(dripX2, cy + 0.5, 0.90, dripH2), 0.45, 0.45);
            painter->drawEllipse(QPointF(dripX2 + 0.45, cy + 0.5 + dripH2), 0.62, 0.72);

            // Верхняя эллиптическая восковая чаша с янтарным расплавленным воском
            QRadialGradient cupGrad(QPointF(cx, cy), halfW);
            cupGrad.setColorAt(0.0, QColor(255, 210, 115, 245));
            cupGrad.setColorAt(0.65, QColor(245, 225, 185, 245));
            cupGrad.setColorAt(1.0, QColor(195, 175, 140, 245));
            painter->setPen(QPen(QColor(150, 130, 100, 160), 0.4));
            painter->setBrush(cupGrad);
            painter->drawEllipse(QPointF(cx, cy), halfW, 0.95);

            // Слегка изогнутый фитиль с раскалённым оранжевым кончиком
            QPainterPath wick;
            wick.moveTo(cx, cy);
            wick.quadTo(cx + 0.25, cy - 1.1, cx + 0.15, cy - 2.1);
            painter->setPen(QPen(QColor(28, 22, 18, 235), 0.65, Qt::SolidLine, Qt::RoundCap));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(wick);
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(255, 110, 20, 230));
            painter->drawEllipse(QPointF(cx + 0.15, cy - 2.1), 0.42, 0.42);

            // Многослойный ореол свечения (гало) вокруг пламени
            double flameTilt = windVal * 2.4 + std::sin(t * 8.2 + c * 2.3) * 0.85;
            double haloR = 13.5 * flicker;
            QPointF haloCenter(cx + flameTilt * 0.35, cy - 5.2);
            QRadialGradient halo(haloCenter, haloR);
            halo.setColorAt(0.00, QColor(255, 205, 95, int(115 * flicker)));
            halo.setColorAt(0.42, QColor(255, 135, 30, int(48 * flicker)));
            halo.setColorAt(1.00, QColor(255, 80, 0, 0));
            painter->setBrush(halo);
            painter->drawEllipse(haloCenter, haloR, haloR);

            // Слой 1: Голубое кислородное основание пламени у фитиля
            painter->setBrush(QColor(75, 145, 255, 165));
            painter->drawEllipse(QPointF(cx + flameTilt * 0.1, cy - 2.3), 1.05, 0.95);

            // Слой 2: Внешний золотисто-оранжевый язычок пламени
            double flameTopY = cy - (7.8 + 1.1 * flicker);
            QPainterPath outerFlame;
            outerFlame.moveTo(cx, cy - 1.6);
            outerFlame.cubicTo(cx - 2.1, cy - 3.4, cx - 1.3 + flameTilt * 0.45, cy - 6.0, cx + flameTilt, flameTopY);
            outerFlame.cubicTo(cx + 1.3 + flameTilt * 0.45, cy - 6.0, cx + 2.1, cy - 3.4, cx, cy - 1.6);
            QLinearGradient flameG(cx, cy - 1.6, cx + flameTilt, flameTopY);
            flameG.setColorAt(0.0, QColor(255, 130, 20, 225));
            flameG.setColorAt(0.55, QColor(255, 195, 55, 242));
            flameG.setColorAt(1.0, QColor(255, 95, 10, 195));
            painter->setBrush(flameG);
            painter->drawPath(outerFlame);

            // Слой 3: Ослепительное бело-жёлтое горячее ядро внутри пламени
            QPainterPath innerCore;
            double coreTopY = cy - (5.6 + 0.6 * flicker);
            innerCore.moveTo(cx, cy - 2.0);
            innerCore.cubicTo(cx - 1.0, cy - 3.1, cx - 0.6 + flameTilt * 0.3, cy - 4.5, cx + flameTilt * 0.55, coreTopY);
            innerCore.cubicTo(cx + 0.6 + flameTilt * 0.3, cy - 4.5, cx + 1.0, cy - 3.1, cx, cy - 2.0);
            painter->setBrush(QColor(255, 250, 215, 248));
            painter->drawPath(innerCore);
        }

        painter->restore();
    } else if (prof == 3 && W > 200 && H > 200) {
        // 🎄 ТЕМА: НОВЫЙ ГОД / РОЖДЕСТВО
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        // 1. ✨ СВЕТЯЩАЯСЯ ГИРЛЯНДА И ЁЛОЧНЫЕ ШАРЫ ВДОЛЬ ВЕРХА ЭКРАНА
        const int spans = 7;
        double spanW = W / spans;
        static const QColor bulbCols[4] = {
            QColor(255, 205, 55), // Тёплое золото
            QColor(255, 65, 65),  // Рубиновый красный
            QColor(55, 225, 115), // Изумрудный зелёный
            QColor(75, 185, 255)  // Ледяной голубой
        };

        for (int sIdx = 0; sIdx < spans; ++sIdx) {
            double x1 = sIdx * spanW, x2 = (sIdx + 1) * spanW;
            double midX = (x1 + x2) * 0.5;
            double sagY = topBarOffset + 24.0 + std::sin(t * 1.2 + sIdx) * 2.0;

            QPainterPath cord;
            cord.moveTo(x1, topBarOffset + 2.0);
            cord.quadTo(midX, sagY, x2, topBarOffset + 2.0);
            painter->setPen(QPen(QColor(28, 48, 32, 210), 1.6));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(cord);

            // Лампочки гирлянды вдоль дуги
            for (int b = 1; b <= 4; ++b) {
                double u = b / 5.0;
                double bx = (1 - u) * (1 - u) * x1 + 2 * (1 - u) * u * midX + u * u * x2;
                double by = (1 - u) * (1 - u) * (topBarOffset + 2.0) + 2 * (1 - u) * u * sagY + u * u * (topBarOffset + 2.0);
                int colIdx = (sIdx * 4 + b) % 4;
                QColor bc = bulbCols[colIdx];
                double twinkle = 0.55 + 0.45 * std::sin(t * 3.6 + sIdx * 2.1 + b * 1.5);

                QRadialGradient bg(bx, by + 2.0, 14.0 * twinkle);
                QColor cGlow = bc; cGlow.setAlpha(int(135 * twinkle));
                bg.setColorAt(0.0, cGlow);
                cGlow.setAlpha(0);
                bg.setColorAt(1.0, cGlow);
                painter->setPen(Qt::NoPen);
                painter->setBrush(bg);
                painter->drawEllipse(QPointF(bx, by + 2.0), 14.0 * twinkle, 14.0 * twinkle);

                painter->setBrush(bc.lighter(int(100 + 35 * twinkle)));
                painter->drawEllipse(QPointF(bx, by + 2.0), 2.6, 3.2);
            }

            // Свисающий на нити глянцевый ёлочный шар между дугами
            if (sIdx > 0) {
                double swing = std::sin(t * 1.8 + sIdx * 1.4) * 3.5 + windVal * 6.0;
                double threadLen = 18.0 + (sIdx % 2) * 10.0;
                QPointF ballPos(x1 + swing, topBarOffset + 2.0 + threadLen);
                painter->setPen(QPen(QColor(210, 190, 140, 160), 0.8));
                painter->drawLine(QPointF(x1, topBarOffset + 2.0), ballPos);

                QColor baubleCol = bulbCols[sIdx % 4];
                QRadialGradient ballG(ballPos.x() - 2.5, ballPos.y() - 2.5, 9.5);
                ballG.setColorAt(0.0, QColor(255, 255, 255, 245));
                ballG.setColorAt(0.28, baubleCol.lighter(115));
                ballG.setColorAt(1.0, baubleCol.darker(165));
                painter->setPen(QPen(baubleCol.darker(190), 0.7));
                painter->setBrush(ballG);
                painter->drawEllipse(ballPos, 6.8, 6.8);
            }
        }

        // 2. 🎄 НАРЯДНАЯ НОВОГОДНЯЯ ЁЛОЧКА С ПОДАРКАМИ (внизу справа)
        double treeX = W - 78.0;
        double treeBaseY = H - 18.0;

        // Ствол
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(72, 44, 24, 245));
        painter->drawRoundedRect(QRectF(treeX - 7.0, treeBaseY - 22.0, 14.0, 22.0), 2.0, 2.0);

        // 4 пушистых яруса хвои со снежными шапками
        for (int tier = 0; tier < 4; ++tier) {
            double tierW = 18.0 + (3 - tier) * 11.5;
            double topY  = treeBaseY - 128.0 + (3 - tier) * 24.0;
            double botY  = topY + 38.0 + (3 - tier) * 4.0;

            QPainterPath branch;
            branch.moveTo(treeX, topY);
            branch.quadTo(treeX + tierW * 0.55, (topY + botY) * 0.5, treeX + tierW, botY);
            branch.quadTo(treeX, botY + 6.0, treeX - tierW, botY);
            branch.quadTo(treeX - tierW * 0.55, (topY + botY) * 0.5, treeX, topY);

            QLinearGradient needleG(treeX - tierW, topY, treeX + tierW, botY);
            needleG.setColorAt(0.0, QColor(34, 96, 52, 248));
            needleG.setColorAt(0.5, QColor(24, 76, 40, 248));
            needleG.setColorAt(1.0, QColor(14, 48, 26, 248));
            painter->setPen(QPen(QColor(12, 38, 20, 220), 0.9));
            painter->setBrush(needleG);
            painter->drawPath(branch);

            // Снежная кромка на лапах ели
            QPainterPath snowCap;
            snowCap.moveTo(treeX - tierW * 0.88, botY - 2.0);
            snowCap.quadTo(treeX, botY + 4.5, treeX + tierW * 0.88, botY - 2.0);
            painter->setPen(QPen(QColor(235, 245, 255, 210), 2.6, Qt::SolidLine, Qt::RoundCap));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(snowCap);

            // Шарики и огоньки на ярусе ёлки
            for (int orn = -1; orn <= 1; orn += 2) {
                double ox = treeX + orn * tierW * 0.46;
                double oy = botY - 8.0;
                QColor oc = bulbCols[(tier + (orn > 0 ? 1 : 0)) % 4];
                double gl = 0.65 + 0.35 * std::sin(t * 4.2 + tier * 1.7 + orn);
                painter->setPen(Qt::NoPen);
                painter->setBrush(oc.lighter(int(105 + 30 * gl)));
                painter->drawEllipse(QPointF(ox, oy), 3.4, 3.4);
            }
        }

        // Сияющая золотая звезда на макушке ёлки
        double starY = treeBaseY - 132.0;
        double starPulse = 0.85 + 0.15 * std::sin(t * 4.5);
        QRadialGradient starGlow(treeX, starY, 22.0 * starPulse);
        starGlow.setColorAt(0.0, QColor(255, 230, 90, 180));
        starGlow.setColorAt(0.5, QColor(255, 180, 30, 65));
        starGlow.setColorAt(1.0, QColor(255, 150, 0, 0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(starGlow);
        painter->drawEllipse(QPointF(treeX, starY), 22.0 * starPulse, 22.0 * starPulse);

        QPolygonF starPoly;
        for (int i = 0; i < 10; ++i) {
            double ang = -3.14159 * 0.5 + i * (3.14159 / 5.0);
            double rad = (i % 2 == 0) ? 10.5 : 4.4;
            starPoly << QPointF(treeX + std::cos(ang) * rad, starY + std::sin(ang) * rad);
        }
        painter->setPen(QPen(QColor(210, 140, 15, 240), 0.8));
        painter->setBrush(QColor(255, 225, 75, 250));
        painter->drawPolygon(starPoly);

        // 🎁 Подарки с лентами под ёлкой
        auto drawGift = [&](double gx, double gy, double gw, double gh, QColor boxC, QColor ribC) {
            painter->setPen(QPen(boxC.darker(150), 0.8));
            painter->setBrush(boxC);
            painter->drawRoundedRect(QRectF(gx - gw * 0.5, gy - gh, gw, gh), 2.0, 2.0);
            painter->setPen(Qt::NoPen);
            painter->setBrush(ribC);
            painter->drawRect(QRectF(gx - 1.8, gy - gh, 3.6, gh));
            painter->drawRect(QRectF(gx - gw * 0.5, gy - gh * 0.55, gw, 3.2));
        };
        drawGift(treeX - 28.0, treeBaseY, 20.0, 16.0, QColor(195, 38, 42, 245), QColor(245, 205, 65, 245));
        drawGift(treeX + 24.0, treeBaseY, 17.0, 14.0, QColor(42, 105, 185, 245), QColor(240, 245, 255, 245));

        // 3. ⛄ СНЕГОВИЧОК В ШАРФЕ И ЦИЛИНДРЕ (рядом с ёлочкой)
        double smX = W - 155.0;
        double smY = H - 18.0;
        painter->setPen(QPen(QColor(185, 205, 228, 220), 0.9));
        painter->setBrush(QColor(242, 248, 255, 248));
        painter->drawEllipse(QPointF(smX, smY - 14.0), 16.5, 14.5); // Нижний ком
        painter->drawEllipse(QPointF(smX, smY - 34.0), 12.0, 11.0); // Средний ком
        painter->drawEllipse(QPointF(smX, smY - 50.0), 8.8,  8.5);  // Голова

        // Нос-морковка и глазки-угольки
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(25, 28, 35, 245));
        painter->drawEllipse(QPointF(smX - 3.0, smY - 52.0), 1.3, 1.3);
        painter->drawEllipse(QPointF(smX + 2.2, smY - 52.0), 1.3, 1.3);
        QPolygonF carrot;
        carrot << QPointF(smX - 0.5, smY - 50.0) << QPointF(smX - 9.5, smY - 48.5) << QPointF(smX - 0.5, smY - 47.5);
        painter->setBrush(QColor(245, 115, 25, 250));
        painter->drawPolygon(carrot);

        // Красный шарф с развевающимся на ветру кончиком
        painter->setBrush(QColor(205, 38, 42, 245));
        painter->drawRoundedRect(QRectF(smX - 9.0, smY - 44.0, 18.0, 4.5), 2.0, 2.0);
        QPainterPath scarfTail;
        double scarfWave = std::sin(t * 4.5) * 3.5 + windVal * 8.0;
        scarfTail.moveTo(smX + 4.5, smY - 42.0);
        scarfTail.quadTo(smX + 10.0 + scarfWave * 0.5, smY - 36.0, smX + 8.0 + scarfWave, smY - 29.0);
        painter->setPen(QPen(QColor(205, 38, 42, 245), 3.8, Qt::SolidLine, Qt::RoundCap));
        painter->drawPath(scarfTail);

        // Цилиндр снеговика
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(28, 30, 38, 248));
        painter->drawRoundedRect(QRectF(smX - 10.5, smY - 59.0, 21.0, 2.8), 1.2, 1.2);
        painter->drawRoundedRect(QRectF(smX - 6.8,  smY - 69.5, 13.6, 11.0), 1.5, 1.5);

        painter->restore();
    }
}

void AtmosphereCanvas::paint(QPainter *painter) {
    const AtmosphereCanvas *src = m_masterCanvas ? m_masterCanvas : this;
    if (src->m_fullscreenPaused) return;
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (m_renderPass == 0) paintWaterMap(painter, src);
    else paintOverlayElements(painter, src);
}
