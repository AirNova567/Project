#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "authdialogs.h"
#include "bookingdialogs.h"
#include "admindialogs.h"
#include "settingsdialog.h"
#include "artworkdialog.h"
#include "appearance.h"

#include <algorithm>

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QHeaderView>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPixmap>
#include <QLabel>
#include <QToolButton>
#include <QEvent>

namespace {

void scaleImageToLabel(QLabel *label, const QString &imagePath)
{
    if (imagePath.isEmpty()) {
        label->setPixmap(QPixmap());
        label->setText(QStringLiteral("Немає зображення"));
        label->setMinimumHeight(120);
        label->setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }

    const QPixmap pix(imagePath);
    if (pix.isNull()) {
        label->setPixmap(QPixmap());
        label->setText(QStringLiteral("Немає зображення"));
        label->setMinimumHeight(120);
        label->setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }

    const int contentW = qMax(50, label->contentsRect().width());
    const QPixmap scaled = pix.scaledToWidth(contentW, Qt::SmoothTransformation);
    label->setText(QString());
    label->setPixmap(scaled);

    const int h = scaled.height() + 6;
    label->setMinimumHeight(h);
    label->setMaximumHeight(h);
}

} // namespace

MainWindow::MainWindow(DataManager *dataManager, const User &currentUser, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_dataManager(dataManager)
    , m_currentUser(currentUser)
{
    ui->setupUi(this);
    setupMenus();
    setupToolBar();
    setupConnections();
    applySettings();
    applyRoleRestrictions();
    refreshCatalogTable();

    connect(m_dataManager, &DataManager::artworksChanged, this, &MainWindow::refreshCatalogTable);
    connect(m_dataManager, &DataManager::settingsChanged, this, &MainWindow::applySettings);

    ui->categoryFilter->addItem(QStringLiteral("Усі категорії"), QString());
    ui->categoryFilter->addItem(QStringLiteral("Живопис"), QStringLiteral("Живопис"));
    ui->categoryFilter->addItem(QStringLiteral("Скульптура"), QStringLiteral("Скульптура"));
    ui->categoryFilter->addItem(QStringLiteral("Графіка"), QStringLiteral("Графіка"));
    ui->categoryFilter->addItem(QStringLiteral("Фотографія"), QStringLiteral("Фотографія"));

    ui->catalogTable->horizontalHeader()->setStretchLastSection(true);
    ui->catalogTable->horizontalHeader()->setSectionsClickable(true);
    ui->catalogTable->horizontalHeader()->setSortIndicatorShown(false);
    ui->catalogTable->horizontalHeader()->setHighlightSections(true);
    ui->catalogTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->catalogTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->catalogTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->catalogTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->catalogTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->catalogTable->verticalHeader()->setVisible(false);
    ui->catalogTable->setAlternatingRowColors(true);
    ui->catalogTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->catalogTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->catalogTable->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->catalogTable->setMouseTracking(true);
    ui->catalogTable->viewport()->setMouseTracking(true);
    ui->catalogTable->setWordWrap(false);

    ui->imageLabel->installEventFilter(this);
    ui->favoriteImageLabel->installEventFilter(this);
    ui->favoritesList->setContextMenuPolicy(Qt::CustomContextMenu);

    statusBar()->showMessage(QStringLiteral("Ласкаво просимо до арт-галереї"), 5000);
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::runAuthFlow(DataManager *dataManager, User *currentUser, QWidget *parent)
{
    while (true) {
        LoginDialog loginDialog(dataManager, parent);
        QObject::connect(&loginDialog, &LoginDialog::registerRequested, &loginDialog, [&]() {
            RegisterDialog registerDialog(dataManager, parent);
            registerDialog.exec();
        });

        if (loginDialog.exec() != QDialog::Accepted) {
            return false;
        }

        const QString userId = loginDialog.loggedInUserId();
        for (const User &user : dataManager->users()) {
            if (user.id() == userId) {
                if (currentUser) {
                    *currentUser = user;
                }
                return true;
            }
        }
    }
}

void MainWindow::setupConnections()
{
    connect(ui->searchEdit, &QLineEdit::textChanged, this, &MainWindow::refreshCatalogTable);
    connect(ui->categoryFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::refreshCatalogTable);
    connect(ui->availableOnlyCheck, &QCheckBox::toggled, this, &MainWindow::refreshCatalogTable);
    connect(ui->sortByTitleRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (!checked) {
            return;
        }
        m_sortSection = -1;
        m_sortState = 0;
        ui->catalogTable->horizontalHeader()->setSortIndicatorShown(false);
        refreshCatalogTable();
    });
    connect(ui->sortByYearRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (!checked) {
            return;
        }
        m_sortSection = -1;
        m_sortState = 0;
        ui->catalogTable->horizontalHeader()->setSortIndicatorShown(false);
        refreshCatalogTable();
    });
    connect(ui->catalogTable, &QTableWidget::itemSelectionChanged,
            this, &MainWindow::onArtworkSelectionChanged);
    connect(ui->catalogTable, &QTableWidget::customContextMenuRequested,
            this, &MainWindow::onCatalogContextMenu);
    connect(ui->catalogTable->horizontalHeader(), &QHeaderView::sectionClicked,
            this, &MainWindow::onCatalogHeaderClicked);
    connect(ui->favoritesList, &QListWidget::itemSelectionChanged,
            this, &MainWindow::onFavoritesSelectionChanged);
    connect(ui->removeFavoriteButton, &QPushButton::clicked,
            this, &MainWindow::onRemoveFavorite);
    connect(ui->favoritesList, &QListWidget::customContextMenuRequested,
            this, &MainWindow::onFavoritesContextMenu);
}

void MainWindow::setupMenus()
{
    const QColor accent = m_dataManager->settings().accentColor;
    auto ico = [&](AppIcons::Type type) { return AppIcons::icon(type, accent); };

    QMenu *fileMenu = menuBar()->addMenu(ico(AppIcons::Type::Export), QStringLiteral("Файл"));
    QAction *settingsAction = fileMenu->addAction(ico(AppIcons::Type::Settings),
                                                  QStringLiteral("Налаштування..."));
    fileMenu->addSeparator();
    QAction *exportAction = fileMenu->addAction(ico(AppIcons::Type::Export), QStringLiteral("Експорт CSV..."));
    QAction *importAction = fileMenu->addAction(ico(AppIcons::Type::Import), QStringLiteral("Імпорт CSV..."));
    fileMenu->addSeparator();
    QAction *logoutAction = fileMenu->addAction(ico(AppIcons::Type::Logout), QStringLiteral("Вийти"));
    QAction *exitAction = fileMenu->addAction(ico(AppIcons::Type::Exit), QStringLiteral("Завершити"));

    QMenu *editMenu = menuBar()->addMenu(ico(AppIcons::Type::Edit), QStringLiteral("Редагування"));
    QAction *addAction = editMenu->addAction(ico(AppIcons::Type::Add), QStringLiteral("Додати експонат"));
    QAction *editAction = editMenu->addAction(ico(AppIcons::Type::Edit), QStringLiteral("Редагувати"));
    QAction *deleteAction = editMenu->addAction(ico(AppIcons::Type::Delete), QStringLiteral("Видалити"));

    QMenu *viewMenu = menuBar()->addMenu(ico(AppIcons::Type::Report), QStringLiteral("Вигляд"));
    QAction *reportAction = viewMenu->addAction(ico(AppIcons::Type::Report), QStringLiteral("Звітність..."));
    QAction *bookingAction = viewMenu->addAction(ico(AppIcons::Type::Calendar),
                                                 QStringLiteral("Запис на візит..."));
    bookingAction->setObjectName(QStringLiteral("bookingAction"));

    QMenu *adminMenu = menuBar()->addMenu(ico(AppIcons::Type::Users), QStringLiteral("Адміністрування"));
    QAction *usersAction = adminMenu->addAction(ico(AppIcons::Type::Users), QStringLiteral("Користувачі..."));
    QAction *bookingsAdminAction = adminMenu->addAction(ico(AppIcons::Type::Calendar),
                                                        QStringLiteral("Записи на візит..."));
    bookingsAdminAction->setObjectName(QStringLiteral("bookingsAdminAction"));

    QMenu *helpMenu = menuBar()->addMenu(ico(AppIcons::Type::About), QStringLiteral("Довідка"));
    QAction *galleryInfoAction = helpMenu->addAction(ico(AppIcons::Type::Gallery),
                                                     QStringLiteral("Про галерею..."));
    QAction *aboutAction = helpMenu->addAction(ico(AppIcons::Type::About), QStringLiteral("Про застосунок"));

    connect(exportAction, &QAction::triggered, this, &MainWindow::onExportCsv);
    connect(importAction, &QAction::triggered, this, &MainWindow::onImportCsv);
    connect(logoutAction, &QAction::triggered, this, &MainWindow::onLogout);
    connect(exitAction, &QAction::triggered, this, &MainWindow::close);
    connect(addAction, &QAction::triggered, this, &MainWindow::onAddArtwork);
    connect(editAction, &QAction::triggered, this, &MainWindow::onEditArtwork);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::onDeleteArtwork);
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onOpenSettings);
    connect(reportAction, &QAction::triggered, this, &MainWindow::onOpenReport);
    connect(bookingAction, &QAction::triggered, this, &MainWindow::onOpenBooking);
    connect(usersAction, &QAction::triggered, this, &MainWindow::onOpenUsers);
    connect(bookingsAdminAction, &QAction::triggered, this, &MainWindow::onOpenBookingsManagement);
    connect(galleryInfoAction, &QAction::triggered, this, &MainWindow::onGalleryInfo);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

    adminMenu->setObjectName(QStringLiteral("adminMenu"));
}

void MainWindow::setupToolBar()
{
    const QColor accent = m_dataManager->settings().accentColor;
    auto ico = [&](AppIcons::Type type) { return AppIcons::icon(type, accent); };

    ui->mainToolBar->setIconSize(QSize(20, 20));
    ui->mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    auto *addBtn = new QToolButton(this);
    addBtn->setIcon(ico(AppIcons::Type::Add));
    addBtn->setText(QStringLiteral("Додати"));
    addBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(addBtn, &QToolButton::clicked, this, &MainWindow::onAddArtwork);
    ui->mainToolBar->addWidget(addBtn);

    auto *refreshBtn = new QToolButton(this);
    refreshBtn->setIcon(ico(AppIcons::Type::Refresh));
    refreshBtn->setText(QStringLiteral("Оновити"));
    refreshBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(refreshBtn, &QToolButton::clicked, this, &MainWindow::onRefreshCatalog);
    ui->mainToolBar->addWidget(refreshBtn);

    ui->mainToolBar->addSeparator();

    auto *settingsBtn = new QToolButton(this);
    settingsBtn->setIcon(ico(AppIcons::Type::Settings));
    settingsBtn->setText(QStringLiteral("Налаштування"));
    settingsBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(settingsBtn, &QToolButton::clicked, this, &MainWindow::onOpenSettings);
    ui->mainToolBar->addWidget(settingsBtn);

    auto *reportBtn = new QToolButton(this);
    reportBtn->setIcon(ico(AppIcons::Type::Report));
    reportBtn->setText(QStringLiteral("Звіт"));
    reportBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(reportBtn, &QToolButton::clicked, this, &MainWindow::onOpenReport);
    ui->mainToolBar->addWidget(reportBtn);

    auto *galleryBtn = new QToolButton(this);
    galleryBtn->setIcon(ico(AppIcons::Type::Gallery));
    galleryBtn->setText(QStringLiteral("Про галерею"));
    galleryBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(galleryBtn, &QToolButton::clicked, this, &MainWindow::onGalleryInfo);
    ui->mainToolBar->addWidget(galleryBtn);

    auto *bookingBtn = new QToolButton(this);
    bookingBtn->setObjectName(QStringLiteral("bookingToolButton"));
    bookingBtn->setIcon(ico(AppIcons::Type::Calendar));
    bookingBtn->setText(QStringLiteral("Запис на візит"));
    bookingBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect(bookingBtn, &QToolButton::clicked, this, &MainWindow::onOpenBooking);
    ui->mainToolBar->addWidget(bookingBtn);
}

void MainWindow::applyRoleRestrictions()
{
    const bool isAdmin = m_currentUser.role() == UserRole::Administrator;
    menuBar()->findChild<QMenu *>(QStringLiteral("adminMenu"))->menuAction()->setVisible(isAdmin);

    if (QAction *bookingAction = menuBar()->findChild<QAction *>(QStringLiteral("bookingAction"))) {
        bookingAction->setVisible(!isAdmin);
    }
    if (QAction *bookingsAdminAction =
            menuBar()->findChild<QAction *>(QStringLiteral("bookingsAdminAction"))) {
        bookingsAdminAction->setVisible(isAdmin);
    }
    if (QWidget *bookingBtn = ui->mainToolBar->findChild<QWidget *>(QStringLiteral("bookingToolButton"))) {
        bookingBtn->setVisible(!isAdmin);
    }

    const QString roleName = isAdmin ? QStringLiteral("Адміністратор")
                                     : QStringLiteral("Користувач");
    statusBar()->showMessage(QStringLiteral("Користувач: %1 (%2)")
                                 .arg(m_currentUser.fullName(), roleName),
                             8000);
}

void MainWindow::applySettings()
{
    const AppSettings settings = m_dataManager->settings();
    setWindowTitle(QStringLiteral("Арт-галерея"));
    ui->titleLabel->setText(QStringLiteral("Арт-галерея — каталог експонатів"));
    ui->subtitleLabel->setText(QStringLiteral("Керування експонатами, пошук і звіти в одному вікні"));
    ui->tabWidget->setTabText(0, QStringLiteral("Каталог"));
    ui->tabWidget->setTabText(1, QStringLiteral("Обране"));
    ui->filterGroup->setTitle(QStringLiteral("Пошук та фільтрація"));
    ui->searchEdit->setPlaceholderText(QStringLiteral("Пошук за назвою або художником..."));
    ui->availableOnlyCheck->setText(QStringLiteral("Лише з описом"));
    ui->sortByTitleRadio->setText(QStringLiteral("Сортувати за назвою"));
    ui->sortByYearRadio->setText(QStringLiteral("Сортувати за роком"));
    ui->previewGroup->setTitle(QStringLiteral("Перегляд експонату"));
    ui->favoritePreviewGroup->setTitle(QStringLiteral("Перегляд обраного"));
    ui->removeFavoriteButton->setText(QStringLiteral("Видалити з обраного"));
    ui->favoritesHint->setText(QStringLiteral("Додайте експонат через контекстне меню таблиці."));
    ui->progressBar->setFormat(QStringLiteral("Завантаження: %p%"));

    ui->catalogTable->horizontalHeaderItem(0)->setText(QStringLiteral("Назва"));
    ui->catalogTable->horizontalHeaderItem(1)->setText(QStringLiteral("Художник"));
    ui->catalogTable->horizontalHeaderItem(2)->setText(QStringLiteral("Категорія"));
    ui->catalogTable->horizontalHeaderItem(3)->setText(QStringLiteral("Рік"));
    ui->catalogTable->horizontalHeaderItem(4)->setText(QStringLiteral("Ціна"));
    ui->catalogTable->horizontalHeaderItem(5)->setText(QStringLiteral("ID"));

    ui->categoryFilter->setItemText(0, QStringLiteral("Усі категорії"));
    ui->categoryFilter->setItemText(1, QStringLiteral("Живопис"));
    ui->categoryFilter->setItemText(2, QStringLiteral("Скульптура"));
    ui->categoryFilter->setItemText(3, QStringLiteral("Графіка"));
    ui->categoryFilter->setItemText(4, QStringLiteral("Фотографія"));

    menuBar()->clear();
    setupMenus();
    ui->mainToolBar->clear();
    setupToolBar();

    ThemeManager::apply(qApp, settings);
    ui->mainToolBar->setVisible(settings.showToolBar);
    applyRoleRestrictions();

    if (settings.sortField == QStringLiteral("year")) {
        ui->sortByYearRadio->setChecked(true);
    } else {
        ui->sortByTitleRadio->setChecked(true);
    }
}

QVector<Artwork> MainWindow::filteredArtworks() const
{
    QVector<Artwork> result = m_dataManager->artworks();
    const QString query = ui->searchEdit->text().trimmed().toLower();
    const QString category = ui->categoryFilter->currentData().toString();

    result.erase(std::remove_if(result.begin(), result.end(), [&](const Artwork &artwork) {
        if (!category.isEmpty() && artwork.category() != category) {
            return true;
        }
        if (ui->availableOnlyCheck->isChecked() && artwork.description().isEmpty()) {
            return true;
        }
        if (!query.isEmpty()) {
            const QString haystack = (artwork.title() + artwork.artist() + artwork.category()).toLower();
            if (!haystack.contains(query)) {
                return true;
            }
        }
        return false;
    }), result.end());

    if (m_sortSection >= 0 && m_sortState != 0) {
        const bool descending = (m_sortState == 1);
        std::sort(result.begin(), result.end(), [&](const Artwork &a, const Artwork &b) {
            int cmp = 0;
            switch (m_sortSection) {
            case 0:
                cmp = a.title().compare(b.title(), Qt::CaseInsensitive);
                break;
            case 1:
                cmp = a.artist().compare(b.artist(), Qt::CaseInsensitive);
                break;
            case 2:
                cmp = a.category().compare(b.category(), Qt::CaseInsensitive);
                break;
            case 3:
                cmp = (a.year() == b.year()) ? 0 : (a.year() < b.year() ? -1 : 1);
                break;
            case 4:
                cmp = (a.price() == b.price()) ? 0 : (a.price() < b.price() ? -1 : 1);
                break;
            default:
                break;
            }
            return descending ? cmp > 0 : cmp < 0;
        });
    } else {
        const bool ascending = m_dataManager->settings().sortAscending;
        const bool sortByYear = ui->sortByYearRadio->isChecked();
        std::sort(result.begin(), result.end(), [&](const Artwork &a, const Artwork &b) {
            int cmp = 0;
            if (sortByYear) {
                cmp = (a.year() == b.year()) ? 0 : (a.year() < b.year() ? -1 : 1);
            } else {
                cmp = a.title().compare(b.title(), Qt::CaseInsensitive);
            }
            if (cmp == 0) {
                cmp = a.artist().compare(b.artist(), Qt::CaseInsensitive);
            }
            return ascending ? cmp < 0 : cmp > 0;
        });
    }

    return result;
}

void MainWindow::cycleSortState(int section)
{
    if (m_sortSection != section) {
        m_sortSection = section;
        m_sortState = 1;
        return;
    }

    m_sortState = (m_sortState + 1) % 3;
}

void MainWindow::onCatalogHeaderClicked(int section)
{
    if (section < 0 || section > 4) {
        return;
    }
    cycleSortState(section);
    if (m_sortState == 0) {
        ui->catalogTable->horizontalHeader()->setSortIndicatorShown(false);
        showStatusMessage(QStringLiteral("Сортування вимкнено"));
    } else {
        const Qt::SortOrder order = (m_sortState == 1) ? Qt::DescendingOrder : Qt::AscendingOrder;
        ui->catalogTable->horizontalHeader()->setSortIndicator(section, order);
        ui->catalogTable->horizontalHeader()->setSortIndicatorShown(true);
        showStatusMessage(m_sortState == 1
                              ? QStringLiteral("Сортування: за спаданням")
                              : QStringLiteral("Сортування: за зростанням"));
    }
    refreshCatalogTable();
}

void MainWindow::refreshCatalogTable()
{
    ui->progressBar->setValue(30);
    const QVector<Artwork> artworks = filteredArtworks();
    ui->catalogTable->setRowCount(artworks.size());

    for (int i = 0; i < artworks.size(); ++i) {
        const Artwork &artwork = artworks[i];
        ui->catalogTable->setItem(i, 0, new QTableWidgetItem(artwork.title()));
        ui->catalogTable->setItem(i, 1, new QTableWidgetItem(artwork.artist()));
        ui->catalogTable->setItem(i, 2, new QTableWidgetItem(artwork.category()));
        auto *yearItem = new QTableWidgetItem(QString::number(artwork.year()));
        yearItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignRight);
        ui->catalogTable->setItem(i, 3, yearItem);
        ui->catalogTable->setItem(i, 4, new QTableWidgetItem(
            QString::number(artwork.price(), 'f', 2) + QStringLiteral(" грн")));
        ui->catalogTable->setItem(i, 5, new QTableWidgetItem(artwork.id()));
    }
    for (int i = 0; i < artworks.size(); ++i) {
        if (auto *priceItem = ui->catalogTable->item(i, 4)) {
            priceItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignRight);
        }
    }
    ui->catalogTable->hideColumn(5);
    ui->progressBar->setValue(100);
    showStatusMessage(QStringLiteral("Знайдено експонатів: %1").arg(artworks.size()));
}

QString MainWindow::selectedArtworkId() const
{
    const int row = ui->catalogTable->currentRow();
    if (row < 0) {
        return QString();
    }
    const QTableWidgetItem *item = ui->catalogTable->item(row, 5);
    return item ? item->text() : QString();
}

void MainWindow::onArtworkSelectionChanged()
{
    const QString id = selectedArtworkId();
    showArtworkPreview(id, ui->imageLabel, ui->detailsLabel, m_currentImagePath);
}

void MainWindow::onFavoritesSelectionChanged()
{
    QListWidgetItem *item = ui->favoritesList->currentItem();
    if (!item) {
        m_currentFavoriteImagePath.clear();
        ui->favoriteImageLabel->setPixmap(QPixmap());
        ui->favoriteImageLabel->setText(QStringLiteral("Оберіть експонат зі списку"));
        ui->favoriteImageLabel->setMinimumHeight(120);
        ui->favoriteImageLabel->setMaximumHeight(QWIDGETSIZE_MAX);
        ui->favoriteDetailsLabel->clear();
        return;
    }

    showArtworkPreview(item->data(Qt::UserRole).toString(),
                       ui->favoriteImageLabel,
                       ui->favoriteDetailsLabel,
                       m_currentFavoriteImagePath);
}

void MainWindow::onRemoveFavorite()
{
    QListWidgetItem *item = ui->favoritesList->currentItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("Інформація"),
                                 QStringLiteral("Оберіть експонат у списку обраного."));
        return;
    }

    delete ui->favoritesList->takeItem(ui->favoritesList->row(item));
    onFavoritesSelectionChanged();
    showStatusMessage(QStringLiteral("Експонат видалено з обраного"));
}

void MainWindow::onFavoritesContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = ui->favoritesList->itemAt(pos);
    if (!item) {
        return;
    }

    ui->favoritesList->setCurrentItem(item);

    const QColor accent = m_dataManager->settings().accentColor;
    QMenu menu(this);
    menu.addAction(AppIcons::icon(AppIcons::Type::Delete, accent),
                   QStringLiteral("Видалити з обраного"),
                   this,
                   &MainWindow::onRemoveFavorite);
    menu.exec(ui->favoritesList->viewport()->mapToGlobal(pos));
}

void MainWindow::showArtworkPreview(const QString &artworkId,
                                    QLabel *imageLabel,
                                    QLabel *detailsLabel,
                                    QString &trackedImagePath)
{
    if (artworkId.isEmpty()) {
        trackedImagePath.clear();
        imageLabel->setPixmap(QPixmap());
        imageLabel->setText(QStringLiteral("Оберіть експонат"));
        imageLabel->setMinimumHeight(120);
        imageLabel->setMaximumHeight(QWIDGETSIZE_MAX);
        detailsLabel->clear();
        return;
    }

    const auto artwork = m_dataManager->artworkById(artworkId);
    if (!artwork.has_value()) {
        return;
    }

    detailsLabel->setText(
        QStringLiteral("<b>%1</b><br>%2<br>%3, %4 р.<br>Ціна: %5 грн<br><br>%6")
            .arg(artwork->title(), artwork->artist(), artwork->category())
            .arg(artwork->year())
            .arg(artwork->price(), 0, 'f', 2)
            .arg(artwork->description()));

    trackedImagePath = artwork->imagePath();
    scaleImageToLabel(imageLabel, trackedImagePath);
}

void MainWindow::updatePreviewImage()
{
    scaleImageToLabel(ui->imageLabel, m_currentImagePath);
}

void MainWindow::updateFavoritePreviewImage()
{
    scaleImageToLabel(ui->favoriteImageLabel, m_currentFavoriteImagePath);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Resize) {
        if (obj == ui->imageLabel && !m_currentImagePath.isEmpty()) {
            updatePreviewImage();
        } else if (obj == ui->favoriteImageLabel && !m_currentFavoriteImagePath.isEmpty()) {
            updateFavoritePreviewImage();
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::onAddArtwork()
{
    if (m_currentUser.role() != UserRole::Administrator) {
        QMessageBox::warning(this, QStringLiteral("Доступ заборонено"),
                             QStringLiteral("Додавання доступне лише адміністратору."));
        return;
    }

    ArtworkDialog dialog(m_dataManager, this);
    if (dialog.exec() != QDialog::Accepted || !dialog.artwork().has_value()) {
        return;
    }

    Artwork artwork = dialog.artwork().value();

    QString error;
    if (!m_dataManager->addArtwork(artwork, &error)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
        return;
    }
    showStatusMessage(QStringLiteral("Експонат додано"));
}

void MainWindow::onEditArtwork()
{
    if (m_currentUser.role() != UserRole::Administrator) {
        QMessageBox::warning(this, QStringLiteral("Доступ заборонено"),
                             QStringLiteral("Редагування доступне лише адміністратору."));
        return;
    }

    const QString id = selectedArtworkId();
    if (id.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Інформація"),
                                 QStringLiteral("Оберіть експонат."));
        return;
    }

    const auto existing = m_dataManager->artworkById(id);
    if (!existing.has_value()) {
        return;
    }

    ArtworkDialog dialog(m_dataManager, this);
    dialog.setArtwork(existing.value());
    if (dialog.exec() != QDialog::Accepted || !dialog.artwork().has_value()) {
        return;
    }

    QString error;
    if (!m_dataManager->updateArtwork(dialog.artwork().value(), &error)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
    }
}

void MainWindow::onDeleteArtwork()
{
    if (m_currentUser.role() != UserRole::Administrator) {
        QMessageBox::warning(this, QStringLiteral("Доступ заборонено"),
                             QStringLiteral("Видалення доступне лише адміністратору."));
        return;
    }

    const QString id = selectedArtworkId();
    if (id.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Інформація"),
                                 QStringLiteral("Оберіть експонат."));
        return;
    }

    if (QMessageBox::question(this, QStringLiteral("Підтвердження"),
                              QStringLiteral("Видалити експонат?")) != QMessageBox::Yes) {
        return;
    }

    QString error;
    if (!m_dataManager->removeArtwork(id, &error)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
    }
}

void MainWindow::onRefreshCatalog()
{
    refreshCatalogTable();
}

void MainWindow::onExportCsv()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Експорт CSV"), QStringLiteral("artworks.csv"),
        QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!m_dataManager->exportArtworksToCsv(path, &error)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
        return;
    }
    QMessageBox::information(this, QStringLiteral("Успіх"), QStringLiteral("Файл збережено."));
}

void MainWindow::onImportCsv()
{
    if (m_currentUser.role() != UserRole::Administrator) {
        QMessageBox::warning(this, QStringLiteral("Доступ заборонено"),
                             QStringLiteral("Імпорт доступний лише адміністратору."));
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Імпорт CSV"), QString(), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!m_dataManager->importArtworksFromCsv(path, &error)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
        return;
    }
    QMessageBox::information(this, QStringLiteral("Успіх"), QStringLiteral("Імпорт завершено."));
}

void MainWindow::onOpenSettings()
{
    SettingsDialog dialog(m_dataManager, this);
    if (dialog.exec() == QDialog::Accepted) {
        applySettings();
        refreshCatalogTable();
    }
}

void MainWindow::onOpenReport()
{
    const bool isAdmin = m_currentUser.role() == UserRole::Administrator;
    ReportDialog dialog(m_dataManager, isAdmin, this);
    dialog.exec();
}

void MainWindow::onOpenUsers()
{
    if (m_currentUser.role() != UserRole::Administrator) {
        QMessageBox::warning(this, QStringLiteral("Доступ заборонено"),
                             QStringLiteral("Доступ лише для адміністратора."));
        return;
    }
    UserManagementDialog dialog(m_dataManager, this);
    dialog.exec();
}

void MainWindow::onGalleryInfo()
{
    GalleryInfoDialog dialog(this);
    dialog.exec();
}

void MainWindow::onOpenBooking()
{
    if (m_currentUser.role() == UserRole::Administrator) {
        QMessageBox::information(this, QStringLiteral("Інформація"),
                                 QStringLiteral("Адміністратор керує записами через меню «Адміністрування»."));
        return;
    }

    BookingDialog dialog(m_dataManager, m_currentUser, this);
    dialog.exec();
}

void MainWindow::onOpenBookingsManagement()
{
    if (m_currentUser.role() != UserRole::Administrator) {
        QMessageBox::warning(this, QStringLiteral("Доступ заборонено"),
                             QStringLiteral("Доступ лише для адміністратора."));
        return;
    }

    BookingsManagementDialog dialog(m_dataManager, this);
    dialog.exec();
}

void MainWindow::onAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::onLogout()
{
    close();
}

void MainWindow::onCatalogContextMenu(const QPoint &pos)
{
    const QColor accent = m_dataManager->settings().accentColor;
    auto ico = [&](AppIcons::Type type) { return AppIcons::icon(type, accent); };

    QMenu menu(this);
    menu.addAction(ico(AppIcons::Type::Add), QStringLiteral("Додати"), this, &MainWindow::onAddArtwork);
    menu.addAction(ico(AppIcons::Type::Edit), QStringLiteral("Редагувати"), this, &MainWindow::onEditArtwork);
    menu.addAction(ico(AppIcons::Type::Delete), QStringLiteral("Видалити"), this, &MainWindow::onDeleteArtwork);
    menu.addSeparator();
    menu.addAction(ico(AppIcons::Type::Favorite), QStringLiteral("Додати до обраного"), this, [this]() {
        const QString id = selectedArtworkId();
        if (id.isEmpty()) {
            return;
        }

        for (int i = 0; i < ui->favoritesList->count(); ++i) {
            if (ui->favoritesList->item(i)->data(Qt::UserRole).toString() == id) {
                QMessageBox::information(this, QStringLiteral("Обране"),
                                         QStringLiteral("Цей експонат уже в обраному."));
                ui->tabWidget->setCurrentWidget(ui->favoritesTab);
                ui->favoritesList->setCurrentRow(i);
                return;
            }
        }

        const auto artwork = m_dataManager->artworkById(id);
        if (!artwork.has_value()) {
            return;
        }

        auto *item = new QListWidgetItem(
            QStringLiteral("%1 — %2").arg(artwork->title(), artwork->artist()));
        item->setData(Qt::UserRole, id);
        ui->favoritesList->addItem(item);
        ui->tabWidget->setCurrentWidget(ui->favoritesTab);
        ui->favoritesList->setCurrentItem(item);
        showStatusMessage(QStringLiteral("Додано до обраного"));
    });
    menu.exec(ui->catalogTable->viewport()->mapToGlobal(pos));
}

void MainWindow::showStatusMessage(const QString &message, int timeoutMs)
{
    statusBar()->showMessage(message, timeoutMs);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ArtGallery"));
    QApplication::setOrganizationName(QStringLiteral("ArtGalleryUA"));

    DataManager dataManager;
    QString initError;
    if (!dataManager.initialize(&initError)) {
        QMessageBox::critical(nullptr, QStringLiteral("Помилка запуску"), initError);
        return 1;
    }

    ThemeManager::apply(&app, dataManager.settings());

    User currentUser;
    if (!MainWindow::runAuthFlow(&dataManager, &currentUser, nullptr)) {
        return 0;
    }

    MainWindow window(&dataManager, currentUser);
    window.show();
    return app.exec();
}
