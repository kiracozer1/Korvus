#pragma once

#include <QString>

enum class Direction { Inbound, Outbound };
enum class Protocol { Tcp, Udp };
enum class Action { Accept, Drop };


struct Rule
{
    QString name; // Yeni eklenen kural ismi
    Direction direction = Direction::Inbound;
    Protocol protocol = Protocol::Tcp;
    int port = 0;
    Action action = Action::Accept;
    bool enabled = true;
};

inline QString directionToString(Direction d)
{
    return d == Direction::Inbound ? QStringLiteral("Inbound") : QStringLiteral("Outbound");
}

inline QString protocolToString(Protocol p)
{
    return p == Protocol::Tcp ? QStringLiteral("TCP") : QStringLiteral("UDP");
}

inline QString actionToString(Action a)
{
    return a == Action::Accept ? QStringLiteral("Accept") : QStringLiteral("Drop");
}

inline QString nftChainForDirection(Direction d)
{
    return d == Direction::Inbound ? QStringLiteral("input") : QStringLiteral("output");
}

inline QString nftProtocolToken(Protocol p)
{
    return p == Protocol::Tcp ? QStringLiteral("tcp") : QStringLiteral("udp");
}

inline QString nftActionToken(Action a)
{
    return a == Action::Accept ? QStringLiteral("accept") : QStringLiteral("drop");
}