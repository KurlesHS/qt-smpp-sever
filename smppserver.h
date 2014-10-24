#ifndef SMPPSERVER_H
#define SMPPSERVER_H

#include <QObject>
#include <QTcpServer>

class SmppServer : public QObject
{
    Q_OBJECT
public:
    explicit SmppServer(const QHostAddress &address, quint16 port, QObject *parent = 0);

signals:

public slots:

private slots:
    void onNewConnection();

private:
    QTcpServer *m_server;

};

#endif // SMPPSERVER_H
