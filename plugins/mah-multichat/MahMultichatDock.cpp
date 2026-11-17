#include "MahMultichatDock.hpp"
#include "KickChatClient.hpp"
#include "TwitchIrcClient.hpp"
#include "YouTubeLiveChatClient.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>

#include <QCheckBox>
#include <QDesktopServices>
#include <QFormLayout>
#include <QGroupBox>
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
	root->addWidget(new QLabel(QStringLiteral("<b style='color:#E5B875'>MAH Multichat</b> "
						  "<span style='color:#94a3b8;font-size:12px'>merged feed · send toggles</span>"),
				   this));

	auto *tabs = new QTabWidget(this);

	/* —— Accounts / connect —— */
	auto *cfg = new QWidget(tabs);
	auto *form = new QFormLayout(cfg);

	twChannel_ = new QLineEdit(cfg);
	twNick_ = new QLineEdit(cfg);
	twOauth_ = new QLineEdit(cfg);
	twOauth_->setEchoMode(QLineEdit::Password);
	twOauth_->setPlaceholderText(QStringLiteral("oauth:… (optional read-only)"));
	twBtn_ = new QPushButton(QStringLiteral("Connect Twitch"), cfg);
	form->addRow(QStringLiteral("Twitch channel"), twChannel_);
	form->addRow(QStringLiteral("Twitch nick"), twNick_);
	form->addRow(QStringLiteral("Twitch OAuth"), twOauth_);
	form->addRow(QString(), twBtn_);

	ytChatId_ = new QLineEdit(cfg);
	ytChatId_->setPlaceholderText(QStringLiteral("liveChatId while live"));
	ytApiKey_ = new QLineEdit(cfg);
	ytApiKey_->setEchoMode(QLineEdit::Password);
	ytApiKey_->setPlaceholderText(QStringLiteral("Google API key"));
	ytOauth_ = new QLineEdit(cfg);
	ytOauth_->setEchoMode(QLineEdit::Password);
	ytOauth_->setPlaceholderText(QStringLiteral("Bearer token (send)"));
	ytBtn_ = new QPushButton(QStringLiteral("Start YouTube poll"), cfg);
	auto *ytHelp = new QPushButton(QStringLiteral("API console"), cfg);
	form->addRow(QStringLiteral("YT liveChatId"), ytChatId_);
	form->addRow(QStringLiteral("YT API key"), ytApiKey_);
	form->addRow(QStringLiteral("YT OAuth"), ytOauth_);
	auto *ytRow = new QHBoxLayout();
	ytRow->addWidget(ytBtn_);
	ytRow->addWidget(ytHelp);
	form->addRow(QString(), ytRow);

	kickSlug_ = new QLineEdit(cfg);
	kickSlug_->setPlaceholderText(QStringLiteral("mayank-raghuvanshi"));
	kickBtn_ = new QPushButton(QStringLiteral("Connect Kick (read)"), cfg);
	form->addRow(QStringLiteral("Kick slug"), kickSlug_);
	form->addRow(QString(), kickBtn_);

	form->addRow(QString(), new QLabel(QStringLiteral("<span style='color:#94a3b8;font-size:11px'>"
							   "Kick send = open Kick app/browser (no stable public API). "
							   "See mah/PLAN.md for Google Cloud setup.</span>"),
					   cfg));
	tabs->addTab(cfg, QStringLiteral("Connect"));

	/* —— Feed —— */
	auto *feed = new QWidget(tabs);
	auto *fv = new QVBoxLayout(feed);
	log_ = new QPlainTextEdit(feed);
	log_->setReadOnly(true);
	fv->addWidget(log_, 1);

	auto *sendBox = new QGroupBox(QStringLiteral("Send to"), feed);
	auto *sb = new QHBoxLayout(sendBox);
	sendTw_ = new QCheckBox(QStringLiteral("Twitch"), sendBox);
	sendYt_ = new QCheckBox(QStringLiteral("YouTube"), sendBox);
	sendKickNote_ = new QCheckBox(QStringLiteral("Kick (open)"), sendBox);
	sendTw_->setChecked(true);
	sendYt_->setChecked(true);
	sb->addWidget(sendTw_);
	sb->addWidget(sendYt_);
	sb->addWidget(sendKickNote_);
	fv->addWidget(sendBox);

	auto *sendRow = new QHBoxLayout();
	compose_ = new QLineEdit(feed);
	compose_->setPlaceholderText(QStringLiteral("Message…"));
	auto *sendBtn = new QPushButton(QStringLiteral("Send"), feed);
	sendRow->addWidget(compose_, 1);
	sendRow->addWidget(sendBtn);
	fv->addLayout(sendRow);
	tabs->addTab(feed, QStringLiteral("Feed"));

	root->addWidget(tabs, 1);

	twitch_ = new TwitchIrcClient(this);
	youtube_ = new YouTubeLiveChatClient(this);
	kick_ = new KickChatClient(this);
	loadSettings();

	connect(twBtn_, &QPushButton::clicked, this, [this]() {
		saveSettings();
		if (twitch_->isConnected()) {
			twitch_->disconnectFromHost();
			twBtn_->setText(QStringLiteral("Connect Twitch"));
			return;
		}
		twitch_->connectTo(twChannel_->text(), twOauth_->text(), twNick_->text());
		twBtn_->setText(QStringLiteral("Disconnect Twitch"));
	});
	connect(ytBtn_, &QPushButton::clicked, this, [this]() {
		saveSettings();
		if (youtube_->isRunning()) {
			youtube_->stop();
			ytBtn_->setText(QStringLiteral("Start YouTube poll"));
			return;
		}
		youtube_->start(ytChatId_->text(), ytApiKey_->text(), ytOauth_->text());
		ytBtn_->setText(QStringLiteral("Stop YouTube"));
	});
	connect(kickBtn_, &QPushButton::clicked, this, [this]() {
		saveSettings();
		if (kick_->isConnected()) {
			kick_->disconnectFromHost();
			kickBtn_->setText(QStringLiteral("Connect Kick (read)"));
			return;
		}
		kick_->connectSlug(kickSlug_->text());
		kickBtn_->setText(QStringLiteral("Disconnect Kick"));
	});
	connect(ytHelp, &QPushButton::clicked, this, []() {
		QDesktopServices::openUrl(QUrl(QStringLiteral("https://console.cloud.google.com/apis/library/youtube.googleapis.com")));
	});

	connect(twitch_, &TwitchIrcClient::chatLine, this, [this](const QString &u, const QString &m) {
		appendLine(QStringLiteral("TW"), QStringLiteral("#bf94ff"), u, m);
	});
	connect(twitch_, &TwitchIrcClient::status, this, [this](const QString &s) {
		appendStatus(QStringLiteral("<i style='color:#94a3b8'>%1</i>").arg(s.toHtmlEscaped()));
	});
	connect(youtube_, &YouTubeLiveChatClient::chatLine, this, [this](const QString &u, const QString &m) {
		appendLine(QStringLiteral("YT"), QStringLiteral("#ff6b6b"), u, m);
	});
	connect(youtube_, &YouTubeLiveChatClient::status, this, [this](const QString &s) {
		appendStatus(QStringLiteral("<i style='color:#94a3b8'>%1</i>").arg(s.toHtmlEscaped()));
	});
	connect(youtube_, &YouTubeLiveChatClient::errorMessage, this, [this](const QString &s) {
		appendStatus(QStringLiteral("<span style='color:#ff8e8e'>%1</span>").arg(s.toHtmlEscaped()));
	});
	connect(kick_, &KickChatClient::chatLine, this, [this](const QString &u, const QString &m) {
		appendLine(QStringLiteral("KK"), QStringLiteral("#53fc18"), u, m);
	});
	connect(kick_, &KickChatClient::status, this, [this](const QString &s) {
		appendStatus(QStringLiteral("<i style='color:#94a3b8'>%1</i>").arg(s.toHtmlEscaped()));
	});
	connect(kick_, &KickChatClient::errorMessage, this, [this](const QString &s) {
		appendStatus(QStringLiteral("<span style='color:#ff8e8e'>%1</span>").arg(s.toHtmlEscaped()));
	});

	auto doSend = [this]() {
		const QString text = compose_->text().trimmed();
		if (text.isEmpty())
			return;
		if (sendTw_->isChecked())
			twitch_->sendMessage(text);
		if (sendYt_->isChecked())
			youtube_->sendMessage(text);
		if (sendKickNote_->isChecked()) {
			appendStatus(QStringLiteral("<i style='color:#94a3b8'>Kick: opening channel (send in Kick UI)</i>"));
			const QString slug = kickSlug_->text().trimmed();
			if (!slug.isEmpty())
				QDesktopServices::openUrl(QUrl(QStringLiteral("https://kick.com/%1").arg(slug)));
		}
		compose_->clear();
	};
	connect(sendBtn, &QPushButton::clicked, this, doSend);
	connect(compose_, &QLineEdit::returnPressed, this, doSend);
}

void MahMultichatDock::loadSettings()
{
	config_t *cfg = obs_frontend_get_app_config();
	if (!cfg)
		return;
	twChannel_->setText(QString::fromUtf8(config_get_string(cfg, "MahMultichat", "TwitchChannel")));
	twNick_->setText(QString::fromUtf8(config_get_string(cfg, "MahMultichat", "TwitchNick")));
	ytChatId_->setText(QString::fromUtf8(config_get_string(cfg, "MahMultichat", "YtLiveChatId")));
	kickSlug_->setText(QString::fromUtf8(config_get_string(cfg, "MahMultichat", "KickSlug")));
}

void MahMultichatDock::saveSettings()
{
	config_t *cfg = obs_frontend_get_app_config();
	if (!cfg)
		return;
	config_set_string(cfg, "MahMultichat", "TwitchChannel", twChannel_->text().toUtf8().constData());
	config_set_string(cfg, "MahMultichat", "TwitchNick", twNick_->text().toUtf8().constData());
	config_set_string(cfg, "MahMultichat", "YtLiveChatId", ytChatId_->text().toUtf8().constData());
	config_set_string(cfg, "MahMultichat", "KickSlug", kickSlug_->text().toUtf8().constData());
	config_save_safe(cfg, "tmp", nullptr);
}

void MahMultichatDock::appendLine(const QString &platform, const QString &color, const QString &user, const QString &msg)
{
	log_->appendHtml(QStringLiteral("<b style='color:%1'>[%2] %3</b>: %4")
				 .arg(color, platform, user.toHtmlEscaped(), msg.toHtmlEscaped()));
}

void MahMultichatDock::appendStatus(const QString &html)
{
	log_->appendHtml(html);
}
