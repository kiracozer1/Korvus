#pragma once

#include <QObject>
#include <QVector>
#include <QString>


#include "rule.h"
#include "auditmanager.h"


class FirewallManager : public QObject
{
    Q_OBJECT
public:
    explicit FirewallManager(QObject *parent = nullptr);

    bool isActive() const;
    QString defaultPolicy() const; // "ALLOW" or "DENY"
    bool setDefaultPolicy(const QString &policy);

    const QVector<Rule> &rules() const;
    bool addRule(const Rule &rule);
    bool removeRule(int index);
    bool setRuleEnabled(int index, bool enabled);

    void refresh();

    QStringList blockedIps() const;
    bool blockIp(const QString &ip);
    bool unblockIp(const QString &ip);

private:
    bool runNft(const QStringList &args) const;
    void ensureTableExists() const;
    void applyEnabledRules() const;

    QString rulesFilePath() const;
    void loadRules();
    void saveRules() const;
    AuditManager m_auditManager;
    QStringList m_protectedIps;
void loadProtectedIps();

    QVector<Rule> m_rules;
    QString m_defaultPolicy = QStringLiteral("ALLOW");
        QVector<QString> m_blockedIps; 
};
