#include "MahMultichatDock.hpp"
#include "TwitchIrcClient.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>

MahMultichatDock::MahMultichatDock(QWidget *parent) : QWidget(parent)
{
	setObjectName(QStringLiteral("mah-multichat-dock"));
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);

	auto *brand = new QLabel(QStringLiteral("<b style='color:#E5B875'>MAH Multichat</b>"), this);
	root->addWidget(brand);

	auto *tabs = new QTabWidget(this);
	root->addWidget(tabs, 1);

	/* —— Twitch (native IRC) —— */
	auto *twitch = new QWidget(tabs);
	auto *tv = new QVBoxLayout(twitch);
	channel_ = new QLineEdit(twitch);
	channel_->setPlaceholderText(QStringLiteral("Twitch channel (e.g. mayank_raghuvanshi)"));
	nick_ = new QLineEdit(twitch);
	nick_->setPlaceholderText(QStringLiteral("Bot / account nick"));
	oauth_ = new QLineEdit(twitch);
	oauth_->setPlaceholderText(QStringLiteral("oauth:xxxx (optional for read-only)"));
	oauth_->setEchoMode(QLineEdit::Password);
	tv->addWidget(channel_);
	tv->addWidget(nick_);
	tv->addWidget(oauth_);

	auto *row = new QHBoxLayout();
	connectBtn_ = new QPushButton(QStringLiteral("Connect"), twitch);
	auto *tokenHelp = new QPushButton(QStringLiteral("Get token"), twitch);
	row->addWidget(connectBtn_);
	row->addWidget(tokenHelp);
	tv->addLayout(row);

	log_ = new QPlainTextEdit(twitch);
	log_->setReadOnly(true);
	tv->addWidget(log_, 1);

	auto *sendRow = new QHBoxLayout();
	compose_ = new QLineEdit(twitch);
	compose_->setPlaceholderText(QStringLiteral("Send to Twitch…"));
	auto *sendBtn = new QPushButton(QStringLiteral("Send"), twitch);
	sendRow->addWidget(compose_, 1);
	sendRow->addWidget(sendBtn);
	tv->addLayout(sendRow);
	tabs->addTab(twitch, QStringLiteral("Twitch"));

	/* —— YouTube / Kick (official popouts) —— */
	auto *yt = new QWidget(tabs);
	auto *yv = new QVBoxLayout(yt);
	yv->addWidget(new QLabel(
		QStringLiteral("YouTube Live Chat: use Studio popout as a Custom Browser Dock "
			       "(Interact to sign in). Native YT API lands next."),
		yt));
	auto *ytOpen = new QPushButton(QStringLiteral("Open YouTube Studio"), yt);
	yv->addWidget(ytOpen);
	yv->addStretch(1);
	tabs->addTab(yt, QStringLiteral("YouTube"));

	auto *kick = new QWidget(tabs);
	auto *kv = new QVBoxLayout(kick);
	kv->addWidget(new QLabel(
		QStringLiteral("Kick chat popout is read-only inside OBS. Add the official chatroom "
			       "URL as a Custom Browser Dock. Native Kick WS next."),
		kick));
	auto *kickOpen = new QPushButton(QStringLiteral("Open Kick"), kick);
	kv->addWidget(kickOpen);
	kv->addStretch(1);
	tabs->addTab(kick, QStringLiteral("Kick"));

	irc_ = new TwitchIrcClient(this);
	loadSettings();

	connect(connectBtn_, &QPushButton::clicked, this, [this]() {
		saveSettings();
		if (irc_->isConnected()) {
			irc_->disconnectFromHost();
			connectBtn_->setText(QStringLiteral("Connect"));
			return;
		}
		irc_->connectTo(channel_->text(), oauth_->text(), nick_->text());
		connectBtn_->setText(QStringLiteral("Disconnect"));
	});
	connect(sendBtn, &QPushButton::clicked, this, [this]() {
		irc_->sendMessage(compose_->text());
		compose_->clear();
	});
	connect(compose_, &QLineEdit::returnPressed, sendBtn, &QPushButton::click);
	connect(tokenHelp, &QPushButton::clicked, this, []() {
		QDesktopServices::openUrl(QUrl(QStringLiteral("https://twitchtokengenerator.com/")));
	});
	connect(ytOpen, &QPushButton::clicked, this, []() {
		QDesktopServices::openUrl(QUrl(QStringLiteral("https://studio.youtube.com/")));
	});
	connect(kickOpen, &QPushButton::clicked, this, []() {
		QDesktopServices::openUrl(QUrl(QStringLiteral("https://kick.com/")));
	});

	connect(irc_, &TwitchIrcClient::chatLine, this, [this](const QString &user, const QString &msg) {
		appendLine(QStringLiteral("<b style='color:#bf94ff'>%1</b>: %2")
				   .arg(user.toHtmlEscaped(), msg.toHtmlEscaped()));
	});
	connect(irc_, &TwitchIrcClient::status, this, [this](const QString &s) {
		appendLine(QStringLiteral("<i style='color:#94a3b8'>%1</i>").arg(s.toHtmlEscaped()));
	});
	connect(irc_, &TwitchIrcClient::errorMessage, this, [this](const QString &s) {
		appendLine(QStringLiteral("<span style='color:#ff8e8e'>%1</span>").arg(s.toHtmlEscaped()));
	});
}

void MahMultichatDock::loadSettings()
{
	config_t *cfg = obs_frontend_get_app_config();
	if (!cfg)
		return;
	channel_->setText(QString::fromUtf8(config_get_string(cfg, "MahMultichat", "TwitchChannel")));
	nick_->setText(QString::fromUtf8(config_get_string(cfg, "MahMultichat", "TwitchNick")));
	/* oauth intentionally not persisted in plain config by default — user pastes each session
	 * or we store only if they opt in later. */
}

void MahMultichatDock::saveSettings()
{
	config_t *cfg = obs_frontend_get_app_config();
	if (!cfg)
		return;
	config_set_string(cfg, "MahMultichat", "TwitchChannel", channel_->text().toUtf8().constData());
	config_set_string(cfg, "MahMultichat", "TwitchNick", nick_->text().toUtf8().constData());
	config_save_safe(cfg, "tmp", nullptr);
}

void MahMultichatDock::appendLine(const QString &html)
{
	log_->appendHtml(html);
}
