#pragma once

#include <QByteArray>
#include <QObject>
#include <QSslSocket>
#include <QString>
#include <QUrl>

/** Minimal client WebSocket over QSslSocket (text frames only). */
class MiniWebSocket : public QObject {
	Q_OBJECT
public:
	explicit MiniWebSocket(QObject *parent = nullptr);

	void open(const QUrl &url);
	void close();
	void sendText(const QString &text);
	bool isOpen() const;

signals:
	void opened();
	void closed();
	void textMessageReceived(const QString &text);
	void errorOccurred(const QString &error);

private slots:
	void onReadyRead();
	void onEncrypted();
	void onError(QAbstractSocket::SocketError);
	void onSslErrors(const QList<QSslError> &errors);

private:
	void flushParse();
	bool parseFrame();

	QSslSocket socket_;
	QByteArray buffer_;
	bool handshakeDone_ = false;
	QString host_;
	QString path_;
	QByteArray key_;
};
