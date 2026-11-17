#include "MahOnboarding.hpp"

#include <OBSApp.hpp>
#include <util/config-file.h>

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPushButton>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWizardPage>

#include "moc_MahOnboarding.cpp"

namespace {

QWizardPage *makePage(const QString &title, const QString &body)
{
	auto *page = new QWizardPage();
	page->setTitle(title);
	auto *layout = new QVBoxLayout(page);
	auto *label = new QLabel(body, page);
	label->setWordWrap(true);
	label->setTextFormat(Qt::RichText);
	label->setOpenExternalLinks(true);
	layout->addWidget(label);
	layout->addStretch(1);
	return page;
}

QString guessLanIp()
{
	/* Best-effort: leave empty — user fills from MAH Mobile Camera source. */
	return {};
}

bool applyLovedLayoutPreset()
{
	const QString appDir = QCoreApplication::applicationDirPath();
	const QStringList paths = {
		appDir + QStringLiteral("/mah/presets/layout-loved.json"),
		appDir + QStringLiteral("/../mah/presets/layout-loved.json"),
		appDir + QStringLiteral("/../../mah/presets/layout-loved.json"),
		QStringLiteral("mah/presets/layout-loved.json"),
		QStringLiteral("D:/Codes/mah-studio/mah/presets/layout-loved.json"),
	};
	QByteArray raw;
	for (const QString &p : paths) {
		QFile f(p);
		if (f.open(QIODevice::ReadOnly)) {
			raw = f.readAll();
			break;
		}
	}
	if (raw.isEmpty())
		return false;

	const auto doc = QJsonDocument::fromJson(raw);
	if (!doc.isObject())
		return false;
	const auto obj = doc.object();

	config_t *user = App()->GetUserConfig();
	if (!user)
		return false;

	if (obj.contains(QStringLiteral("ExtraBrowserDocks")))
		config_set_string(user, "BasicWindow", "ExtraBrowserDocks",
				  obj.value(QStringLiteral("ExtraBrowserDocks")).toString().toUtf8().constData());
	if (obj.contains(QStringLiteral("DockState")))
		config_set_string(user, "BasicWindow", "DockState",
				  obj.value(QStringLiteral("DockState")).toString().toUtf8().constData());
	if (obj.contains(QStringLiteral("geometry")))
		config_set_string(user, "BasicWindow", "geometry",
				  obj.value(QStringLiteral("geometry")).toString().toUtf8().constData());
	config_set_bool(user, "BasicWindow", "SideDocks", true);
	config_set_bool(user, "BasicWindow", "DocksLocked", true);
	config_save_safe(user, "tmp", nullptr);
	return true;
}

} // namespace

MahOnboarding::MahOnboarding(QWidget *parent) : QWizard(parent)
{
	setWindowTitle(QStringLiteral("MAH Studio — After Hours setup"));
	setMinimumSize(600, 480);
	setWizardStyle(QWizard::ModernStyle);

	addPage(makePage(
		QStringLiteral("Welcome"),
		QStringLiteral(
			"<p style='font-size:15px'><b>Mayank After Hours</b> · MAH Studio</p>"
			"<p>Phone cam, multistream, multichat — Hinglish-friendly setup.</p>")));

	/* Mobile — web-first: phone opens link → Allow → live */
	auto *cam = new QWizardPage();
	cam->setTitle(QStringLiteral("Phone camera (no app)"));
	auto *camLay = new QVBoxLayout(cam);
	camLay->addWidget(new QLabel(
		QStringLiteral(
			"<p><b>Optimistic flow:</b> Add source <b>MAH Mobile Camera</b> → "
			"copy the <b>Phone URL</b> from properties (or generate QR below) → "
			"open on phone (same Wi‑Fi) → <b>Allow camera</b> → you’re live.</p>"
			"<p style='color:#94a3b8;font-size:13px'>No App Store / Play install. "
			"Native MAH Cam remains optional for USB / H.264.</p>"),
		cam));
	auto *ip = new QLineEdit(cam);
	ip->setPlaceholderText(QStringLiteral("LAN IP (from source properties)"));
	auto *port = new QLineEdit(cam);
	port->setText(QStringLiteral("28889"));
	port->setPlaceholderText(QStringLiteral("Web port (default 28889)"));
	auto *pair = new QLineEdit(cam);
	pair->setText(QStringLiteral("MAH1"));
	pair->setPlaceholderText(QStringLiteral("Pairing code"));
	camLay->addWidget(ip);
	camLay->addWidget(port);
	camLay->addWidget(pair);
	auto *qrLabel = new QLabel(cam);
	qrLabel->setMinimumHeight(180);
	qrLabel->setAlignment(Qt::AlignCenter);
	qrLabel->setText(QStringLiteral("<i style='color:#94a3b8'>QR appears after Generate</i>"));
	camLay->addWidget(qrLabel);
	auto *uriLabel = new QLabel(cam);
	uriLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	uriLabel->setWordWrap(true);
	camLay->addWidget(uriLabel);
	auto *gen = new QPushButton(QStringLiteral("Generate QR for phone"), cam);
	camLay->addWidget(gen);
	auto *nam = new QNetworkAccessManager(cam);
	QObject::connect(gen, &QPushButton::clicked, cam, [=]() {
		const QString uri = QStringLiteral("http://%1:%2/?p=%3")
					    .arg(ip->text().trimmed(), port->text().trimmed(), pair->text().trimmed());
		uriLabel->setText(QStringLiteral("<code>%1</code><br/>"
						 "<span style='color:#94a3b8'>Scan → Allow camera → live in OBS</span>")
					  .arg(uri.toHtmlEscaped()));
		QUrl url(QStringLiteral("https://api.qrserver.com/v1/create-qr-code/"));
		QUrlQuery q;
		q.addQueryItem(QStringLiteral("size"), QStringLiteral("180x180"));
		q.addQueryItem(QStringLiteral("data"), uri);
		url.setQuery(q);
		qrLabel->setText(QStringLiteral("<img src=\"%1\" width=\"180\" height=\"180\"/>").arg(url.toString()));
		auto *reply = nam->get(QNetworkRequest(url));
		QObject::connect(reply, &QNetworkReply::finished, cam, [reply, qrLabel]() {
			reply->deleteLater();
			if (reply->error() != QNetworkReply::NoError)
				return;
			QPixmap px;
			px.loadFromData(reply->readAll());
			if (!px.isNull())
				qrLabel->setPixmap(px);
		});
	});
	camLay->addStretch(1);
	addPage(cam);

	addPage(makePage(
		QStringLiteral("Multistream + chat"),
		QStringLiteral(
			"<p>Open <b>MAH Multistream</b> — add YT / Twitch / Kick Custom RTMP.</p>"
			"<p>Open <b>MAH Multichat</b> — Twitch + YouTube + Kick read; send toggles.</p>"
			"<p>Then <b>Docks → Lock Docks</b>.</p>")));

	/* Layout apply */
	auto *layoutPage = new QWizardPage();
	layoutPage->setTitle(QStringLiteral("Loved layout"));
	auto *ll = new QVBoxLayout(layoutPage);
	ll->addWidget(new QLabel(
		QStringLiteral(
			"<p>Apply the After Hours vertical chat / operator dock preset "
			"(from <code>mah/presets/layout-loved.json</code>). "
			"<b>Restart OBS</b> after applying so DockState loads cleanly.</p>"),
		layoutPage));
	auto *applyBtn = new QPushButton(QStringLiteral("Apply loved layout + lock docks"), layoutPage);
	auto *status = new QLabel(layoutPage);
	ll->addWidget(applyBtn);
	ll->addWidget(status);
	ll->addStretch(1);
	QObject::connect(applyBtn, &QPushButton::clicked, layoutPage, [status]() {
		if (applyLovedLayoutPreset())
			status->setText(QStringLiteral("<span style='color:#8dffa8'>Applied. Restart OBS to see docks settle.</span>"));
		else
			status->setText(QStringLiteral(
				"<span style='color:#ff8e8e'>Preset not found — copy mah/presets next to the build or run mah-obs installer.</span>"));
	});
	addPage(layoutPage);

	addPage(makePage(
		QStringLiteral("You're set"),
		QStringLiteral(
			"<p>Starter dock: <code>mah/starter/index.html</code></p>"
			"<p>Finish → optional OBS Auto-Configuration.</p>")));

	setOption(QWizard::NoBackButtonOnStartPage, true);
	Q_UNUSED(guessLanIp);
}
