#include "authdialogs.h"
#include "datamanager.h"

#include "ui_logindialog.h"
#include "ui_registerdialog.h"
#include "ui_aboutdialog.h"
#include "ui_galleryinfodialog.h"

#include <QMessageBox>
LoginDialog::LoginDialog(DataManager *dataManager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
    , m_dataManager(dataManager)
{
    ui->setupUi(this);
    setModal(true);

    connect(ui->loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(ui->registerButton, &QPushButton::clicked, this, &LoginDialog::onRegisterClicked);
    connect(ui->passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

QString LoginDialog::loggedInUserId() const
{
    return m_loggedInUserId;
}

void LoginDialog::onLoginClicked()
{
    User user;
    QString error;
    if (!m_dataManager->authenticate(ui->loginEdit->text(), ui->passwordEdit->text(), &user, &error)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
        return;
    }

    m_loggedInUserId = user.id();
    emit loginSucceeded(m_loggedInUserId);
    accept();
}

void LoginDialog::onRegisterClicked()
{
    emit registerRequested();
}

RegisterDialog::RegisterDialog(DataManager *dataManager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
    , m_dataManager(dataManager)
{
    ui->setupUi(this);

    connect(ui->registerButton, &QPushButton::clicked, this, &RegisterDialog::onRegisterClicked);
    connect(ui->cancelButton, &QPushButton::clicked, this, &RegisterDialog::reject);
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

User RegisterDialog::buildUser() const
{
    User user;
    user.setLogin(ui->loginEdit->text());
    user.setFullName(ui->fullNameEdit->text());
    user.setRole(UserRole::Visitor);
    return user;
}

void RegisterDialog::onRegisterClicked()
{
    if (ui->passwordEdit->text() != ui->confirmEdit->text()) {
        QMessageBox::warning(this, QStringLiteral("Помилка"),
                             QStringLiteral("Паролі не збігаються."));
        return;
    }

    const User user = buildUser();
    QString error;
    if (!m_dataManager->registerUser(user, ui->passwordEdit->text(), &error)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
        return;
    }

    QMessageBox::information(this, QStringLiteral("Успіх"),
                             QStringLiteral("Користувача зареєстровано."));
    accept();
}

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    connect(ui->closeButton, &QPushButton::clicked, this, &AboutDialog::accept);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

GalleryInfoDialog::GalleryInfoDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::GalleryInfoDialog)
{
    ui->setupUi(this);
    connect(ui->closeButton, &QPushButton::clicked, this, &GalleryInfoDialog::accept);
}

GalleryInfoDialog::~GalleryInfoDialog()
{
    delete ui;
}

namespace {
constexpr double kVisitPriceUsd = 25.0;
}
