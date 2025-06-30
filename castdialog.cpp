#include "castdialog.h"
#include <QUrl>
#include <QUrlQuery>
#include <QPixmap>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDesktopServices>
#include <QGraphicsDropShadowEffect>
#include <QPushButton>

// CastMemberWidget implementation
CastMemberWidget::CastMemberWidget(const QString &name, const QString &character, const QString &profilePath, QWidget *parent)
    : QWidget(parent)
{
    m_networkManager = new QNetworkAccessManager(this);

    // Create layout
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(4);

    // Create image label
    m_imageLabel = new QLabel(this);
    m_imageLabel->setFixedSize(110, 165);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet(
        "QLabel {"
        "   background-color: #1a1a1a;"
        "   border-radius: 6px;"
        "   color: #cccccc;"
        "}"
    );

    // Add drop shadow effect to image
    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(10);
    shadowEffect->setColor(QColor(0, 0, 0, 160));
    shadowEffect->setOffset(0, 2);
    m_imageLabel->setGraphicsEffect(shadowEffect);

    // Create name label
    m_nameLabel = new QLabel(name, this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setFixedHeight(36);
    m_nameLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-weight: bold;"
        "   font-size: 12px;"
        "}"
    );

    // Create character label
    m_characterLabel = new QLabel(character, this);
    m_characterLabel->setAlignment(Qt::AlignCenter);
    m_characterLabel->setWordWrap(true);
    m_characterLabel->setFixedHeight(32);
    m_characterLabel->setStyleSheet(
        "QLabel {"
        "   color: #8a9cbc;"
        "   font-size: 11px;"
        "   font-style: italic;"
        "}"
    );

    // Add widgets to layout
    layout->addWidget(m_imageLabel);
    layout->addWidget(m_nameLabel);
    layout->addWidget(m_characterLabel);

    // Load image
    loadImage(profilePath);

    // Set widget style
    setStyleSheet(
        "CastMemberWidget {"
        "   background-color: #2c3e50;"
        "   border-radius: 8px;"
        "   padding: 5px;"
        "}"
    );

    // Set fixed size
    setFixedWidth(120);
}

void CastMemberWidget::loadImage(const QString &profilePath)
{
    if (profilePath.isEmpty()) {
        // Set placeholder image
        m_imageLabel->setText("No Image");
        return;
    }

    // Check if image exists in cache
    QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/cast";
    QDir cacheDir(cachePath);
    if (!cacheDir.exists()) {
        cacheDir.mkpath(".");
    }

    QString fileName = profilePath.mid(1); // Remove leading slash
    QString filePath = cachePath + "/" + fileName;

    if (QFile::exists(filePath)) {
        // Load from cache
        QPixmap pixmap(filePath);
        m_imageLabel->setPixmap(pixmap.scaled(m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        // Download from TMDb
        QString imageUrl = "https://image.tmdb.org/t/p/w185" + profilePath;
        QNetworkRequest request(imageUrl);
        QNetworkReply *reply = m_networkManager->get(request);

        connect(reply, &QNetworkReply::finished, [=]() {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray data = reply->readAll();
                QPixmap pixmap;
                pixmap.loadFromData(data);

                // Save to cache
                QFile file(filePath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(data);
                    file.close();
                }

                // Display image
                m_imageLabel->setPixmap(pixmap.scaled(m_imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                m_imageLabel->setText("Error");
            }

            reply->deleteLater();
        });
    }
}

// CastDialog implementation
CastDialog::CastDialog(int apiMovieId, const QString &movieTitle, QWidget *parent)
    : QDialog(parent),
      m_apiMovieId(apiMovieId),
      m_movieTitle(movieTitle),
      m_networkManager(nullptr),
      m_castReply(nullptr),
      m_crewReply(nullptr)
{
    m_networkManager = new QNetworkAccessManager(this);

    setupUi();
    fetchCastData();

    // Set window properties
    setWindowTitle("Cast & Crew - " + movieTitle);
    resize(900, 700);
    setMinimumSize(800, 600);

    // Center the dialog on the parent
    if (parent) {
        QRect parentGeometry = parent->geometry();
        int x = parentGeometry.x() + (parentGeometry.width() - width()) / 2;
        int y = parentGeometry.y() + (parentGeometry.height() - height()) / 2;
        move(x, y);
    }
}

CastDialog::~CastDialog()
{
}

void CastDialog::setupUi()
{
    // Create main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(15);

    // Create title label
    m_titleLabel = new QLabel("Cast & Crew - " + m_movieTitle, this);
    m_titleLabel->setStyleSheet(
        "QLabel {"
        "   color: white;"
        "   font-size: 18px;"
        "   font-weight: bold;"
        "   padding: 10px;"
        "   background-color: #1a2530;"
        "   border-radius: 5px;"
        "}"
    );
    m_titleLabel->setAlignment(Qt::AlignCenter);

    // Create tab widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane {"
        "   border: 1px solid #34495e;"
        "   background-color: #2c3e50;"
        "   border-radius: 5px;"
        "}"
        "QTabBar::tab {"
        "   background-color: #34495e;"
        "   color: #ecf0f1;"
        "   padding: 8px 20px;"
        "   border-top-left-radius: 5px;"
        "   border-top-right-radius: 5px;"
        "   margin-right: 2px;"
        "}"
        "QTabBar::tab:selected {"
        "   background-color: #2c3e50;"
        "   border-bottom: 2px solid #3498db;"
        "}"
        "QTabBar::tab:hover {"
        "   background-color: #3498db;"
        "}"
    );

    // Create cast tab with scroll area
    m_castTab = new QWidget();
    QVBoxLayout* castMainLayout = new QVBoxLayout(m_castTab);

    QScrollArea* castScrollArea = new QScrollArea();
    castScrollArea->setWidgetResizable(true);
    castScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    castScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    castScrollArea->setFrameShape(QFrame::NoFrame);

    QWidget* castScrollContent = new QWidget();
    m_castLayout = new QGridLayout(castScrollContent);
    m_castLayout->setSpacing(15);
    m_castLayout->setContentsMargins(15, 15, 15, 15);

    castScrollArea->setWidget(castScrollContent);
    castMainLayout->addWidget(castScrollArea);

    // Create crew tab with scroll area
    m_crewTab = new QWidget();
    QVBoxLayout* crewMainLayout = new QVBoxLayout(m_crewTab);

    QScrollArea* crewScrollArea = new QScrollArea();
    crewScrollArea->setWidgetResizable(true);
    crewScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    crewScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    crewScrollArea->setFrameShape(QFrame::NoFrame);

    QWidget* crewScrollContent = new QWidget();
    m_crewLayout = new QGridLayout(crewScrollContent);
    m_crewLayout->setSpacing(15);
    m_crewLayout->setContentsMargins(15, 15, 15, 15);

    crewScrollArea->setWidget(crewScrollContent);
    crewMainLayout->addWidget(crewScrollArea);

    // Add tabs to tab widget
    m_tabWidget->addTab(m_castTab, "Cast");
    m_tabWidget->addTab(m_crewTab, "Crew");

    // Create progress bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 0); // Indeterminate
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "   border: 1px solid #34495e;"
        "   border-radius: 5px;"
        "   background-color: #2c3e50;"
        "   height: 10px;"
        "}"
        "QProgressBar::chunk {"
        "   background-color: #3498db;"
        "   border-radius: 5px;"
        "}"
    );

    // Create status label
    m_statusLabel = new QLabel("Loading cast and crew information...", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet("color: #bdc3c7;");

    // Add widgets to main layout
    m_mainLayout->addWidget(m_titleLabel);
    m_mainLayout->addWidget(m_tabWidget);
    m_mainLayout->addWidget(m_progressBar);
    m_mainLayout->addWidget(m_statusLabel);

    // Set dialog style
    setStyleSheet(
        "QDialog {"
        "   background-color: #1a2530;"
        "}"
    );
}

void CastDialog::fetchCastData()
{
    // Create URL for cast request using the API ID
    QUrl castUrl(m_baseUrl + "/movie/" + QString::number(m_apiMovieId) + "/credits");
    QUrlQuery query;
    query.addQueryItem("api_key", m_apiKey);
    query.addQueryItem("language", "en-US");
    castUrl.setQuery(query);

    qDebug() << "Fetching cast data for movie ID:" << m_apiMovieId << "(" << m_movieTitle << ")";

    // Create request
    QNetworkRequest request(castUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Send request
    m_castReply = m_networkManager->get(request);

    // Connect signals
    connect(m_castReply, &QNetworkReply::finished, this, &CastDialog::onCastDataReceived);
    connect(m_castReply, &QNetworkReply::errorOccurred, this, &CastDialog::onNetworkError);
}

void CastDialog::onCastDataReceived()
{
    // Check for errors
    if (m_castReply->error() != QNetworkReply::NoError) {
        showError("Failed to load cast data: " + m_castReply->errorString());
        qDebug() << "Network error when fetching cast data:" << m_castReply->errorString();
        m_castReply->deleteLater();
        m_castReply = nullptr;
        return;
    }

    // Parse JSON response
    QByteArray data = m_castReply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isNull() || !doc.isObject()) {
        showError("Invalid cast data received");
        qDebug() << "Invalid JSON response for cast data";
        m_castReply->deleteLater();
        m_castReply = nullptr;
        return;
    }

    qDebug() << "Successfully received cast data for movie:" << m_movieTitle;

    // Get cast and crew arrays
    QJsonObject obj = doc.object();
    QJsonArray castArray = obj["cast"].toArray();
    QJsonArray crewArray = obj["crew"].toArray();

    // Display cast and crew
    displayCastMembers(castArray);
    displayCrewMembers(crewArray);

    // Hide progress bar and update status
    m_progressBar->hide();

    int castCount = castArray.size();
    int crewCount = crewArray.size();
    m_statusLabel->setText(QString("Loaded %1 cast members and %2 crew members").arg(castCount).arg(crewCount));

    // Clean up
    m_castReply->deleteLater();
    m_castReply = nullptr;
}

void CastDialog::onCrewDataReceived()
{
    // This method is reserved for future use if we need to fetch crew data separately
}

void CastDialog::onNetworkError(QNetworkReply::NetworkError error)
{
    showError("Network error: " + QString::number(error));
}

void CastDialog::displayCastMembers(const QJsonArray &castArray)
{
    // Clear existing widgets
    QLayoutItem *item;
    while ((item = m_castLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    // Display all cast members
    int count = castArray.size();
    int row = 0;
    int col = 0;
    int maxCols = 5; // 5 cast members per row

    for (int i = 0; i < count; i++) {
        QJsonObject castObj = castArray[i].toObject();
        QString name = castObj["name"].toString();
        QString character = castObj["character"].toString();
        QString profilePath = castObj["profile_path"].toString();

        CastMemberWidget *widget = new CastMemberWidget(name, character, profilePath, this);
        m_castLayout->addWidget(widget, row, col, 1, 1, Qt::AlignCenter);

        col++;
        if (col >= maxCols) {
            col = 0;
            row++;
        }
    }
}

void CastDialog::displayCrewMembers(const QJsonArray &crewArray)
{
    // Clear existing widgets
    QLayoutItem *item;
    while ((item = m_crewLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    // Filter crew by department and importance
    QJsonArray directors;
    QJsonArray producers;
    QJsonArray writers;
    QJsonArray cinematographers;
    QJsonArray composers;

    for (const QJsonValue &value : crewArray) {
        QJsonObject crewObj = value.toObject();
        QString department = crewObj["department"].toString();
        QString job = crewObj["job"].toString();

        if (department == "Directing" && job == "Director") {
            directors.append(crewObj);
        } else if (department == "Production" && (job == "Producer" || job == "Executive Producer")) {
            producers.append(crewObj);
        } else if (department == "Writing" && (job == "Screenplay" || job == "Writer" || job == "Story")) {
            writers.append(crewObj);
        } else if (department == "Camera" && job == "Director of Photography") {
            cinematographers.append(crewObj);
        } else if (department == "Sound" && job == "Original Music Composer") {
            composers.append(crewObj);
        }
    }

    // Display crew members by department
    int row = 0;
    int col = 0;
    int maxCols = 5; // 5 crew members per row

    // Add department headers and crew members
    QStringList departments = {"Directors", "Writers", "Producers", "Cinematographers", "Composers"};
    QList<QJsonArray> departmentArrays = {directors, writers, producers, cinematographers, composers};

    for (int i = 0; i < departments.size(); i++) {
        // Skip empty departments
        if (departmentArrays[i].isEmpty()) {
            continue;
        }

        // Add department header
        QLabel *headerLabel = new QLabel(departments[i], this);
        headerLabel->setStyleSheet(
            "QLabel {"
            "   color: #3498db;"
            "   font-size: 16px;"
            "   font-weight: bold;"
            "   padding: 5px;"
            "   border-bottom: 1px solid #3498db;"
            "}"
        );
        m_crewLayout->addWidget(headerLabel, row, 0, 1, maxCols);
        row++;
        col = 0;

        // Add crew members
        for (const QJsonValue &value : departmentArrays[i]) {
            QJsonObject crewObj = value.toObject();
            QString name = crewObj["name"].toString();
            QString job = crewObj["job"].toString();
            QString profilePath = crewObj["profile_path"].toString();

            CastMemberWidget *widget = new CastMemberWidget(name, job, profilePath, this);
            m_crewLayout->addWidget(widget, row, col, 1, 1, Qt::AlignCenter);

            col++;
            if (col >= maxCols) {
                col = 0;
                row++;
            }
        }

        // Add spacing between departments
        QSpacerItem *spacer = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed);
        m_crewLayout->addItem(spacer, row, 0, 1, maxCols);
        row++;
        col = 0;
    }
}

void CastDialog::showError(const QString &message)
{
    m_progressBar->hide();
    m_statusLabel->setText("Error: " + message);
    m_statusLabel->setStyleSheet("color: #e74c3c;");
}
