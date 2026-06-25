#ifndef APPEARANCE_H
#define APPEARANCE_H

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QIcon>
#include <QString>

#include "models.h"

class ThemeManager
{
public:
    static void apply(QApplication *app, const AppSettings &settings);
    static QString stylesheet(const AppSettings &settings);
    static QFont applicationFont(int pointSize);
};

class AppIcons
{
public:
    enum class Type {
        Add,
        Refresh,
        Settings,
        Report,
        Gallery,
        Export,
        Import,
        Logout,
        Exit,
        Edit,
        Delete,
        Users,
        About,
        Calendar,
        Favorite
    };

    static QIcon icon(Type type, const QColor &accent = QColor(59, 110, 165));
};

#endif
