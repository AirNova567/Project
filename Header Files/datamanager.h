#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <QColor>
#include <QObject>
#include <QVector>
#include <optional>

#include "database.h"
#include "models.h"

class DataManager : public QObject
{
    Q_OBJECT

public:
    explicit DataManager(QObject *parent = nullptr);
    ~DataManager() override;

    bool initialize(QString *errorMessage = nullptr);

    QVector<Artwork> artworks() const;
    QVector<User> users() const;
    AppSettings settings() const;

    std::optional<Artwork> artworkById(const QString &id) const;
    std::optional<User> userByLogin(const QString &login) const;

    bool addArtwork(const Artwork &artwork, QString *errorMessage = nullptr);
    bool updateArtwork(const Artwork &artwork, QString *errorMessage = nullptr);
    bool removeArtwork(const QString &id, QString *errorMessage = nullptr);

    bool registerUser(const User &user, const QString &plainPassword, QString *errorMessage = nullptr);
    bool updateUser(const User &user, QString *errorMessage = nullptr);
    bool removeUser(const QString &id, QString *errorMessage = nullptr);

    bool authenticate(const QString &login, const QString &password, User *user,
                      QString *errorMessage = nullptr);

    bool saveSettings(const AppSettings &settings, QString *errorMessage = nullptr);
    bool exportArtworksToCsv(const QString &filePath, QString *errorMessage = nullptr) const;
    bool importArtworksFromCsv(const QString &filePath, QString *errorMessage = nullptr);

    QString dataDirectory() const;
    QString databasePath() const;
    QString imagesDirectory() const;
    QString copyImageToStorage(const QString &sourcePath, QString *errorMessage = nullptr);

    QVector<VisitBooking> visitBookings() const;
    bool addVisitBooking(const VisitBooking &booking, QString *errorMessage = nullptr);
    bool removeVisitBooking(const QString &id, QString *errorMessage = nullptr);

signals:
    void artworksChanged();
    void usersChanged();
    void settingsChanged();
    void bookingsChanged();

private:
    bool reloadArtworks(QString *errorMessage = nullptr);
    bool reloadUsers(QString *errorMessage = nullptr);
    bool reloadSettings(QString *errorMessage = nullptr);
    bool reloadVisitBookings(QString *errorMessage = nullptr);
    void ensureDefaultData();

    Database m_database;
    QVector<Artwork> m_artworks;
    QVector<User> m_users;
    AppSettings m_settings;
    QString m_dataDir;
    QVector<VisitBooking> m_visitBookings;
};


#endif
