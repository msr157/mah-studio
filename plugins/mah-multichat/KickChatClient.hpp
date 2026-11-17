#pragma once

#include <QObject>
#include <QString>

class MiniWebSocket;
class QNetworkAccessManager;

/** Kick chat read via unofficial Pusher WebSocket. Send is not supported. */
class KickChatClient : public QObject {
	Q_OBJECT
public:
	explicit KickChatClient(QObject *parent = nullptr);

	void connectSlug(const QString &slug);
	void disconnectFromHost();
	bool isConnected() const;

signals:
	void connected();
	void disconnected();
	void chatLine(const QString &user, const QString &message);
	void status(const QString &text);
	void errorMessage(const QString &text);

private slots:
	void onWsText(const QString &text);
	void onChannelResolved();

private:
	void subscribe(quint64 chatroomId);

	QNetworkAccessManager *nam_ = nullptr;
	MiniWebSocket *ws_ = nullptr;
	QString slug_;
	quint64 chatroomId_ = 0;
};
