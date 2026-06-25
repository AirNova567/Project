#include "models.h"

#include <QJsonObject>
#include <QRegularExpression>

ArtObject::ArtObject(const QString &id)
    : m_id(id)
{
}

ArtObject::~ArtObject() = default;

QString ArtObject::id() const
{
    return m_id;
}

void ArtObject::setId(const QString &id)
{
    m_id = id;
}

#include <QJsonObject>
#include <QRegularExpression>

Artwork::Artwork()
    : ArtObject()
    , m_year(2000)
    , m_price(0.0)
{
}

Artwork::Artwork(const QString &id, const QString &title, const QString &artist,
                 const QString &category, int year, double price,
                 const QString &description, const QString &imagePath)
    : ArtObject(id)
    , m_title(title)
    , m_artist(artist)
    , m_category(category)
    , m_year(year)
    , m_price(price)
    , m_description(description)
    , m_imagePath(imagePath)
{
}

Artwork::~Artwork() = default;

QString Artwork::title() const { return m_title; }
QString Artwork::artist() const { return m_artist; }
QString Artwork::category() const { return m_category; }
int Artwork::year() const { return m_year; }
double Artwork::price() const { return m_price; }
QString Artwork::description() const { return m_description; }
QString Artwork::imagePath() const { return m_imagePath; }

void Artwork::setTitle(const QString &title) { m_title = title.trimmed(); }
void Artwork::setArtist(const QString &artist) { m_artist = artist.trimmed(); }
void Artwork::setCategory(const QString &category) { m_category = category.trimmed(); }
void Artwork::setYear(int year) { m_year = year; }
void Artwork::setPrice(double price) { m_price = price; }
void Artwork::setDescription(const QString &description) { m_description = description.trimmed(); }
void Artwork::setImagePath(const QString &imagePath) { m_imagePath = imagePath; }

QString Artwork::typeName() const
{
    return QStringLiteral("Експонат");
}

bool Artwork::validate(QString *errorMessage) const
{
    if (m_title.length() < 2) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Назва повинна містити щонайменше 2 символи.");
        }
        return false;
    }
    if (m_artist.length() < 2) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Ім'я художника повинно містити щонайменше 2 символи.");
        }
        return false;
    }
    if (m_category.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Оберіть категорію експонату.");
        }
        return false;
    }
    if (m_year < 1000 || m_year > QDate::currentDate().year()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Рік створення вказано некоректно.");
        }
        return false;
    }
    if (m_price < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Ціна не може бути від'ємною.");
        }
        return false;
    }
    return true;
}

QJsonObject Artwork::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = m_id;
    obj[QStringLiteral("title")] = m_title;
    obj[QStringLiteral("artist")] = m_artist;
    obj[QStringLiteral("category")] = m_category;
    obj[QStringLiteral("year")] = m_year;
    obj[QStringLiteral("price")] = m_price;
    obj[QStringLiteral("description")] = m_description;
    obj[QStringLiteral("imagePath")] = m_imagePath;
    return obj;
}

QString Artwork::toCsvRow() const
{
    auto escape = [](const QString &value) {
        QString escaped = value;
        escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
        return QStringLiteral("\"%1\"").arg(escaped);
    };
    return QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8")
        .arg(escape(m_id), escape(m_title), escape(m_artist), escape(m_category))
        .arg(m_year)
        .arg(m_price, 0, 'f', 2)
        .arg(escape(m_description), escape(m_imagePath));
}

QString Artwork::displayTitle() const
{
    return m_title;
}

Artwork Artwork::fromJson(const QJsonObject &obj)
{
    Artwork artwork;
    artwork.setId(obj.value(QStringLiteral("id")).toString());
    artwork.setTitle(obj.value(QStringLiteral("title")).toString());
    artwork.setArtist(obj.value(QStringLiteral("artist")).toString());
    artwork.setCategory(obj.value(QStringLiteral("category")).toString());
    artwork.setYear(obj.value(QStringLiteral("year")).toInt());
    artwork.setPrice(obj.value(QStringLiteral("price")).toDouble());
    artwork.setDescription(obj.value(QStringLiteral("description")).toString());
    artwork.setImagePath(obj.value(QStringLiteral("imagePath")).toString());
    return artwork;
}

QString Artwork::csvHeader()
{
    return QStringLiteral("id,title,artist,category,year,price,description,imagePath");
}

QDataStream &operator<<(QDataStream &stream, const Artwork &artwork)
{
    stream << artwork.m_id << artwork.m_title << artwork.m_artist
           << artwork.m_category << artwork.m_year << artwork.m_price
           << artwork.m_description << artwork.m_imagePath;
    return stream;
}

bool artworksEqualByTitle(const Artwork &a, const Artwork &b)
{
    return a.m_title.compare(b.m_title, Qt::CaseInsensitive) == 0
           && a.m_artist.compare(b.m_artist, Qt::CaseInsensitive) == 0;
}

#include <QRegularExpression>

User::User()
    : m_role(UserRole::Visitor)
{
}

User::User(const QString &id, const QString &login, const QString &password,
           const QString &fullName, UserRole role)
    : m_id(id)
    , m_login(login.trimmed())
    , m_password(password)
    , m_fullName(fullName.trimmed())
    , m_role(role)
{
}

User::~User() = default;

QString User::id() const { return m_id; }
QString User::login() const { return m_login; }
QString User::password() const { return m_password; }
QString User::fullName() const { return m_fullName; }
UserRole User::role() const { return m_role; }

void User::setId(const QString &id) { m_id = id; }
void User::setLogin(const QString &login) { m_login = login.trimmed(); }
void User::setPassword(const QString &password) { m_password = password; }
void User::setFullName(const QString &fullName) { m_fullName = fullName.trimmed(); }
void User::setRole(UserRole role) { m_role = role; }

bool User::validate(QString *errorMessage) const
{
    static const QRegularExpression loginPattern(QStringLiteral("^[a-zA-Z0-9_]{3,20}$"));
    if (!loginPattern.match(m_login).hasMatch()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Логін: 3–20 символів, лише латиниця, цифри та _.");
        }
        return false;
    }
    if (m_fullName.length() < 3) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Повне ім'я повинно містити щонайменше 3 символи.");
        }
        return false;
    }
    return true;
}

bool User::checkPassword(const QString &password) const
{
    return m_password == password;
}

User User::fromJson(const QJsonObject &obj)
{
    User user;
    user.setId(obj.value(QStringLiteral("id")).toString());
    user.setLogin(obj.value(QStringLiteral("login")).toString());
    if (obj.contains(QStringLiteral("password"))) {
        user.setPassword(obj.value(QStringLiteral("password")).toString());
    } else {
        user.setPassword(obj.value(QStringLiteral("passwordHash")).toString());
    }
    user.setFullName(obj.value(QStringLiteral("fullName")).toString());
    const QString roleStr = obj.value(QStringLiteral("role")).toString();
    user.setRole(roleStr == QStringLiteral("admin") ? UserRole::Administrator : UserRole::Visitor);
    return user;
}

QJsonObject User::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = m_id;
    obj[QStringLiteral("login")] = m_login;
    obj[QStringLiteral("password")] = m_password;
    obj[QStringLiteral("fullName")] = m_fullName;
    obj[QStringLiteral("role")] = m_role == UserRole::Administrator
                                      ? QStringLiteral("admin")
                                      : QStringLiteral("visitor");
    return obj;
}

QString User::roleName() const
{
    return roleToString(m_role);
}

QString User::roleToString(UserRole role)
{
    return role == UserRole::Administrator
               ? QStringLiteral("Адміністратор")
               : QStringLiteral("Користувач");
}

bool operator==(const User &lhs, const User &rhs)
{
    return lhs.m_id == rhs.m_id;
}

bool usersShareLogin(const User &lhs, const User &rhs)
{
    return lhs.m_login.compare(rhs.m_login, Qt::CaseInsensitive) == 0;
}

bool VisitBooking::validate(QString *errorMessage) const
{
    if (m_firstName.trimmed().isEmpty() || m_lastName.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Вкажіть ім'я та прізвище.");
        }
        return false;
    }
    if (!m_visitDate.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Оберіть дату візиту.");
        }
        return false;
    }
    if (!m_visitTime.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Оберіть час візиту.");
        }
        return false;
    }
    const QDate today = QDate::currentDate();
    if (m_visitDate < today) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Дата візиту не може бути в минулому.");
        }
        return false;
    }
    if (m_visitDate.dayOfWeek() == 7) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Галерея не працює у неділю.");
        }
        return false;
    }
    return true;
}

VisitBooking::VisitBooking() = default;

QString VisitBooking::id() const { return m_id; }
QString VisitBooking::userId() const { return m_userId; }
QString VisitBooking::firstName() const { return m_firstName; }
QString VisitBooking::lastName() const { return m_lastName; }
QDate VisitBooking::visitDate() const { return m_visitDate; }
QTime VisitBooking::visitTime() const { return m_visitTime; }
double VisitBooking::priceUsd() const { return m_priceUsd; }
QString VisitBooking::userLogin() const { return m_userLogin; }

void VisitBooking::setId(const QString &id) { m_id = id; }
void VisitBooking::setUserId(const QString &userId) { m_userId = userId; }
void VisitBooking::setFirstName(const QString &firstName) { m_firstName = firstName; }
void VisitBooking::setLastName(const QString &lastName) { m_lastName = lastName; }
void VisitBooking::setVisitDate(const QDate &date) { m_visitDate = date; }
void VisitBooking::setVisitTime(const QTime &time) { m_visitTime = time; }
void VisitBooking::setPriceUsd(double priceUsd) { m_priceUsd = priceUsd; }
void VisitBooking::setUserLogin(const QString &login) { m_userLogin = login; }

