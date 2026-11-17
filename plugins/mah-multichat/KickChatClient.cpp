#include "KickChatClient.hpp"
#include "MiniWebSocket.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

static const char *kPusherUrl =
	"wss://ws-us2.pusher.com/app/32cbd69e4b950bf97679?protocol=7&client=js&version=8.4.0&flash=false";

KickChatClient::KickChatClient(QObject *parent) : QObject(parent)
{
	nam_ = new QNetworkAccessManager(this);
	ws_ = new MiniWebSocket(this);
	connect(ws_, &MiniWebSocket::textMessageReceived, this, &KickChatClient::onWsText);
	connect(ws_, &MiniWebSocket::opened, this, [this]() {
		emit status(QStringLiteral("Kick Pusher connected — waiting subscribe…"));
	});
	connect(ws_, &MiniWebSocket::closed, this, [this]() {
		emit disconnected();
		emit status(QStringLiteral("Kick disconnected"));
	});
	connect(ws_, &MiniWebSocket::errorOccurred, this, &KickChatClient::errorMessage);
}

void KickChatClient::connectSlug(const QString &slug)
{
	slug_ = slug.trimmed().toLower();
	if (slug_.startsWith(QStringLiteral("@")))
		slug_ = slug_.mid(1);
	if (slug_.isEmpty()) {
		emit errorMessage(QStringLiteral("Kick slug required"));
		return;
	}

	emit status(QStringLiteral("Resolving Kick chatroom for %1…").arg(slug_));
	QNetworkRequest req(QUrl(QStringLiteral("https://kick.com/api/v2/channels/%1").arg(slug_)));
	req.setHeader(QNetworkRequest::UserAgentHeader,
		      QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) MAHStudio/1.0"));
	req.setRawHeader("Accept", "application/json");
	auto *reply = nam_->get(req);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			emit errorMessage(QStringLiteral("Kick resolve failed: %1 (try chatroom id manually later)")
						  .arg(reply->errorString()));
			return;
		}
		QJsonParseError err;
		const auto doc = QJsonDocument::fromJson(reply->readAll(), &err);
		if (err.error != QJsonParseError::NoError || !doc.isObject()) {
			emit errorMessage(QStringLiteral("Kick JSON parse error"));
			return;
		}
		const auto chatroom = doc.object().value(QStringLiteral("chatroom")).toObject();
		chatroomId_ = quint64(chatroom.value(QStringLiteral("id")).toDouble());
		if (chatroomId_ == 0) {
			emit errorMessage(QStringLiteral("No chatroom.id in Kick response"));
			return;
		}
		emit status(QStringLiteral("Kick chatroom id %1").arg(chatroomId_));
		subscribe(chatroomId_);
	});
}

void KickChatClient::subscribe(quint64 chatroomId)
{
	ws_->open(QUrl(QString::fromUtf8(kPusherUrl)));
	/* subscribe after connection_established — handled in onWsText */
	QObject::connect(
		ws_, &MiniWebSocket::opened, this,
		[this, chatroomId]() {
			const QString channel = QStringLiteral("chatrooms.%1.v2").arg(chatroomId);
			const QString sub = QStringLiteral(
						   "{\"event\":\"pusher:subscribe\",\"data\":{\"auth\":\"\",\"channel\":\"%1\"}}")
						   .arg(channel);
			ws_->sendText(sub);
			emit connected();
			emit status(QStringLiteral("Subscribed %1").arg(channel));
		},
		Qt::SingleShotConnection);
}

void KickChatClient::disconnectFromHost()
{
	ws_->close();
}

bool KickChatClient::isConnected() const
{
	return ws_->isOpen();
}

void KickChatClient::onWsText(const QString &text)
{
	QJsonParseError err;
	const auto doc = QJsonDocument::fromJson(text.toUtf8(), &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
		return;
	const auto obj = doc.object();
	const QString event = obj.value(QStringLiteral("event")).toString();

	if (event == QStringLiteral("pusher:ping")) {
		ws_->sendText(QStringLiteral("{\"event\":\"pusher:pong\",\"data\":\"{}\"}"));
		return;
	}

	if (event != QStringLiteral("App\\Events\\ChatMessageEvent"))
		return;

	/* data is a JSON string */
	const QString dataStr = obj.value(QStringLiteral("data")).toString();
	const auto inner = QJsonDocument::fromJson(dataStr.toUtf8()).object();
	const QString user = inner.value(QStringLiteral("sender")).toObject().value(QStringLiteral("username")).toString();
	QString content = inner.value(QStringLiteral("content")).toString();
	if (content.isEmpty())
		content = inner.value(QStringLiteral("message")).toString();
	if (!user.isEmpty())
		emit chatLine(user, content);
}

void KickChatClient::onChannelResolved() {}
