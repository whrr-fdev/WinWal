#pragma once
#include <QObject>
#include <QString>
#include <QColor>
#include <QHash>

class AppConfig : public QObject {
    Q_OBJECT

    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY fontSizeChanged)
    Q_PROPERTY(qreal uiScale READ uiScale WRITE setUiScale NOTIFY uiScaleChanged)
    Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowHeightChanged)
    Q_PROPERTY(bool alignCenter READ alignCenter WRITE setAlignCenter NOTIFY alignCenterChanged)
    Q_PROPERTY(int welcomeAlignMode READ welcomeAlignMode WRITE setWelcomeAlignMode NOTIFY welcomeAlignModeChanged)
    Q_PROPERTY(int sidebarWidth READ sidebarWidth WRITE setSidebarWidth NOTIFY sidebarWidthChanged)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
    Q_PROPERTY(qreal sidebarOpacity READ sidebarOpacity WRITE setSidebarOpacity NOTIFY sidebarOpacityChanged)
    Q_PROPERTY(qreal cardOpacity READ cardOpacity WRITE setCardOpacity NOTIFY cardOpacityChanged)
    Q_PROPERTY(qreal wallpapersOpacity READ wallpapersOpacity WRITE setWallpapersOpacity NOTIFY wallpapersOpacityChanged)
    Q_PROPERTY(qreal wallpapersBlur READ wallpapersBlur WRITE setWallpapersBlur NOTIFY wallpapersBlurChanged)
    Q_PROPERTY(qreal dialogOpacity READ dialogOpacity WRITE setDialogOpacity NOTIFY dialogOpacityChanged)
    Q_PROPERTY(qreal dialogBlur READ dialogBlur WRITE setDialogBlur NOTIFY dialogBlurChanged)
    Q_PROPERTY(bool autoGlass READ autoGlass WRITE setAutoGlass NOTIFY autoGlassChanged)
    Q_PROPERTY(QString animType READ animType WRITE setAnimType NOTIFY animTypeChanged)
    Q_PROPERTY(QString welcomeAnimType READ welcomeAnimType WRITE setWelcomeAnimType NOTIFY welcomeAnimTypeChanged)
    Q_PROPERTY(int animDuration READ animDuration WRITE setAnimDuration NOTIFY animDurationChanged)
    Q_PROPERTY(bool rememberLastTab READ rememberLastTab WRITE setRememberLastTab NOTIFY rememberLastTabChanged)
    Q_PROPERTY(QString hiddenSidebarTabs READ hiddenSidebarTabs WRITE setHiddenSidebarTabs NOTIFY hiddenSidebarTabsChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

    Q_PROPERTY(bool borderGlowEnabled READ borderGlowEnabled WRITE setBorderGlowEnabled NOTIFY borderGlowEnabledChanged)
    Q_PROPERTY(bool borderGlowAllPages READ borderGlowAllPages WRITE setBorderGlowAllPages NOTIFY borderGlowAllPagesChanged)
    Q_PROPERTY(qreal borderGlowIntensity READ borderGlowIntensity WRITE setBorderGlowIntensity NOTIFY borderGlowIntensityChanged)
    Q_PROPERTY(qreal borderGlowSpread READ borderGlowSpread WRITE setBorderGlowSpread NOTIFY borderGlowSpreadChanged)
    Q_PROPERTY(qreal borderGlowSpeed READ borderGlowSpeed WRITE setBorderGlowSpeed NOTIFY borderGlowSpeedChanged)
    Q_PROPERTY(int borderGlowColorMode READ borderGlowColorMode WRITE setBorderGlowColorMode NOTIFY borderGlowColorModeChanged)
    Q_PROPERTY(int borderGlowDirection READ borderGlowDirection WRITE setBorderGlowDirection NOTIFY borderGlowDirectionChanged)
    Q_PROPERTY(qreal cursorGlowIntensity READ cursorGlowIntensity WRITE setCursorGlowIntensity NOTIFY cursorGlowIntensityChanged)
    Q_PROPERTY(qreal clickFlashIntensity READ clickFlashIntensity WRITE setClickFlashIntensity NOTIFY clickFlashIntensityChanged)
    Q_PROPERTY(int clickFlashShape READ clickFlashShape WRITE setClickFlashShape NOTIFY clickFlashShapeChanged)

    // Цвета и раздельное редактирование палитры
    Q_PROPERTY(bool syncColorsWithAccent READ syncColorsWithAccent WRITE setSyncColorsWithAccent NOTIFY syncColorsWithAccentChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged)
    Q_PROPERTY(QColor accentTextColor READ accentTextColor NOTIFY accentColorChanged)
    Q_PROPERTY(QColor borderGlowCustomColor READ borderGlowCustomColor WRITE setBorderGlowCustomColor NOTIFY borderGlowCustomColorChanged)
    Q_PROPERTY(QColor cursorCustomColor READ cursorCustomColor WRITE setCursorCustomColor NOTIFY cursorCustomColorChanged)
    Q_PROPERTY(QColor windowBgColor READ windowBgColor WRITE setWindowBgColor NOTIFY windowBgColorChanged)
    Q_PROPERTY(QColor cardBgColor READ cardBgColor WRITE setCardBgColor NOTIFY cardBgColorChanged)

public:
    explicit AppConfig(QObject *parent = nullptr);

    int fontSize() const { return m_fontSize; }
    void setFontSize(int v);
    qreal uiScale() const { return m_uiScale; }
    void setUiScale(qreal v);
    int windowHeight() const { return m_windowHeight; }
    void setWindowHeight(int v);
    bool alignCenter() const { return m_alignCenter; }
    void setAlignCenter(bool v);
    int welcomeAlignMode() const { return m_welcomeAlignMode; }
    void setWelcomeAlignMode(int v);
    int sidebarWidth() const { return m_sidebarWidth; }
    void setSidebarWidth(int v);
    qreal opacity() const { return m_opacity; }
    void setOpacity(qreal v);
    qreal sidebarOpacity() const { return m_sidebarOpacity; }
    void setSidebarOpacity(qreal v);
    qreal cardOpacity() const { return m_cardOpacity; }
    void setCardOpacity(qreal v);
    qreal wallpapersOpacity() const { return m_wallpapersOpacity; }
    void setWallpapersOpacity(qreal v);
    qreal wallpapersBlur() const { return m_wallpapersBlur; }
    void setWallpapersBlur(qreal v);
    qreal dialogOpacity() const { return m_dialogOpacity; }
    void setDialogOpacity(qreal v);
    qreal dialogBlur() const { return m_dialogBlur; }
    void setDialogBlur(qreal v);
    bool autoGlass() const { return m_autoGlass; }
    void setAutoGlass(bool v);
    QString animType() const { return m_animType; }
    void setAnimType(const QString &v);
    QString welcomeAnimType() const { return m_welcomeAnimType; }
    void setWelcomeAnimType(const QString &v);
    int animDuration() const { return m_animDuration; }
    void setAnimDuration(int v);
    bool rememberLastTab() const { return m_rememberLastTab; }
    void setRememberLastTab(bool v);

    QString hiddenSidebarTabs() const { return m_hiddenSidebarTabs; }
    void setHiddenSidebarTabs(const QString &v);
    Q_INVOKABLE bool isTabVisible(int idx) const;
    Q_INVOKABLE void setTabVisible(int idx, bool visible);

    QString language() const { return m_language; }
    void setLanguage(const QString &v);
    Q_INVOKABLE QString trAuto(const QString &ruText) const;

    bool borderGlowEnabled() const { return m_borderGlowEnabled; }
    void setBorderGlowEnabled(bool v);
    bool borderGlowAllPages() const { return m_borderGlowAllPages; }
    void setBorderGlowAllPages(bool v);
    qreal borderGlowIntensity() const { return m_borderGlowIntensity; }
    void setBorderGlowIntensity(qreal v);
    qreal borderGlowSpread() const { return m_borderGlowSpread; }
    void setBorderGlowSpread(qreal v);
    qreal borderGlowSpeed() const { return m_borderGlowSpeed; }
    void setBorderGlowSpeed(qreal v);
    int borderGlowColorMode() const { return m_borderGlowColorMode; }
    void setBorderGlowColorMode(int v);
    int borderGlowDirection() const { return m_borderGlowDirection; }
    void setBorderGlowDirection(int v);
    qreal cursorGlowIntensity() const { return m_cursorGlowIntensity; }
    void setCursorGlowIntensity(qreal v);
    qreal clickFlashIntensity() const { return m_clickFlashIntensity; }
    void setClickFlashIntensity(qreal v);
    int clickFlashShape() const { return m_clickFlashShape; }
    void setClickFlashShape(int v);

    bool syncColorsWithAccent() const { return m_syncColorsWithAccent; }
    void setSyncColorsWithAccent(bool v);
    QColor accentColor() const { return m_accentColor; }
    void setAccentColor(const QColor &c);
    QColor accentTextColor() const;
    QColor borderGlowCustomColor() const { return m_borderGlowCustomColor; }
    void setBorderGlowCustomColor(const QColor &c);
    QColor cursorCustomColor() const { return m_cursorCustomColor; }
    void setCursorCustomColor(const QColor &c);
    QColor windowBgColor() const { return m_windowBgColor; }
    void setWindowBgColor(const QColor &c);
    QColor cardBgColor() const { return m_cardBgColor; }
    void setCardBgColor(const QColor &c);

    Q_INVOKABLE QString consumeLaunchCmd();
    Q_INVOKABLE bool isDaemonStart() const;
    Q_INVOKABLE void resetColors();
    Q_INVOKABLE void resetDefaults();
    Q_INVOKABLE void save();
    void load();

signals:
    void fontSizeChanged();
    void uiScaleChanged();
    void windowHeightChanged();
    void alignCenterChanged();
    void welcomeAlignModeChanged();
    void sidebarWidthChanged();
    void opacityChanged();
    void sidebarOpacityChanged();
    void cardOpacityChanged();
    void wallpapersOpacityChanged();
    void wallpapersBlurChanged();
    void dialogOpacityChanged();
    void dialogBlurChanged();
    void autoGlassChanged();
    void animTypeChanged();
    void welcomeAnimTypeChanged();
    void animDurationChanged();
    void rememberLastTabChanged();
    void hiddenSidebarTabsChanged();
    void languageChanged();
    void borderGlowEnabledChanged();
    void borderGlowAllPagesChanged();
    void borderGlowIntensityChanged();
    void borderGlowSpreadChanged();
    void borderGlowSpeedChanged();
    void borderGlowColorModeChanged();
    void borderGlowDirectionChanged();
    void cursorGlowIntensityChanged();
    void clickFlashIntensityChanged();
    void clickFlashShapeChanged();
    void syncColorsWithAccentChanged();
    void accentColorChanged();
    void borderGlowCustomColorChanged();
    void cursorCustomColorChanged();
    void windowBgColorChanged();
    void cardBgColorChanged();

private:
    QString configFilePath() const;
    void syncHyprlandLuaHeight(int h) const;
    void initTranslations();

    int m_fontSize = 11;
    qreal m_uiScale = 1.0;
    int m_windowHeight = 720;
    bool m_alignCenter = false;
    int m_welcomeAlignMode = 1;
    int m_sidebarWidth = 136;
    qreal m_opacity = 0.70;
    qreal m_sidebarOpacity = 0.00;
    qreal m_cardOpacity = 0.10;
    qreal m_wallpapersOpacity = 0.59;
    qreal m_wallpapersBlur = 0.15;
    qreal m_dialogOpacity = 0.45;
    qreal m_dialogBlur = 0.65;
    bool m_autoGlass = true;
    QString m_animType = "bounce";
    QString m_welcomeAnimType = "blackhole";
    int m_animDuration = 150;
    bool m_rememberLastTab = false;
    QString m_hiddenSidebarTabs = "";
    QString m_language = "ru";

    bool m_borderGlowEnabled = true;
    bool m_borderGlowAllPages = true;
    qreal m_borderGlowIntensity = 0.75;
    qreal m_borderGlowSpread = 0.45;
    qreal m_borderGlowSpeed = 1.00;
    int m_borderGlowColorMode = 0;
    int m_borderGlowDirection = 0;
    qreal m_cursorGlowIntensity = 0.65;
    qreal m_clickFlashIntensity = 0.70;
    int m_clickFlashShape = 0;

    bool m_syncColorsWithAccent = true;
    QColor m_accentColor = QColor("#ffffff");
    QColor m_borderGlowCustomColor = QColor("#ffffff");
    QColor m_cursorCustomColor = QColor("#ffffff");
    QColor m_windowBgColor = QColor("#0a0a0c");
    QColor m_cardBgColor = QColor("#141419");

    QHash<QString, QString> m_enDict;
};
