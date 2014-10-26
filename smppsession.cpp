#include "smppsession.h"
#include "qtopia/qgsmcodec.h"

#include <QTimer>
#include <QHostAddress>
#include <QUuid>

QAtomicInt SmppSession::m_sequenceNumber = QAtomicInt(0x01);

class SmppHeader : public Smpp::Request {
public:
    SmppHeader() : Smpp::Request(Smpp::CommandLength(0x10),
                                Smpp::CommandId(0x00),
                                Smpp::SequenceNumber(0x01)) {

    }
    void decode(const Smpp::Uint8* b) { Header::decode(b); }
};

SmppSession::SmppSession(QTcpSocket *clientSocket, QObject *parent) :
    QObject(parent),
    m_clientSocket(clientSocket),
    m_timeoutTimer(new QTimer(this)),
    m_clientStrAddr("Unknown address")
{
    m_timeoutTimer->setSingleShot(true);
    if (m_clientSocket) {
        m_clientStrAddr = QString("%0:%1").arg(m_clientSocket->localAddress().toString()).arg(m_clientSocket->localPort());
    }
    m_logStringTemplage = QString("[SmppSession: %0] %1").arg(m_clientStrAddr);
    connect(clientSocket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(clientSocket, SIGNAL(disconnected()), this, SLOT(onClientDisconnected()));
    connect(m_timeoutTimer, SIGNAL(timeout()), this, SLOT(onTimerTimeout()));
    // первый запрос ждём 10 секунд
    m_timeoutTimer->start(10000);
}

void SmppSession::handleIncomingData()
{
    if (m_buffer.length() < 16) {
        m_timeoutTimer->start(5000);
        return;
    }

    SmppHeader header;
    header.decode((const Smpp::Uint8 *)m_buffer.data());
    if ((int)header.command_length() < m_buffer.length()) {
        m_timeoutTimer->start(5000);
        return;
    }

    Smpp::Uint32 commandId = header.command_id();
    QByteArray pdu = m_buffer.mid(0, header.command_length());
    m_buffer.remove(0, header.command_length());

    switch (commandId) {
    case Smpp::CommandId::BindTransceiver:
        handleBindTransceiver(pdu);
        break;
    case Smpp::CommandId::SubmitSm:
        handleSubmitSm(pdu);
        break;
    case Smpp::CommandId::EnquireLink:
        handleEnquireLink(pdu);
        break;
    case Smpp::CommandId::Unbind:
        handleUnbind(pdu);
        break;
    default:
        logMsg(QString("unsupported commandId: 0x%0")
               .arg(commandId, 0x08, 0x10, QChar('0')));
        break;
    }
}

int SmppSession::nextSequenceNumber()
{
    return m_sequenceNumber.fetchAndAddOrdered(0x01);
}

void SmppSession::handleBindTransceiver(const QByteArray &pdu)
{
    Smpp::BindTransceiver bindTransceiver;
    try {
        bindTransceiver.decode((const Smpp::Uint8 *)pdu.data());
    } catch (Smpp::Error e) {
        printErrorAndDisconnect(e);
        return;
    }
}

void SmppSession::handleSubmitSm(const QByteArray &pdu)
{
    Smpp::SubmitSm submitSm;
    try {
        submitSm.decode((const Smpp::Uint8 *)pdu.data());
    } catch (Smpp::Error e) {
        printErrorAndDisconnect(e);
        return;
    }
    Smpp::String addr = submitSm.destination_addr().address();

    QString recepitient = QString::fromUtf8(addr.data());
    bool registeredDelivery = submitSm.registered_delivery();
    std::vector<Smpp::Uint8> shortMessage = submitSm.short_message();
    Smpp::DataCoding dataCoding = submitSm.data_coding();
    QByteArray smsTextCoded((const char*)shortMessage.data(),
                            shortMessage.size());
    Smpp::SubmitSmResp resp;
    resp.message_id();
    resp.sequence_number(submitSm.sequence_number());
    resp.command_status(0);
    if (dataCoding != 0x00 || dataCoding != 0x08) {
        logMsg(QString("SubmitSm command received with unknown datacoding (0x%0)").arg(dataCoding, 2, 16, QChar('0')));
        resp.command_status(0x45);
    } else {
        QTextCodec *codec = QTextCodec::codecForName("UTF-16");
        QString smsText = codec->toUnicode(smsTextCoded);
        logMsg(QString("SubmitSm command received. Text: %0, recepient: %1, registered delivery: %2")
               .arg(smsText, recepitient, registeredDelivery ? "true" : "false"));
    }
    writeSmppPacket<Smpp::SubmitSmResp>(resp);
}

void SmppSession::handleEnquireLink(const QByteArray &pdu)
{
    Smpp::EnquireLink enquireLink;
    try {
        enquireLink.decode((const Smpp::Uint8 *)pdu.data());
    } catch (Smpp::Error e) {
        printErrorAndDisconnect(e);
        return;
    }
    logMsg(QString("EnquireLink command received"));
    Smpp::EnquireLinkResp resp;
    resp.sequence_number(enquireLink.sequence_number());
    resp.command_status(0);
    writeSmppPacket<Smpp::EnquireLinkResp>(resp);
}

void SmppSession::handleUnbind(const QByteArray &pdu)
{
    Smpp::Unbind unbind;
    try {
        unbind.decode((const Smpp::Uint8 *)pdu.data());
    } catch (Smpp::Error e) {
        printErrorAndDisconnect(e);
        return;
    }
    logMsg(QString("Unbind command received, disconnecting..."));
    Smpp::UnbindResp resp;
    resp.sequence_number(unbind.sequence_number());
    resp.command_status(0);
    writeSmppPacket<Smpp::UnbindResp>(resp);
    m_clientSocket->waitForBytesWritten(4000);
    disconnectFromClient();
    deleteLater();
}

void SmppSession::sendDeliveryReport(const QString &messageId)
{
    Smpp::DeliverSm deliverSm;
    deliverSm.esm_class(0x04); // delivery report
    deliverSm.sequence_number(nextSequenceNumber());
    deliverSm.insert_string_tlv(Smpp::Tlv::receipted_message_id, messageId.toStdString());
#if 0
    ENROUTE       1 Сообщение находится в состоянии в пути (enroute).
    DELIVERED     2 Сообщение доставлено адресату.
    EXPIRED       3 Истек период допустимости сообщения.
    DELETED       4 Сообщение было удалено.
    UNDELIVERABLE 5 Сообщение является недоставляемым.
    ACCEPTED      6 Сообщение находится в принятом состоянии
                    (т.е. читалось вручную от имени абонента
                    клиентской службой).
    UNKNOWN       7 Сообщение находится в недопустимом состоянии.
    REJECTED      8 Сообщение находится в отклоненном состоянии.
#endif
    deliverSm.insert_8bit_tlv(Smpp::Tlv::message_state, 0x02); //delivered
    writeSmppPacket<Smpp::DeliverSm>(deliverSm);

}

void SmppSession::startTimeoutTimer()
{
    // 5 секунд на допринятие данных? It's fucking too much time for do this?
    m_timeoutTimer->start(5000);
}

void SmppSession::stopTimeoutTimer()
{
    if (m_timeoutTimer->isActive()) {
        m_timeoutTimer->stop();
    }
}

void SmppSession::printErrorAndDisconnect(const Smpp::Error &e)
{
    logMsg(QString("error: %e").arg(e.what()));
    disconnectFromClient();
    deleteLater();
}

void SmppSession::disconnectFromClient()
{
    if (m_clientSocket) {
        m_clientSocket->disconnectFromHost();
        m_clientSocket->deleteLater();
    }
}

void SmppSession::logMsg(const QString &msg)
{

    qDebug(m_logStringTemplage.arg(msg).toUtf8());
}

QString SmppSession::createNewMessageId()
{
    return QUuid::createUuid().toString().remove("-").remove("{").remove("}");
}

void SmppSession::onClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());
    if (client) {
        client->deleteLater();
    }
    deleteLater();
}

void SmppSession::onReadyRead()
{
    QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());
    if (client) {
        m_buffer.append(client->readAll());
        handleIncomingData();
    }
}

void SmppSession::onTimerTimeout()
{
    stopTimeoutTimer();
    m_buffer.clear();
    Smpp::Error e("timeout while wait data from client, disconnecting...");
    printErrorAndDisconnect(e);
}
