#include "MiniWebSocket.hpp"

#include <QCryptographicHash>
#include <QRandomGenerator>

MiniWebSocket::MiniWebSocket(QObject *parent) : QObject(parent)
{
	connect(&socket_, &QSslSocket::encrypted, this, &MiniWebSocket::onEncrypted);
	connect(&socket_, &QSslSocket::readyRead, this, &MiniWebSocket::onReadyRead);
	connect(&socket_, &QSslSocket::errorOccurred, this, &MiniWebSocket::onError);
	connect(&socket_, &QSslSocket::sslErrors, this, &MiniWebSocket::onSslErrors);
	connect(&socket_, &QSslSocket::disconnected, this, [this]() {
		handshakeDone_ = false;
		emit closed();
	});
}

void MiniWebSocket::open(const QUrl &url)
{
	handshakeDone_ = false;
	buffer_.clear();
	host_ = url.host();
	path_ = url.path();
	if (url.hasQuery())
		path_ += QStringLiteral("?") + url.query();
	if (path_.isEmpty())
		path_ = QStringLiteral("/");

	key_ = QByteArray(16, Qt::Uninitialized);
	for (int i = 0; i < 16; ++i)
		key_[i] = char(QRandomGenerator::global()->bounded(256));
	key_ = key_.toBase64();

	const int port = url.port(443);
	socket_.connectToHostEncrypted(host_, quint16(port));
}

void MiniWebSocket::close()
{
	socket_.disconnectFromHost();
}

void MiniWebSocket::sendText(const QString &text)
{
	if (!handshakeDone_)
		return;
	const QByteArray payload = text.toUtf8();
	QByteArray frame;
	frame.append(char(0x81)); /* FIN + text */
	const int len = payload.size();
	if (len < 126) {
		frame.append(char(0x80 | len));
	} else if (len <= 0xFFFF) {
		frame.append(char(0x80 | 126));
		frame.append(char((len >> 8) & 0xFF));
		frame.append(char(len & 0xFF));
	} else {
		return; /* oversized for this mini client */
	}
	QByteArray mask(4, Qt::Uninitialized);
	for (int i = 0; i < 4; ++i)
		mask[i] = char(QRandomGenerator::global()->bounded(256));
	frame.append(mask);
	for (int i = 0; i < len; ++i)
		frame.append(char(payload[i] ^ mask[i % 4]));
	socket_.write(frame);
}

bool MiniWebSocket::isOpen() const
{
	return handshakeDone_ && socket_.state() == QAbstractSocket::ConnectedState;
}

void MiniWebSocket::onEncrypted()
{
	QByteArray req;
	req += "GET " + path_.toUtf8() + " HTTP/1.1\r\n";
	req += "Host: " + host_.toUtf8() + "\r\n";
	req += "Upgrade: websocket\r\n";
	req += "Connection: Upgrade\r\n";
	req += "Sec-WebSocket-Key: " + key_ + "\r\n";
	req += "Sec-WebSocket-Version: 13\r\n";
	req += "\r\n";
	socket_.write(req);
}

void MiniWebSocket::onReadyRead()
{
	buffer_.append(socket_.readAll());
	if (!handshakeDone_) {
		const int idx = buffer_.indexOf("\r\n\r\n");
		if (idx < 0)
			return;
		const QByteArray header = buffer_.left(idx);
		buffer_.remove(0, idx + 4);
		if (!header.contains("101")) {
			emit errorOccurred(QStringLiteral("WebSocket handshake failed"));
			socket_.disconnectFromHost();
			return;
		}
		handshakeDone_ = true;
		emit opened();
	}
	flushParse();
}

void MiniWebSocket::flushParse()
{
	while (parseFrame()) {
	}
}

bool MiniWebSocket::parseFrame()
{
	if (buffer_.size() < 2)
		return false;
	const unsigned char b0 = (unsigned char)buffer_[0];
	const unsigned char b1 = (unsigned char)buffer_[1];
	const bool masked = (b1 & 0x80) != 0;
	quint64 len = b1 & 0x7F;
	int offset = 2;
	if (len == 126) {
		if (buffer_.size() < 4)
			return false;
		len = ((unsigned char)buffer_[2] << 8) | (unsigned char)buffer_[3];
		offset = 4;
	} else if (len == 127) {
		return false; /* not supported */
	}
	const int maskLen = masked ? 4 : 0;
	if (buffer_.size() < offset + maskLen + int(len))
		return false;

	QByteArray mask;
	if (masked)
		mask = buffer_.mid(offset, 4);
	offset += maskLen;
	QByteArray payload = buffer_.mid(offset, int(len));
	buffer_.remove(0, offset + int(len));
	if (masked) {
		for (int i = 0; i < payload.size(); ++i)
			payload[i] = char(payload[i] ^ mask[i % 4]);
	}

	const int opcode = b0 & 0x0F;
	if (opcode == 0x1) {
		emit textMessageReceived(QString::fromUtf8(payload));
	} else if (opcode == 0x8) {
		socket_.disconnectFromHost();
	} else if (opcode == 0x9) { /* ping → pong */
		QByteArray pong;
		pong.append(char(0x8A));
		pong.append(char(0x80 | payload.size()));
		QByteArray m(4, Qt::Uninitialized);
		for (int i = 0; i < 4; ++i)
			m[i] = char(QRandomGenerator::global()->bounded(256));
		pong.append(m);
		for (int i = 0; i < payload.size(); ++i)
			pong.append(char(payload[i] ^ m[i % 4]));
		socket_.write(pong);
	}
	return true;
}

void MiniWebSocket::onError(QAbstractSocket::SocketError)
{
	emit errorOccurred(socket_.errorString());
}

void MiniWebSocket::onSslErrors(const QList<QSslError> &)
{
	socket_.ignoreSslErrors();
}
