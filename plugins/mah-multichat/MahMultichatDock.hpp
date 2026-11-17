#pragma once

#include <QWidget>

class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class TwitchIrcClient;
class KickChatClient;
class YouTubeLiveChatClient;

class MahMultichatDock : public QWidget {
	Q_OBJECT
public:
	explicit MahMultichatDock(QWidget *parent = nullptr);

private:
	void loadSettings();
	void saveSettings();
	void appendLine(const QString &platform, const QString &color, const QString &user, const QString &msg);
	void appendStatus(const QString &html);

	/* Twitch */
	QLineEdit *twChannel_ = nullptr;
	QLineEdit *twNick_ = nullptr;
	QLineEdit *twOauth_ = nullptr;
	QPushButton *twBtn_ = nullptr;
	TwitchIrcClient *twitch_ = nullptr;

	/* YouTube */
	QLineEdit *ytChatId_ = nullptr;
	QLineEdit *ytApiKey_ = nullptr;
	QLineEdit *ytOauth_ = nullptr;
	QPushButton *ytBtn_ = nullptr;
	YouTubeLiveChatClient *youtube_ = nullptr;

	/* Kick */
	QLineEdit *kickSlug_ = nullptr;
	QPushButton *kickBtn_ = nullptr;
	KickChatClient *kick_ = nullptr;

	/* Shared */
	QPlainTextEdit *log_ = nullptr;
	QLineEdit *compose_ = nullptr;
	QCheckBox *sendTw_ = nullptr;
	QCheckBox *sendYt_ = nullptr;
	QCheckBox *sendKickNote_ = nullptr;
};
