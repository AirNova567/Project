#ifndef ARTWORKDIALOG_H
#define ARTWORKDIALOG_H

#include <QDialog>
#include <optional>

#include "models.h"

class DataManager;

namespace Ui {
class ArtworkDialog;
}

class ArtworkDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ArtworkDialog(DataManager *dataManager, QWidget *parent = nullptr);
    ~ArtworkDialog() override;

    void setArtwork(const Artwork &artwork);
    std::optional<Artwork> artwork() const;

private slots:
    void onBrowseImage();
    void onSave();

private:
    void loadCategories();

    Ui::ArtworkDialog *ui;
    DataManager *m_dataManager;
    std::optional<Artwork> m_artwork;
};

#endif
