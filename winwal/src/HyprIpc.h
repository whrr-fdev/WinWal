#pragma once
#include <QObject>
#include <QString>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <functional>

class HyprIpc : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString appVersion READ appVersion NOTIFY updateStateChanged)
    Q_PROPERTY(QString updateStatus READ updateStatus NOTIFY updateStateChanged)
    Q_PROPERTY(bool checkingUpdate READ checkingUpdate NOTIFY updateStateChanged)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateStateChanged)
    Q_PROPERTY(qreal updateProgress READ updateProgress NOTIFY updateStateChanged)

public:
    explicit HyprIpc(QObject *parent = nullptr);
    void setEngine(QQmlApplicationEngine *eng) { m_engine = eng; }

    std::function<void()> onCleanShutdown;
    std::function<void()> onPlayIntro;

    QString appVersion() const { return m_appVersion; }
    QString updateStatus() const { return m_updateStatus; }
    bool checkingUpdate() const { return m_checkingUpdate; }
    bool updateAvailable() const { return m_updateAvailable; }
    qreal updateProgress() const { return m_updateProgress; }

    Q_INVOKABLE static void dispatch(const QString &cmd);
    Q_INVOKABLE static QString getActiveWorkspace();
    Q_INVOKABLE void trimMemory();
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void installUpdate();
    Q_INVOKABLE void quitApplication();
    Q_INVOKABLE void playIntro();

signals:
    void updateStateChanged();

private:
    void ensureUpdProcess();

    QQmlApplicationEngine *m_engine = nullptr;
    QString m_appVersion = "1.2.0";
    QString m_remoteVersion;
    QString m_updateStatus = "Установлена актуальная версия (v1.2.0)";
    bool m_checkingUpdate = false;
    bool m_updateAvailable = false;
    qreal m_updateProgress = 0.0;
    QProcess *m_updProc = nullptr;
};