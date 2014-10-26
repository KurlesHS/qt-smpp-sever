#ifndef SMPPSESSION_H
#define SMPPSESSION_H
#include "smppcxx/smpp.hpp"

#include <QObject>
#include <QTcpSocket>
#include <QAtomicInt>

class QTimer;

class SmppSession : public QObject
{
    Q_OBJECT
public:
    explicit SmppSession(QTcpSocket *clientSocket, QObject *parent = 0);
    void handleIncomingData();

signals:

private:
    static int nextSequenceNumber();
    void handleBindTransceiver(const QByteArray &pdu);
    void handleSubmitSm(const QByteArray &pdu);
    void handleEnquireLink(const QByteArray &pdu);
    void handleUnbind(const QByteArray &pdu);

    void sendDeliveryReport(const QString &messageId);

    void startTimeoutTimer();
    void stopTimeoutTimer();

    void printErrorAndDisconnect(const Smpp::Error &e);
    void disconnectFromClient();

    void logMsg(const QString &msg);

    template<typename T>
    void writeSmppPacket(T &packet) {
        QByteArray outgoingPdu((const char *)packet.encode(), packet.command_length());
        m_clientSocket->write(outgoingPdu);
    }

    static QString createNewMessageId();

private slots:
   void onClientDisconnected();
   void onReadyRead();
   void onTimerTimeout();

private:
    QTcpSocket * const m_clientSocket;
    QByteArray m_buffer;
    static QAtomicInt m_sequenceNumber;
    QTimer *m_timeoutTimer;
    QString m_logStringTemplage;
    QString m_clientStrAddr;

};

#endif // SMPPSESSION_H
