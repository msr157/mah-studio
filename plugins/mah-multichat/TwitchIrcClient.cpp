#include "TwitchIrcClient.hpp"

#include <QRegularExpression>

TwitchIrcClient::TwitchIrcClient(QObject *parent) : QObject(parent)
{
	connect(&socket_, &QSslSocket::readyRead, this, &TwitchIrcClient::onReadyRead);
	connect(&socket_, &QSslSocket::sslErrors, this, &TwitchIrcClient::onSslErrors);
	connect(&socket_, &QSslSocket::stateChanged, this, &TwitchIrcClient::onStateChanged);
}

void TwitchIrcClient::connectTo(const QString &channel, const QString &oauthToken, const QString &nick)
{
	channel_ = channel.trimmed().toLower();
	if (channel_.startsWith('#'))
		channel_ = channel_.mid(1);
	nick_ = nick.trimmed().isEmpty() ? QStringLiteral("justinfan12345") : nick.trimmed();

	emit status(QStringLiteral("Connecting to Twitch IRC…"));
	socket_.connectToHostEncrypted(QStringLiteral("irc.chat.twitch.tv"), 6697);

	// Auth after encrypted connect — handled in stateChanged when Connected
	disconnect(&socket_, &QSslSocket::encrypted, nullptr, nullptr);
	connect(&socket_, &QSslSocket::encrypted, this, [this, oauthToken]() {
		const QString token = oauthToken.trimmed();
		if (!token.isEmpty()) {
			QString pass = token;
			if (!pass.startsWith(QStringLiteral("oauth:"), Qt::CaseInsensitive))
				pass = QStringLiteral("oauth:") + pass;
			sendRaw(QStringLiteral("PASS %1").arg(pass));
			sendRaw(QStringLiteral("NICK %1").arg(nick_));
		} else {
			/* anonymous read-only */
			sendRaw(QStringLiteral("NICK %1").arg(nick_));
		}
		sendRaw(QStringLiteral("CAP REQ :twitch.tv/tags twitch.tv/commands"));
		sendRaw(QStringLiteral("JOIN #%1").arg(channel_));
		emit status(QStringLiteral("Joined #%1").arg(channel_));
		emit connected();
	});
}

void TwitchIrcClient::disconnectFromHost()
{
	socket_.disconnectFromHost();
}

void TwitchIrcClient::sendMessage(const QString &text)
{
	if (!isConnected() || text.trimmed().isEmpty())
		return;
	sendRaw(QStringLiteral("PRIVMSG #%1 :%2").arg(channel_, text));
}

bool TwitchIrcClient::isConnected() const
{
	return socket_.state() == QAbstractSocket::ConnectedState;
}

void TwitchIrcClient::sendRaw(const QString &line)
{
	const QByteArray bytes = (line + QStringLiteral("\r\n")).toUtf8();
	socket_.write(bytes);
}

void TwitchIrcClient::onReadyRead()
{
	buffer_.append(socket_.readAll());
	while (true) {
		const int idx = buffer_.indexOf("\r\n");
		if (idx < 0)
			break;
		QString line = QString::fromUtf8(buffer_.left(idx));
		buffer_.remove(0, idx + 2);

		if (line.startsWith(QStringLiteral("PING "))) {
			sendRaw(QStringLiteral("PONG %1").arg(line.mid(5)));
			continue;
		}

		/* :user!user@user.tmi.twitch.tv PRIVMSG #chan :msg */
		static const QRegularExpression re(
			QStringLiteral(R"((?:^@[^ ]+ )?:([^!]+)![^ ]+ PRIVMSG #[^ ]+ :(.*)$)"));
		const auto m = re.match(line);
		if (m.hasMatch())
			emit chatLine(m.captured(1), m.captured(2));
	}
}

void TwitchIrcClient::onSslErrors(const QList<QSslError> &errors)
{
	Q_UNUSED(errors);
	socket_.ignoreSslErrors();
}

void TwitchIrcClient::onStateChanged(QAbstractSocket::SocketState state)
{
	if (state == QAbstractSocket::UnconnectedState) {
		emit disconnected();
		emit status(QStringLiteral("Twitch IRC disconnected"));
	}
}
