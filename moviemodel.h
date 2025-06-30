#ifndef MOVIEMODEL_H
#define MOVIEMODEL_H

#include <QAbstractTableModel>
#include <QSqlQuery>
#include <QList>
#include <QVariant>

struct Movie {
    int id;         // ID local dans la base de données
    int apiId;      // ID de l'API TMDb
    QString title;
    int year;
    double rating;
    int duration;
    bool inFavorites;
    bool inWatchlist;
    QString imagePath;
    QStringList genres;  // List of genres for the movie
};

class MovieModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Columns {
        Title,
        Year,
        Rating,
        Duration,
        Favorite,
        Watchlist,
        ColumnCount
    };

    explicit MovieModel(QObject *parent = nullptr);

    // Model interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Movie operations
    void loadMovies(const QString &sortBy = "title", int userId = -1);
    void loadFavorites(int userId);
    void loadWatchlist(int userId);
    void loadMoviesByGenre(const QString &genre, const QString &sortBy = "title", int userId = -1);
    void searchMovies(const QString &searchQuery, const QString &sortBy = "title", int userId = -1);

    int getMovieId(int row) const;
    int getApiId(int row) const;
    QString getImagePath(int row) const;
    QList<Movie> getMovies() const { return m_movies; }

private:
    QList<Movie> m_movies;
    int m_userId;
};

#endif // MOVIEMODEL_H
