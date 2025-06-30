#ifndef MOVIEWIDGET_H
#define MOVIEWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDir>
#include <QStandardPaths>

class MovieWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MovieWidget(int movieId, int apiId, const QString &title, int year, double rating,
                         int duration, bool inFavorites, bool inWatchlist,
                         const QString &imagePath, QWidget *parent = nullptr);

    int movieId() const;
    int apiId() const;
    void updateMovieData(double rating, int duration);

signals:
    void favoriteClicked(int movieId);
    void watchlistClicked(int movieId);
    void movieClicked(int movieId);
    void trailerClicked(int movieId);
    void castClicked(int movieId);

private slots:
    void onFavoriteClicked();
    void onWatchlistClicked();
    void onTrailerClicked();
    void onCastClicked();
    void onImageDownloaded();

private:
    int m_movieId;      // ID local dans la base de données
    int m_apiId;        // ID de l'API TMDb
    QString m_title;
    int m_year;
    double m_rating;
    int m_duration;
    bool m_inFavorites;
    bool m_inWatchlist;
    QString m_imagePath;

    QLabel *m_imageLabel;
    QLabel *m_titleLabel;
    QLabel *m_yearLabel;
    QLabel *m_ratingLabel;
    QLabel *m_durationLabel;
    QPushButton *m_favoriteButton;
    QPushButton *m_watchlistButton;
    QPushButton *m_trailerButton;
    QPushButton *m_castButton;

    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_imageReply;
    QString m_localImagePath;

    void setupUi();
    void updateFavoriteButton();
    void updateWatchlistButton();
    void loadImage();
    void downloadImage();
    QString getLocalImagePath();
};

#endif // MOVIEWIDGET_H
