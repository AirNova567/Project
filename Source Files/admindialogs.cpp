#include "admindialogs.h"
#include "authdialogs.h"
#include "datamanager.h"

#include "ui_reportdialog.h"
#include "ui_usermanagementdialog.h"

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QMap>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QTimer>
ReportGenerator::ReportGenerator() = default;
ReportGenerator::~ReportGenerator() = default;

QString ReportGenerator::formatHeader(const QString &title) const
{
    return QStringLiteral("══════════════════════════════════════\n")
           + title + QStringLiteral("\n")
           + QStringLiteral("Дата формування: ")
           + QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm"))
           + QStringLiteral("\n══════════════════════════════════════\n\n");
}

QString ReportGenerator::generateArtworksReport(const QVector<Artwork> &artworks) const
{
    QString report = formatHeader(QStringLiteral("ЗВІТ ПРО ЕКСПОНАТИ АРТ-ГАЛЕРЕЇ"));

    double totalPrice = 0.0;
    QMap<QString, int> byCategory;

    for (const Artwork &artwork : artworks) {
        totalPrice += artwork.price();
        byCategory[artwork.category()]++;
        report += QStringLiteral("• %1 — %2 (%3, %4 рік, %5 грн)\n")
                      .arg(artwork.title(), artwork.artist(), artwork.category())
                      .arg(artwork.year())
                      .arg(artwork.price(), 0, 'f', 2);
        if (!artwork.description().isEmpty()) {
            report += QStringLiteral("  Опис: %1\n").arg(artwork.description());
        }
        report += QLatin1Char('\n');
    }

    report += QStringLiteral("Загальна кількість: %1\n").arg(artworks.size());
    report += QStringLiteral("Загальна вартість: %1 грн\n\n").arg(totalPrice, 0, 'f', 2);
    report += QStringLiteral("Розподіл за категоріями:\n");
    for (auto it = byCategory.constBegin(); it != byCategory.constEnd(); ++it) {
        report += QStringLiteral("  - %1: %2\n").arg(it.key()).arg(it.value());
    }
    return report;
}

QString ReportGenerator::generateUsersReport(const QVector<User> &users) const
{
    QString report = formatHeader(QStringLiteral("ЗВІТ ПРО КОРИСТУВАЧІВ СИСТЕМИ"));

    int admins = 0;
    int visitors = 0;
    for (const User &user : users) {
        if (user.role() == UserRole::Administrator) {
            ++admins;
        } else {
            ++visitors;
        }
        report += QStringLiteral("• %1 (@%2) — %3\n")
                      .arg(user.fullName(), user.login(), user.roleName());
    }

    report += QStringLiteral("\nВсього користувачів: %1\n").arg(users.size());
    report += QStringLiteral("Адміністраторів: %1\n").arg(admins);
    report += QStringLiteral("Звичайних користувачів: %1\n").arg(visitors);
    return report;
}

QString ReportGenerator::generateSummaryReport(const QVector<Artwork> &artworks,
                                                 const QVector<User> &users) const
{
    return generateArtworksReport(artworks) + QStringLiteral("\n\n")
           + generateUsersReport(users);
}

bool ReportGenerator::saveReportToFile(const QString &filePath, const QString &content,
                                       QString *errorMessage) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не вдалося зберегти звіт у файл.");
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << content;
    return true;
}

ReportDialog::ReportDialog(DataManager *dataManager, bool isAdmin, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ReportDialog)
    , m_dataManager(dataManager)
    , m_isAdmin(isAdmin)
{
    ui->setupUi(this);

    ui->typeCombo->addItem(QStringLiteral("Звіт про експонати"), QStringLiteral("artworks"));
    if (m_isAdmin) {
        ui->typeCombo->addItem(QStringLiteral("Звіт про користувачів"), QStringLiteral("users"));
        ui->typeCombo->addItem(QStringLiteral("Зведений звіт"), QStringLiteral("summary"));
    }

    connect(ui->generateButton, &QPushButton::clicked, this, &ReportDialog::onGenerate);
    connect(ui->saveButton, &QPushButton::clicked, this, &ReportDialog::onSave);
    connect(ui->closeButton, &QPushButton::clicked, this, &ReportDialog::reject);
}

ReportDialog::~ReportDialog()
{
    delete ui;
}

void ReportDialog::onGenerate()
{
    ui->progressBar->setValue(0);
    const QString type = ui->typeCombo->currentData().toString();

    QTimer::singleShot(50, this, [this, type]() {
        ui->progressBar->setValue(50);
        const QVector<Artwork> artworks = m_dataManager->artworks();
        const QVector<User> users = m_dataManager->users();

        if (type == QStringLiteral("users")) {
            m_currentReport = m_reportGenerator.generateUsersReport(users);
        } else if (type == QStringLiteral("summary")) {
            m_currentReport = m_reportGenerator.generateSummaryReport(artworks, users);
        } else {
            m_currentReport = m_reportGenerator.generateArtworksReport(artworks);
        }

        ui->progressBar->setValue(100);
        ui->reportEdit->setPlainText(m_currentReport);
    });
}

void ReportDialog::onSave()
{
    if (m_currentReport.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Інформація"),
                                 QStringLiteral("Спочатку сформуйте звіт."));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Зберегти звіт"), QStringLiteral("zvit.txt"),
        QStringLiteral("Текст (*.txt)"));
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!m_reportGenerator.saveReportToFile(path, m_currentReport, &error)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
        return;
    }
    QMessageBox::information(this, QStringLiteral("Успіх"), QStringLiteral("Звіт збережено."));
}

UserManagementDialog::UserManagementDialog(DataManager *dataManager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::UserManagementDialog)
    , m_dataManager(dataManager)
{
    ui->setupUi(this);

    ui->usersTable->horizontalHeader()->setStretchLastSection(true);
    ui->usersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->usersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(ui->addButton, &QPushButton::clicked, this, &UserManagementDialog::onAddUser);
    connect(ui->deleteButton, &QPushButton::clicked, this, &UserManagementDialog::onDeleteUser);
    connect(ui->refreshButton, &QPushButton::clicked, this, &UserManagementDialog::refreshTable);
    connect(ui->closeButton, &QPushButton::clicked, this, &UserManagementDialog::accept);
    connect(m_dataManager, &DataManager::usersChanged, this, &UserManagementDialog::refreshTable);

    refreshTable();
}

UserManagementDialog::~UserManagementDialog()
{
    delete ui;
}

void UserManagementDialog::refreshTable()
{
    const QVector<User> users = m_dataManager->users();
    ui->usersTable->setRowCount(users.size());

    for (int i = 0; i < users.size(); ++i) {
        const User &user = users[i];
        ui->usersTable->setItem(i, 0, new QTableWidgetItem(user.login()));
        ui->usersTable->setItem(i, 1, new QTableWidgetItem(user.fullName()));
        ui->usersTable->setItem(i, 2, new QTableWidgetItem(user.roleName()));
        ui->usersTable->setItem(i, 3, new QTableWidgetItem(user.id()));
    }
    ui->usersTable->hideColumn(3);
}

void UserManagementDialog::onAddUser()
{
    RegisterDialog dialog(m_dataManager, this);
    if (dialog.exec() == QDialog::Accepted) {
        refreshTable();
    }
}

void UserManagementDialog::onDeleteUser()
{
    const int row = ui->usersTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("Інформація"),
                                 QStringLiteral("Оберіть користувача."));
        return;
    }

    const QString id = ui->usersTable->item(row, 3)->text();
    if (QMessageBox::question(this, QStringLiteral("Підтвердження"),
                              QStringLiteral("Видалити користувача?")) != QMessageBox::Yes) {
        return;
    }

    QString error;
    if (!m_dataManager->removeUser(id, &error)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
        return;
    }
    refreshTable();
}
