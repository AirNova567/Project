#include "database.h"

#include <QColor>
#include <QDate>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QTime>
#include <QVariant>

namespace {
QString sqlError(const QSqlQuery &query)
{
    return query.lastError().text();
}

constexpr int kUserIdBase = 100;
constexpr int kArtworkIdBase = 200;
constexpr int kBookingIdBase = 300;

QString allocateNextId(QSqlDatabase db, const QString &table, int idBase, QString *errorMessage)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT COALESCE(MAX(id), %1) + 1 FROM %2;")
                        .arg(idBase)
                        .arg(table))
        || !query.next()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка генерації ID: %1").arg(sqlError(query));
        }
        return QString();
    }
    return QString::number(query.value(0).toInt());
}

bool tableHasColumn(QSqlDatabase db, const QString &table, const QString &column)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1);").arg(table))) {
        return false;
    }
    while (query.next()) {
        if (query.value(1).toString() == column) {
            return true;
        }
    }
    return false;
}

bool tableIdIsInteger(QSqlDatabase db, const QString &table)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1);").arg(table))) {
        return false;
    }
    while (query.next()) {
        if (query.value(1).toString() == QStringLiteral("id")) {
            return query.value(2).toString().compare(QStringLiteral("INTEGER"),
                                                     Qt::CaseInsensitive) == 0;
        }
    }
    return false;
}

QString plainPasswordForLogin(const QString &login, const QString &storedSecret)
{
    const QString normalized = login.trimmed().toLower();
    if (normalized == QStringLiteral("admin")) {
        return QStringLiteral("admin123");
    }
    if (normalized == QStringLiteral("visitor")) {
        return QStringLiteral("visitor");
    }
    return storedSecret;
}
}

Database::Database() = default;

Database::~Database()
{
    close();
}

bool Database::open(const QString &dbPath, QString *errorMessage)
{
    if (m_db.isOpen()) {
        close();
    }

    m_dbPath = dbPath;
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                     QStringLiteral("artgallery_connection"));
    m_db.setDatabaseName(m_dbPath);

    if (!m_db.open()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не вдалося відкрити базу даних: %1")
                                .arg(m_db.lastError().text());
        }
        return false;
    }

    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON;"));
    return true;
}

void Database::close()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
    if (QSqlDatabase::contains(QStringLiteral("artgallery_connection"))) {
        QSqlDatabase::removeDatabase(QStringLiteral("artgallery_connection"));
    }
}

bool Database::isOpen() const
{
    return m_db.isOpen();
}

QString Database::databasePath() const
{
    return m_dbPath;
}

bool Database::createSchema(QString *errorMessage)
{
    return executeSchema(errorMessage);
}

bool Database::executeSchema(QString *errorMessage)
{
    QSqlQuery query(m_db);

    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS users ("
            "id INTEGER PRIMARY KEY,"
            "login TEXT NOT NULL UNIQUE COLLATE NOCASE,"
            "password TEXT NOT NULL,"
            "full_name TEXT NOT NULL,"
            "role TEXT NOT NULL CHECK(role IN ('admin', 'visitor'))"
            ");"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS artworks ("
            "id INTEGER PRIMARY KEY,"
            "title TEXT NOT NULL,"
            "artist TEXT NOT NULL,"
            "category TEXT NOT NULL,"
            "year INTEGER NOT NULL,"
            "price REAL NOT NULL,"
            "description TEXT,"
            "image_path TEXT"
            ");"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS settings ("
            "key TEXT PRIMARY KEY,"
            "value TEXT NOT NULL"
            ");"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_artworks_category ON artworks(category);"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_artworks_artist ON artworks(artist);"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_artworks_title ON artworks(title);"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS visit_bookings ("
            "id INTEGER PRIMARY KEY,"
            "user_id INTEGER NOT NULL,"
            "first_name TEXT NOT NULL,"
            "last_name TEXT NOT NULL,"
            "visit_date TEXT NOT NULL,"
            "visit_time TEXT NOT NULL,"
            "price_usd REAL NOT NULL,"
            "FOREIGN KEY(user_id) REFERENCES users(id)"
            ");"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_visit_bookings_date ON visit_bookings(visit_date);")
    };

    for (const QString &sql : statements) {
        if (!query.exec(sql)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Помилка створення схеми БД: %1")
                                    .arg(sqlError(query));
            }
            return false;
        }
    }

    return true;
}

bool Database::migrateLegacySchemaIfNeeded(QString *errorMessage)
{
    bool hasLegacyPassword = false;
    bool hasPasswordColumn = false;
    bool integerUserId = false;
    bool integerArtworkId = false;
  {
    QSqlQuery existsQuery(m_db);
    if (!existsQuery.exec(QStringLiteral(
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name='users';"))
        || !existsQuery.next()) {
        return true;
    }

    hasLegacyPassword = tableHasColumn(m_db, QStringLiteral("users"),
                                       QStringLiteral("password_hash"));
    hasPasswordColumn = tableHasColumn(m_db, QStringLiteral("users"),
                                      QStringLiteral("password"));
    integerUserId = tableIdIsInteger(m_db, QStringLiteral("users"));
    integerArtworkId = tableHasColumn(m_db, QStringLiteral("artworks"), QStringLiteral("id"))
                       && tableIdIsInteger(m_db, QStringLiteral("artworks"));
    if (hasPasswordColumn && integerUserId && integerArtworkId && !hasLegacyPassword) {
        return true;
    }
  }

    struct LegacyUserRow {
        QString oldId;
        QString login;
        QString secret;
        QString fullName;
        QString role;
    };
    struct LegacyBookingRow {
        QString firstName;
        QString lastName;
        QString oldUserId;
        QDate visitDate;
        QTime visitTime;
        double priceUsd = 25.0;
    };

    QVector<LegacyUserRow> legacyUsers;
    QVector<Artwork> legacyArtworks;
    QVector<LegacyBookingRow> legacyBookings;
    AppSettings legacySettings;

  {
    QSqlQuery usersQuery(m_db);
    const QString usersSql = hasLegacyPassword
                                 ? QStringLiteral(
                                       "SELECT id, login, password_hash, full_name, role FROM users;")
                                 : QStringLiteral(
                                       "SELECT id, login, password, full_name, role FROM users;");
    if (usersQuery.exec(usersSql)) {
        while (usersQuery.next()) {
            LegacyUserRow row;
            row.oldId = usersQuery.value(0).toString();
            row.login = usersQuery.value(1).toString();
            row.secret = usersQuery.value(2).toString();
            row.fullName = usersQuery.value(3).toString();
            row.role = usersQuery.value(4).toString();
            legacyUsers.append(row);
        }
    }

    QSqlQuery artworksQuery(m_db);
    if (artworksQuery.exec(QStringLiteral(
            "SELECT id, title, artist, category, year, price, description, image_path "
            "FROM artworks;"))) {
        while (artworksQuery.next()) {
            legacyArtworks.append(artworkFromQuery(artworksQuery));
        }
    }

    if (tableHasColumn(m_db, QStringLiteral("visit_bookings"), QStringLiteral("user_id"))) {
        QSqlQuery bookingsQuery(m_db);
        if (bookingsQuery.exec(QStringLiteral(
                "SELECT user_id, first_name, last_name, visit_date, visit_time, price_usd "
                "FROM visit_bookings;"))) {
            while (bookingsQuery.next()) {
                LegacyBookingRow row;
                row.oldUserId = bookingsQuery.value(0).toString();
                row.firstName = bookingsQuery.value(1).toString();
                row.lastName = bookingsQuery.value(2).toString();
                row.visitDate = QDate::fromString(bookingsQuery.value(3).toString(), Qt::ISODate);
                row.visitTime = QTime::fromString(bookingsQuery.value(4).toString(),
                                                  QStringLiteral("HH:mm"));
                row.priceUsd = bookingsQuery.value(5).toDouble();
                legacyBookings.append(row);
            }
        }
    }

    loadSettings(&legacySettings, nullptr);
  }

  {
    QSqlQuery pragmaQuery(m_db);
    pragmaQuery.exec(QStringLiteral("PRAGMA foreign_keys = OFF;"));
  }

    if (!m_db.transaction()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не вдалося розпочати міграцію БД.");
        }
        return false;
    }

    QSqlQuery dropQuery(m_db);
    dropQuery.setForwardOnly(true);
    const QStringList dropStatements = {
        QStringLiteral("DROP TABLE IF EXISTS visit_bookings;"),
        QStringLiteral("DROP TABLE IF EXISTS artworks;"),
        QStringLiteral("DROP TABLE IF EXISTS settings;"),
        QStringLiteral("DROP TABLE IF EXISTS users;"),
    };
    for (const QString &sql : dropStatements) {
        dropQuery.finish();
        if (!dropQuery.exec(sql)) {
            m_db.rollback();
            if (errorMessage) {
                *errorMessage = QStringLiteral("Помилка міграції БД: %1").arg(sqlError(dropQuery));
            }
            return false;
        }
    }

    if (!executeSchema(errorMessage)) {
        m_db.rollback();
        return false;
    }

    QHash<QString, int> userIdMap;
    for (const LegacyUserRow &row : std::as_const(legacyUsers)) {
        const QString newId = allocateUserId(errorMessage);
        if (newId.isEmpty()) {
            m_db.rollback();
            return false;
        }

        User user(newId,
                  row.login,
                  plainPasswordForLogin(row.login, row.secret),
                  row.fullName,
                  roleFromDb(row.role));
        if (!insertUser(user, errorMessage)) {
            m_db.rollback();
            return false;
        }
        userIdMap.insert(row.oldId, newId.toInt());
    }

    for (Artwork &artwork : legacyArtworks) {
        const QString newId = allocateArtworkId(errorMessage);
        if (newId.isEmpty()) {
            m_db.rollback();
            return false;
        }
        artwork.setId(newId);
        if (!insertArtwork(artwork, errorMessage)) {
            m_db.rollback();
            return false;
        }
    }

    for (const LegacyBookingRow &row : std::as_const(legacyBookings)) {
        const int mappedUserId = userIdMap.value(row.oldUserId, 0);
        if (mappedUserId <= 0) {
            continue;
        }

        VisitBooking booking;
        const QString newId = allocateVisitBookingId(errorMessage);
        if (newId.isEmpty()) {
            m_db.rollback();
            return false;
        }
        booking.setId(newId);
        booking.setUserId(QString::number(mappedUserId));
        booking.setFirstName(row.firstName);
        booking.setLastName(row.lastName);
        booking.setVisitDate(row.visitDate);
        booking.setVisitTime(row.visitTime);
        booking.setPriceUsd(row.priceUsd);
        if (!insertVisitBooking(booking, errorMessage)) {
            m_db.rollback();
            return false;
        }
    }

    if (!writeSettingsRows(legacySettings, errorMessage)) {
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не вдалося завершити міграцію БД.");
        }
        return false;
    }

    {
        QSqlQuery pragmaOn(m_db);
        pragmaOn.exec(QStringLiteral("PRAGMA foreign_keys = ON;"));
    }

    return true;
}

QString Database::allocateUserId(QString *errorMessage) const
{
    return allocateNextId(m_db, QStringLiteral("users"), kUserIdBase, errorMessage);
}

QString Database::allocateArtworkId(QString *errorMessage) const
{
    return allocateNextId(m_db, QStringLiteral("artworks"), kArtworkIdBase, errorMessage);
}

QString Database::allocateVisitBookingId(QString *errorMessage) const
{
    return allocateNextId(m_db, QStringLiteral("visit_bookings"), kBookingIdBase, errorMessage);
}

bool Database::migrateFromJson(const QString &dataDir, QString *errorMessage)
{
    const QString usersPath = dataDir + QStringLiteral("/users.json");
    const QString artworksPath = dataDir + QStringLiteral("/artworks.json");
    const QString settingsPath = dataDir + QStringLiteral("/settings.json");

    if (!QFile::exists(usersPath) && !QFile::exists(artworksPath)) {
        return true;
    }

    QSqlQuery countQuery(m_db);
    if (!countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM users"))
        || !countQuery.next() || countQuery.value(0).toInt() > 0) {
        return true;
    }

    if (QFile::exists(usersPath)) {
        QFile file(usersPath);
        if (!file.open(QIODevice::ReadOnly)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Не вдалося прочитати users.json для міграції.");
            }
            return false;
        }

        const QJsonArray array = QJsonDocument::fromJson(file.readAll()).array();
        for (const QJsonValue &value : array) {
            User user = User::fromJson(value.toObject());
            const QString newId = allocateUserId(errorMessage);
            if (newId.isEmpty()) {
                return false;
            }
            user.setId(newId);
            if (!insertUser(user, errorMessage)) {
                return false;
            }
        }
    }

    if (QFile::exists(artworksPath)) {
        QFile file(artworksPath);
        if (!file.open(QIODevice::ReadOnly)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Не вдалося прочитати artworks.json для міграції.");
            }
            return false;
        }

        const QJsonArray array = QJsonDocument::fromJson(file.readAll()).array();
        for (const QJsonValue &value : array) {
            Artwork artwork = Artwork::fromJson(value.toObject());
            const QString newId = allocateArtworkId(errorMessage);
            if (newId.isEmpty()) {
                return false;
            }
            artwork.setId(newId);
            if (!insertArtwork(artwork, errorMessage)) {
                return false;
            }
        }
    }

    if (QFile::exists(settingsPath)) {
        QFile file(settingsPath);
        if (file.open(QIODevice::ReadOnly)) {
            const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
            AppSettings settings;
            settings.accentColor = QColor(obj.value(QStringLiteral("accentColor")).toString());
            settings.fontSize = obj.value(QStringLiteral("fontSize")).toInt(12);
            settings.showToolBar = obj.value(QStringLiteral("showToolBar")).toBool(true);
            settings.sortField = obj.value(QStringLiteral("sortField")).toString(QStringLiteral("title"));
            settings.sortAscending = obj.value(QStringLiteral("sortAscending")).toBool(true);
            settings.darkMode = obj.value(QStringLiteral("darkMode")).toBool(false);
            if (!saveSettings(settings, errorMessage)) {
                return false;
            }
        }
    }

    return true;
}

QString Database::roleToDb(UserRole role)
{
    return role == UserRole::Administrator ? QStringLiteral("admin")
                                           : QStringLiteral("visitor");
}

UserRole Database::roleFromDb(const QString &value)
{
    return value == QStringLiteral("admin") ? UserRole::Administrator : UserRole::Visitor;
}

User Database::userFromQuery(const QSqlQuery &query) const
{
    User user;
    user.setId(QString::number(query.value(QStringLiteral("id")).toInt()));
    user.setLogin(query.value(QStringLiteral("login")).toString());
    user.setPassword(query.value(QStringLiteral("password")).toString());
    user.setFullName(query.value(QStringLiteral("full_name")).toString());
    user.setRole(roleFromDb(query.value(QStringLiteral("role")).toString()));
    return user;
}

Artwork Database::artworkFromQuery(const QSqlQuery &query) const
{
    Artwork artwork;
    artwork.setId(QString::number(query.value(QStringLiteral("id")).toInt()));
    artwork.setTitle(query.value(QStringLiteral("title")).toString());
    artwork.setArtist(query.value(QStringLiteral("artist")).toString());
    artwork.setCategory(query.value(QStringLiteral("category")).toString());
    artwork.setYear(query.value(QStringLiteral("year")).toInt());
    artwork.setPrice(query.value(QStringLiteral("price")).toDouble());
    artwork.setDescription(query.value(QStringLiteral("description")).toString());
    artwork.setImagePath(query.value(QStringLiteral("image_path")).toString());
    return artwork;
}

QVector<User> Database::loadUsers(QString *errorMessage) const
{
    QVector<User> users;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral(
            "SELECT id, login, password, full_name, role FROM users ORDER BY login;"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка завантаження користувачів: %1")
                                .arg(sqlError(query));
        }
        return users;
    }

    while (query.next()) {
        users.append(userFromQuery(query));
    }
    return users;
}

QVector<Artwork> Database::loadArtworks(QString *errorMessage) const
{
    QVector<Artwork> artworks;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral(
            "SELECT id, title, artist, category, year, price, description, image_path "
            "FROM artworks ORDER BY title;"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка завантаження експонатів: %1")
                                .arg(sqlError(query));
        }
        return artworks;
    }

    while (query.next()) {
        artworks.append(artworkFromQuery(query));
    }
    return artworks;
}

bool Database::loadSettings(AppSettings *settings, QString *errorMessage) const
{
    if (!settings) {
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT key, value FROM settings;"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка завантаження налаштувань: %1")
                                .arg(sqlError(query));
        }
        return false;
    }

    while (query.next()) {
        const QString key = query.value(0).toString();
        const QString value = query.value(1).toString();
        if (key == QStringLiteral("accentColor")) {
            settings->accentColor = QColor(value);
        } else if (key == QStringLiteral("fontSize")) {
            settings->fontSize = value.toInt();
        } else if (key == QStringLiteral("showToolBar")) {
            settings->showToolBar = value == QStringLiteral("1");
        } else if (key == QStringLiteral("sortField")) {
            settings->sortField = value;
        } else if (key == QStringLiteral("sortAscending")) {
            settings->sortAscending = value == QStringLiteral("1");
        } else if (key == QStringLiteral("darkMode")) {
            settings->darkMode = (value == QStringLiteral("1")
                                  || value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0);
        }
    }
    return true;
}

bool Database::insertUser(const User &user, QString *errorMessage)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO users (id, login, password, full_name, role) "
        "VALUES (:id, :login, :password, :full_name, :role);"));
    query.bindValue(QStringLiteral(":id"), user.id().toInt());
    query.bindValue(QStringLiteral(":login"), user.login());
    query.bindValue(QStringLiteral(":password"), user.password());
    query.bindValue(QStringLiteral(":full_name"), user.fullName());
    query.bindValue(QStringLiteral(":role"), roleToDb(user.role()));

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка додавання користувача: %1")
                                .arg(sqlError(query));
        }
        return false;
    }
    return true;
}

bool Database::updateUser(const User &user, QString *errorMessage)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE users SET login=:login, password=:password, "
        "full_name=:full_name, role=:role WHERE id=:id;"));
    query.bindValue(QStringLiteral(":id"), user.id().toInt());
    query.bindValue(QStringLiteral(":login"), user.login());
    query.bindValue(QStringLiteral(":password"), user.password());
    query.bindValue(QStringLiteral(":full_name"), user.fullName());
    query.bindValue(QStringLiteral(":role"), roleToDb(user.role()));

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка оновлення користувача: %1")
                                .arg(sqlError(query));
        }
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool Database::deleteUser(const QString &id, QString *errorMessage)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM users WHERE id=:id;"));
    query.bindValue(QStringLiteral(":id"), id.toInt());

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка видалення користувача: %1")
                                .arg(sqlError(query));
        }
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool Database::userLoginExists(const QString &login, const QString &excludeId,
                               QString *errorMessage) const
{
    QSqlQuery query(m_db);
    if (excludeId.isEmpty()) {
        query.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM users WHERE login = :login COLLATE NOCASE;"));
        query.bindValue(QStringLiteral(":login"), login);
    } else {
        query.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM users WHERE login = :login COLLATE NOCASE AND id != :id;"));
        query.bindValue(QStringLiteral(":login"), login);
        query.bindValue(QStringLiteral(":id"), excludeId.toInt());
    }

    if (!query.exec() || !query.next()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка перевірки логіна: %1").arg(sqlError(query));
        }
        return false;
    }
    return query.value(0).toInt() > 0;
}

int Database::countAdmins(QString *errorMessage) const
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM users WHERE role='admin';"))
        || !query.next()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка підрахунку адміністраторів: %1")
                                .arg(sqlError(query));
        }
        return 0;
    }
    return query.value(0).toInt();
}

bool Database::insertArtwork(const Artwork &artwork, QString *errorMessage)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO artworks (id, title, artist, category, year, price, description, image_path) "
        "VALUES (:id, :title, :artist, :category, :year, :price, :description, :image_path);"));
    query.bindValue(QStringLiteral(":id"), artwork.id().toInt());
    query.bindValue(QStringLiteral(":title"), artwork.title());
    query.bindValue(QStringLiteral(":artist"), artwork.artist());
    query.bindValue(QStringLiteral(":category"), artwork.category());
    query.bindValue(QStringLiteral(":year"), artwork.year());
    query.bindValue(QStringLiteral(":price"), artwork.price());
    query.bindValue(QStringLiteral(":description"), artwork.description());
    query.bindValue(QStringLiteral(":image_path"), artwork.imagePath());

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка додавання експонату: %1").arg(sqlError(query));
        }
        return false;
    }
    return true;
}

bool Database::updateArtwork(const Artwork &artwork, QString *errorMessage)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE artworks SET title=:title, artist=:artist, category=:category, year=:year, "
        "price=:price, description=:description, image_path=:image_path WHERE id=:id;"));
    query.bindValue(QStringLiteral(":id"), artwork.id().toInt());
    query.bindValue(QStringLiteral(":title"), artwork.title());
    query.bindValue(QStringLiteral(":artist"), artwork.artist());
    query.bindValue(QStringLiteral(":category"), artwork.category());
    query.bindValue(QStringLiteral(":year"), artwork.year());
    query.bindValue(QStringLiteral(":price"), artwork.price());
    query.bindValue(QStringLiteral(":description"), artwork.description());
    query.bindValue(QStringLiteral(":image_path"), artwork.imagePath());

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка оновлення експонату: %1").arg(sqlError(query));
        }
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool Database::deleteArtwork(const QString &id, QString *errorMessage)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM artworks WHERE id=:id;"));
    query.bindValue(QStringLiteral(":id"), id.toInt());

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка видалення експонату: %1").arg(sqlError(query));
        }
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool Database::artworkTitleExists(const QString &title, const QString &artist,
                                  const QString &excludeId, QString *errorMessage) const
{
    QSqlQuery query(m_db);
    if (excludeId.isEmpty()) {
        query.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM artworks "
            "WHERE LOWER(title)=LOWER(:title) AND LOWER(artist)=LOWER(:artist);"));
    } else {
        query.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM artworks "
            "WHERE LOWER(title)=LOWER(:title) AND LOWER(artist)=LOWER(:artist) AND id!=:id;"));
        query.bindValue(QStringLiteral(":id"), excludeId.toInt());
    }
    query.bindValue(QStringLiteral(":title"), title);
    query.bindValue(QStringLiteral(":artist"), artist);

    if (!query.exec() || !query.next()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка перевірки експонату: %1").arg(sqlError(query));
        }
        return false;
    }
    return query.value(0).toInt() > 0;
}

bool Database::writeSettingsRows(const AppSettings &settings, QString *errorMessage)
{
    QSqlQuery query(m_db);
    const QList<QPair<QString, QString>> values = {
        {QStringLiteral("accentColor"), settings.accentColor.name(QColor::HexArgb)},
        {QStringLiteral("fontSize"), QString::number(settings.fontSize)},
        {QStringLiteral("showToolBar"), settings.showToolBar ? QStringLiteral("1")
                                                             : QStringLiteral("0")},
        {QStringLiteral("sortField"), settings.sortField},
        {QStringLiteral("sortAscending"), settings.sortAscending ? QStringLiteral("1")
                                                                 : QStringLiteral("0")},
        {QStringLiteral("darkMode"), settings.darkMode ? QStringLiteral("1") : QStringLiteral("0")}
    };

    for (const auto &pair : values) {
        query.prepare(QStringLiteral(
            "INSERT INTO settings (key, value) VALUES (:key, :value) "
            "ON CONFLICT(key) DO UPDATE SET value=excluded.value;"));
        query.bindValue(QStringLiteral(":key"), pair.first);
        query.bindValue(QStringLiteral(":value"), pair.second);
        if (!query.exec()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Помилка збереження налаштувань: %1")
                                    .arg(sqlError(query));
            }
            return false;
        }
    }
    return true;
}

bool Database::saveSettings(const AppSettings &settings, QString *errorMessage)
{
    if (!m_db.transaction()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не вдалося розпочати транзакцію.");
        }
        return false;
    }

    if (!writeSettingsRows(settings, errorMessage)) {
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не вдалося зафіксувати налаштування.");
        }
        return false;
    }
    return true;
}

VisitBooking Database::visitBookingFromQuery(const QSqlQuery &query) const
{
    VisitBooking booking;
    booking.setId(QString::number(query.value(QStringLiteral("id")).toInt()));
    booking.setUserId(QString::number(query.value(QStringLiteral("user_id")).toInt()));
    booking.setFirstName(query.value(QStringLiteral("first_name")).toString());
    booking.setLastName(query.value(QStringLiteral("last_name")).toString());
    booking.setVisitDate(QDate::fromString(query.value(QStringLiteral("visit_date")).toString(),
                                           Qt::ISODate));
    booking.setVisitTime(QTime::fromString(query.value(QStringLiteral("visit_time")).toString(),
                                           QStringLiteral("HH:mm")));
    booking.setPriceUsd(query.value(QStringLiteral("price_usd")).toDouble());
    booking.setUserLogin(query.value(QStringLiteral("login")).toString());
    return booking;
}

QVector<VisitBooking> Database::loadVisitBookings(QString *errorMessage) const
{
    QVector<VisitBooking> bookings;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral(
            "SELECT b.id, b.user_id, b.first_name, b.last_name, b.visit_date, b.visit_time, "
            "b.price_usd, u.login "
            "FROM visit_bookings b "
            "LEFT JOIN users u ON u.id = b.user_id "
            "ORDER BY b.visit_date, b.visit_time;"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка завантаження записів: %1").arg(sqlError(query));
        }
        return bookings;
    }
    while (query.next()) {
        bookings.append(visitBookingFromQuery(query));
    }
    return bookings;
}

bool Database::insertVisitBooking(const VisitBooking &booking, QString *errorMessage)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO visit_bookings (id, user_id, first_name, last_name, visit_date, visit_time, price_usd) "
        "VALUES (:id, :user_id, :first_name, :last_name, :visit_date, :visit_time, :price_usd);"));
    query.bindValue(QStringLiteral(":id"), booking.id().toInt());
    query.bindValue(QStringLiteral(":user_id"), booking.userId().toInt());
    query.bindValue(QStringLiteral(":first_name"), booking.firstName());
    query.bindValue(QStringLiteral(":last_name"), booking.lastName());
    query.bindValue(QStringLiteral(":visit_date"), booking.visitDate().toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":visit_time"), booking.visitTime().toString(QStringLiteral("HH:mm")));
    query.bindValue(QStringLiteral(":price_usd"), booking.priceUsd());

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка створення запису: %1").arg(sqlError(query));
        }
        return false;
    }
    return true;
}

bool Database::deleteVisitBooking(const QString &id, QString *errorMessage)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM visit_bookings WHERE id=:id;"));
    query.bindValue(QStringLiteral(":id"), id.toInt());

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Помилка видалення запису: %1").arg(sqlError(query));
        }
        return false;
    }
    return query.numRowsAffected() > 0;
}

