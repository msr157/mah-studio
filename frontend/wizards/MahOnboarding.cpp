#include "MahOnboarding.hpp"

#include <QLabel>
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

} // namespace

MahOnboarding::MahOnboarding(QWidget *parent) : QWizard(parent)
{
	setWindowTitle(QStringLiteral("MAH Studio — After Hours setup"));
	setMinimumSize(560, 420);
	setWizardStyle(QWizard::ModernStyle);

	addPage(makePage(
		QStringLiteral("Welcome"),
		QStringLiteral(
			"<p style='font-size:15px'><b>Mayank After Hours</b> · MAH Studio</p>"
			"<p>This short setup covers the pieces stock OBS leaves to plugins:</p>"
			"<ul>"
			"<li>Phone as camera (Wi‑Fi or USB tether)</li>"
			"<li>YouTube + Twitch + Kick together</li>"
			"<li>Chat docks + locked operator layout</li>"
			"</ul>"
			"<p>Hinglish / English — same steps. Next →</p>")));

	addPage(makePage(
		QStringLiteral("Mobile camera"),
		QStringLiteral(
			"<p>Add source <b>MAH Mobile Camera</b>.</p>"
			"<ol>"
			"<li>Note the <b>LAN IP</b>, port <b>28887</b>, and <b>pairing code</b>.</li>"
			"<li>Install <b>MAH Cam</b> on Android / iOS (<code>mobile/</code> in this repo).</li>"
			"<li>Same Wi‑Fi <i>or</i> USB tethering → Find on LAN → Start stream.</li>"
			"</ol>"
			"<p>Protocol: <code>mah/PROTOCOL.md</code>. Test without a phone: "
			"<code>mobile/tools/mah_mjpeg_test_client.py</code></p>")));

	addPage(makePage(
		QStringLiteral("Multistream + chat"),
		QStringLiteral(
			"<p>OBS <b>Settings → Stream</b> is always one destination. That is normal.</p>"
			"<p>Use the free <b>Multiple Output</b> dock from "
			"<a href='https://github.com/msr157/mah-obs'>mah-obs</a> "
			"(obs-multi-rtmp) — three Custom RTMP targets for YouTube, Twitch, Kick.</p>"
			"<p>Chat: Custom Browser Docks with official popouts. Help dock HTML lives in "
			"<code>mah/docks/</code>. Then <b>Docks → Lock Docks</b>.</p>")));

	auto *done = makePage(
		QStringLiteral("You're set"),
		QStringLiteral(
			"<p>Optional starter dock page:</p>"
			"<p><code>mah/starter/index.html</code> → View → Docks → Custom Browser Docks.</p>"
			"<p>Finish runs the normal OBS Auto-Configuration next (optional but useful).</p>"));
	addPage(done);

	setOption(QWizard::NoBackButtonOnStartPage, true);
}
