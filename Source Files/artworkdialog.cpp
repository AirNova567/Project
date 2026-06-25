#include "artworkdialog.h"
#include "datamanager.h"

#include "ui_artworkdialog.h"

#include <QDate>
#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
ArtworkDialog::ArtworkDialog(DataManager *dataManager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ArtworkDialog)
    , m_dataManager(dataManager)
{
    ui->setupUi(this);
    loadCategories();

    ui->yearSpin->setRange(1000, QDate::currentDate().year());
    ui->yearSpin->setValue(2000);
    ui->priceSpin->setRange(0, 999999999);
    ui->priceSpin->setDecimals(2);

    connect(ui->browseButton, &QPushButton::clicked, this, &ArtworkDialog::onBrowseImage);
    connect(ui->saveButton, &QPushButton::clicked, this, &ArtworkDialog::onSave);
    connect(ui->cancelButton, &QPushButton::clicked, this, &ArtworkDialog::reject);
}

ArtworkDialog::~ArtworkDialog()
{
    delete ui;
}

void ArtworkDialog::loadCategories()
{
    ui->categoryCombo->addItems({
        QStringLiteral("Живопис"),
        QStringLiteral("Скульптура"),
        QStringLiteral("Графіка"),
        QStringLiteral("Фотографія"),
        QStringLiteral("Кераміка")
    });
}

void ArtworkDialog::setArtwork(const Artwork &artwork)
{
    m_artwork = artwork;
    ui->titleEdit->setText(artwork.title());
    ui->artistEdit->setText(artwork.artist());

    const int catIndex = ui->categoryCombo->findText(artwork.category());
    if (catIndex >= 0) {
        ui->categoryCombo->setCurrentIndex(catIndex);
    } else {
        ui->categoryCombo->setEditText(artwork.category());
    }

    ui->yearSpin->setValue(artwork.year());
    ui->priceSpin->setValue(artwork.price());
    ui->descriptionEdit->setPlainText(artwork.description());
    ui->imagePathEdit->setText(artwork.imagePath());

    if (!artwork.imagePath().isEmpty()) {
        QPixmap pix(artwork.imagePath());
        if (!pix.isNull()) {
            ui->previewLabel->setPixmap(pix.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            ui->previewLabel->setText(QString());
        }
    }
}

std::optional<Artwork> ArtworkDialog::artwork() const
{
    return m_artwork;
}

void ArtworkDialog::onBrowseImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Обрати зображення"), QString(),
        QStringLiteral("Зображення (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty()) {
        return;
    }

    ui->imagePathEdit->setText(path);
    QPixmap pix(path);
    if (!pix.isNull()) {
        ui->previewLabel->setPixmap(pix.scaled(200, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui->previewLabel->setText(QString());
    }
}

void ArtworkDialog::onSave()
{
    Artwork artwork = m_artwork.value_or(Artwork());
    artwork.setTitle(ui->titleEdit->text());
    artwork.setArtist(ui->artistEdit->text());
    artwork.setCategory(ui->categoryCombo->currentText());
    artwork.setYear(ui->yearSpin->value());
    artwork.setPrice(ui->priceSpin->value());
    artwork.setDescription(ui->descriptionEdit->toPlainText());

    QString error;
    const QString sourceImage = ui->imagePathEdit->text();
    if (!sourceImage.isEmpty() && sourceImage != artwork.imagePath()) {
        const QString stored = m_dataManager->copyImageToStorage(sourceImage, &error);
        if (stored.isEmpty() && !error.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Помилка"), error);
            return;
        }
        artwork.setImagePath(stored);
    }

    QString validationError;
    if (!artwork.validate(&validationError)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), validationError);
        return;
    }

    m_artwork = artwork;
    accept();
}
