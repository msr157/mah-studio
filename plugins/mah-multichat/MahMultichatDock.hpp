#pragma once

#include <QWidget>

class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class TwitchIrcClient;

class MahMultichatDock : public QWidget {
	Q_OBJECT
public:
	explicit MahMultichatDock(QWidget *parent = nullptr);

private:
	void loadSettings();
	void saveSettings();
	void appendLine(const QString &html);

	QLineEdit *channel_ = nullptr;
	QLineEdit *nick_ = nullptr;
	QLineEdit *oauth_ = nullptr;
	QPlainTextEdit *log_ = nullptr;
	QLineEdit *compose_ = nullptr;
	QPushButton *connectBtn_ = nullptr;
	TwitchIrcClient *irc_ = nullptr;
};
