#pragma once

#include <QStringList>

class AuditManager
{
public:
    void logAction(const QString &description) const;
    QStringList recentActions(int count = 100) const;

private:
    QString logFilePath() const;
};