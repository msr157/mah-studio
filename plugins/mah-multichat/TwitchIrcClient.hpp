#pragma once

#include <QObject>
#include <QSslSocket>
#include <QString>

/** Minimal Twitch IRC (TLS) client for read + send. */
class TwitchIrcClient : public QObject {
	Q_OBJECT
public:
	explicit TwitchIrcClient(QObject *parent = nullptr);

	void connectTo(const QString &channel, const QString &oauthToken, const QString &nick);
	void disconnectFromHost();
	void sendMessage(const QString &text);
	bool isConnected() const;

signals:
	void connected();
	void disconnected();
	void chatLine(const QString &user, const QString &message);
	void status(const QString &text);
	void errorMessage(const QString &text);

private slots:
	void onReadyRead();
	void onSslErrors(const QList<QSslError> &errors);
	void onStateChanged(QAbstractSocket::SocketState state);

private:
	void sendRaw(const QString &line);

	QSslSocket socket_;
	QString channel_;
	QString nick_;
	QByteArray buffer_;
};
