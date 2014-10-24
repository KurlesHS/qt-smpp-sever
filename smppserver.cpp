#include "smppserver.h"

SmppServer::SmppServer(const QHostAddress &address = QHostAddress::Any, quint16 port = 0, QObject *parent) :
    QObject(parent),
    m_server(new QTcpServer(this))
{
    connect(m_server, SIGNAL(newConnection()), this, SLOT(onNewConnection()));
    m_server->listen(address, port);
}

void SmppServer::onNewConnection()
{
      QTcpServer *server = qobject_cast<QTcpServer*>(sender());
      if (server) {
          while (server->hasPendingConnections()) {
              QTcpSocket *socket = server->nextPendingConnection();
              if (socket) {

              }
          }
      }
}
