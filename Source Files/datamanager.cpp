#include "datamanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace {
QString detectProjectRoot()
{
    auto hasProjectMarkers = [](const QDir &dir) {
        return dir.exists(QStringLiteral("CMakeLists.txt"));
    };

    QDir dir(QDir::currentPath());
    for (int i = 0; i < 8; ++i) {
        if (hasProjectMarkers(dir)) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }

    QDir appDir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 10; ++i) {
        if (hasProjectMarkers(appDir)) {
            return appDir.absolutePath();
        }
        if (!appDir.cdUp()) {
            break;
        }
    }
    return QDir::currentPath();
}
}

DataManager::DataManager(QObject *parent)
    : QObject(parent)
{
}

DataManager::~DataManager()
{
    m_database.close();
}

bool DataManager::initialize(QString *errorMessage)
{
    m_dataDir = detectProjectRoot();
    if (m_dataDir.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не вдалося визначити каталог даних.");
        }
        return false;
    }

    QDir dir(m_dataDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не вдалося створити каталог даних.");
        }
        return false;
    }

    QDir imagesDir(imagesDirectory());
    if (!imagesDir.exists() && !imagesDir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не вдалося створити каталог зображень.");
        }
        return false;
    }

    const QString dbPath = m_dataDir + QStringLiteral("/artgallery.db");
    const QString oldDbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                              + QStringLiteral("/artgallery.db");
    if (!QFile::exists(dbPath) && QFile::exists(oldDbPath)) {
        QFile::copy(oldDbPath, dbPath);
    }
    if (!m_database.open(dbPath, errorMessage)) {
        return false;
    }
    if (!m_database.migrateLegacySchemaIfNeeded(errorMessage)) {
        return false;
    }
    if (!m_database.createSchema(errorMessage)) {
        return false;
    }
    if (!m_database.migrateFromJson(m_dataDir, errorMessage)) {
        return false;
    }

    if (!reloadUsers(errorMessage)) {
        return false;
    }
    if (!reloadArtworks(errorMessage)) {
        return false;
    }
    if (!reloadSettings(errorMessage)) {
        return false;
    }
    if (!reloadVisitBookings(errorMessage)) {
        return false;
    }

    ensureDefaultData();
    return true;
}

QVector<Artwork> DataManager::artworks() const
{
    return m_artworks;
}

QVector<User> DataManager::users() const
{
    return m_users;
}

AppSettings DataManager::settings() const
{
    return m_settings;
}

std::optional<Artwork> DataManager::artworkById(const QString &id) const
{
    for (const Artwork &artwork : m_artworks) {
        if (artwork.id() == id) {
            return artwork;
        }
    }
    return std::nullopt;
}

std::optional<User> DataManager::userByLogin(const QString &login) const
{
    for (const User &user : m_users) {
        if (user.login().compare(login, Qt::CaseInsensitive) == 0) {
            return user;
        }
    }
    return std::nullopt;
}

bool DataManager::addArtwork(const Artwork &artwork, QString *errorMessage)
{
    Artwork newArtwork = artwork;
    if (newArtwork.id().isEmpty()) {
        newArtwork.setId(m_database.allocateArtworkId(errorMessage));
        if (newArtwork.id().isEmpty()) {
            return false;
        }
    }

    QString validationError;
    if (!newArtwork.validate(&validationError)) {
        if (errorMessage) {
            *errorMessage = validationError;
        }
        return false;
    }

    if (m_database.artworkTitleExists(newArtwork.title(), newArtwork.artist(), newArtwork.id(), errorMessage)) {
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = QStringLiteral("Експонат з такою назвою вже існує.");
        }
        return false;
    }
    if (errorMessage && !errorMessage->isEmpty()) {
        return false;
    }

    if (!m_database.insertArtwork(newArtwork, errorMessage)) {
        return false;
    }

    if (!reloadArtworks(errorMessage)) {
        return false;
    }

    emit artworksChanged();
    return true;
}

bool DataManager::updateArtwork(const Artwork &artwork, QString *errorMessage)
{
    QString validationError;
    if (!artwork.validate(&validationError)) {
        if (errorMessage) {
            *errorMessage = validationError;
        }
        return false;
    }

    if (m_database.artworkTitleExists(artwork.title(), artwork.artist(), artwork.id(), errorMessage)) {
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = QStringLiteral("Експонат з такою назвою вже існує.");
        }
        return false;
    }
    if (errorMessage && !errorMessage->isEmpty()) {
        return false;
    }

    if (!m_database.updateArtwork(artwork, errorMessage)) {
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = QStringLiteral("Експонат не знайдено.");
        }
        return false;
    }

    if (!reloadArtworks(errorMessage)) {
        return false;
    }

    emit artworksChanged();
    return true;
}

bool DataManager::removeArtwork(const QString &id, QString *errorMessage)
{
    if (!m_database.deleteArtwork(id, errorMessage)) {
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = QStringLiteral("Експонат не знайдено.");
        }
        return false;
    }

    if (!reloadArtworks(errorMessage)) {
        return false;
    }

    emit artworksChanged();
    return true;
}

bool DataManager::registerUser(const User &user, const QString &plainPassword, QString *errorMessage)
{
    if (plainPassword.length() < 6) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Пароль повинен містити щонайменше 6 символів.");
        }
        return false;
    }

    QString validationError;
    if (!user.validate(&validationError)) {
        if (errorMessage) {
            *errorMessage = validationError;
        }
        return false;
    }

    QString loginCheckError;
    if (m_database.userLoginExists(user.login(), QString(), &loginCheckError)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Користувач з таким логіном вже існує.");
        }
        return false;
    }
    if (!loginCheckError.isEmpty()) {
        if (errorMessage) {
            *errorMessage = loginCheckError;
        }
        return false;
    }

    User newUser = user;
    newUser.setPassword(plainPassword);
    if (newUser.id().isEmpty()) {
        newUser.setId(m_database.allocateUserId(errorMessage));
        if (newUser.id().isEmpty()) {
            return false;
        }
    }

    if (!m_database.insertUser(newUser, errorMessage)) {
        return false;
    }

    if (!reloadUsers(errorMessage)) {
        return false;
    }

    emit usersChanged();
    return true;
}

bool DataManager::updateUser(const User &user, QString *errorMessage)
{
    QString validationError;
    if (!user.validate(&validationError)) {
        if (errorMessage) {
            *errorMessage = validationError;
        }
        return false;
    }

    if (!m_database.updateUser(user, errorMessage)) {
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = QStringLiteral("Користувача не знайдено.");
        }
        return false;
    }

    if (!reloadUsers(errorMessage)) {
        return false;
    }

    emit usersChanged();
    return true;
}

bool DataManager::removeUser(const QString &id, QString *errorMessage)
{
    const auto found = std::find_if(m_users.begin(), m_users.end(),
                                    [&](const User &u) { return u.id() == id; });
    if (found != m_users.end() && found->role() == UserRole::Administrator) {
        if (m_database.countAdmins(errorMessage) <= 1) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Неможливо видалити останнього адміністратора.");
            }
            return false;
        }
    }

    if (!m_database.deleteUser(id, errorMessage)) {
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = QStringLiteral("Користувача не знайдено.");
        }
        return false;
    }

    if (!reloadUsers(errorMessage)) {
        return false;
    }

    emit usersChanged();
    return true;
}

bool DataManager::authenticate(const QString &login, const QString &password, User *user,
                               QString *errorMessage)
{
    const auto found = userByLogin(login);
    if (!found.has_value()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Невірний логін або пароль.");
        }
        return false;
    }

    if (!found->checkPassword(password)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Невірний логін або пароль.");
        }
        return false;
    }

    if (user) {
        *user = found.value();
    }
    return true;
}

bool DataManager::saveSettings(const AppSettings &settings, QString *errorMessage)
{
    if (!m_database.saveSettings(settings, errorMessage)) {
        return false;
    }

    m_settings = settings;
    emit settingsChanged();
    return true;
}

bool DataManager::exportArtworksToCsv(const QString &filePath, QString *errorMessage) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не вдалося створити файл експорту.");
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << Artwork::csvHeader() << '\n';
    for (const Artwork &artwork : m_artworks) {
        stream << artwork.toCsvRow() << '\n';
    }
    return true;
}

bool DataManager::importArtworksFromCsv(const QString &filePath, QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не вдалося відкрити файл імпорту.");
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    if (stream.atEnd()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Файл імпорту порожній.");
        }
        return false;
    }

    const QString header = stream.readLine();
    if (header != Artwork::csvHeader()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Невірний формат CSV-файлу.");
        }
        return false;
    }

    int imported = 0;
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QStringList parts = line.split(',');
        if (parts.size() < 8) {
            continue;
        }

        auto unquote = [](const QString &value) {
            QString result = value;
            if (result.startsWith('"') && result.endsWith('"')) {
                result = result.mid(1, result.size() - 2);
                result.replace(QStringLiteral("\"\""), QStringLiteral("\""));
            }
            return result;
        };

        Artwork artwork;
        artwork.setId(unquote(parts[0]));
        artwork.setTitle(unquote(parts[1]));
        artwork.setArtist(unquote(parts[2]));
        artwork.setCategory(unquote(parts[3]));
        artwork.setYear(parts[4].toInt());
        artwork.setPrice(parts[5].toDouble());
        artwork.setDescription(unquote(parts[6]));
        artwork.setImagePath(unquote(parts[7]));

        QString rowError;
        const int parsedId = artwork.id().toInt();
        if (artwork.id().isEmpty() || parsedId < 201) {
            artwork.setId(m_database.allocateArtworkId(&rowError));
            if (artwork.id().isEmpty()) {
                continue;
            }
        }

        const auto existing = artworkById(artwork.id());
        if (existing.has_value()) {
            if (!m_database.updateArtwork(artwork, &rowError)) {
                continue;
            }
        } else {
            if (!m_database.insertArtwork(artwork, &rowError)) {
                continue;
            }
        }
        ++imported;
    }

    if (imported == 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Жодного запису не імпортовано.");
        }
        return false;
    }

    if (!reloadArtworks(errorMessage)) {
        return false;
    }

    emit artworksChanged();
    return true;
}

QString DataManager::dataDirectory() const
{
    return m_dataDir;
}

QString DataManager::databasePath() const
{
    return m_database.databasePath();
}

QString DataManager::imagesDirectory() const
{
    return m_dataDir + QStringLiteral("/images");
}

QString DataManager::copyImageToStorage(const QString &sourcePath, QString *errorMessage)
{
    if (sourcePath.isEmpty()) {
        return QString();
    }

    QFileInfo info(sourcePath);
    if (!info.exists()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Файл зображення не знайдено.");
        }
        return QString();
    }

    const QString destPath = imagesDirectory() + QStringLiteral("/")
                             + QUuid::createUuid().toString(QUuid::WithoutBraces)
                             + QStringLiteral(".") + info.suffix();
    if (QFile::exists(destPath)) {
        QFile::remove(destPath);
    }
    if (!QFile::copy(sourcePath, destPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Не вдалося скопіювати зображення.");
        }
        return QString();
    }
    return destPath;
}

bool DataManager::reloadArtworks(QString *errorMessage)
{
    m_artworks = m_database.loadArtworks(errorMessage);
    return errorMessage == nullptr || errorMessage->isEmpty();
}

bool DataManager::reloadUsers(QString *errorMessage)
{
    m_users = m_database.loadUsers(errorMessage);
    return errorMessage == nullptr || errorMessage->isEmpty();
}

bool DataManager::reloadSettings(QString *errorMessage)
{
    return m_database.loadSettings(&m_settings, errorMessage);
}

QVector<VisitBooking> DataManager::visitBookings() const
{
    return m_visitBookings;
}

bool DataManager::addVisitBooking(const VisitBooking &booking, QString *errorMessage)
{
    QString validationError;
    if (!booking.validate(&validationError)) {
        if (errorMessage) {
            *errorMessage = validationError;
        }
        return false;
    }

    VisitBooking newBooking = booking;
    if (newBooking.id().isEmpty()) {
        newBooking.setId(m_database.allocateVisitBookingId(errorMessage));
        if (newBooking.id().isEmpty()) {
            return false;
        }
    }

    if (!m_database.insertVisitBooking(newBooking, errorMessage)) {
        return false;
    }

    if (!reloadVisitBookings(errorMessage)) {
        return false;
    }

    emit bookingsChanged();
    return true;
}

bool DataManager::removeVisitBooking(const QString &id, QString *errorMessage)
{
    if (!m_database.deleteVisitBooking(id, errorMessage)) {
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = QStringLiteral("Запис не знайдено.");
        }
        return false;
    }

    if (!reloadVisitBookings(errorMessage)) {
        return false;
    }

    emit bookingsChanged();
    return true;
}

bool DataManager::reloadVisitBookings(QString *errorMessage)
{
    m_visitBookings = m_database.loadVisitBookings(errorMessage);
    return errorMessage == nullptr || errorMessage->isEmpty();
}

void DataManager::ensureDefaultData()
{
    if (m_users.isEmpty()) {
        User admin(QStringLiteral("101"), QStringLiteral("admin"),
                   QStringLiteral("admin123"),
                   QStringLiteral("Адміністратор галереї"), UserRole::Administrator);
        m_database.insertUser(admin, nullptr);

        User visitor(QStringLiteral("102"), QStringLiteral("visitor"),
                     QStringLiteral("visitor"),
                     QStringLiteral("Гість галереї"), UserRole::Visitor);
        m_database.insertUser(visitor, nullptr);

        reloadUsers(nullptr);
    }

    {
        for (const Artwork &artwork : std::as_const(m_artworks)) {
            if (artwork.title().compare(QStringLiteral("Великий сфінкс"), Qt::CaseInsensitive) == 0) {
                m_database.deleteArtwork(artwork.id(), nullptr);
            }
        }
        reloadArtworks(nullptr);

        struct SeedArtwork {
            const char *title;
            const char *artist;
            const char *category;
            int year;
            double price;
            const char *description;
        };
        const QVector<SeedArtwork> seeds = {
            {"Зоряна ніч", "Вінсент ван Гог", "Живопис", 1889, 850000.0, "Постімпресіоністичний шедевр."},
            {"Враження. Схід сонця", "Клод Моне", "Живопис", 1872, 730000.0, "Картина, що дала назву імпресіонізму."},
            {"Герніка", "Пабло Пікассо", "Живопис", 1937, 990000.0, "Антивоєнний символ XX століття."},
            {"Поцілунок", "Густав Клімт", "Живопис", 1908, 780000.0, "Декоративний модерн із золотими мотивами."},
            {"Нічний дозор", "Рембрандт", "Живопис", 1642, 940000.0, "Монументальний груповий портрет."},
            {"Крик", "Едвард Мунк", "Живопис", 1893, 810000.0, "Експресіоністичний образ тривоги."},
            {"Американська готика", "Грант Вуд", "Живопис", 1930, 520000.0, "Ікона американського реалізму."},
            {"Сад земних насолод", "Ієронім Босх", "Живопис", 1503, 1120000.0, "Триптих зі складною символікою."},
            {"Сніданок на траві", "Едуар Мане", "Живопис", 1863, 640000.0, "Провокативний твір для свого часу."},
            {"Мона Ліза", "Леонардо да Вінчі", "Живопис", 1506, 1500000.0, "Найвідоміший жіночий портрет."},

            {"Мислитель", "Огюст Роден", "Скульптура", 1904, 620000.0, "Бронзова скульптура роздумів."},
            {"Давид", "Мікеланджело", "Скульптура", 1504, 1450000.0, "Ренесансний образ ідеальної людини."},
            {"Венера Мілоська", "Олександр Антіохійський", "Скульптура", -100, 980000.0, "Класична мармурова статуя."},
            {"П'єта", "Мікеланджело", "Скульптура", 1499, 1170000.0, "Скульптура скорботи та гармонії."},
            {"Дискобол", "Мирон", "Скульптура", -450, 760000.0, "Антична динаміка руху."},
            {"Балерина", "Едгар Дега", "Скульптура", 1881, 430000.0, "Пластичний образ танцю."},
            {"Птах у просторі", "Костянтин Бранкузі", "Скульптура", 1923, 690000.0, "Абстрактне втілення польоту."},
            {"Людина, що крокує", "Альберто Джакометті", "Скульптура", 1960, 710000.0, "Екзистенційний образ людини."},
            {"Русалонька", "Едвард Еріксен", "Скульптура", 1913, 370000.0, "Відома скульптура Копенгагена."},

            {"Козак Мамай", "Невідомий автор", "Графіка", 1800, 240000.0, "Традиційний український сюжет."},
            {"Капрічос №43", "Франсиско Гойя", "Графіка", 1799, 320000.0, "Серія сатиричних офортів."},
            {"Заяць", "Альбрехт Дюрер", "Графіка", 1502, 510000.0, "Майстерний рисунок натури."},
            {"Велика хвиля (гравюра)", "Кацусіка Хокусай", "Графіка", 1831, 460000.0, "Знакова японська ксилографія."},
            {"Композиція VIII", "Василь Кандинський", "Графіка", 1923, 390000.0, "Геометрична абстракція."},
            {"Літографія миру", "Пабло Пікассо", "Графіка", 1949, 280000.0, "Голуб миру в графічній техніці."},
            {"Ілюстрація до «Кобзаря»", "Георгій Нарбут", "Графіка", 1917, 260000.0, "Книжкова графіка модерну."},
            {"Портрет дівчини", "Тарас Шевченко", "Графіка", 1843, 350000.0, "Академічний рисунок олівцем."},
            {"Старе місто", "Олена Кульчицька", "Графіка", 1932, 220000.0, "Українська ліногравюра."},
            {"Сонце і місто", "Марія Примаченко", "Графіка", 1974, 300000.0, "Декоративний графічний аркуш."},

            {"Мігрантська мати", "Доротея Ленг", "Фотографія", 1936, 540000.0, "Історичний документ епохи депресії."},
            {"Поцілунок біля Hôtel de Ville", "Робер Дуано", "Фотографія", 1950, 410000.0, "Романтична вулична фотографія."},
            {"Полудень, Нью-Йорк", "Альфред Стігліц", "Фотографія", 1902, 360000.0, "Класика пікторіалізму."},
            {"Afghan Girl", "Стів Маккаррі", "Фотографія", 1984, 670000.0, "Один із найвідоміших портретів."},
            {"Moonrise, Hernandez", "Ансель Адамс", "Фотографія", 1941, 620000.0, "Пейзаж із драматичним світлом."},
            {"Вид з вікна в Ле-Гра", "Жозеф Ньєпс", "Фотографія", 1827, 990000.0, "Одна з перших фотографій у світі."},
            {"Скляний дім", "Юрій Кушнір", "Фотографія", 2012, 190000.0, "Сучасна архітектурна зйомка."},
            {"Ранок у Карпатах", "Ірина Мельник", "Фотографія", 2018, 160000.0, "Пейзажна фотографія України."},
            {"Тіні мегаполіса", "Андрій Сова", "Фотографія", 2020, 175000.0, "Чорно-біла міська серія."},
            {"Портрет музиканта", "Олексій Бондар", "Фотографія", 2019, 155000.0, "Студійний портрет з контровим світлом."}
        };

        QHash<QString, int> categoryCount;
        QSet<QString> existingPairs;
        for (const Artwork &artwork : m_artworks) {
            categoryCount[artwork.category()] += 1;
            existingPairs.insert((artwork.title() + QStringLiteral("|") + artwork.artist()).toLower());
        }

        for (const SeedArtwork &seed : seeds) {
            const QString category = QString::fromUtf8(seed.category);
            if (categoryCount.value(category) >= 10) {
                continue;
            }
            const QString title = QString::fromUtf8(seed.title);
            const QString artist = QString::fromUtf8(seed.artist);
            const QString pairKey = (title + QStringLiteral("|") + artist).toLower();
            if (existingPairs.contains(pairKey)) {
                continue;
            }

            Artwork artwork(m_database.allocateArtworkId(nullptr),
                            title,
                            artist,
                            category,
                            seed.year,
                            seed.price,
                            QString::fromUtf8(seed.description),
                            QString());
            if (m_database.insertArtwork(artwork, nullptr)) {
                categoryCount[category] += 1;
                existingPairs.insert(pairKey);
            }
        }
        reloadArtworks(nullptr);
    }
}
