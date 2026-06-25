#include "bookingdialogs.h"
#include "datamanager.h"

#include "ui_bookingdialog.h"
#include "ui_bookingsmanagementdialog.h"

#include <QDate>
#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QTime>
namespace {
constexpr double kVisitPriceUsd = 25.0;
}

BookingDialog::BookingDialog(DataManager *dataManager, const User &currentUser, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::BookingDialog)
    , m_dataManager(dataManager)
    , m_currentUser(currentUser)
{
    ui->setupUi(this);

    ui->dateEdit->setDate(QDate::currentDate());
    ui->dateEdit->setMinimumDate(QDate::currentDate());
    ui->priceLabel->setText(
        QStringLiteral("<b>Вартість візиту:</b> $%1 USD").arg(kVisitPriceUsd, 0, 'f', 2));

    QString firstName;
    QString lastName;
    splitFullName(m_currentUser.fullName(), &firstName, &lastName);
    ui->firstNameEdit->setText(firstName);
    ui->lastNameEdit->setText(lastName);

    updateTimeSlots();

    connect(ui->dateEdit, &QDateEdit::dateChanged, this, &BookingDialog::onDateChanged);
    connect(ui->bookButton, &QPushButton::clicked, this, &BookingDialog::onBook);
    connect(ui->cancelButton, &QPushButton::clicked, this, &BookingDialog::reject);
}

BookingDialog::~BookingDialog()
{
    delete ui;
}

void BookingDialog::splitFullName(const QString &fullName, QString *firstName, QString *lastName) const
{
    const QStringList parts = fullName.trimmed().split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return;
    }
    if (parts.size() == 1) {
        *firstName = parts.first();
        return;
    }
    *firstName = parts.first();
    *lastName = parts.mid(1).join(' ');
}

void BookingDialog::updateTimeSlots()
{
    ui->timeCombo->clear();
    const QDate date = ui->dateEdit->date();
    if (date.dayOfWeek() == 7) {
        ui->timeCombo->addItem(QStringLiteral("Неділя — вихідний"));
        ui->timeCombo->setEnabled(false);
        return;
    }
    ui->timeCombo->setEnabled(true);

    const int startHour = (date.dayOfWeek() == 6) ? 11 : 10;
    const int endHour = (date.dayOfWeek() == 6) ? 17 : 19;
    for (int hour = startHour; hour < endHour; ++hour) {
        const QTime time(hour, 0);
        ui->timeCombo->addItem(time.toString(QStringLiteral("HH:mm")), time);
    }
}

void BookingDialog::onDateChanged()
{
    updateTimeSlots();
}

void BookingDialog::onBook()
{
    if (ui->timeCombo->count() == 0 || !ui->timeCombo->isEnabled()) {
        QMessageBox::warning(this, QStringLiteral("Помилка"),
                             QStringLiteral("Оберіть інший день — галерея зачинена."));
        return;
    }

    VisitBooking booking;
    booking.setFirstName(ui->firstNameEdit->text().trimmed());
    booking.setLastName(ui->lastNameEdit->text().trimmed());
    booking.setVisitDate(ui->dateEdit->date());
    booking.setVisitTime(ui->timeCombo->currentData().toTime());
    booking.setPriceUsd(kVisitPriceUsd);
    booking.setUserId(m_currentUser.id());

    QString error;
    if (!m_dataManager->addVisitBooking(booking, &error)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
        return;
    }

    QMessageBox::information(this, QStringLiteral("Успіх"),
                             QStringLiteral("Вас записано на %1 о %2. Вартість: $%3 USD")
                                 .arg(booking.visitDate().toString(QStringLiteral("dd.MM.yyyy")),
                                      booking.visitTime().toString(QStringLiteral("HH:mm")),
                                      QString::number(booking.priceUsd(), 'f', 2)));
    accept();
}

BookingsManagementDialog::BookingsManagementDialog(DataManager *dataManager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::BookingsManagementDialog)
    , m_dataManager(dataManager)
{
    ui->setupUi(this);

    ui->bookingsTable->horizontalHeader()->setStretchLastSection(true);
    ui->bookingsTable->verticalHeader()->setVisible(false);
    ui->bookingsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->bookingsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->bookingsTable->setAlternatingRowColors(true);

    connect(ui->refreshButton, &QPushButton::clicked, this, &BookingsManagementDialog::refreshTable);
    connect(ui->deleteButton, &QPushButton::clicked, this, &BookingsManagementDialog::onDeleteBooking);
    connect(ui->closeButton, &QPushButton::clicked, this, &BookingsManagementDialog::accept);
    connect(m_dataManager, &DataManager::bookingsChanged, this, &BookingsManagementDialog::refreshTable);

    refreshTable();
}

BookingsManagementDialog::~BookingsManagementDialog()
{
    delete ui;
}

void BookingsManagementDialog::refreshTable()
{
    const QVector<VisitBooking> bookings = m_dataManager->visitBookings();
    ui->bookingsTable->setRowCount(bookings.size());
    for (int i = 0; i < bookings.size(); ++i) {
        const VisitBooking &booking = bookings[i];
        ui->bookingsTable->setItem(i, 0, new QTableWidgetItem(booking.firstName()));
        ui->bookingsTable->setItem(i, 1, new QTableWidgetItem(booking.lastName()));
        ui->bookingsTable->setItem(i, 2,
                                   new QTableWidgetItem(
                                       booking.visitDate().toString(QStringLiteral("dd.MM.yyyy"))));
        ui->bookingsTable->setItem(i, 3,
                                   new QTableWidgetItem(
                                       booking.visitTime().toString(QStringLiteral("HH:mm"))));
        ui->bookingsTable->setItem(i, 4,
                                   new QTableWidgetItem(QString::number(booking.priceUsd(), 'f', 2)));
        ui->bookingsTable->setItem(i, 5, new QTableWidgetItem(booking.userLogin()));
        ui->bookingsTable->setItem(i, 6, new QTableWidgetItem(booking.id()));
    }
    ui->bookingsTable->hideColumn(6);
}

void BookingsManagementDialog::onDeleteBooking()
{
    const int row = ui->bookingsTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("Інформація"),
                                 QStringLiteral("Оберіть запис для видалення."));
        return;
    }
    const QString id = ui->bookingsTable->item(row, 6)->text();
    if (QMessageBox::question(this, QStringLiteral("Підтвердження"),
                              QStringLiteral("Видалити цей запис?")) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_dataManager->removeVisitBooking(id, &error)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
    }
}
