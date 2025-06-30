#include "moviewidget.h"
#include <QPixmap>
#include <QIcon>
#include <QFile>
#include <QGraphicsOpacityEffect>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QDebug>

MovieWidget::MovieWidget(int movieId, int apiId, const QString &title, int year, double rating,
                         int duration, bool inFavorites, bool inWatchlist,
                         const QString &imagePath, QWidget *parent)
    : QWidget(parent),
      m_movieId(movieId),
      m_apiId(apiId),
      m_title(title),
      m_year(year),
      m_rating(rating),
      m_duration(duration),
      m_inFavorites(inFavorites),
      m_inWatchlist(inWatchlist),
      m_imagePath(imagePath),
      m_networkManager(nullptr),
      m_imageReply(nullptr)
{
    m_networkManager = new QNetworkAccessManager(this);
    setupUi();
}

int MovieWidget::movieId() const
{
    return m_movieId;
}

int MovieWidget::apiId() const
{
    return m_apiId;
}

void MovieWidget::updateMovieData(double rating, int duration)
{
    // Update member variables
    m_rating = rating;
    m_duration = duration;

    // Update UI elements
    m_ratingLabel->setText(QString("%1/10").arg(m_rating, 0, 'f', 1));
    m_durationLabel->setText(QString("%1 min").arg(m_duration));

    // Ajouter une animation subtile pour indiquer la mise à jour
    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(m_durationLabel);
    m_durationLabel->setGraphicsEffect(effect);

    QPropertyAnimation* animation = new QPropertyAnimation(effect, "opacity");
    animation->setDuration(500);
    animation->setStartValue(0.5);
    animation->setEndValue(1.0);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void MovieWidget::setupUi()
{
    // Définir une taille fixe pour le widget
    this->setFixedSize(180, 400);  // Hauteur ajustée pour contenir l'image et les informations
    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);  // Suppression des marges externes
    mainLayout->setSpacing(0);  // Suppression de l'espacement
    mainLayout->setAlignment(Qt::AlignCenter);  // Centrer le contenu

    // Top buttons layout
    QHBoxLayout *topButtonsLayout = new QHBoxLayout();
    topButtonsLayout->setContentsMargins(0, 0, 0, 0);
    topButtonsLayout->setSpacing(4);

    // Create watchlist button with "+" design to overlay on image
    m_watchlistButton = new QPushButton("+", this);
    m_watchlistButton->setObjectName("watchlistButton");
    m_watchlistButton->setFixedSize(36, 36);
    m_watchlistButton->setCursor(Qt::PointingHandCursor);
    m_watchlistButton->setToolTip("Ajouter à la liste de visionnage");
    m_watchlistButton->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(0, 0, 0, 0.6);"
        "   color: white;"
        "   border-radius: 18px;"
        "   font-size: 20px;"
        "   font-weight: bold;"
        "   opacity: 0;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(61, 90, 241, 0.8);"
        "   opacity: 1;"
        "}"
    );

    // Create favorite button with heart icon
    m_favoriteButton = new QPushButton("♥", this);
    m_favoriteButton->setObjectName("favoriteButton");
    m_favoriteButton->setFixedSize(36, 36);
    m_favoriteButton->setCursor(Qt::PointingHandCursor);
    m_favoriteButton->setToolTip("Ajouter aux favoris");
    m_favoriteButton->setStyleSheet(
        "QPushButton {"
        "   background-color: rgba(0, 0, 0, 0.6);"
        "   color: white;"
        "   border-radius: 18px;"
        "   font-size: 18px;"
        "   font-weight: bold;"
        "   text-align: center;"
        "   opacity: 0;"
        "}"
        "QPushButton:hover {"
        "   background-color: rgba(61, 90, 241, 0.8);"
        "   color: white;"
        "   opacity: 1;"
        "}"
    );

    // Create image label with improved design
    m_imageLabel = new QLabel(this);
    m_imageLabel->setFixedSize(180, 270);  // Adjusted size for horizontal layout
    m_imageLabel->setScaledContents(true);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setCursor(Qt::PointingHandCursor);
    m_imageLabel->setStyleSheet(
        "QLabel {"
        "   border-top-left-radius: 8px;"
        "   border-top-right-radius: 8px;"
        "   border-bottom-left-radius: 0px;"
        "   border-bottom-right-radius: 0px;"
        "   border: 2px solid #30363d;"
        "   border-bottom: none;"
        "   background-color: #0f1620;"
        "}"
    );

    // Create a container for the image and the buttons
    QWidget* imageContainer = new QWidget(this);
    imageContainer->setFixedSize(180, 280);  // Même taille que l'image
    QHBoxLayout* imageLayout = new QHBoxLayout(imageContainer);
    imageLayout->setContentsMargins(0, 0, 0, 10);
    imageLayout->setSpacing(0);
    imageLayout->setAlignment(Qt::AlignCenter);  // Centrer l'image

    // Add the image to the container
    imageLayout->addWidget(m_imageLabel);

    // Position the watchlist button on top of the image (top-right corner)
    m_watchlistButton->setParent(m_imageLabel);
    m_watchlistButton->move(m_imageLabel->width() - m_watchlistButton->width() - 5, 5);

    // Position the favorite button on top of the image (bottom-right corner)
    m_favoriteButton->setParent(m_imageLabel);
    m_favoriteButton->move(m_imageLabel->width() - m_favoriteButton->width() - 5,
                          m_imageLabel->height() - m_favoriteButton->height() - 5);

    updateWatchlistButton();
    updateFavoriteButton();

    // Set placeholder image initially
    QPixmap placeholder(":/images/placeholder.jpg");
    if (!placeholder.isNull()) {
        m_imageLabel->setPixmap(placeholder);
    } else {
        m_imageLabel->setText(m_title);
        m_imageLabel->setStyleSheet("QLabel { background-color: #2c3440; color: white; }");
    }

    // Load the actual image (local or remote)
    loadImage();

    // Create a container for movie information with a professional style
    QWidget* infoContainer = new QWidget(this);
    infoContainer->setObjectName("infoContainer");
    infoContainer->setFixedWidth(180);  // Même largeur que l'image
    infoContainer->setStyleSheet(
        "QWidget#infoContainer {"
        "   background-color: #1a2234;"
        "   border-radius: 0px;"  // Pas d'arrondi pour éviter les problèmes d'alignement
        "   border-bottom-left-radius: 8px;"  // Arrondi uniquement en bas
        "   border-bottom-right-radius: 8px;"
        "   border: 2px solid #30363d;"  // Même bordure que l'image
        "   border-top: none;"  // Pas de bordure en haut pour une transition fluide
        "   margin: 0px;"  // Pas de marge pour éviter les décalages
        "   padding: 12px 8px 8px 8px;"
        "}"
    );

    QVBoxLayout* infoContainerLayout = new QVBoxLayout(infoContainer);
    infoContainerLayout->setContentsMargins(8, 10, 8, 8);
    infoContainerLayout->setSpacing(4);

    // Titre du film avec style professionnel et ellipse pour les titres longs
    // Limiter le titre à un maximum de caractères et ajouter une ellipse si nécessaire
    QString displayTitle = m_title;
    QStringList words = m_title.split(" ", Qt::SkipEmptyParts);
    if (words.size() > 2) {
        displayTitle = words.mid(0, 2).join(" ") + "...";
    }

    QLabel* titleLabel = new QLabel(displayTitle, infoContainer);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: white;");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setWordWrap(true);
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    titleLabel->setMaximumHeight(30); // Reduce the height of the title label
    infoContainerLayout->addWidget(titleLabel);

    // Créer un séparateur horizontal pour une meilleure structure visuelle
    QFrame* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    separator->setStyleSheet(
        "QFrame {"
        "   border: none;"
        "   background-color: #2d3446;"
        "   max-height: 1px;"
        "   margin: 4px 0px;"
        "}"
    );

    // Créer un widget pour les métadonnées (année et durée)
    QWidget* metadataWidget = new QWidget(this);
    QHBoxLayout* metadataLayout = new QHBoxLayout(metadataWidget);
    metadataLayout->setContentsMargins(0, 0, 0, 0);
    metadataLayout->setSpacing(8);

    // Année du film avec icône
    QLabel* yearIconLabel = new QLabel("📅", this);
    yearIconLabel->setStyleSheet("font-size: 12px; color: #8a9cbc;");
    m_yearLabel = new QLabel(QString::number(m_year), this);
    m_yearLabel->setStyleSheet(
        "QLabel { "
        "   color: #8a9cbc; "
        "   font-size: 12px; "
        "   font-weight: medium; "
        "   background-color: transparent; "
        "}"
    );

    // Durée du film avec icône
    QLabel* durationIconLabel = new QLabel("⏱️", this);
    durationIconLabel->setStyleSheet("font-size: 12px; color: #8a9cbc;");
    m_durationLabel = new QLabel(QString("%1 min").arg(m_duration), this);
    m_durationLabel->setStyleSheet(
        "QLabel { "
        "   color: #8a9cbc; "
        "   font-size: 12px; "
        "   font-weight: medium; "
        "   background-color: transparent; "
        "}"
    );

    // Ajouter les widgets de métadonnées au layout
    QHBoxLayout* yearLayout = new QHBoxLayout();
    yearLayout->setContentsMargins(0, 0, 0, 0);
    yearLayout->setSpacing(2);
    yearLayout->addWidget(yearIconLabel);
    yearLayout->addWidget(m_yearLabel);

    QHBoxLayout* durationLayout = new QHBoxLayout();
    durationLayout->setContentsMargins(0, 0, 0, 0);
    durationLayout->setSpacing(2);
    durationLayout->addWidget(durationIconLabel);
    durationLayout->addWidget(m_durationLabel);

    metadataLayout->addLayout(yearLayout);
    metadataLayout->addStretch();
    metadataLayout->addLayout(durationLayout);

    // Note du film avec style professionnel et bouton cast
    QWidget* ratingWidget = new QWidget(this);
    QHBoxLayout* ratingLayout = new QHBoxLayout(ratingWidget);
    ratingLayout->setContentsMargins(0, 0, 0, 0);
    ratingLayout->setSpacing(4);

    // Créer une étoile dorée pour la note
    QLabel* starLabel = new QLabel("★", this);
    starLabel->setStyleSheet(
        "QLabel { "
        "   color: #f1c40f; "
        "   font-size: 16px; "
        "   font-weight: bold; "
        "   background-color: transparent; "
        "}"
    );

    m_ratingLabel = new QLabel(QString("%1/10").arg(m_rating, 0, 'f', 1), this);
    m_ratingLabel->setStyleSheet(
        "QLabel { "
        "   color: #f1c40f; "
        "   font-weight: bold; "
        "   font-size: 14px; "
        "   background-color: transparent; "
        "}"
    );

    // Create cast button with about.png icon
    m_castButton = new QPushButton("", this);
    m_castButton->setToolTip("View Cast & Crew");
    m_castButton->setCursor(Qt::PointingHandCursor);
    m_castButton->setFixedSize(34, 34);

    // Debug: Check if resource file is accessible
    QFile resourceFile(":/images/about.png");
    qDebug() << "Resource file exists:" << resourceFile.exists();

    // Try multiple methods to load the icon
    QIcon castIcon(":/images/about.png");
    QPixmap testPixmap(":/images/about.png");

    qDebug() << "QIcon isNull:" << castIcon.isNull();
    qDebug() << "QPixmap isNull:" << testPixmap.isNull();
    qDebug() << "QPixmap size:" << testPixmap.size();

    // Method 1: Try QIcon first (most reliable)
    if (!castIcon.isNull() && !castIcon.pixmap(22, 22).isNull()) {
        m_castButton->setIcon(castIcon);
        m_castButton->setIconSize(QSize(22, 22));
        qDebug() << "✓ Cast icon loaded successfully using QIcon";
    }
    // Method 2: Try direct pixmap
    else if (!testPixmap.isNull()) {
        QIcon pixmapIcon(testPixmap);
        m_castButton->setIcon(pixmapIcon);
        m_castButton->setIconSize(QSize(22, 22));
        qDebug() << "✓ Cast icon loaded using QPixmap method";
    }
    // Method 3: CSS background-image with fallback
    else {
        qDebug() << "⚠ QIcon and QPixmap failed, trying CSS background-image";

        // First try CSS with the resource
        if (resourceFile.exists()) {
            qDebug() << "Resource exists, using CSS background-image";
            m_castButton->setStyleSheet(
                "QPushButton {"
                "   background-color: #2c3e50;"
                "   color: white;"
                "   border-radius: 17px;"
                "   border: 2px solid #34495e;"
                "   background-image: url(:/images/about.png);"
                "   background-position: center center;"
                "   background-repeat: no-repeat;"
                "   background-size: 16px 16px;"
                "   padding: 0px;"
                "   margin: 0px;"
                "}"
                "QPushButton:hover {"
                "   background-color: #3498db;"
                "   border: 2px solid #2980b9;"
                "   background-image: url(:/images/about.png);"
                "   background-position: center center;"
                "   background-repeat: no-repeat;"
                "   background-size: 16px 16px;"
                "}"
                "QPushButton:pressed {"
                "   background-color: #2980b9;"
                "   border: 2px solid #1f618d;"
                "   background-image: url(:/images/about.png);"
                "   background-position: center center;"
                "   background-repeat: no-repeat;"
                "   background-size: 16px 16px;"
                "}"
            );
        } else {
            // Ultimate fallback: Use Unicode icon
            qDebug() << "⚠ Resource not found, using Unicode fallback";
            m_castButton->setText("👥");
            m_castButton->setStyleSheet(
                "QPushButton {"
                "   background-color: #2c3e50;"
                "   color: white;"
                "   border-radius: 17px;"
                "   border: 2px solid #34495e;"
                "   font-size: 16px;"
                "   font-weight: bold;"
                "   padding: 0px;"
                "}"
                "QPushButton:hover {"
                "   background-color: #3498db;"
                "   border: 2px solid #2980b9;"
                "}"
                "QPushButton:pressed {"
                "   background-color: #2980b9;"
                "   border: 2px solid #1f618d;"
                "}"
            );
        }
        return; // Exit early since we set the style here
    }

    m_castButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #2c3e50;"
        "   color: white;"
        "   border-radius: 17px;"
        "   border: 2px solid #34495e;"
        "   padding: 2px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3498db;"
        "   border: 2px solid #2980b9;"
        "   transform: scale(1.05);"
        "}"
        "QPushButton:pressed {"
        "   background-color: #2980b9;"
        "   border: 2px solid #1f618d;"
        "   transform: scale(0.95);"
        "}"
    );
    connect(m_castButton, &QPushButton::clicked, this, &MovieWidget::onCastClicked);

    ratingLayout->addWidget(starLabel);
    ratingLayout->addWidget(m_ratingLabel);
    ratingLayout->addStretch();
    ratingLayout->addWidget(m_castButton);

    // Créer le bouton de bande-annonce
    m_trailerButton = new QPushButton("▶ Bande-annonce", this);
    m_trailerButton->setCursor(Qt::PointingHandCursor);
    m_trailerButton->setFixedWidth(164);  // 180 - 2*8 (marges du conteneur)
    m_trailerButton->setStyleSheet(
        "QPushButton {"
        "   background-color: #000000;"
        "   color: #ffffff;"
        "   border: none;"
        "   border-radius: 4px;"
        "   padding: 6px 10px;"
        "   font-size: 12px;"
        "   font-weight: bold;"
        "   text-align: center;"
        "}"
        "QPushButton:hover {"
        "   background-color: #333333;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #555555;"
        "}"
    );
    connect(m_trailerButton, &QPushButton::clicked, this, &MovieWidget::onTrailerClicked);

    // Ajouter tous les widgets au conteneur d'informations
    infoContainerLayout->addWidget(separator);
    infoContainerLayout->addWidget(metadataWidget);
    infoContainerLayout->addWidget(ratingWidget);
    infoContainerLayout->addWidget(m_trailerButton);

    // Créer un conteneur principal pour l'image et les informations
    QWidget* cardContainer = new QWidget(this);
    cardContainer->setObjectName("cardContainer");

    QVBoxLayout* cardLayout = new QVBoxLayout(cardContainer);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);  // Pas d'espace entre l'image et le conteneur d'infos
    cardLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);  // Alignement en haut et centré horizontalement

    // Ajouter l'image et le conteneur d'informations au conteneur de carte
    cardLayout->addWidget(imageContainer);
    cardLayout->addWidget(infoContainer);

    // Ajouter le conteneur de carte au layout principal
    mainLayout->addWidget(cardContainer);

    // Connect signals
    connect(m_favoriteButton, &QPushButton::clicked, this, &MovieWidget::onFavoriteClicked);
    connect(m_watchlistButton, &QPushButton::clicked, this, &MovieWidget::onWatchlistClicked);
    connect(m_imageLabel, &QLabel::linkActivated, [this](const QString&) {
        emit movieClicked(m_movieId);
    });
    connect(m_trailerButton, &QPushButton::clicked, this, &MovieWidget::onTrailerClicked);

    // Make the whole widget clickable with professional styling
    this->setStyleSheet(
        "MovieWidget { "
        "   background-color: transparent; "
        "   border-radius: 10px; "
        "   padding: 0px; "
        "   margin: 0px; "  // Suppression des marges
        "   transition: all 0.3s ease; "
        "} "
        "MovieWidget:hover { "
        "   transform: translateY(-5px); "
        "} "
        "QLabel { "
        "   background-color: transparent; "
        "   color: white; "
        "} "
    );

    // Ajouter un style au conteneur de carte pour une meilleure apparence
    cardContainer->setStyleSheet(
        "QWidget#cardContainer {"
        "   background-color: transparent;"
        "   border-radius: 0px;"
        "   margin: 0px;"
        "   padding: 0px;"
        "}"
    );

    // Ajouter un effet d'ombre pour donner de la profondeur
    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(15);
    shadowEffect->setColor(QColor(0, 0, 0, 80));
    shadowEffect->setOffset(0, 5);
    this->setGraphicsEffect(shadowEffect);

    this->setCursor(Qt::PointingHandCursor);
}

void MovieWidget::updateFavoriteButton()
{
    if (m_inFavorites) {
        m_favoriteButton->setText("♥");
        m_favoriteButton->setToolTip("Retirer des favoris");
        m_favoriteButton->setStyleSheet(
            "QPushButton {"
            "   background-color: rgba(61, 90, 241, 0.8);"
            "   color: #ff3366;"
            "   border-radius: 18px;"
            "   font-size: 18px;"
            "   font-weight: bold;"
            "   text-align: center;"
            "   opacity: 1;"
            "}"
            "QPushButton:hover {"
            "   background-color: rgba(41, 70, 221, 0.9);"
            "   color: white;"
            "}"
        );
    } else {
        m_favoriteButton->setText("♥");
        m_favoriteButton->setToolTip("Ajouter aux favoris");
        m_favoriteButton->setStyleSheet(
            "QPushButton {"
            "   background-color: rgba(0, 0, 0, 0.6);"
            "   color: white;"
            "   border-radius: 18px;"
            "   font-size: 18px;"
            "   font-weight: bold;"
            "   text-align: center;"
            "   opacity: 0;"
            "}"
            "QPushButton:hover {"
            "   background-color: rgba(61, 90, 241, 0.8);"
            "   color: white;"
            "   opacity: 1;"
            "}"
        );
    }
}

void MovieWidget::updateWatchlistButton()
{
    if (m_inWatchlist) {
        m_watchlistButton->setText("-");
        m_watchlistButton->setToolTip("Retirer de la liste de visionnage");
        m_watchlistButton->setStyleSheet(
            "QPushButton {"
            "   background-color: rgba(61, 90, 241, 0.8);"
            "   color: white;"
            "   border-radius: 18px;"
            "   font-size: 20px;"
            "   font-weight: bold;"
            "}"
            "QPushButton:hover {"
            "   background-color: rgba(41, 70, 221, 0.9);"
            "}"
        );
    } else {
        m_watchlistButton->setText("+");
        m_watchlistButton->setToolTip("Ajouter à la liste de visionnage");
        m_watchlistButton->setStyleSheet(
            "QPushButton {"
            "   background-color: rgba(0, 0, 0, 0.6);"
            "   color: white;"
            "   border-radius: 18px;"
            "   font-size: 20px;"
            "   font-weight: bold;"
            "}"
            "QPushButton:hover {"
            "   background-color: rgba(61, 90, 241, 0.8);"
            "}"
        );
    }
}

void MovieWidget::onFavoriteClicked()
{
    m_inFavorites = !m_inFavorites;
    updateFavoriteButton();
    emit favoriteClicked(m_movieId);
}

void MovieWidget::onWatchlistClicked()
{
    m_inWatchlist = !m_inWatchlist;
    updateWatchlistButton();
    emit watchlistClicked(m_movieId);
}

void MovieWidget::onTrailerClicked()
{
    // Émettre le signal pour demander la bande-annonce
    emit trailerClicked(m_movieId);
}

void MovieWidget::onCastClicked()
{
    // Émettre le signal pour afficher les informations de casting
    emit castClicked(m_movieId);
}

void MovieWidget::loadImage()
{
    // Check if we have a local path first
    m_localImagePath = getLocalImagePath();

    if (QFile::exists(m_localImagePath)) {
        // Load from local file
        QPixmap pixmap(m_localImagePath);
        if (!pixmap.isNull()) {
            m_imageLabel->setPixmap(pixmap);
            return;
        }
    }

    // If the image path starts with http, it's a remote URL
    if (m_imagePath.startsWith("http")) {
        downloadImage();
    } else if (!m_imagePath.isEmpty() && QFile::exists(m_imagePath)) {
        // It's a local file path
        QPixmap pixmap(m_imagePath);
        if (!pixmap.isNull()) {
            m_imageLabel->setPixmap(pixmap);
        }
    }
}

void MovieWidget::downloadImage()
{
    QUrl url(m_imagePath);
    QNetworkRequest request(url);

    qDebug() << "Downloading image from:" << url.toString();

    m_imageReply = m_networkManager->get(request);
    connect(m_imageReply, &QNetworkReply::finished, this, &MovieWidget::onImageDownloaded);
}

void MovieWidget::onImageDownloaded()
{
    if (m_imageReply->error() == QNetworkReply::NoError) {
        QByteArray imageData = m_imageReply->readAll();

        // Save the image to a local file
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/MovieApp");
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        QFile file(m_localImagePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(imageData);
            file.close();

            // Load the saved image
            QPixmap pixmap;
            if (pixmap.loadFromData(imageData)) {
                m_imageLabel->setPixmap(pixmap);

                // Repositionner les boutons après le chargement de l'image
                m_watchlistButton->move(m_imageLabel->width() - m_watchlistButton->width() - 5, 5);
                m_favoriteButton->move(m_imageLabel->width() - m_favoriteButton->width() - 5,
                                      m_imageLabel->height() - m_favoriteButton->height() - 5);
            }
        }
    } else {
        qDebug() << "Error downloading image:" << m_imageReply->errorString();
    }

    m_imageReply->deleteLater();
    m_imageReply = nullptr;
}

QString MovieWidget::getLocalImagePath()
{
    // Create a unique filename based on the API ID (not the local ID)
    // This ensures that the same movie always gets the same image file
    QString fileName = QString("movie_api_%1.jpg").arg(m_apiId);

    // Get the path to the Pictures location
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/MovieApp");
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    qDebug() << "Local image path for movie" << m_title << "(API ID:" << m_apiId << "):" << dir.absoluteFilePath(fileName);
    return dir.absoluteFilePath(fileName);
}
