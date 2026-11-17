#pragma once

#include <QWizard>

/**
 * MAH Studio first-run onboarding (Welcome → Mobile cam → Multistream → Done).
 * Runs before or alongside AutoConfig when General/MahOnboardingDone is unset.
 */
class MahOnboarding : public QWizard {
	Q_OBJECT

public:
	explicit MahOnboarding(QWidget *parent = nullptr);
};
