#include "settingsdialog.h"
#include "datamanager.h"

#include "ui_settingsdialog.h"

#include <QColorDialog>
#include <QMessageBox>
SettingsDialog::SettingsDialog(DataManager *dataManager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
    , m_dataManager(dataManager)
    , m_settings(dataManager->settings())
{
    ui->setupUi(this);

    ui->sortCombo->addItem(QStringLiteral("За назвою"), QStringLiteral("title"));
    ui->sortCombo->addItem(QStringLiteral("За художником"), QStringLiteral("artist"));
    ui->sortCombo->addItem(QStringLiteral("За роком"), QStringLiteral("year"));
    ui->sortCombo->addItem(QStringLiteral("За ціною"), QStringLiteral("price"));

    ui->themeCombo->addItem(QStringLiteral("Світла"), QStringLiteral("light"));
    ui->themeCombo->addItem(QStringLiteral("Темна"), QStringLiteral("dark"));

    loadSettings();

    connect(ui->colorButton, &QPushButton::clicked, this, &SettingsDialog::onPickColor);
    connect(ui->applyButton, &QPushButton::clicked, this, &SettingsDialog::onApply);
    connect(ui->cancelButton, &QPushButton::clicked, this, &SettingsDialog::reject);
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

AppSettings SettingsDialog::settings() const
{
    return m_settings;
}

void SettingsDialog::loadSettings()
{
    ui->colorPreview->setStyleSheet(
        QStringLiteral("background-color: %1;").arg(m_settings.accentColor.name()));
    ui->fontSpin->setValue(m_settings.fontSize);
    ui->toolBarCheck->setChecked(m_settings.showToolBar);

    const int sortIndex = ui->sortCombo->findData(m_settings.sortField);
    if (sortIndex >= 0) {
        ui->sortCombo->setCurrentIndex(sortIndex);
    }
    ui->sortAscCheck->setChecked(m_settings.sortAscending);
    ui->themeCombo->setCurrentIndex(m_settings.darkMode ? 1 : 0);
}

void SettingsDialog::onPickColor()
{
    const QColor color = QColorDialog::getColor(m_settings.accentColor, this,
                                                QStringLiteral("Оберіть колір"));
    if (!color.isValid()) {
        return;
    }
    m_settings.accentColor = color;
    ui->colorPreview->setStyleSheet(QStringLiteral("background-color: %1;").arg(color.name()));
}

void SettingsDialog::onApply()
{
    m_settings.fontSize = ui->fontSpin->value();
    m_settings.showToolBar = ui->toolBarCheck->isChecked();
    m_settings.sortField = ui->sortCombo->currentData().toString();
    m_settings.sortAscending = ui->sortAscCheck->isChecked();
    m_settings.darkMode = (ui->themeCombo->currentData().toString() == QStringLiteral("dark"));
    QString error;
    if (!m_dataManager->saveSettings(m_settings, &error)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
        return;
    }
    accept();
}
