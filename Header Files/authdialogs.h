#ifndef AUTHDIALOGS_H
#define AUTHDIALOGS_H

#include <QDialog>
#include <QString>

#include "models.h"

class DataManager;

namespace Ui {
class LoginDialog;
class RegisterDialog;
class AboutDialog;
class GalleryInfoDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(DataManager *dataManager, QWidget *parent = nullptr);
    ~LoginDialog() override;

    QString loggedInUserId() const;

signals:
    void loginSucceeded(const QString &userId);
    void registerRequested();

private slots:
    void onLoginClicked();
    void onRegisterClicked();

private:
    Ui::LoginDialog *ui;
    DataManager *m_dataManager;
    QString m_loggedInUserId;
};

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(DataManager *dataManager, QWidget *parent = nullptr);
    ~RegisterDialog() override;

private slots:
    void onRegisterClicked();

private:
    User buildUser() const;

    Ui::RegisterDialog *ui;
    DataManager *m_dataManager;
};

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
    ~AboutDialog() override;

private:
    Ui::AboutDialog *ui;
};

class GalleryInfoDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GalleryInfoDialog(QWidget *parent = nullptr);
    ~GalleryInfoDialog() override;

private:
    Ui::GalleryInfoDialog *ui;
};

#endif
