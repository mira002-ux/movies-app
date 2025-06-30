#include "databasemanager.h"
#include <QCryptographicHash>
#include <QcoreApplication>

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent)
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");

    // Utilisez un chemin absolu pour la base de données
    QString dbPath = QCoreApplication::applicationDirPath() + "/movies.db";
    m_db.setDatabaseName(dbPath);

    qDebug() << "Database path:" << dbPath;
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen())
        m_db.close();
}

bool DatabaseManager::initDatabase()
{
    if (!m_db.open()) {
        qDebug() << "Error opening database:" << m_db.lastError().text();
        return false;
    }

    return createTables();
}

bool DatabaseManager::createTables()
{
    QSqlQuery query;
    bool success = true;

    // Users table
    if (!query.exec("CREATE TABLE IF NOT EXISTS users ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                   "username TEXT UNIQUE NOT NULL, "
                   "password TEXT NOT NULL)")) {
        qDebug() << "Error creating users table:" << query.lastError().text();
        success = false;
    }

    // Drop existing movies table to ensure schema is correct
    query.exec("DROP TABLE IF EXISTS movies");

    // Movies table
    if (!query.exec("CREATE TABLE IF NOT EXISTS movies ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                   "api_id INTEGER UNIQUE, "
                   "title TEXT NOT NULL, "
                   "year INTEGER, "
                   "rating REAL DEFAULT 0, "
                   "duration INTEGER, "
                   "votes INTEGER DEFAULT 1, "
                   "image_path TEXT, "
                   "genres TEXT)")) {  // Store genres as a comma-separated string
        qDebug() << "Error creating movies table:" << query.lastError().text();
        success = false;
    }

    // Add indexes for performance
    query.exec("CREATE INDEX IF NOT EXISTS idx_movies_api_id ON movies(api_id)");

    qDebug() << "Movies table recreated with correct schema";

    // Favorites table
    if (!query.exec("CREATE TABLE IF NOT EXISTS favorites ("
                   "user_id INTEGER, "
                   "movie_id INTEGER, "
                   "PRIMARY KEY (user_id, movie_id), "
                   "FOREIGN KEY (user_id) REFERENCES users(id), "
                   "FOREIGN KEY (movie_id) REFERENCES movies(id))")) {
        qDebug() << "Error creating favorites table:" << query.lastError().text();
        success = false;
    }

    // Add indexes for performance
    query.exec("CREATE INDEX IF NOT EXISTS idx_favorites_user_movie ON favorites(user_id, movie_id)");

    // Watchlist table
    if (!query.exec("CREATE TABLE IF NOT EXISTS watchlist ("
                   "user_id INTEGER, "
                   "movie_id INTEGER, "
                   "PRIMARY KEY (user_id, movie_id), "
                   "FOREIGN KEY (user_id) REFERENCES users(id), "
                   "FOREIGN KEY (movie_id) REFERENCES movies(id))")) {
        qDebug() << "Error creating watchlist table:" << query.lastError().text();
        success = false;
    }

    // Add indexes for performance
    query.exec("CREATE INDEX IF NOT EXISTS idx_watchlist_user_movie ON watchlist(user_id, movie_id)");

    // Vérifiez les tables créées
    QStringList tables = m_db.tables();
    qDebug() << "Tables in database:" << tables;

    return success;
}

// Hash password for security
QString hashPassword(const QString& password) {
    return QString(QCryptographicHash::hash(password.toUtf8(),
                                           QCryptographicHash::Sha256).toHex());
}

bool DatabaseManager::registerUser(const QString& username, const QString& password)
{
    QSqlQuery query;
    query.prepare("INSERT INTO users (username, password) VALUES (?, ?)");
    query.addBindValue(username);
    query.addBindValue(hashPassword(password));

    if (!query.exec()) {
        qDebug() << "Error registering user:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::loginUser(const QString& username, const QString& password)
{
    QSqlQuery query;
    query.prepare("SELECT id FROM users WHERE username = ? AND password = ?");
    query.addBindValue(username);
    query.addBindValue(hashPassword(password));

    if (!query.exec() || !query.next()) {
        qDebug() << "Login failed:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::addMovie(const QString& title, int year, double rating, int duration, const QString& imagePath)
{
    QSqlQuery query;
    query.prepare("INSERT INTO movies (title, year, rating, duration, image_path) VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(title);
    query.addBindValue(year);
    query.addBindValue(rating);
    query.addBindValue(duration);
    query.addBindValue(imagePath);

    if (!query.exec()) {
        qDebug() << "Error adding movie:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::updateMovieRating(int movieId, double rating)
{
    QSqlQuery query;
    query.prepare("UPDATE movies SET rating = (rating * votes + ?) / (votes + 1), "
                 "votes = votes + 1 WHERE id = ?");
    query.addBindValue(rating);
    query.addBindValue(movieId);

    if (!query.exec()) {
        qDebug() << "Error updating movie rating:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::addToFavorites(int userId, int movieId)
{
    QSqlQuery checkQuery;
    checkQuery.prepare("SELECT * FROM favorites WHERE user_id = ? AND movie_id = ?");
    checkQuery.addBindValue(userId);
    checkQuery.addBindValue(movieId);

    if (checkQuery.exec() && checkQuery.next()) {
        // Already in favorites, remove it
        QSqlQuery deleteQuery;
        deleteQuery.prepare("DELETE FROM favorites WHERE user_id = ? AND movie_id = ?");
        deleteQuery.addBindValue(userId);
        deleteQuery.addBindValue(movieId);

        if (!deleteQuery.exec()) {
            qDebug() << "Error removing from favorites:" << deleteQuery.lastError().text();
            return false;
        }
    } else {
        // Not in favorites, add it
        QSqlQuery insertQuery;
        insertQuery.prepare("INSERT INTO favorites (user_id, movie_id) VALUES (?, ?)");
        insertQuery.addBindValue(userId);
        insertQuery.addBindValue(movieId);

        if (!insertQuery.exec()) {
            qDebug() << "Error adding to favorites:" << insertQuery.lastError().text();
            return false;
        }
    }

    return true;
}

bool DatabaseManager::addToWatchlist(int userId, int movieId)
{
    QSqlQuery checkQuery;
    checkQuery.prepare("SELECT * FROM watchlist WHERE user_id = ? AND movie_id = ?");
    checkQuery.addBindValue(userId);
    checkQuery.addBindValue(movieId);

    if (checkQuery.exec() && checkQuery.next()) {
        // Already in watchlist, remove it
        QSqlQuery deleteQuery;
        deleteQuery.prepare("DELETE FROM watchlist WHERE user_id = ? AND movie_id = ?");
        deleteQuery.addBindValue(userId);
        deleteQuery.addBindValue(movieId);

        if (!deleteQuery.exec()) {
            qDebug() << "Error removing from watchlist:" << deleteQuery.lastError().text();
            return false;
        }
    } else {
        // Not in watchlist, add it
        QSqlQuery insertQuery;
        insertQuery.prepare("INSERT INTO watchlist (user_id, movie_id) VALUES (?, ?)");
        insertQuery.addBindValue(userId);
        insertQuery.addBindValue(movieId);

        if (!insertQuery.exec()) {
            qDebug() << "Error adding to watchlist:" << insertQuery.lastError().text();
            return false;
        }
    }

    return true;
}

QSqlQuery DatabaseManager::getMovies(const QString& sortBy)
{
    QSqlQuery query;
    QString queryStr = QString("SELECT * FROM movies ORDER BY %1").arg(sortBy);

    if (!query.exec(queryStr)) {
        qDebug() << "Error getting movies:" << query.lastError().text();
    }

    return query;
}

QSqlQuery DatabaseManager::getFavorites(int userId)
{
    QSqlQuery query;
    query.prepare("SELECT m.* FROM movies m "
                 "JOIN favorites f ON m.id = f.movie_id "
                 "WHERE f.user_id = ? "
                 "ORDER BY m.title");
    query.addBindValue(userId);

    if (!query.exec()) {
        qDebug() << "Error getting favorites:" << query.lastError().text();
    }

    return query;
}

QSqlQuery DatabaseManager::getWatchlist(int userId)
{
    QSqlQuery query;
    query.prepare("SELECT m.* FROM movies m "
                 "JOIN watchlist w ON m.id = w.movie_id "
                 "WHERE w.user_id = ? "
                 "ORDER BY m.title");
    query.addBindValue(userId);

    if (!query.exec()) {
        qDebug() << "Error getting watchlist:" << query.lastError().text();
    }

    return query;
}

QSqlQuery DatabaseManager::getMoviesByGenre(const QString& genre, const QString& sortBy)
{
    QSqlQuery query;

    // Use a more precise LIKE pattern to match exact genre names in the comma-separated list
    // This handles cases where one genre name is a substring of another (e.g., "Action" vs "Action & Adventure")
    query.prepare(QString("SELECT * FROM movies WHERE genres LIKE ? OR genres LIKE ? OR genres LIKE ? ORDER BY %1").arg(sortBy));

    // Match genre at the beginning, middle, or end of the list
    query.addBindValue(genre + ",%");       // Genre at the beginning
    query.addBindValue("%," + genre + ",%"); // Genre in the middle
    query.addBindValue("%," + genre);       // Genre at the end

    if (!query.exec()) {
        qDebug() << "Error getting movies by genre:" << query.lastError().text();
    }

    qDebug() << "Genre filter query for '" << genre << "' returned " << query.size() << " results";

    return query;
}

QSqlQuery DatabaseManager::searchMovies(const QString& searchQuery, const QString& sortBy)
{
    QSqlQuery query;

    // Use LIKE with wildcards for case-insensitive search
    query.prepare(QString("SELECT * FROM movies WHERE LOWER(title) LIKE LOWER(?) ORDER BY %1").arg(sortBy));
    query.addBindValue("%" + searchQuery + "%");

    if (!query.exec()) {
        qDebug() << "Error searching movies:" << query.lastError().text();
    }

    qDebug() << "Search query for '" << searchQuery << "' returned " << query.size() << " results";

    return query;
}

QStringList DatabaseManager::getAllGenres()
{
    QStringList allGenres;
    QSqlQuery query;

    // Get all distinct genres from the database
    if (query.exec("SELECT DISTINCT genres FROM movies WHERE genres IS NOT NULL AND genres != ''")) {
        while (query.next()) {
            QString genresStr = query.value(0).toString();
            QStringList movieGenres = genresStr.split(",");

            // Add each genre to the list if it's not already there
            for (const QString& genre : movieGenres) {
                if (!genre.trimmed().isEmpty() && !allGenres.contains(genre.trimmed())) {
                    allGenres.append(genre.trimmed());
                }
            }
        }
    } else {
        qDebug() << "Error getting all genres:" << query.lastError().text();
    }

    // Sort genres alphabetically
    allGenres.sort();

    qDebug() << "Found" << allGenres.size() << "unique genres:" << allGenres.join(", ");
    return allGenres;
}

int DatabaseManager::getUserId(const QString& username)
{
    QSqlQuery query;
    query.prepare("SELECT id FROM users WHERE username = ?");
    query.addBindValue(username);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return -1;
}

bool DatabaseManager::addMovieFromApi(int apiId, const QString& title, int year, double rating, int duration, const QString& imagePath, const QStringList& genres)
{
    // Print all parameters for debugging
    qDebug() << "Adding movie to DB - API ID:" << apiId
             << "Title:" << title
             << "Year:" << year
             << "Rating:" << rating
             << "Duration:" << duration
             << "Image:" << imagePath
             << "Genres:" << genres.join(", ");

    // Vérifier si le film existe déjà avec cet ID API
    if (movieExists(apiId)) {
        qDebug() << "Movie with API ID" << apiId << "already exists, updating instead";
        return updateMovieFromApi(apiId, title, year, rating, duration, imagePath, genres);
    }

    QSqlQuery query;

    // Convert genres list to comma-separated string
    QString genresStr = genres.join(",");

    // Use a simpler query first to debug
    query.prepare("INSERT INTO movies (api_id, title, year, rating, duration, image_path, genres) "
                 "VALUES (?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(apiId);
    query.addBindValue(title);
    query.addBindValue(year);
    query.addBindValue(rating);
    query.addBindValue(duration);
    query.addBindValue(imagePath);
    query.addBindValue(genresStr);

    if (!query.exec()) {
        qDebug() << "Error adding movie from API:" << query.lastError().text();

        // Try an even simpler query as a fallback
        query.clear();
        query.prepare("INSERT INTO movies (title, year) VALUES (?, ?)");
        query.addBindValue(title);
        query.addBindValue(year);

        if (!query.exec()) {
            qDebug() << "Even simple insert failed:" << query.lastError().text();
            return false;
        }

        qDebug() << "Simple insert succeeded for movie:" << title;
        return true;
    }

    qDebug() << "Successfully added movie:" << title;
    return true;
}

bool DatabaseManager::movieExists(int apiId)
{
    QSqlQuery query;
    query.prepare("SELECT id FROM movies WHERE api_id = ?");
    query.addBindValue(apiId);

    if (query.exec() && query.next()) {
        return true;
    }

    return false;
}

int DatabaseManager::getLocalMovieId(int apiId)
{
    QSqlQuery query;
    query.prepare("SELECT id FROM movies WHERE api_id = ?");
    query.addBindValue(apiId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return -1;
}

bool DatabaseManager::updateMovieFromApi(int apiId, const QString& title, int year, double rating, int duration, const QString& imagePath, const QStringList& genres)
{
    // Convert genres list to comma-separated string
    QString genresStr = genres.join(",");

    QSqlQuery query;
    query.prepare("UPDATE movies SET title = ?, year = ?, rating = ?, duration = ?, image_path = ?, genres = ? WHERE api_id = ?");
    query.addBindValue(title);
    query.addBindValue(year);
    query.addBindValue(rating);
    query.addBindValue(duration);
    query.addBindValue(imagePath);
    query.addBindValue(genresStr);
    query.addBindValue(apiId);

    if (!query.exec()) {
        qDebug() << "Error updating movie from API:" << query.lastError().text();
        return false;
    }

    qDebug() << "Updated movie with API ID" << apiId << ":" << title << "with genres:" << genresStr;
    return true;
}

bool DatabaseManager::updateMovieDuration(int apiId, int duration)
{
    QSqlQuery query;
    query.prepare("UPDATE movies SET duration = ? WHERE api_id = ?");
    query.addBindValue(duration);
    query.addBindValue(apiId);

    if (!query.exec()) {
        qDebug() << "Error updating movie duration:" << query.lastError().text();
        return false;
    }

    qDebug() << "Updated duration for movie with API ID" << apiId << "to" << duration << "minutes";
    return true;
}

bool DatabaseManager::movieHasDuration(int apiId)
{
    QSqlQuery query;
    query.prepare("SELECT duration FROM movies WHERE api_id = ? AND duration > 0");
    query.addBindValue(apiId);

    if (query.exec() && query.next()) {
        int duration = query.value(0).toInt();
        return duration > 0;
    }

    return false;
}

int DatabaseManager::getMovieCount()
{
    QSqlQuery query;
    if (query.exec("SELECT COUNT(*) FROM movies")) {
        if (query.next()) {
            return query.value(0).toInt();
        }
    } else {
        qDebug() << "Error getting movie count:" << query.lastError().text();
    }
    return 0;
}

bool DatabaseManager::saveMoviesFromApi(const QList<Movie>& movies)
{
    bool success = true;
    int addedCount = 0;
    int updatedCount = 0;

    qDebug() << "Saving" << movies.size() << "movies from API to database";

    for (const Movie& movie : movies) {
        qDebug() << "Processing movie:" << movie.apiId << movie.title << "with genres:" << movie.genres.join(", ");

        if (!movieExists(movie.apiId)) {
            qDebug() << "Movie doesn't exist in DB, adding:" << movie.title;
            if (!addMovieFromApi(movie.apiId, movie.title, movie.year, movie.rating, movie.duration, movie.imagePath, movie.genres)) {
                qDebug() << "Failed to add movie:" << movie.title;
                success = false;
            } else {
                addedCount++;
            }
        } else {
            qDebug() << "Movie already exists in DB:" << movie.title;
            // Update the movie information to ensure it's up to date
            if (updateMovieFromApi(movie.apiId, movie.title, movie.year, movie.rating, movie.duration, movie.imagePath, movie.genres)) {
                updatedCount++;
            }
        }
    }

    qDebug() << "Added" << addedCount << "new movies and updated" << updatedCount << "existing movies in database";
    return success;
}


