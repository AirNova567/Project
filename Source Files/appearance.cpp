#include "appearance.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>


#include <QFont>

void ThemeManager::apply(QApplication *app, const AppSettings &settings)
{
    if (!app) {
        return;
    }
    app->setFont(applicationFont(settings.fontSize));
    app->setStyleSheet(stylesheet(settings));
}

QFont ThemeManager::applicationFont(int pointSize)
{
    QFont font;
    font.setPointSize(pointSize);
    font.setFamily(QStringLiteral("Segoe UI"));
    return font;
}

QString ThemeManager::stylesheet(const AppSettings &settings)
{
    const QString accent = settings.accentColor.isValid()
                               ? settings.accentColor.name()
                               : QStringLiteral("#3b6ea5");
    const QString accentDark = settings.accentColor.isValid()
                                   ? settings.accentColor.darker(115).name()
                                   : QStringLiteral("#2f5a87");
    const bool dark = settings.darkMode;

    const QString windowBg = dark ? QStringLiteral("#1f2430") : QStringLiteral("#f4f6f9");
    const QString panelBg = dark ? QStringLiteral("#2b3242") : QStringLiteral("#ffffff");
    const QString textColor = dark ? QStringLiteral("#e6edf3") : QStringLiteral("#2c3e50");
    const QString mutedText = dark ? QStringLiteral("#9fb0c1") : QStringLiteral("#607285");
    const QString border = dark ? QStringLiteral("#3a4457") : QStringLiteral("#d0d7de");
    const QString fieldBorder = dark ? QStringLiteral("#445067") : QStringLiteral("#c8d0d8");
    const QString headerBg = dark ? QStringLiteral("#303a4d") : QStringLiteral("#e8ecf0");
    const QString headerHover = dark ? QStringLiteral("#36415a") : QStringLiteral("#dce6f2");
    const QString hoverRow = dark ? QStringLiteral("#2a3448") : QStringLiteral("#e9f1fb");
    const QString altRow = dark ? QStringLiteral("#242c3c") : QStringLiteral("#f8fafc");

    return QStringLiteral(
        "QMainWindow, QDialog { background-color: %3; }"
        "QWidget { color: %4; }"

        "QLabel#titleLabel {"
        "  color: %1;"
        "  padding: 6px 0;"
        "  border-bottom: 2px solid %1;"
        "}"
        "QLabel#subtitleLabel {"
        "  color: %5;"
        "  padding: 0 0 4px 0;"
        "}"

        "QGroupBox {"
        "  font-weight: bold;"
        "  border: 1px solid %6;"
        "  border-radius: 6px;"
        "  margin-top: 10px;"
        "  padding: 10px 8px 8px 8px;"
        "  background-color: %7;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 10px;"
        "  padding: 0 6px;"
        "  color: %1;"
        "}"

        "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QTextEdit, QListWidget {"
        "  background-color: %7;"
        "  border: 1px solid %8;"
        "  border-radius: 4px;"
        "  padding: 4px 6px;"
        "  selection-background-color: %1;"
        "}"
        "QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus, QTextEdit:focus {"
        "  border: 1px solid %1;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: %7;"
        "  color: %4;"
        "  selection-background-color: %1;"
        "  selection-color: #ffffff;"
        "  border: 1px solid %8;"
        "}"

        "QPushButton {"
        "  background-color: %1;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 6px 14px;"
        "  min-height: 22px;"
        "}"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %2; padding-top: 7px; padding-bottom: 5px; }"
        "QPushButton:disabled { background-color: #66758a; color: #e8ecf0; }"
        "QDialog QPushButton { min-width: 120px; }"

        "QTabWidget::pane {"
        "  border: 1px solid %6;"
        "  border-radius: 6px;"
        "  background-color: %7;"
        "  top: -1px;"
        "}"
        "QTabBar::tab {"
        "  background-color: %9;"
        "  border: 1px solid %6;"
        "  border-bottom: none;"
        "  border-top-left-radius: 4px;"
        "  border-top-right-radius: 4px;"
        "  padding: 6px 16px;"
        "  margin-right: 2px;"
        "}"
        "QTabBar::tab:selected {"
        "  background-color: %7;"
        "  color: %1;"
        "  font-weight: bold;"
        "}"
        "QTabBar::tab:hover:!selected { background-color: %10; }"

        "QTableWidget {"
        "  background-color: %7;"
        "  alternate-background-color: %11;"
        "  border: 1px solid %6;"
        "  border-radius: 4px;"
        "  gridline-color: %6;"
        "  selection-background-color: %1;"
        "  selection-color: white;"
        "}"
        "QTableWidget::item {"
        "  padding: 4px 6px;"
        "}"
        "QTableWidget::item:hover {"
        "  background-color: %12;"
        "}"
        "QTableWidget::item:selected:hover {"
        "  background-color: %1;"
        "}"
        "QHeaderView::section {"
        "  background-color: %9;"
        "  color: %4;"
        "  padding: 5px;"
        "  border: none;"
        "  border-right: 1px solid %6;"
        "  border-bottom: 1px solid %6;"
        "  font-weight: bold;"
        "}"
        "QHeaderView::section:hover {"
        "  background-color: %10;"
        "}"
        "QHeaderView::section:checked {"
        "  background-color: %10;"
        "  color: %1;"
        "}"
        "QHeaderView::up-arrow {"
        "  image: none;"
        "  width: 0px;"
        "  height: 0px;"
        "  border-left: 5px solid transparent;"
        "  border-right: 5px solid transparent;"
        "  border-bottom: 7px solid %1;"
        "}"
        "QHeaderView::down-arrow {"
        "  image: none;"
        "  width: 0px;"
        "  height: 0px;"
        "  border-left: 5px solid transparent;"
        "  border-right: 5px solid transparent;"
        "  border-top: 7px solid %1;"
        "}"

        "QListWidget {"
        "  border: 1px solid %6;"
        "  border-radius: 4px;"
        "}"
        "QListWidget::item:selected { background-color: %1; color: white; }"

        "QProgressBar {"
        "  border: 1px solid %6;"
        "  border-radius: 4px;"
        "  text-align: center;"
        "  background-color: %9;"
        "}"
        "QProgressBar::chunk { background-color: %1; border-radius: 3px; }"

        "QToolBar {"
        "  background-color: %7;"
        "  border-bottom: 1px solid %6;"
        "  spacing: 6px;"
        "  padding: 4px 6px;"
        "}"
        "QToolButton {"
        "  background-color: transparent;"
        "  border: 1px solid transparent;"
        "  border-radius: 6px;"
        "  padding: 4px 8px;"
        "  margin: 1px;"
        "}"
        "QToolButton:hover {"
        "  background-color: %12;"
        "  border: 1px solid %6;"
        "}"
        "QToolButton:pressed {"
        "  background-color: %10;"
        "}"
        "QStatusBar { background-color: %9; border-top: 1px solid %6; }"
        "QMenuBar { background-color: %7; border-bottom: 1px solid %6; }"
        "QMenuBar::item:selected { background-color: %1; color: white; }"
        "QMenu {"
        "  background-color: %7;"
        "  border: 1px solid %6;"
        "  padding: 6px;"
        "}"
        "QMenu::item { padding: 6px 16px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: %1; color: white; }"
        "QMenu::item:disabled { color: %5; }"
        "QMenu::separator { height: 1px; background: %6; margin: 6px 4px; }"

        "QCheckBox::indicator {"
        "  width: 16px;"
        "  height: 16px;"
        "  border: 1px solid %8;"
        "  border-radius: 3px;"
        "  background-color: %7;"
        "}"
        "QCheckBox::indicator:checked {"
        "  background-color: %1;"
        "  border-color: %1;"
        "}"
        "QRadioButton::indicator {"
        "  width: 16px;"
        "  height: 16px;"
        "  border: 1px solid %8;"
        "  border-radius: 8px;"
        "  background-color: %7;"
        "}"
        "QRadioButton::indicator:checked {"
        "  background-color: %1;"
        "  border-color: %1;"
        "}"
        "QFrame[frameShape=\"4\"] { border: 1px dashed %8; border-radius: 4px; background-color: %7; }"
        )
        .arg(accent, accentDark, windowBg, textColor, mutedText, border, panelBg, fieldBorder,
             headerBg, headerHover, altRow, hoverRow);
}


#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace {
QPixmap drawIcon(AppIcons::Type type, int size, const QColor &accent)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor stroke = accent.isValid() ? accent : QColor(59, 110, 165);
    const QColor fill = stroke.lighter(165);
    const QPen pen(stroke, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(fill);

    const qreal s = size;
    const qreal m = s * 0.18;

    switch (type) {
    case AppIcons::Type::Add: {
        painter.drawRoundedRect(QRectF(m, m, s - 2 * m, s - 2 * m), 3, 3);
        painter.setPen(QPen(stroke, 2.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(s * 0.5, s * 0.32), QPointF(s * 0.5, s * 0.68));
        painter.drawLine(QPointF(s * 0.32, s * 0.5), QPointF(s * 0.68, s * 0.5));
        break;
    }
    case AppIcons::Type::Refresh: {
        QPainterPath arc;
        arc.arcMoveTo(m, m, s - 2 * m, s - 2 * m, 45);
        arc.arcTo(m, m, s - 2 * m, s - 2 * m, 45, 270);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(arc);
        QPainterPath arrow;
        arrow.moveTo(s * 0.72, s * 0.22);
        arrow.lineTo(s * 0.84, s * 0.22);
        arrow.lineTo(s * 0.78, s * 0.34);
        painter.setBrush(stroke);
        painter.drawPath(arrow);
        break;
    }
    case AppIcons::Type::Settings: {
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(s * 0.5, s * 0.5), s * 0.28, s * 0.28);
        painter.setBrush(stroke);
        for (int i = 0; i < 6; ++i) {
            painter.save();
            painter.translate(s * 0.5, s * 0.5);
            painter.rotate(i * 60);
            painter.drawRoundedRect(QRectF(-s * 0.05, -s * 0.38, s * 0.1, s * 0.14), 1, 1);
            painter.restore();
        }
        painter.setBrush(fill);
        painter.drawEllipse(QPointF(s * 0.5, s * 0.5), s * 0.1, s * 0.1);
        break;
    }
    case AppIcons::Type::Report: {
        painter.drawRoundedRect(QRectF(s * 0.24, s * 0.16, s * 0.52, s * 0.68), 2, 2);
        painter.setPen(QPen(stroke, 1.6, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(s * 0.34, s * 0.58), QPointF(s * 0.34, s * 0.42));
        painter.drawLine(QPointF(s * 0.5, s * 0.58), QPointF(s * 0.5, s * 0.34));
        painter.drawLine(QPointF(s * 0.66, s * 0.58), QPointF(s * 0.66, s * 0.48));
        break;
    }
    case AppIcons::Type::Gallery: {
        painter.drawRoundedRect(QRectF(s * 0.2, s * 0.34, s * 0.6, s * 0.46), 2, 2);
        QPainterPath roof;
        roof.moveTo(s * 0.16, s * 0.36);
        roof.lineTo(s * 0.5, s * 0.14);
        roof.lineTo(s * 0.84, s * 0.36);
        roof.closeSubpath();
        painter.setBrush(stroke.lighter(140));
        painter.drawPath(roof);
        painter.setBrush(fill);
        painter.drawRect(QRectF(s * 0.42, s * 0.52, s * 0.16, s * 0.28));
        break;
    }
    case AppIcons::Type::Export: {
        painter.drawRoundedRect(QRectF(s * 0.22, s * 0.48, s * 0.56, s * 0.34), 2, 2);
        painter.setPen(QPen(stroke, 1.8, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(s * 0.5, s * 0.58), QPointF(s * 0.5, s * 0.2));
        QPainterPath arrow;
        arrow.moveTo(s * 0.38, s * 0.3);
        arrow.lineTo(s * 0.5, s * 0.18);
        arrow.lineTo(s * 0.62, s * 0.3);
        painter.setBrush(stroke);
        painter.drawPath(arrow);
        break;
    }
    case AppIcons::Type::Import: {
        painter.drawRoundedRect(QRectF(s * 0.22, s * 0.48, s * 0.56, s * 0.34), 2, 2);
        painter.setPen(QPen(stroke, 1.8, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(s * 0.5, s * 0.2), QPointF(s * 0.5, s * 0.58));
        QPainterPath arrow;
        arrow.moveTo(s * 0.38, s * 0.48);
        arrow.lineTo(s * 0.5, s * 0.6);
        arrow.lineTo(s * 0.62, s * 0.48);
        painter.setBrush(stroke);
        painter.drawPath(arrow);
        break;
    }
    case AppIcons::Type::Logout: {
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(s * 0.18, s * 0.22, s * 0.36, s * 0.56), 2, 2);
        painter.drawLine(QPointF(s * 0.54, s * 0.5), QPointF(s * 0.82, s * 0.5));
        QPainterPath arrow;
        arrow.moveTo(s * 0.7, s * 0.38);
        arrow.lineTo(s * 0.84, s * 0.5);
        arrow.lineTo(s * 0.7, s * 0.62);
        painter.setBrush(stroke);
        painter.drawPath(arrow);
        break;
    }
    case AppIcons::Type::Exit: {
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(QPointF(s * 0.28, s * 0.28), QPointF(s * 0.72, s * 0.72));
        painter.drawLine(QPointF(s * 0.72, s * 0.28), QPointF(s * 0.28, s * 0.72));
        break;
    }
    case AppIcons::Type::Edit: {
        QPainterPath pencil;
        pencil.moveTo(s * 0.62, s * 0.18);
        pencil.lineTo(s * 0.82, s * 0.38);
        pencil.lineTo(s * 0.42, s * 0.78);
        pencil.lineTo(s * 0.22, s * 0.78);
        pencil.lineTo(s * 0.22, s * 0.58);
        pencil.closeSubpath();
        painter.drawPath(pencil);
        break;
    }
    case AppIcons::Type::Delete: {
        painter.drawRoundedRect(QRectF(s * 0.26, s * 0.3, s * 0.48, s * 0.5), 2, 2);
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(QPointF(s * 0.32, s * 0.24), QPointF(s * 0.68, s * 0.24));
        painter.drawLine(QPointF(s * 0.42, s * 0.44), QPointF(s * 0.42, s * 0.66));
        painter.drawLine(QPointF(s * 0.58, s * 0.44), QPointF(s * 0.58, s * 0.66));
        break;
    }
    case AppIcons::Type::Users: {
        painter.drawEllipse(QPointF(s * 0.38, s * 0.34), s * 0.1, s * 0.1);
        painter.drawEllipse(QPointF(s * 0.62, s * 0.34), s * 0.1, s * 0.1);
        QPainterPath group;
        group.addEllipse(QPointF(s * 0.38, s * 0.72), s * 0.16, s * 0.12);
        group.addEllipse(QPointF(s * 0.62, s * 0.72), s * 0.16, s * 0.12);
        painter.drawPath(group);
        break;
    }
    case AppIcons::Type::About: {
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(s * 0.5, s * 0.5), s * 0.3, s * 0.3);
        painter.setPen(QPen(stroke, 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(s * 0.5, s * 0.38), QPointF(s * 0.5, s * 0.56));
        painter.drawPoint(QPointF(s * 0.5, s * 0.66));
        break;
    }
    case AppIcons::Type::Favorite: {
        QPainterPath heart;
        heart.moveTo(s * 0.5, s * 0.72);
        heart.cubicTo(s * 0.2, s * 0.5, s * 0.2, s * 0.24, s * 0.5, s * 0.36);
        heart.cubicTo(s * 0.8, s * 0.24, s * 0.8, s * 0.5, s * 0.5, s * 0.72);
        painter.drawPath(heart);
        break;
    }
    case AppIcons::Type::Calendar: {
        painter.drawRoundedRect(QRectF(s * 0.2, s * 0.22, s * 0.6, s * 0.58), 2, 2);
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(QPointF(s * 0.2, s * 0.36), QPointF(s * 0.8, s * 0.36));
        painter.drawLine(QPointF(s * 0.34, s * 0.16), QPointF(s * 0.34, s * 0.28));
        painter.drawLine(QPointF(s * 0.66, s * 0.16), QPointF(s * 0.66, s * 0.28));
        painter.setBrush(stroke);
        painter.drawEllipse(QPointF(s * 0.38, s * 0.5), s * 0.05, s * 0.05);
        painter.drawEllipse(QPointF(s * 0.5, s * 0.5), s * 0.05, s * 0.05);
        painter.drawEllipse(QPointF(s * 0.62, s * 0.5), s * 0.05, s * 0.05);
        break;
    }
    }

    return pixmap;
}
}

QIcon AppIcons::icon(Type type, const QColor &accent)
{
    QIcon result;
    for (int size : {16, 20, 24, 32}) {
        result.addPixmap(drawIcon(type, size, accent));
    }
    return result;
}
