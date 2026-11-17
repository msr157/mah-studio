#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class QNetworkAccessManager;

/**
 * YouTube Live Chat via Data API v3.
 * Read: API key + liveChatId (or OAuth).
 * Send: OAuth bearer token required.
 */
class YouTubeLiveChatClient : public QObject {
	Q_OBJECT
public:
	explicit YouTubeLiveChatClient(QObject *parent = nullptr);

	void start(const QString &liveChatId, const QString &apiKey, const QString &oauthBearer = {});
	void stop();
	void sendMessage(const QString &text);
	bool isRunning() const { return running_; }

signals:
	void chatLine(const QString &user, const QString &message);
	void status(const QString &text);
	void errorMessage(const QString &text);

private slots:
	void poll();

private:
	QNetworkAccessManager *nam_ = nullptr;
	QTimer timer_;
	QString liveChatId_;
	QString apiKey_;
	QString oauth_;
	QString pageToken_;
	bool running_ = false;
	bool first_ = true;
};
