#include "moviemodel.h"
#include "databasemanager.h"
#include <QFont>
#include <QBrush>
#include <QIcon>

MovieModel::MovieModel(QObject *parent)
    : QAbstractTableModel(parent), m_userId(-1)
{
}

int MovieModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_movies.size();
}

int MovieModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return ColumnCount;
}

QVariant MovieModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_movies.size())
        return QVariant();

    const Movie &movie = m_movies[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case Title:
                return movie.title;
            case Year:
                return movie.year;
            case Rating:
                return QString::number(movie.rating, 'f', 1);
            case Duration:
                return QString("%1 min").arg(movie.duration);
            case Favorite:
            case Watchlist:
                return QVariant();
            default:
                return QVariant();
        }
    } else if (role == Qt::CheckStateRole) {
        if (index.column() == Favorite) {
            return movie.inFavorites ? Qt::Checked : Qt::Unchecked;
        } else if (index.column() == Watchlist) {
            return movie.inWatchlist ? Qt::Checked : Qt::Unchecked;
        }
    }

    return QVariant();
}

QVariant MovieModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant();

    switch (section) {
        case Title:
            return tr("Title");
        case Year:
            return tr("Year");
        case Rating:
            return tr("Rating");
        case Duration:
            return tr("Duration");
        case Favorite:
            return tr("Favorite");
        case Watchlist:
            return tr("Watchlist");
        default:
            return QVariant();
    }
}

void MovieModel::loadMovies(const QString &sortBy, int userId)
{
    beginResetModel();
    m_movies.clear();
    m_userId = userId;

    QSqlQuery query;
    QString queryStr = QString(
        "SELECT m.id, m.api_id, m.title, m.year, m.rating, m.duration, m.image_path, "
        "CASE WHEN f.user_id IS NOT NULL THEN 1 ELSE 0 END AS is_favorite, "
        "CASE WHEN w.user_id IS NOT NULL THEN 1 ELSE 0 END AS is_watchlist, "
        "m.genres "
        "FROM movies m "
        "LEFT JOIN favorites f ON m.id = f.movie_id AND f.user_id = ? "
        "LEFT JOIN watchlist w ON m.id = w.movie_id AND w.user_id = ? "
        "ORDER BY m.%1").arg(sortBy);

    query.prepare(queryStr);
    query.addBindValue(userId);
    query.addBindValue(userId);

    if (query.exec()) {
        while (query.next()) {
            Movie movie;
            movie.id = query.value(0).toInt();
            movie.apiId = query.value(1).toInt();
            movie.title = query.value(2).toString();
            movie.year = query.value(3).toInt();
            movie.rating = query.value(4).toDouble();
            movie.duration = query.value(5).toInt();
            movie.imagePath = query.value(6).toString();
            movie.inFavorites = query.value(7).toBool();
            movie.inWatchlist = query.value(8).toBool();

            // Parse genres from comma-separated string
            QString genresStr = query.value(9).toString();
            if (!genresStr.isEmpty()) {
                movie.genres = genresStr.split(",");
            }

            m_movies.append(movie);
        }
    } else {
        qDebug() << "Error loading movies:" << query.lastError().text();
    }

    endResetModel();

    // Ajoutez ce débogage pour voir combien de films sont chargés
    qDebug() << "Loaded" << m_movies.size() << "movies";
}

int MovieModel::getMovieId(int row) const
{
    if (row >= 0 && row < m_movies.size())
        return m_movies[row].id;

    return -1;
}

int MovieModel::getApiId(int row) const
{
    if (row >= 0 && row < m_movies.size())
        return m_movies[row].apiId;

    return -1;
}

QString MovieModel::getImagePath(int row) const
{
    if (row >= 0 && row < m_movies.size())
        return m_movies[row].imagePath;

    return QString();
}

void MovieModel::loadFavorites(int userId)
{
    beginResetModel();
    m_movies.clear();
    m_userId = userId;

    QSqlQuery query;
    query.prepare(
        "SELECT m.id, m.api_id, m.title, m.year, m.rating, m.duration, m.image_path, 1 AS is_favorite, "
        "(SELECT COUNT(*) FROM watchlist w WHERE w.movie_id = m.id AND w.user_id = ?) AS is_watchlist, "
        "m.genres "
        "FROM movies m "
        "JOIN favorites f ON m.id = f.movie_id "
        "WHERE f.user_id = ? "
        "ORDER BY m.title");

    query.addBindValue(userId);
    query.addBindValue(userId);

    if (query.exec()) {
        while (query.next()) {
            Movie movie;
            movie.id = query.value(0).toInt();
            movie.apiId = query.value(1).toInt();
            movie.title = query.value(2).toString();
            movie.year = query.value(3).toInt();
            movie.rating = query.value(4).toDouble();
            movie.duration = query.value(5).toInt();
            movie.imagePath = query.value(6).toString();
            movie.inFavorites = true;
            movie.inWatchlist = query.value(8).toBool();

            // Parse genres from comma-separated string
            QString genresStr = query.value(9).toString();
            if (!genresStr.isEmpty()) {
                movie.genres = genresStr.split(",");
            }

            m_movies.append(movie);
        }
    }

    endResetModel();
}

void MovieModel::loadMoviesByGenre(const QString &genre, const QString &sortBy, int userId)
{
    beginResetModel();
    m_movies.clear();
    m_userId = userId;

    qDebug() << "Loading movies for genre:" << genre << "with sort:" << sortBy;

    QSqlQuery query = DatabaseManager::instance().getMoviesByGenre(genre, sortBy);

    int count = 0;
    while (query.next()) {
        Movie movie;
        movie.id = query.value(0).toInt();
        movie.apiId = query.value(1).toInt();
        movie.title = query.value(2).toString();
        movie.year = query.value(3).toInt();
        movie.rating = query.value(4).toDouble();
        movie.duration = query.value(5).toInt();
        movie.imagePath = query.value(7).toString();

        // Parse genres from comma-separated string
        QString genresStr = query.value(8).toString();
        if (!genresStr.isEmpty()) {
            movie.genres = genresStr.split(",");

            // Double-check that this movie actually contains the requested genre
            // This is a safeguard against SQL LIKE pattern matching issues
            bool containsGenre = false;
            for (const QString &movieGenre : movie.genres) {
                if (movieGenre.trimmed() == genre) {
                    containsGenre = true;
                    break;
                }
            }

            if (!containsGenre) {
                qDebug() << "Skipping movie" << movie.title << "as it doesn't contain genre" << genre
                         << "despite SQL match. Genres:" << genresStr;
                continue;
            }
        } else {
            // Skip movies with no genres
            qDebug() << "Skipping movie" << movie.title << "as it has no genres";
            continue;
        }

        // Check if movie is in favorites or watchlist
        QSqlQuery favQuery;
        favQuery.prepare("SELECT COUNT(*) FROM favorites WHERE user_id = ? AND movie_id = ?");
        favQuery.addBindValue(userId);
        favQuery.addBindValue(movie.id);
        favQuery.exec();
        if (favQuery.next()) {
            movie.inFavorites = favQuery.value(0).toBool();
        }

        QSqlQuery watchQuery;
        watchQuery.prepare("SELECT COUNT(*) FROM watchlist WHERE user_id = ? AND movie_id = ?");
        watchQuery.addBindValue(userId);
        watchQuery.addBindValue(movie.id);
        watchQuery.exec();
        if (watchQuery.next()) {
            movie.inWatchlist = watchQuery.value(0).toBool();
        }

        m_movies.append(movie);
        count++;
    }

    endResetModel();

    qDebug() << "Loaded" << m_movies.size() << "movies for genre:" << genre;
}

void MovieModel::searchMovies(const QString &searchQuery, const QString &sortBy, int userId)
{
    beginResetModel();
    m_movies.clear();
    m_userId = userId;

    qDebug() << "Searching movies for query:" << searchQuery << "with sort:" << sortBy;

    QSqlQuery query = DatabaseManager::instance().searchMovies(searchQuery, sortBy);

    int count = 0;
    while (query.next()) {
        Movie movie;
        movie.id = query.value(0).toInt();
        movie.apiId = query.value(1).toInt();
        movie.title = query.value(2).toString();
        movie.year = query.value(3).toInt();
        movie.rating = query.value(4).toDouble();
        movie.duration = query.value(5).toInt();
        movie.imagePath = query.value(7).toString();

        // Check if movie is in user's favorites and watchlist
        if (userId != -1) {
            QSqlQuery favQuery;
            favQuery.prepare("SELECT COUNT(*) FROM favorites WHERE user_id = ? AND movie_id = ?");
            favQuery.addBindValue(userId);
            favQuery.addBindValue(movie.id);
            if (favQuery.exec() && favQuery.next()) {
                movie.inFavorites = favQuery.value(0).toInt() > 0;
            }

            QSqlQuery watchQuery;
            watchQuery.prepare("SELECT COUNT(*) FROM watchlist WHERE user_id = ? AND movie_id = ?");
            watchQuery.addBindValue(userId);
            watchQuery.addBindValue(movie.id);
            if (watchQuery.exec() && watchQuery.next()) {
                movie.inWatchlist = watchQuery.value(0).toInt() > 0;
            }
        }

        // Parse genres from comma-separated string
        QString genresStr = query.value(8).toString();
        if (!genresStr.isEmpty()) {
            movie.genres = genresStr.split(",");
        }

        m_movies.append(movie);
        count++;
    }

    qDebug() << "Search found" << count << "movies for query:" << searchQuery;

    endResetModel();
}

void MovieModel::loadWatchlist(int userId)
{
    beginResetModel();
    m_movies.clear();
    m_userId = userId;

    QSqlQuery query;
    query.prepare(
        "SELECT m.id, m.api_id, m.title, m.year, m.rating, m.duration, m.image_path, "
        "(SELECT COUNT(*) FROM favorites f WHERE f.movie_id = m.id AND f.user_id = ?) AS is_favorite, "
        "1 AS is_watchlist, "
        "m.genres "
        "FROM movies m "
        "JOIN watchlist w ON m.id = w.movie_id "
        "WHERE w.user_id = ? "
        "ORDER BY m.title");

    query.addBindValue(userId);
    query.addBindValue(userId);

    if (query.exec()) {
        while (query.next()) {
            Movie movie;
            movie.id = query.value(0).toInt();
            movie.apiId = query.value(1).toInt();
            movie.title = query.value(2).toString();
            movie.year = query.value(3).toInt();
            movie.rating = query.value(4).toDouble();
            movie.duration = query.value(5).toInt();
            movie.imagePath = query.value(6).toString();
            movie.inFavorites = query.value(7).toBool();
            movie.inWatchlist = true;

            // Parse genres from comma-separated string
            QString genresStr = query.value(9).toString();
            if (!genresStr.isEmpty()) {
                movie.genres = genresStr.split(",");
            }

            m_movies.append(movie);
        }
    }

    endResetModel();
}





