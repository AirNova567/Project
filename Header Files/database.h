#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QVector>

#include "models.h"

class Database
{
public:
    Database();
    ~Database();

    bool open(const QString &dbPath, QString *errorMessage = nullptr);
    void close();
    bool isOpen() const;
    QString databasePath() const;

    bool createSchema(QString *errorMessage = nullptr);
    bool migrateLegacySchemaIfNeeded(QString *errorMessage = nullptr);
    bool migrateFromJson(const QString &dataDir, QString *errorMessage = nullptr);

    QString allocateUserId(QString *errorMessage = nullptr) const;
    QString allocateArtworkId(QString *errorMessage = nullptr) const;
    QString allocateVisitBookingId(QString *errorMessage = nullptr) const;

    QVector<User> loadUsers(QString *errorMessage = nullptr) const;
    QVector<Artwork> loadArtworks(QString *errorMessage = nullptr) const;
    bool loadSettings(AppSettings *settings, QString *errorMessage = nullptr) const;

    bool insertUser(const User &user, QString *errorMessage = nullptr);
    bool updateUser(const User &user, QString *errorMessage = nullptr);
    bool deleteUser(const QString &id, QString *errorMessage = nullptr);
    bool userLoginExists(const QString &login, const QString &excludeId = QString(),
                         QString *errorMessage = nullptr) const;
    int countAdmins(QString *errorMessage = nullptr) const;

    bool insertArtwork(const Artwork &artwork, QString *errorMessage = nullptr);
    bool updateArtwork(const Artwork &artwork, QString *errorMessage = nullptr);
    bool deleteArtwork(const QString &id, QString *errorMessage = nullptr);
    bool artworkTitleExists(const QString &title, const QString &artist,
                            const QString &excludeId = QString(),
                            QString *errorMessage = nullptr) const;

    bool saveSettings(const AppSettings &settings, QString *errorMessage = nullptr);

    QVector<VisitBooking> loadVisitBookings(QString *errorMessage = nullptr) const;
    bool insertVisitBooking(const VisitBooking &booking, QString *errorMessage = nullptr);
    bool deleteVisitBooking(const QString &id, QString *errorMessage = nullptr);

private:
    bool executeSchema(QString *errorMessage);
    bool writeSettingsRows(const AppSettings &settings, QString *errorMessage);
    User userFromQuery(const class QSqlQuery &query) const;
    Artwork artworkFromQuery(const class QSqlQuery &query) const;
    VisitBooking visitBookingFromQuery(const class QSqlQuery &query) const;
    static QString roleToDb(UserRole role);
    static UserRole roleFromDb(const QString &value);

    QSqlDatabase m_db;
    QString m_dbPath;
};


#endif
