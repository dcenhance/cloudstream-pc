#include "ProviderPickerDialog.h"

#include "ProviderSelectionModel.h"
#include "../ui/SmoothScrollController.h"

#include <QButtonGroup>
#include <QEventLoop>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSet>
#include <QShortcut>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace CloudStream {

namespace {
constexpr int KeyRole = Qt::UserRole + 1;
constexpr int ProviderRole = Qt::UserRole + 2;
constexpr int SearchRole = Qt::UserRole + 3;
constexpr int SelectedRole = Qt::UserRole + 4;
constexpr int MediaRole = Qt::UserRole + 5;
constexpr int HeaderRole = Qt::UserRole + 6;

class ClickableProviderRow final : public QWidget {
public:
    using QWidget::QWidget;
    std::function<void()> activated;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
            if (activated) activated();
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    }
};

QStringList normalizedTypes(const QJsonObject &provider) {
    QStringList types;
    for (const auto &value : provider.value("supportedTypes").toArray()) {
        const auto type = value.toString().trimmed();
        if (!type.isEmpty()) types << type;
    }
    return types;
}

QString mediaCategory(const QString &type) {
    if (type.contains("nsfw", Qt::CaseInsensitive)) return "NSFW";
    if (type.contains("live", Qt::CaseInsensitive)) return "Live";
    if (type.contains("anime", Qt::CaseInsensitive) ||
        type.compare("OVA", Qt::CaseInsensitive) == 0) return "Anime";
    if (type.contains("series", Qt::CaseInsensitive) ||
        type.contains("drama", Qt::CaseInsensitive) ||
        type.contains("cartoon", Qt::CaseInsensitive)) return "TV";
    if (type.contains("movie", Qt::CaseInsensitive)) return "Movies";
    return {};
}

void repolish(QWidget *widget) {
    if (!widget) return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}
} // namespace

ProviderPickerDialog::ProviderPickerDialog(const QList<QJsonObject> &providers,
                                           const QString &selectedKey,
                                           const QStringList &pinnedProviderKeys,
                                           QWidget *parent)
    : QWidget(parent), providers_(providers), selectedKey_(selectedKey),
      pinnedProviderKeys_(pinnedProviderKeys) {
    setObjectName("providerPickerOverlay");
    setProperty("controllerNavigationScope", true);
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    if (parent) {
        setGeometry(parent->rect());
        parent->installEventFilter(this);
    } else {
        resize(680, 820);
    }
    panel_ = new QWidget(this);
    panel_->setObjectName("providerPickerPanel");
    panel_->setAttribute(Qt::WA_StyledBackground, true);
    updatePanelGeometry();

    auto *layout = new QVBoxLayout(panel_);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(12);

    auto *header = new QHBoxLayout;
    auto *headingText = new QVBoxLayout;
    headingText->setSpacing(2);
    auto *heading = new QLabel("Choose provider");
    heading->setObjectName("providerPickerTitle");
    auto *subheading = new QLabel("Select what fills Home. Pin favorites with the star.");
    subheading->setObjectName("providerPickerSubtitle");
    headingText->addWidget(heading);
    headingText->addWidget(subheading);
    header->addLayout(headingText, 1);
    auto *close = new QToolButton;
    close->setObjectName("providerPickerClose");
    close->setText("×");
    close->setAccessibleName("Close provider picker");
    close->setFixedSize(36, 36);
    connect(close, &QToolButton::clicked, this, &ProviderPickerDialog::reject);
    header->addWidget(close, 0, Qt::AlignTop);
    layout->addLayout(header);

    search_ = new QLineEdit;
    search_->setObjectName("providerPickerSearch");
    search_->setPlaceholderText("Search providers, languages, or websites…");
    search_->setClearButtonEnabled(true);
    search_->setMinimumHeight(46);
    search_->addAction(QIcon(":/icons/search.svg"), QLineEdit::LeadingPosition);
    layout->addWidget(search_);

    list_ = new QListWidget;
    list_->setObjectName("providerPickerList");
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setUniformItemSizes(false);
    list_->setFocusPolicy(Qt::StrongFocus);
    CloudStream::SmoothScrollController::attach(list_);
    layout->addWidget(list_, 1);

    auto *filterTitle = new QLabel("BROWSE PROVIDER TYPES");
    filterTitle->setObjectName("providerPickerSectionLabel");
    layout->addWidget(filterTitle);
    auto *filters = new QHBoxLayout;
    filters->setSpacing(7);
    typeButtons_ = new QButtonGroup(this);
    typeButtons_->setExclusive(true);
    QSet<QString> availableCategories;
    for (const auto &provider : providers_) {
        for (const auto &type : normalizedTypes(provider)) {
            const auto category = mediaCategory(type);
            if (!category.isEmpty()) availableCategories.insert(category);
        }
    }
    const QList<QPair<QString, QString>> categories{
        {"All", "providerTypeAll"}, {"Movies", "providerTypeMovies"},
        {"TV", "providerTypeTV"}, {"Anime", "providerTypeAnime"},
        {"Live", "providerTypeLive"}, {"NSFW", "providerTypeNsfw"},
    };
    for (const auto &[name, objectName] : categories) {
        auto *chip = new QPushButton(name);
        chip->setObjectName(objectName);
        chip->setProperty("providerTypeChip", true);
        chip->setCheckable(true);
        chip->setMinimumHeight(36);
        if (name != "All") chip->setVisible(availableCategories.contains(name));
        typeButtons_->addButton(chip);
        filters->addWidget(chip);
        connect(chip, &QPushButton::clicked, this, [this, name] {
            mediaFilter_ = name;
            applyFilter();
        });
        if (name == "All") chip->setChecked(true);
    }
    filters->addStretch();
    layout->addLayout(filters);

    connect(search_, &QLineEdit::textChanged, this, [this] { applyFilter(); });
    connect(list_, &QListWidget::itemActivated, this, &ProviderPickerDialog::activateItem);
    connect(list_, &QListWidget::itemClicked, this, &ProviderPickerDialog::activateItem);
    connect(list_, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *, QListWidgetItem *) { updateCurrentRows(); });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escape->setContext(Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, &ProviderPickerDialog::reject);
    rebuildItems();
    search_->setFocus();
}

QString ProviderPickerDialog::selectedKey() const { return selectedKey_; }
QStringList ProviderPickerDialog::pinnedProviderKeys() const { return pinnedProviderKeys_; }

int ProviderPickerDialog::exec() {
    result_ = Rejected;
    if (parentWidget()) setGeometry(parentWidget()->rect());
    show();
    raise();
    search_->setFocus();
    QEventLoop loop;
    const auto connection = connect(this, &ProviderPickerDialog::finished,
                                    &loop, [&loop] { loop.quit(); });
    loop.exec(QEventLoop::DialogExec);
    disconnect(connection);
    return result_;
}

int ProviderPickerDialog::result() const { return result_; }

void ProviderPickerDialog::accept() {
    result_ = Accepted;
    hide();
    emit accepted();
    emit finished(result_);
}

void ProviderPickerDialog::reject() {
    result_ = Rejected;
    hide();
    emit rejected();
    emit finished(result_);
}

bool ProviderPickerDialog::eventFilter(QObject *watched, QEvent *event) {
    if (watched == parentWidget() &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        setGeometry(parentWidget()->rect());
    }
    return QWidget::eventFilter(watched, event);
}

void ProviderPickerDialog::mousePressEvent(QMouseEvent *event) {
    if (panel_ && !panel_->geometry().contains(event->position().toPoint())) {
        reject();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ProviderPickerDialog::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updatePanelGeometry();
}

void ProviderPickerDialog::updatePanelGeometry() {
    if (!panel_) return;
    const auto panelWidth = std::min(620, std::max(360, width() - 48));
    const auto panelHeight = std::min(760, std::max(500, height() - 48));
    panel_->setGeometry((width() - panelWidth) / 2, (height() - panelHeight) / 2,
                        panelWidth, panelHeight);
}

QString ProviderPickerDialog::providerKey(const QJsonObject &provider) {
    return ProviderSelectionModel::key(provider);
}

QString ProviderPickerDialog::languageBadge(const QString &language) {
    return ProviderSelectionModel::languageFlag(language);
}

QString ProviderPickerDialog::providerSummary(const QJsonObject &provider) {
    QStringList summary;
    const auto types = normalizedTypes(provider);
    if (!types.isEmpty()) summary << types.join(" · ");
    auto domain = provider.value("mainUrl").toString();
    domain.remove(QRegularExpression("^https?://"));
    domain = domain.section('/', 0, 0);
    if (!domain.isEmpty()) summary << domain;
    return summary.join("  •  ");
}

void ProviderPickerDialog::addHeader(const QString &text) {
    auto *item = new QListWidgetItem(text);
    item->setData(Qt::UserRole, "header");
    item->setData(HeaderRole, true);
    item->setFlags(Qt::NoItemFlags);
    item->setSizeHint(QSize(0, 34));
    list_->addItem(item);
}

void ProviderPickerDialog::addChoice(const QString &mode, const QString &title,
                                     const QString &summary, const QString &badge) {
    auto *item = new QListWidgetItem;
    item->setData(Qt::UserRole, mode);
    item->setData(KeyRole, mode);
    item->setData(SearchRole, title.toLower());
    item->setData(SelectedRole, selectedKey_ == mode);
    item->setSizeHint(QSize(0, 58));
    item->setData(Qt::AccessibleTextRole, title + ", " + summary);
    list_->addItem(item);

    auto *row = new ClickableProviderRow;
    row->setObjectName("providerPickerRow");
    row->setProperty("selected", selectedKey_ == mode);
    row->activated = [this, item] {
        list_->setCurrentItem(item);
        activateItem(item);
    };
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(12, 5, 12, 5);
    rowLayout->setSpacing(12);
    auto *icon = new QLabel(badge);
    icon->setObjectName("providerPickerBadge");
    icon->setAttribute(Qt::WA_TransparentForMouseEvents);
    icon->setAlignment(Qt::AlignCenter);
    icon->setFixedSize(36, 36);
    rowLayout->addWidget(icon);
    auto *copy = new QVBoxLayout;
    copy->setSpacing(0);
    auto *name = new QLabel(title);
    name->setObjectName("providerPickerRowTitle");
    name->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *detail = new QLabel(summary);
    detail->setObjectName("providerPickerRowSummary");
    detail->setAttribute(Qt::WA_TransparentForMouseEvents);
    copy->addWidget(name);
    copy->addWidget(detail);
    rowLayout->addLayout(copy, 1);
    auto *check = new QLabel;
    check->setObjectName("providerPickerCheck");
    check->setAttribute(Qt::WA_TransparentForMouseEvents);
    check->setFixedWidth(24);
    check->setAlignment(Qt::AlignCenter);
    if (selectedKey_ == mode) check->setPixmap(QIcon(":/icons/provider-check.svg").pixmap(20, 20));
    rowLayout->addWidget(check);
    list_->setItemWidget(item, row);
}

void ProviderPickerDialog::addProvider(const QJsonObject &provider) {
    const auto key = providerKey(provider);
    const auto pinned = pinnedProviderKeys_.contains(key);
    const auto selected = selectedKey_ == key;
    const auto types = normalizedTypes(provider);
    QStringList categories;
    for (const auto &type : types) {
        const auto category = mediaCategory(type);
        if (!category.isEmpty() && !categories.contains(category)) categories << category;
    }
    const auto nameText = provider.value("name").toString();
    const auto summaryText = providerSummary(provider);
    auto *item = new QListWidgetItem;
    item->setData(Qt::UserRole, "provider");
    item->setData(KeyRole, key);
    item->setData(ProviderRole, provider.toVariantMap());
    item->setData(SearchRole, (nameText + " " + summaryText + " " +
        provider.value("extensionName").toString()).toLower());
    item->setData(SelectedRole, selected);
    item->setData(MediaRole, categories);
    item->setSizeHint(QSize(0, 64));
    item->setData(Qt::AccessibleTextRole, nameText + ", " + summaryText +
        (pinned ? ", pinned" : QString()) + (selected ? ", selected" : QString()));
    item->setToolTip(nameText + (summaryText.isEmpty() ? QString() : "\n" + summaryText));
    list_->addItem(item);

    auto *row = new ClickableProviderRow;
    row->setObjectName("providerPickerRow");
    row->setProperty("selected", selected);
    row->activated = [this, item] {
        list_->setCurrentItem(item);
        activateItem(item);
    };
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(12, 5, 10, 5);
    rowLayout->setSpacing(12);
    auto *badge = new QLabel(languageBadge(provider.value("language").toString()));
    badge->setObjectName("providerPickerBadge");
    badge->setAttribute(Qt::WA_TransparentForMouseEvents);
    badge->setAlignment(Qt::AlignCenter);
    badge->setFixedSize(36, 36);
    rowLayout->addWidget(badge);
    auto *copy = new QVBoxLayout;
    copy->setSpacing(0);
    auto *name = new QLabel(nameText);
    name->setObjectName("providerPickerRowTitle");
    name->setTextInteractionFlags(Qt::NoTextInteraction);
    name->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *summary = new QLabel(summaryText);
    summary->setObjectName("providerPickerRowSummary");
    summary->setTextInteractionFlags(Qt::NoTextInteraction);
    summary->setAttribute(Qt::WA_TransparentForMouseEvents);
    copy->addWidget(name);
    copy->addWidget(summary);
    rowLayout->addLayout(copy, 1);
    auto *pin = new QToolButton;
    pin->setObjectName("providerPickerPin");
    pin->setText(pinned ? "★" : "☆");
    pin->setToolTip(pinned ? "Unpin provider" : "Pin provider");
    pin->setAccessibleName(pin->toolTip());
    pin->setFixedSize(32, 32);
    connect(pin, &QToolButton::clicked, this, [this, key] {
        if (pinnedProviderKeys_.contains(key)) pinnedProviderKeys_.removeAll(key);
        else pinnedProviderKeys_.prepend(key);
        rebuildItems();
    });
    rowLayout->addWidget(pin);
    auto *check = new QLabel;
    check->setObjectName("providerPickerCheck");
    check->setFixedWidth(24);
    check->setAlignment(Qt::AlignCenter);
    if (selected) check->setPixmap(QIcon(":/icons/provider-check.svg").pixmap(20, 20));
    rowLayout->addWidget(check);
    list_->setItemWidget(item, row);
}

void ProviderPickerDialog::rebuildItems() {
    const auto query = search_ ? search_->text() : QString();
    list_->clear();
    addHeader("QUICK CHOICES");
    addChoice("none", "None", "Show Home without a provider", "—");
    addChoice("random", "Random", "Choose from automatic providers", "↝");
    addHeader("PROVIDERS · " + QString::number(providers_.size()));
    providerHeader_ = list_->item(list_->count() - 1);

    std::stable_sort(providers_.begin(), providers_.end(), [this](const auto &left, const auto &right) {
        const auto leftPinned = pinnedProviderKeys_.contains(providerKey(left));
        const auto rightPinned = pinnedProviderKeys_.contains(providerKey(right));
        if (leftPinned != rightPinned) return leftPinned;
        return left.value("name").toString().localeAwareCompare(right.value("name").toString()) < 0;
    });
    for (const auto &provider : providers_) addProvider(provider);
    if (search_) search_->setText(query);
    applyFilter();
    for (int row = 0; row < list_->count(); ++row) {
        auto *item = list_->item(row);
        if (item->data(SelectedRole).toBool()) {
            list_->setCurrentItem(item);
            list_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
            break;
        }
    }
    updateCurrentRows();
}

bool ProviderPickerDialog::matchesMediaFilter(const QJsonObject &provider) const {
    if (mediaFilter_ == "All") return true;
    for (const auto &type : normalizedTypes(provider)) {
        if (mediaCategory(type) == mediaFilter_) return true;
    }
    return false;
}

void ProviderPickerDialog::applyFilter() {
    const auto terms = search_->text().trimmed().toLower().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    int visibleProviders = 0;
    for (int row = 0; row < list_->count(); ++row) {
        auto *item = list_->item(row);
        const auto kind = item->data(Qt::UserRole).toString();
        if (kind == "header" || kind == "none" || kind == "random") {
            item->setHidden(false);
            continue;
        }
        const auto corpus = item->data(SearchRole).toString();
        bool matches = true;
        for (const auto &term : terms) {
            if (!corpus.contains(term)) {
                matches = false;
                break;
            }
        }
        const auto provider = QJsonObject::fromVariantMap(item->data(ProviderRole).toMap());
        matches = matches && matchesMediaFilter(provider);
        item->setHidden(!matches);
        if (matches) ++visibleProviders;
    }
    if (providerHeader_) providerHeader_->setText("PROVIDERS · " + QString::number(visibleProviders));
}

void ProviderPickerDialog::activateItem(QListWidgetItem *item) {
    if (!item || item->data(HeaderRole).toBool()) return;
    const auto kind = item->data(Qt::UserRole).toString();
    if (kind != "provider" && kind != "none" && kind != "random") return;
    selectedKey_ = item->data(KeyRole).toString();
    accept();
}

void ProviderPickerDialog::updateCurrentRows() {
    for (int row = 0; row < list_->count(); ++row) {
        auto *item = list_->item(row);
        auto *widget = list_->itemWidget(item);
        if (!widget) continue;
        widget->setProperty("focused", list_->currentItem() == item);
        repolish(widget);
    }
}

} // namespace CloudStream
