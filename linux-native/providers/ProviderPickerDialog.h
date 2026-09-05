#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QList>
#include <QStringList>

class QButtonGroup;
class QEvent;
class QKeyEvent;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMouseEvent;
class QPushButton;
class QResizeEvent;

namespace CloudStream {

class ProviderPickerDialog final : public QWidget {
    Q_OBJECT
public:
    enum Result { Rejected = 0, Accepted = 1 };

    explicit ProviderPickerDialog(const QList<QJsonObject> &providers,
                                  const QString &selectedKey,
                                  const QStringList &pinnedProviderKeys,
                                  QWidget *parent = nullptr);

    QString selectedKey() const;
    QStringList pinnedProviderKeys() const;
    int exec();
    int result() const;

public slots:
    void accept();
    void reject();

signals:
    void accepted();
    void rejected();
    void finished(int result);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QList<QJsonObject> providers_;
    QString selectedKey_;
    QStringList pinnedProviderKeys_;
    QString mediaFilter_ = "All";
    QLineEdit *search_{};
    QListWidget *list_{};
    QListWidgetItem *providerHeader_{};
    QButtonGroup *typeButtons_{};
    QWidget *panel_{};
    int result_ = Rejected;

    void updatePanelGeometry();

    void rebuildItems();
    void applyFilter();
    void addHeader(const QString &text);
    void addChoice(const QString &mode, const QString &title, const QString &summary,
                   const QString &badge);
    void addProvider(const QJsonObject &provider);
    void activateItem(QListWidgetItem *item);
    void updateCurrentRows();
    bool matchesMediaFilter(const QJsonObject &provider) const;
    static QString providerKey(const QJsonObject &provider);
    static QString languageBadge(const QString &language);
    static QString providerSummary(const QJsonObject &provider);
};

} // namespace CloudStream
