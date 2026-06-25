#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

#include "models.h"

class DataManager;

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(DataManager *dataManager, QWidget *parent = nullptr);
    ~SettingsDialog() override;

    AppSettings settings() const;

private slots:
    void onPickColor();
    void onApply();

private:
    void loadSettings();

    Ui::SettingsDialog *ui;
    DataManager *m_dataManager;
    AppSettings m_settings;
};

#endif
