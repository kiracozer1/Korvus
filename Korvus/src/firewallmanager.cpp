#include "firewallmanager.h"

#include <QProcess>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

namespace {
constexpr auto kTableName = "Korvus";
}

FirewallManager::FirewallManager(QObject *parent)
    : QObject(parent)
{
    loadRules();
    loadProtectedIps();
    ensureTableExists();
    setDefaultPolicy(m_defaultPolicy);
}

bool FirewallManager::runNft(const QStringList &args) const
{
      QProcess process;
    process.start(QStringLiteral("nft"), args);
    process.waitForFinished(3000);

    const bool ok = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    if (!ok) {
        qWarning().noquote() << "nft failed:" << args.join(QStringLiteral(" "))
                              << "->" << QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
    }
    return ok;
}

void FirewallManager::ensureTableExists() const
{
    runNft({"add", "table", "inet", kTableName});
    runNft({"add", "chain", "inet", kTableName, "input",
            "{", "type", "filter", "hook", "input", "priority", "0", ";", "policy", "accept", ";", "}"});
    runNft({"add", "chain", "inet", kTableName, "output",
            "{", "type", "filter", "hook", "output", "priority", "0", ";", "policy", "accept", ";", "}"});
}

bool FirewallManager::isActive() const
{
    QProcess process;
    process.start(QStringLiteral("nft"), {"list", "table", "inet", QString::fromLatin1(kTableName)});
    process.waitForFinished(3000);
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

QString FirewallManager::defaultPolicy() const
{
    return m_defaultPolicy;
}

bool FirewallManager::setDefaultPolicy(const QString &policy)
{
    const QString nftPolicy = (policy == QStringLiteral("DENY")) ? QStringLiteral("drop") : QStringLiteral("accept");

    const bool inputOk = runNft({"chain", "inet", kTableName, "input", "{", "policy", nftPolicy, ";", "}"});
    const bool outputOk = runNft({"chain", "inet", kTableName, "output", "{", "policy", nftPolicy, ";", "}"});

    if (!inputOk || !outputOk)
        return false;

    m_defaultPolicy = policy;
    saveRules();
     m_auditManager.logAction(QStringLiteral("Default policy set to %1").arg(policy));
    return true;
}

const QVector<Rule> &FirewallManager::rules() const
{
    return m_rules;
}

void FirewallManager::applyEnabledRules() const
{
    runNft({"flush", "chain", "inet", kTableName, "input"});
    runNft({"flush", "chain", "inet", kTableName, "output"});
     
    
    for (const QString &ip : m_blockedIps) {
        runNft({"add", "rule", "inet", kTableName, "input", "ip", "saddr", ip,
                "log", "prefix", "\"Korvus-AUTOBLOCK-IN: \"", "drop"});
    }

 
    runNft({"add", "rule", "inet", kTableName, "input", "ct", "state", "established,related", "accept"});
    runNft({"add", "rule", "inet", kTableName, "output", "ct", "state", "established,related", "accept"});


    
    runNft({"add", "rule", "inet", kTableName, "input", "log", "prefix", "\"Korvus-TRAFFIC-IN: \""});
    runNft({"add", "rule", "inet", kTableName, "output", "log", "prefix", "\"Korvus-TRAFFIC-OUT: \""});

    for (const Rule &rule : m_rules) {
        if (!rule.enabled)
            continue;

        const QString chain = nftChainForDirection(rule.direction);
        const QString suffix = chain == QStringLiteral("input") ? QStringLiteral("IN") : QStringLiteral("OUT");
        const QString verdictTag = rule.action == Action::Accept ? QStringLiteral("ALLOW") : QStringLiteral("BLOCK");
        const QString prefix = QStringLiteral("\"Korvus-%1-%2: \"").arg(verdictTag, suffix);

        runNft({"add", "rule", "inet", kTableName, chain,
                nftProtocolToken(rule.protocol), "dport", QString::number(rule.port),
                "log", "prefix", prefix, nftActionToken(rule.action)});
    }

    const QString policyVerdictTag = m_defaultPolicy == QStringLiteral("DENY")
        ? QStringLiteral("BLOCK") : QStringLiteral("ALLOW");

    runNft({"add", "rule", "inet", kTableName, "input", "log", "prefix",
            QStringLiteral("\"Korvus-%1-IN: \"").arg(policyVerdictTag)});
    runNft({"add", "rule", "inet", kTableName, "output", "log", "prefix",
            QStringLiteral("\"Korvus-%1-OUT: \"").arg(policyVerdictTag)});
}
bool FirewallManager::addRule(const Rule &rule)
{
    m_rules.append(rule);
    applyEnabledRules();
    saveRules();
    m_auditManager.logAction(QStringLiteral("Rule added: \"%1\" (%2, %3, port %4, %5)")
        .arg(rule.name, directionToString(rule.direction), protocolToString(rule.protocol))
        .arg(rule.port)
        .arg(actionToString(rule.action)));
    return true;
}

bool FirewallManager::removeRule(int index)
{
    if (index < 0 || index >= m_rules.size())
        return false;

    const QString name = m_rules[index].name;
    m_rules.remove(index);
    applyEnabledRules();
    saveRules();
    m_auditManager.logAction(QStringLiteral("Rule removed: \"%1\"").arg(name));
    return true;
}

bool FirewallManager::setRuleEnabled(int index, bool enabled)
{
    if (index < 0 || index >= m_rules.size())
        return false;

    m_rules[index].enabled = enabled;
    applyEnabledRules();
    saveRules();
    return true;
}

void FirewallManager::refresh()
{
    ensureTableExists();
    applyEnabledRules();
}

QString FirewallManager::rulesFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/rules.json");
}

void FirewallManager::loadRules()
{
    QFile file(rulesFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = doc.object();

    m_defaultPolicy = root.value(QStringLiteral("defaultPolicy")).toString(QStringLiteral("ALLOW"));

    const QJsonArray array = root.value(QStringLiteral("rules")).toArray();
    for (const QJsonValue &value : array) {
        const QJsonObject obj = value.toObject();
        Rule rule;
        rule.name = obj.value(QStringLiteral("name")).toString(); 
        rule.direction = obj.value(QStringLiteral("direction")).toString() == QStringLiteral("out")
                            ? Direction::Outbound
                            : Direction::Inbound;
        rule.protocol = obj.value(QStringLiteral("protocol")).toString() == QStringLiteral("udp")
                           ? Protocol::Udp
                           : Protocol::Tcp;
        rule.port = obj.value(QStringLiteral("port")).toInt();
        rule.action = obj.value(QStringLiteral("action")).toString() == QStringLiteral("drop")
                        ? Action::Drop
                        : Action::Accept;
        rule.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
        m_rules.append(rule);
    }
    const QJsonArray blockedArray = root.value(QStringLiteral("blockedIps")).toArray();
    for (const QJsonValue &value : blockedArray)
        m_blockedIps.append(value.toString());
}

void FirewallManager::saveRules() const
{
    QJsonArray array;
    for (const Rule &rule : m_rules) {
        QJsonObject obj;
        obj[QStringLiteral("name")] = rule.name; 
        obj[QStringLiteral("direction")] = rule.direction == Direction::Outbound ? QStringLiteral("out") : QStringLiteral("in");
        obj[QStringLiteral("protocol")] = rule.protocol == Protocol::Udp ? QStringLiteral("udp") : QStringLiteral("tcp");
        obj[QStringLiteral("port")] = rule.port;
        obj[QStringLiteral("action")] = rule.action == Action::Drop ? QStringLiteral("drop") : QStringLiteral("accept");
        obj[QStringLiteral("enabled")] = rule.enabled;
        array.append(obj);
    }

    QJsonObject root;
    root[QStringLiteral("defaultPolicy")] = m_defaultPolicy;
    root[QStringLiteral("rules")] = array;
    QJsonArray blockedArray;
    for (const QString &ip : m_blockedIps)
        blockedArray.append(ip);
    root[QStringLiteral("blockedIps")] = blockedArray;

    QFile file(rulesFilePath());
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(root).toJson());
}
QStringList FirewallManager::blockedIps() const
{
    return QStringList(m_blockedIps.begin(), m_blockedIps.end());
}

bool FirewallManager::blockIp(const QString &ip)
{
    if (ip.startsWith(QStringLiteral("127.")) || m_protectedIps.contains(ip))
        return false;

    if (m_blockedIps.contains(ip))
        return false;

    m_blockedIps.append(ip);
    applyEnabledRules();
    saveRules();
    m_auditManager.logAction(QStringLiteral("IP blocked: %1").arg(ip));
    return true;
}

bool FirewallManager::unblockIp(const QString &ip)
{
    if (!m_blockedIps.removeOne(ip))
        return false;

    applyEnabledRules();
    saveRules();
    m_auditManager.logAction(QStringLiteral("IP unblocked: %1").arg(ip));
    return true;
}
void FirewallManager::loadProtectedIps()
{
    m_protectedIps << QStringLiteral("127.0.0.1") << QStringLiteral("::1");

    QProcess process;
    process.start(QStringLiteral("hostname"), {"-I"});
    process.waitForFinished(2000);

    if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
        const QString output = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
        for (const QString &ip : output.split(QChar(' '), Qt::SkipEmptyParts))
            m_protectedIps << ip;
    }
}