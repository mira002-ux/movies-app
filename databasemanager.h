#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include "moviemodel.h"

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    static DatabaseManager& instance();

    bool initDatabase();
    bool createTables();

    // Authentication methods
    bool registerUser(const QString& username, const QString& password);
    bool loginUser(const QString& username, const QString& password);
    int getUserId(const QString& username);

    // Movie methods
    bool addMovie(const QString& title, int year, double rating, int duration, const QString& imagePath = "");
    bool addMovieFromApi(int apiId, const QString& title, int year, double rating, int duration, const QString& imagePath = "", const QStringList& genres = QStringList());
    bool updateMovieFromApi(int apiId, const QString& title, int year, double rating, int duration, const QString& imagePath = "", const QStringList& genres = QStringList());
    bool updateMovieRating(int movieId, double rating);
    bool addToFavorites(int userId, int movieId);
    bool addToWatchlist(int userId, int movieId);
    bool movieExists(int apiId);
    int getLocalMovieId(int apiId);
    bool updateMovieDuration(int apiId, int duration);
    bool movieHasDuration(int apiId);

    // Batch operations
    bool saveMoviesFromApi(const QList<Movie>& movies);

    // Queries
    QSqlQuery getMovies(const QString& sortBy = "title");
    QSqlQuery getFavorites(int userId);
    QSqlQuery getWatchlist(int userId);
    QSqlQuery getMoviesByGenre(const QString& genre, const QString& sortBy = "title");
    QSqlQuery searchMovies(const QString& searchQuery, const QString& sortBy = "title");
    QStringList getAllGenres();
    int getMovieCount();

private:
    DatabaseManager(QObject* parent = nullptr);
    ~DatabaseManager();

    QSqlDatabase m_db;
};

#endif // DATABASEMANAGER_H


