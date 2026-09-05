#include "../providers/ProviderValidation.h"

#include <QtTest>

class ProviderValidationTest : public QObject {
    Q_OBJECT

private slots:
    void distinguishesRunnableUtilityConfigurableAndroidAndFailedArchives() {
        QCOMPARE(CloudStream::ProviderValidation::classify(0, 2, {}),
                 CloudStream::ProviderValidationResult::Runnable);
        QCOMPARE(CloudStream::ProviderValidation::classify(
                     1, 0, "No BasePlugin or direct MainAPI implementation found in MegaProvider.jar"),
                 CloudStream::ProviderValidationResult::UtilityOnly);
        QCOMPARE(CloudStream::ProviderValidation::classify(
                     1, 0, "Plugin registered no providers through load() | Provider: java.lang.NoSuchMethodException: Provider.<init>()"),
                 CloudStream::ProviderValidationResult::RequiresConfiguration);
        QCOMPARE(CloudStream::ProviderValidation::classify(
                     1, 0, "java.lang.ClassNotFoundException: android.graphics.drawable.Drawable"),
                 CloudStream::ProviderValidationResult::AndroidOnly);
        QCOMPARE(CloudStream::ProviderValidation::classify(
                     1, 0, "java.lang.ClassNotFoundException: com.lagradost.cloudstream3.ui.player.CSPlayerEvent"),
                 CloudStream::ProviderValidationResult::UtilityOnly);
        QCOMPARE(CloudStream::ProviderValidation::classify(1, 0, "android/webkit/WebView"),
                 CloudStream::ProviderValidationResult::AndroidOnly);
        QCOMPARE(CloudStream::ProviderValidation::classify(1, 0, "java.lang.VerifyError"),
                 CloudStream::ProviderValidationResult::Failed);
    }
};

QTEST_MAIN(ProviderValidationTest)
#include "test_provider_validation.moc"
