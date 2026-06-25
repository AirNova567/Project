#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "models.h"
#include "datamanager.h"

class QLabel;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(DataManager *dataManager, const User &currentUser,
                        QWidget *parent = nullptr);
    ~MainWindow() override;

    static bool runAuthFlow(DataManager *dataManager, User *currentUser, QWidget *parent);

private slots:
    void onAddArtwork();
    void onEditArtwork();
    void onDeleteArtwork();
    void onRefreshCatalog();
    void onExportCsv();
    void onImportCsv();
    void onOpenSettings();
    void onOpenReport();
    void onOpenUsers();
    void onAbout();
    void onGalleryInfo();
    void onOpenBooking();
    void onOpenBookingsManagement();
    void onLogout();
    void onArtworkSelectionChanged();
    void onFavoritesSelectionChanged();
    void onRemoveFavorite();
    void onFavoritesContextMenu(const QPoint &pos);
    void onCatalogContextMenu(const QPoint &pos);
    void onCatalogHeaderClicked(int section);
    void applySettings();
    void refreshCatalogTable();

private:
    void setupConnections();
    void setupMenus();
    void setupToolBar();
    void applyRoleRestrictions();
    void showStatusMessage(const QString &message, int timeoutMs = 3000);
    QVector<Artwork> filteredArtworks() const;
    QString selectedArtworkId() const;
    void cycleSortState(int section);
    void updatePreviewImage();
    void updateFavoritePreviewImage();
    void showArtworkPreview(const QString &artworkId, QLabel *imageLabel, QLabel *detailsLabel,
                            QString &trackedImagePath);
    bool eventFilter(QObject *obj, QEvent *event) override;

    Ui::MainWindow *ui;
    DataManager *m_dataManager;
    User m_currentUser;
    int m_sortSection = -1;
    int m_sortState = 0;
    QString m_currentImagePath;
    QString m_currentFavoriteImagePath;
};

#endif
