#ifndef ADMINDIALOGS_H
#define ADMINDIALOGS_H

#include <QDialog>
#include <QString>
#include <QVector>

#include "models.h"

class DataManager;

namespace Ui {
class ReportDialog;
class UserManagementDialog;
}

class ReportGenerator
{
public:
    ReportGenerator();
    ~ReportGenerator();

    QString generateArtworksReport(const QVector<Artwork> &artworks) const;
    QString generateUsersReport(const QVector<User> &users) const;
    QString generateSummaryReport(const QVector<Artwork> &artworks,
                                  const QVector<User> &users) const;
    bool saveReportToFile(const QString &filePath, const QString &content,
                          QString *errorMessage = nullptr) const;

private:
    QString formatHeader(const QString &title) const;
};

class ReportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReportDialog(DataManager *dataManager, bool isAdmin, QWidget *parent = nullptr);
    ~ReportDialog() override;

private slots:
    void onGenerate();
    void onSave();

private:
    Ui::ReportDialog *ui;
    DataManager *m_dataManager;
    ReportGenerator m_reportGenerator;
    bool m_isAdmin;
    QString m_currentReport;
};

class UserManagementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserManagementDialog(DataManager *dataManager, QWidget *parent = nullptr);
    ~UserManagementDialog() override;

private slots:
    void refreshTable();
    void onAddUser();
    void onDeleteUser();

private:
    Ui::UserManagementDialog *ui;
    DataManager *m_dataManager;
};

#endif
