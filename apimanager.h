#ifndef APIMANAGER_H
#define APIMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QMap>
#include "moviemodel.h"

class ApiManager : public QObject
{
    Q_OBJECT

public:
    explicit ApiManager(QObject *parent = nullptr);
    ~ApiManager();

    // Fetch popular movies
    void fetchPopularMovies(int page = 1, int totalPages = 1);

    // Search for movies by title
    void searchMovies(const QString &query, int page = 1, int totalPages = 1);

    // Fetch movie details by ID
    void fetchMovieDetails(int movieId);

    // Fetch movie trailers by ID
    void fetchMovieTrailers(int movieId);

    // Batch fetch movie details for multiple movies
    void fetchMovieDetailsBatch(const QList<int> &movieIds);

signals:
    // Signal emitted when movies are fetched
    void moviesLoaded(const QList<Movie> &movies);

    // Signal emitted when movie details are fetched
    void movieDetailsLoaded(const Movie &movie);

    // Signal emitted when an error occurs
    void error(const QString &errorMessage);

    // Signal emitted when a movie trailer is found
    void movieTrailerFound(int movieId, const QString &trailerUrl);

private slots:
    void onPopularMoviesReply();
    void onSearchMoviesReply();
    void onMovieDetailsReply();
    void onMovieTrailersReply();

private:
    QNetworkAccessManager *m_networkManager;

    // TMDb API key - you'll need to register for one
    const QString m_apiKey = "process.env.REACT_APP_TMDB_API_KEY";

    // Base URL for TMDb API
    const QString m_baseUrl = "https://api.themoviedb.org/3";

    // Base URL for TMDb images
    const QString m_imageBaseUrl = "https://image.tmdb.org/t/p/w500";

    // Current search parameters for pagination
    QString m_currentSearchQuery;
    int m_currentPage;
    int m_totalPagesToFetch;
    QList<Movie> m_accumulatedMovies;
    int m_detailsToFetch;
    int m_detailsFetched;

    // Performance optimization variables
    int m_activeDetailRequests;
    static const int MAX_CONCURRENT_REQUESTS = 25;
    QList<int> m_pendingDetailRequests;

    // Parse movie data from JSON
    Movie parseMovieJson(const QJsonObject &movieObject);

    // Download movie poster
    void downloadMoviePoster(const QString &posterPath, Movie &movie);

    // Continue fetching next page
    void fetchNextPage();

    // Fetch details for all movies in the accumulated list
    void fetchDetailsForAllMovies();

    // Process pending detail requests
    void processPendingDetailRequests();

    // Fetch genre list from TMDb API
    void fetchGenres();
    void onGenresReply();

    // Map of genre IDs to genre names
    QMap<int, QString> m_genreMap;

public:
    // Check if genres are loaded
    bool areGenresLoaded() const { return !m_genreMap.isEmpty(); }

signals:
    void genresLoaded();
};

#endif // APIMANAGER_H
