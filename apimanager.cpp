#include "apimanager.h"
#include "databasemanager.h"
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QTimer>

ApiManager::ApiManager(QObject *parent)
    : QObject(parent),
      m_networkManager(nullptr),
      m_currentPage(1),
      m_totalPagesToFetch(1),
      m_detailsToFetch(0),
      m_detailsFetched(0),
      m_activeDetailRequests(0)
{
    m_networkManager = new QNetworkAccessManager(this);

    // Optimize network manager for better performance
    m_networkManager->setTransferTimeout(10000); // 10 second timeout

    // Fetch genres when the API manager is created
    fetchGenres();
}

ApiManager::~ApiManager()
{
}

void ApiManager::fetchPopularMovies(int page, int totalPages)
{
    // Verify API key is set
    if (m_apiKey == "YOUR_API_KEY_HERE" || m_apiKey.isEmpty()) {
        qDebug() << "ERROR: API key not set! Please set your TMDb API key in apimanager.h";
        emit error("API key not set. Please set your TMDb API key.");
        return;
    }

    // Reset accumulated movies if this is a new request (page 1)
    if (page == 1) {
        m_accumulatedMovies.clear();
        m_currentSearchQuery = "";
        m_currentPage = 1;
        m_totalPagesToFetch = totalPages;
    }

    QUrl url(m_baseUrl + "/movie/popular");
    QUrlQuery query;
    query.addQueryItem("api_key", m_apiKey);
    query.addQueryItem("language", "en-US");
    query.addQueryItem("page", QString::number(page));
    url.setQuery(query);

    qDebug() << "Fetching popular movies from URL:" << url.toString(QUrl::RemoveQuery)
             << " (API key hidden) - Page" << page << "of" << totalPages;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_networkManager->get(request);

    // Connect to error signals to debug network issues
    connect(reply, &QNetworkReply::errorOccurred, this, [this, reply](QNetworkReply::NetworkError error) {
        qDebug() << "Network error occurred:" << error << "-" << reply->errorString();
    });

    connect(reply, &QNetworkReply::finished, this, &ApiManager::onPopularMoviesReply);
}

void ApiManager::searchMovies(const QString &query, int page, int totalPages)
{
    // Verify API key is set
    if (m_apiKey == "YOUR_API_KEY_HERE" || m_apiKey.isEmpty()) {
        qDebug() << "ERROR: API key not set! Please set your TMDb API key in apimanager.h";
        emit error("API key not set. Please set your TMDb API key.");
        return;
    }

    // Reset accumulated movies if this is a new request (page 1)
    if (page == 1) {
        m_accumulatedMovies.clear();
        m_currentSearchQuery = query;
        m_currentPage = 1;
        m_totalPagesToFetch = totalPages;
    }

    QUrl url(m_baseUrl + "/search/movie");
    QUrlQuery urlQuery;
    urlQuery.addQueryItem("api_key", m_apiKey);
    urlQuery.addQueryItem("language", "en-US");
    urlQuery.addQueryItem("query", query);
    urlQuery.addQueryItem("page", QString::number(page));
    url.setQuery(urlQuery);

    qDebug() << "Searching movies with query:" << query << "at URL:" << url.toString(QUrl::RemoveQuery)
             << " - Page" << page << "of" << totalPages;

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_networkManager->get(request);

    // Connect to error signals to debug network issues
    connect(reply, &QNetworkReply::errorOccurred, this, [this, reply](QNetworkReply::NetworkError error) {
        qDebug() << "Network error occurred:" << error << "-" << reply->errorString();
    });

    connect(reply, &QNetworkReply::finished, this, &ApiManager::onSearchMoviesReply);
}

void ApiManager::fetchMovieDetails(int movieId)
{
    QUrl url(m_baseUrl + "/movie/" + QString::number(movieId));
    QUrlQuery query;
    query.addQueryItem("api_key", m_apiKey);
    query.addQueryItem("language", "en-US");
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, &ApiManager::onMovieDetailsReply);
}

void ApiManager::fetchMovieDetailsBatch(const QList<int> &movieIds)
{
    // Store pending requests and start processing
    m_pendingDetailRequests = movieIds;
    m_detailsToFetch = movieIds.size();
    m_detailsFetched = 0;
    m_activeDetailRequests = 0;

    qDebug() << "Starting batch fetch for" << movieIds.size() << "movies";

    // Start processing with high concurrency
    processPendingDetailRequests();
}

void ApiManager::fetchMovieTrailers(int movieId)
{
    QUrl url(m_baseUrl + "/movie/" + QString::number(movieId) + "/videos");
    QUrlQuery query;
    query.addQueryItem("api_key", m_apiKey);
    query.addQueryItem("language", "en-US");
    url.setQuery(query);

    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager->get(request);

    // Store the movie ID in the reply's property so we can access it in the slot
    reply->setProperty("movieId", movieId);

    connect(reply, &QNetworkReply::finished, this, &ApiManager::onMovieTrailersReply);
}

void ApiManager::onPopularMoviesReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        qDebug() << "Error: Invalid reply object";
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = "Network error: " + reply->errorString();
        qDebug() << errorMsg;
        emit error(errorMsg);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    qDebug() << "Received data size:" << data.size() << "bytes";

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        QString errorMsg = "Invalid JSON response";
        qDebug() << errorMsg;
        qDebug() << "Response data:" << data;
        emit error(errorMsg);
        reply->deleteLater();
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();

    // Check for API errors
    if (jsonObj.contains("status_code") && jsonObj.contains("status_message")) {
        QString errorMsg = "API Error: " + jsonObj["status_message"].toString();
        qDebug() << errorMsg;
        emit error(errorMsg);
        reply->deleteLater();
        return;
    }

    // Get total pages information
    int totalPages = jsonObj["total_pages"].toInt();
    int currentPage = jsonObj["page"].toInt();

    qDebug() << "Processing page" << currentPage << "of" << totalPages
             << "(requested to fetch" << m_totalPagesToFetch << "pages)";

    QJsonArray results = jsonObj["results"].toArray();
    qDebug() << "Found" << results.size() << "movies in response";

    // Add movies from this page to our accumulated list
    for (const QJsonValue &value : results) {
        QJsonObject movieObj = value.toObject();
        Movie movie = parseMovieJson(movieObj);
        m_accumulatedMovies.append(movie);
    }

    // Check if we need to fetch more pages
    if (currentPage < m_totalPagesToFetch && currentPage < totalPages) {
        m_currentPage = currentPage + 1;
        qDebug() << "Fetching next page:" << m_currentPage;

        // Fetch next page immediately for faster loading
        fetchNextPage();
    } else {
        // We've fetched all pages, now fetch details for each movie
        qDebug() << "Finished fetching pages. Fetching details for" << m_accumulatedMovies.size() << "movies";
        fetchDetailsForAllMovies();
    }

    reply->deleteLater();
}

void ApiManager::onSearchMoviesReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        qDebug() << "Error: Invalid reply object";
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = "Network error: " + reply->errorString();
        qDebug() << errorMsg;
        emit error(errorMsg);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    qDebug() << "Received search data size:" << data.size() << "bytes";

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        QString errorMsg = "Invalid JSON response";
        qDebug() << errorMsg;
        emit error(errorMsg);
        reply->deleteLater();
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();

    // Check for API errors
    if (jsonObj.contains("status_code") && jsonObj.contains("status_message")) {
        QString errorMsg = "API Error: " + jsonObj["status_message"].toString();
        qDebug() << errorMsg;
        emit error(errorMsg);
        reply->deleteLater();
        return;
    }

    // Get total pages information
    int totalPages = jsonObj["total_pages"].toInt();
    int currentPage = jsonObj["page"].toInt();

    qDebug() << "Processing search page" << currentPage << "of" << totalPages
             << "(requested to fetch" << m_totalPagesToFetch << "pages)";

    QJsonArray results = jsonObj["results"].toArray();
    qDebug() << "Found" << results.size() << "movies in search response";

    // Add movies from this page to our accumulated list
    for (const QJsonValue &value : results) {
        QJsonObject movieObj = value.toObject();
        Movie movie = parseMovieJson(movieObj);
        m_accumulatedMovies.append(movie);
    }

    // Check if we need to fetch more pages
    if (currentPage < m_totalPagesToFetch && currentPage < totalPages) {
        m_currentPage = currentPage + 1;
        qDebug() << "Fetching next search page:" << m_currentPage;

        // Fetch next page immediately for faster loading
        fetchNextPage();
    } else {
        // We've fetched all pages, now fetch details for each movie
        qDebug() << "Finished fetching search pages. Fetching details for" << m_accumulatedMovies.size() << "movies";
        fetchDetailsForAllMovies();
    }

    reply->deleteLater();
}

void ApiManager::onMovieDetailsReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit error("Network error: " + reply->errorString());
        reply->deleteLater();
        m_detailsFetched++;
        m_activeDetailRequests--;

        // Process more pending requests
        processPendingDetailRequests();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        emit error("Invalid JSON response");
        reply->deleteLater();
        m_detailsFetched++;
        m_activeDetailRequests--;

        // Process more pending requests
        processPendingDetailRequests();
        return;
    }

    QJsonObject movieObj = jsonDoc.object();
    Movie movie = parseMovieJson(movieObj);

    // Update the movie duration in the database
    int apiId = movieObj["id"].toInt();
    int runtime = movieObj["runtime"].toInt();

    if (runtime > 0) {
        qDebug() << "Updating duration for movie" << movie.title << "(API ID:" << apiId << "):" << runtime << "minutes";
        DatabaseManager::instance().updateMovieDuration(apiId, runtime);
    }

    m_detailsFetched++;
    m_activeDetailRequests--;

    // Always emit the individual movie update
    emit movieDetailsLoaded(movie);

    // Check if we've fetched all movie details
    if (m_detailsFetched >= m_detailsToFetch) {
        qDebug() << "Finished fetching details for" << m_detailsFetched << "movies. All done!";
    } else {
        qDebug() << "Fetched details for movie" << m_detailsFetched << "of" << m_detailsToFetch
                 << "Active requests:" << m_activeDetailRequests;
    }

    // Process more pending requests
    processPendingDetailRequests();

    reply->deleteLater();
}

Movie ApiManager::parseMovieJson(const QJsonObject &movieObject)
{
    Movie movie;

    // TMDb ID (we'll use this as our API ID)
    movie.apiId = movieObject["id"].toInt();
    movie.id = 0; // ID local sera défini lors de l'insertion dans la base de données

    // Basic movie info
    movie.title = movieObject["title"].toString();

    // Release date (format: YYYY-MM-DD)
    QString releaseDate = movieObject["release_date"].toString();
    if (!releaseDate.isEmpty()) {
        movie.year = releaseDate.left(4).toInt();
    } else {
        movie.year = 0;
    }

    // Rating (TMDb uses a 0-10 scale)
    movie.rating = movieObject["vote_average"].toDouble();

    // Runtime (in minutes)
    movie.duration = movieObject.contains("runtime") ?
                     movieObject["runtime"].toInt() :
                     0;

    // Poster path
    QString posterPath = movieObject["poster_path"].toString();
    if (!posterPath.isEmpty()) {
        QString posterUrl = m_imageBaseUrl + posterPath;
        movie.imagePath = posterUrl;  // Temporarily set to URL

        // We'll download the images separately after returning the movie
    }

    // Extract genres
    if (movieObject.contains("genres")) {
        // Detailed movie info has a 'genres' array with objects
        QJsonArray genresArray = movieObject["genres"].toArray();
        for (const QJsonValue &genreValue : genresArray) {
            QJsonObject genreObj = genreValue.toObject();
            movie.genres.append(genreObj["name"].toString());
        }
    } else if (movieObject.contains("genre_ids")) {
        // Movie list responses have 'genre_ids' array with just IDs
        // We'll convert these to names using our genre map
        QJsonArray genreIdsArray = movieObject["genre_ids"].toArray();
        for (const QJsonValue &genreIdValue : genreIdsArray) {
            int genreId = genreIdValue.toInt();
            if (m_genreMap.contains(genreId)) {
                movie.genres.append(m_genreMap[genreId]);
            }
        }
    }

    // Default values for favorites and watchlist
    movie.inFavorites = false;
    movie.inWatchlist = false;

    return movie;
}

void ApiManager::downloadMoviePoster(const QString &posterUrl, Movie &movie)
{
    // Create a unique filename for the poster
    QString fileName = QString("poster_%1.jpg").arg(movie.id);

    // Get the path to the application's data location
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(dataPath + "/posters");

    // Create the directory if it doesn't exist
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Full path to save the poster
    QString filePath = dir.absoluteFilePath(fileName);

    // Check if we already have this poster
    if (QFile::exists(filePath)) {
        movie.imagePath = filePath;
        return;
    }

    // Download the poster
    QNetworkRequest request(posterUrl);
    QNetworkReply *reply = m_networkManager->get(request);

    // Use a lambda to handle the download completion
    connect(reply, &QNetworkReply::finished, [=, &movie]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray imageData = reply->readAll();
            QFile file(filePath);

            if (file.open(QIODevice::WriteOnly)) {
                file.write(imageData);
                file.close();
                movie.imagePath = filePath;
            }
        }

        reply->deleteLater();
    });
}

void ApiManager::fetchNextPage()
{
    if (m_currentSearchQuery.isEmpty()) {
        // Fetching popular movies
        fetchPopularMovies(m_currentPage, m_totalPagesToFetch);
    } else {
        // Fetching search results
        searchMovies(m_currentSearchQuery, m_currentPage, m_totalPagesToFetch);
    }
}

void ApiManager::onMovieTrailersReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }

    // Get the movie API ID from the reply's property
    int apiId = reply->property("movieId").toInt();

    if (reply->error() != QNetworkReply::NoError) {
        emit error("Network error when fetching trailers: " + reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        emit error("Invalid JSON response for trailers");
        reply->deleteLater();
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    QJsonArray results = jsonObj["results"].toArray();

    QString trailerUrl;

    // First, try to find an official trailer from YouTube
    for (const QJsonValue &value : results) {
        QJsonObject videoObj = value.toObject();
        if (videoObj["type"].toString() == "Trailer" &&
            videoObj["site"].toString() == "YouTube" &&
            videoObj["official"].toBool(true)) {

            trailerUrl = "https://www.youtube.com/watch?v=" + videoObj["key"].toString();
            qDebug() << "Found official trailer for movie with API ID" << apiId << ":" << trailerUrl;
            break;
        }
    }

    // If no official trailer found, try any trailer
    if (trailerUrl.isEmpty()) {
        for (const QJsonValue &value : results) {
            QJsonObject videoObj = value.toObject();
            if (videoObj["type"].toString() == "Trailer" &&
                videoObj["site"].toString() == "YouTube") {

                trailerUrl = "https://www.youtube.com/watch?v=" + videoObj["key"].toString();
                qDebug() << "Found trailer for movie with API ID" << apiId << ":" << trailerUrl;
                break;
            }
        }
    }

    // If still no trailer found, try any video
    if (trailerUrl.isEmpty() && !results.isEmpty()) {
        QJsonObject videoObj = results.first().toObject();
        if (videoObj["site"].toString() == "YouTube") {
            trailerUrl = "https://www.youtube.com/watch?v=" + videoObj["key"].toString();
            qDebug() << "Found video for movie with API ID" << apiId << ":" << trailerUrl;
        }
    }

    if (!trailerUrl.isEmpty()) {
        emit movieTrailerFound(apiId, trailerUrl);
    } else {
        qDebug() << "No trailer found for movie with API ID" << apiId;
    }

    reply->deleteLater();
}

void ApiManager::processPendingDetailRequests()
{
    // Process as many requests as we can up to the maximum concurrent limit
    while (m_activeDetailRequests < MAX_CONCURRENT_REQUESTS && !m_pendingDetailRequests.isEmpty()) {
        int movieId = m_pendingDetailRequests.takeFirst();
        m_activeDetailRequests++;

        qDebug() << "Starting detail request for movie ID:" << movieId
                 << "Active requests:" << m_activeDetailRequests;

        fetchMovieDetails(movieId);
    }

    if (m_pendingDetailRequests.isEmpty() && m_activeDetailRequests == 0) {
        qDebug() << "All movie details have been processed";
    }
}

void ApiManager::fetchGenres()
{
    // If genres are already loaded, emit the signal immediately
    if (!m_genreMap.isEmpty()) {
        qDebug() << "Genres already loaded, emitting genresLoaded signal";
        emit genresLoaded();
        return;
    }

    // Verify API key is set
    if (m_apiKey == "YOUR_API_KEY_HERE" || m_apiKey.isEmpty()) {
        qDebug() << "ERROR: API key not set! Please set your TMDb API key in apimanager.h";
        emit error("API key not set. Please set your TMDb API key.");
        return;
    }

    QUrl url(m_baseUrl + "/genre/movie/list");
    QUrlQuery query;
    query.addQueryItem("api_key", m_apiKey);
    query.addQueryItem("language", "en-US");
    url.setQuery(query);

    qDebug() << "Fetching movie genres from URL:" << url.toString(QUrl::RemoveQuery);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, &ApiManager::onGenresReply);
}

void ApiManager::onGenresReply()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        qDebug() << "Error: Invalid reply object for genres";
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = "Network error when fetching genres: " + reply->errorString();
        qDebug() << errorMsg;
        emit error(errorMsg);
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data);

    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        QString errorMsg = "Invalid JSON response for genres";
        qDebug() << errorMsg;
        emit error(errorMsg);
        reply->deleteLater();
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    QJsonArray genresArray = jsonObj["genres"].toArray();

    // Clear existing genre map
    m_genreMap.clear();

    // Populate genre map with ID -> name mapping
    for (const QJsonValue &value : genresArray) {
        QJsonObject genreObj = value.toObject();
        int id = genreObj["id"].toInt();
        QString name = genreObj["name"].toString();
        m_genreMap[id] = name;
        qDebug() << "Genre:" << id << "-" << name;
    }

    qDebug() << "Loaded" << m_genreMap.size() << "genres";
    reply->deleteLater();

    // Emit signal that genres are loaded
    emit genresLoaded();
}

void ApiManager::fetchDetailsForAllMovies()
{
    // Filter out movies that likely already have details in the database
    QList<int> moviesToFetch;

    for (const Movie& movie : m_accumulatedMovies) {
        // Check if this movie needs details (has no duration or duration is 0)
        bool hasDuration = DatabaseManager::instance().movieHasDuration(movie.apiId);
        if (movie.duration == 0 && !hasDuration) {
            qDebug() << "Movie needs duration details:" << movie.title << "(API ID:" << movie.apiId << ")";
            moviesToFetch.append(movie.apiId);
        } else {
            qDebug() << "Movie already has duration:" << movie.title << "(API ID:" << movie.apiId << ")";
        }
    }

    // Reset counters
    m_detailsToFetch = moviesToFetch.size();
    m_detailsFetched = 0;
    m_activeDetailRequests = 0;

    if (m_detailsToFetch == 0) {
        // No movies need details, emit immediately
        qDebug() << "No movies need duration details, emitting immediately";
        emit moviesLoaded(m_accumulatedMovies);
        return;
    }

    // Emit movies immediately so UI can show them while details are loading
    emit moviesLoaded(m_accumulatedMovies);

    qDebug() << "Fetching details for" << m_detailsToFetch << "movies with high concurrency";

    // Store all pending requests
    m_pendingDetailRequests = moviesToFetch;

    // Start processing requests with high concurrency - no delays!
    processPendingDetailRequests();
}
