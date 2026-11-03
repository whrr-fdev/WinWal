#pragma once
#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>

class AppInstance : public QObject {
    Q_OBJECT
public:
    explicit AppInstance(const QString &serverName, QObject *parent = nullptr);
    bool checkSingleInstance();

signals:
    void toggleRequested();

private:
    QString m_serverName;
    QLocalServer *m_server = nullptr;
};
