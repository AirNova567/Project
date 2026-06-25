#ifndef BOOKINGDIALOGS_H
#define BOOKINGDIALOGS_H

#include <QDialog>

#include "models.h"

class DataManager;

namespace Ui {
class BookingDialog;
class BookingsManagementDialog;
}

class BookingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BookingDialog(DataManager *dataManager, const User &currentUser, QWidget *parent = nullptr);
    ~BookingDialog() override;

private slots:
    void onDateChanged();
    void onBook();

private:
    void updateTimeSlots();
    void splitFullName(const QString &fullName, QString *firstName, QString *lastName) const;

    Ui::BookingDialog *ui;
    DataManager *m_dataManager;
    User m_currentUser;
};

class BookingsManagementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BookingsManagementDialog(DataManager *dataManager, QWidget *parent = nullptr);
    ~BookingsManagementDialog() override;

private slots:
    void refreshTable();
    void onDeleteBooking();

private:
    Ui::BookingsManagementDialog *ui;
    DataManager *m_dataManager;
};

#endif
