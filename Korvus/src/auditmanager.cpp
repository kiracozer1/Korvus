#include "auditmanager.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>

QString AuditManager::logFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/actions.log");
}

void AuditManager::logAction(const QString &description) const
{
    QFile file(logFilePath());
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return;

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    file.write(QStringLiteral("[%1] %2\n").arg(timestamp, description).toUtf8());
}

QStringList AuditManager::recentActions(int count) const
{
    QFile file(logFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QStringList lines;
    while (!file.atEnd())
        lines.append(QString::fromUtf8(file.readLine()).trimmed());

    if (lines.size() > count)
        lines = lines.mid(lines.size() - count);

    QStringList newestFirst;
    for (int i = lines.size() - 1; i >= 0; --i)
        newestFirst.append(lines.at(i));

    return newestFirst;
}