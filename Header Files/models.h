#ifndef MODELS_H
#define MODELS_H

#include <QColor>
#include <QDataStream>
#include <QDate>
#include <QJsonObject>
#include <QString>
#include <QTime>

struct AppSettings {
    QColor accentColor = QColor(70, 130, 180);
    int fontSize = 12;
    bool showToolBar = true;
    QString sortField = QStringLiteral("title");
    bool sortAscending = true;
    bool darkMode = false;
};

class ArtObject
{
public:
    explicit ArtObject(const QString &id = QString());
    virtual ~ArtObject();

    QString id() const;
    void setId(const QString &id);

    virtual QString typeName() const = 0;
    virtual bool validate(QString *errorMessage = nullptr) const = 0;
    virtual QJsonObject toJson() const = 0;
    virtual QString toCsvRow() const = 0;
    virtual QString displayTitle() const = 0;

protected:
    QString m_id;
};

class Artwork : public ArtObject
{
    friend QDataStream &operator<<(QDataStream &stream, const Artwork &artwork);
    friend bool artworksEqualByTitle(const Artwork &a, const Artwork &b);

public:
    Artwork();
    Artwork(const QString &id, const QString &title, const QString &artist,
            const QString &category, int year, double price,
            const QString &description, const QString &imagePath);
    ~Artwork() override;

    QString title() const;
    QString artist() const;
    QString category() const;
    int year() const;
    double price() const;
    QString description() const;
    QString imagePath() const;

    void setTitle(const QString &title);
    void setArtist(const QString &artist);
    void setCategory(const QString &category);
    void setYear(int year);
    void setPrice(double price);
    void setDescription(const QString &description);
    void setImagePath(const QString &imagePath);

    QString typeName() const override;
    bool validate(QString *errorMessage = nullptr) const override;
    QJsonObject toJson() const override;
    QString toCsvRow() const override;
    QString displayTitle() const override;

    static Artwork fromJson(const QJsonObject &obj);
    static QString csvHeader();

private:
    QString m_title;
    QString m_artist;
    QString m_category;
    int m_year;
    double m_price;
    QString m_description;
    QString m_imagePath;
};

QDataStream &operator<<(QDataStream &stream, const Artwork &artwork);
bool artworksEqualByTitle(const Artwork &a, const Artwork &b);

enum class UserRole {
    Visitor,
    Administrator
};

class User
{
    friend bool operator==(const User &lhs, const User &rhs);
    friend bool usersShareLogin(const User &lhs, const User &rhs);

public:
    User();
    User(const QString &id, const QString &login, const QString &password,
         const QString &fullName, UserRole role);
    ~User();

    QString id() const;
    QString login() const;
    QString password() const;
    QString fullName() const;
    UserRole role() const;

    void setId(const QString &id);
    void setLogin(const QString &login);
    void setPassword(const QString &password);
    void setFullName(const QString &fullName);
    void setRole(UserRole role);

    bool validate(QString *errorMessage = nullptr) const;
    bool checkPassword(const QString &password) const;
    static User fromJson(const QJsonObject &obj);
    QJsonObject toJson() const;
    QString roleName() const;
    static QString roleToString(UserRole role);

private:
    QString m_id;
    QString m_login;
    QString m_password;
    QString m_fullName;
    UserRole m_role;
};

bool operator==(const User &lhs, const User &rhs);
bool usersShareLogin(const User &lhs, const User &rhs);

class VisitBooking
{
public:
    VisitBooking();

    QString id() const;
    QString userId() const;
    QString firstName() const;
    QString lastName() const;
    QDate visitDate() const;
    QTime visitTime() const;
    double priceUsd() const;
    QString userLogin() const;

    void setId(const QString &id);
    void setUserId(const QString &userId);
    void setFirstName(const QString &firstName);
    void setLastName(const QString &lastName);
    void setVisitDate(const QDate &date);
    void setVisitTime(const QTime &time);
    void setPriceUsd(double priceUsd);
    void setUserLogin(const QString &login);

    bool validate(QString *errorMessage = nullptr) const;

private:
    QString m_id;
    QString m_userId;
    QString m_firstName;
    QString m_lastName;
    QDate m_visitDate;
    QTime m_visitTime;
    double m_priceUsd = 25.0;
    QString m_userLogin;
};

#endif
