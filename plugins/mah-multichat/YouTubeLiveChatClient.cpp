#include "YouTubeLiveChatClient.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

YouTubeLiveChatClient::YouTubeLiveChatClient(QObject *parent) : QObject(parent)
{
	nam_ = new QNetworkAccessManager(this);
	connect(&timer_, &QTimer::timeout, this, &YouTubeLiveChatClient::poll);
}

void YouTubeLiveChatClient::start(const QString &liveChatId, const QString &apiKey, const QString &oauthBearer)
{
	stop();
	liveChatId_ = liveChatId.trimmed();
	apiKey_ = apiKey.trimmed();
	oauth_ = oauthBearer.trimmed();
	pageToken_.clear();
	first_ = true;
	if (liveChatId_.isEmpty() || (apiKey_.isEmpty() && oauth_.isEmpty())) {
		emit errorMessage(QStringLiteral("YouTube needs liveChatId + API key (or OAuth)"));
		return;
	}
	running_ = true;
	emit status(QStringLiteral("YouTube chat polling…"));
	poll();
}

void YouTubeLiveChatClient::stop()
{
	running_ = false;
	timer_.stop();
}

void YouTubeLiveChatClient::sendMessage(const QString &text)
{
	if (oauth_.isEmpty()) {
		emit errorMessage(QStringLiteral("YouTube send needs OAuth bearer token"));
		return;
	}
	if (text.trimmed().isEmpty())
		return;

	QUrl url(QStringLiteral("https://www.googleapis.com/youtube/v3/liveChat/messages"));
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("part"), QStringLiteral("snippet"));
	url.setQuery(q);

	QNetworkRequest req(url);
	req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	req.setRawHeader("Authorization", QByteArray("Bearer ") + oauth_.toUtf8());

	QJsonObject snippet;
	snippet.insert(QStringLiteral("liveChatId"), liveChatId_);
	snippet.insert(QStringLiteral("type"), QStringLiteral("textMessageEvent"));
	QJsonObject details;
	details.insert(QStringLiteral("messageText"), text);
	snippet.insert(QStringLiteral("textMessageDetails"), details);
	QJsonObject body;
	body.insert(QStringLiteral("snippet"), snippet);

	auto *reply = nam_->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError)
			emit errorMessage(QStringLiteral("YT send failed: %1").arg(reply->errorString()));
		else
			emit status(QStringLiteral("YouTube message sent"));
	});
}

void YouTubeLiveChatClient::poll()
{
	if (!running_)
		return;

	QUrl url(QStringLiteral("https://www.googleapis.com/youtube/v3/liveChat/messages"));
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("liveChatId"), liveChatId_);
	q.addQueryItem(QStringLiteral("part"), QStringLiteral("snippet,authorDetails"));
	if (!apiKey_.isEmpty())
		q.addQueryItem(QStringLiteral("key"), apiKey_);
	if (!pageToken_.isEmpty())
		q.addQueryItem(QStringLiteral("pageToken"), pageToken_);
	url.setQuery(q);

	QNetworkRequest req(url);
	if (!oauth_.isEmpty())
		req.setRawHeader("Authorization", QByteArray("Bearer ") + oauth_.toUtf8());

	auto *reply = nam_->get(req);
	connect(reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		if (!running_)
			return;
		if (reply->error() != QNetworkReply::NoError) {
			emit errorMessage(QStringLiteral("YT poll: %1").arg(reply->errorString()));
			timer_.start(5000);
			return;
		}
		const auto doc = QJsonDocument::fromJson(reply->readAll());
		const auto obj = doc.object();
		pageToken_ = obj.value(QStringLiteral("nextPageToken")).toString();
		const int interval = obj.value(QStringLiteral("pollingIntervalMillis")).toInt(5000);

		const auto items = obj.value(QStringLiteral("items")).toArray();
		/* Skip dumping full history flood on first page — still show latest batch */
		for (const auto &v : items) {
			const auto item = v.toObject();
			const auto author = item.value(QStringLiteral("authorDetails")).toObject();
			const auto snippet = item.value(QStringLiteral("snippet")).toObject();
			if (snippet.value(QStringLiteral("type")).toString() != QStringLiteral("textMessageEvent"))
				continue;
			const QString user = author.value(QStringLiteral("displayName")).toString();
			const QString msg =
				snippet.value(QStringLiteral("textMessageDetails")).toObject().value(QStringLiteral("messageText")).toString();
			if (!user.isEmpty())
				emit chatLine(user, msg);
		}
		first_ = false;
		timer_.start(qMax(1500, interval));
	});
}
