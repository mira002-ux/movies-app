#include "movieswindow.h"
#include "ui_movieswindow.h"
#include "databasemanager.h"
#include "castdialog.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QGridLayout>
#include <QStatusBar>
#include <QCloseEvent>
#include <QApplication>
#include <QEvent>
#include <QWindowStateChangeEvent>
#include <QDesktopServices>
#include <QMenu>
#include <QElapsedTimer>

MoviesWindow::MoviesWindow(int userId, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MoviesWindow),
    m_userId(userId),
    m_selectedMovieId(-1),
    m_currentPage(0),
    m_moviesPerPage(20),  // Show 20 movies per page (5x4 grid) for better performance
    m_currentGenre(""),   // Initialize with no genre filter
    m_isLoadingMovies(false),
    m_targetMovieCount(700),  // Target 700 movies (35 pages × 20 movies per page)
    m_lastRefreshTime()
{
    ui->setupUi(this);

    // Set window attributes and flags to ensure it stays in the taskbar
    setAttribute(Qt::WA_QuitOnClose, false);
    setAttribute(Qt::WA_DeleteOnClose, false);

    // Use Qt::Window with minimize and close buttons to ensure proper taskbar behavior
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);

    // Définir l'icône de la fenêtre (utiliser une icône existante)
    setWindowIcon(QIcon(":/images/logo.png"));

    setupModel();
    setupApiManager();

    // Setup sort combo box
    ui->sortComboBox->addItem("Title", "title");
    ui->sortComboBox->addItem("Year", "year DESC");
    ui->sortComboBox->addItem("Rating", "rating DESC");
    ui->sortComboBox->addItem("Duration", "duration");
    ui->sortComboBox->setCurrentIndex(1); // Set 'Year' as default

    // Setup search timer for delayed search
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(200); // Reduced to 200ms for faster response
    connect(m_searchTimer, &QTimer::timeout, this, [this]() {
        QString searchText = ui->searchLineEdit->text().trimmed();
        if (searchText.isEmpty()) {
            // If search is empty, show all movies
            m_currentGenre.clear();
            loadMovies();
        } else {
            // First try local search for instant results
            searchMoviesLocally(searchText);
        }
    });

    // Setup loading progress timer for professional status updates
    m_loadingProgressTimer = new QTimer(this);
    m_loadingProgressTimer->setInterval(1000); // Update every second
    connect(m_loadingProgressTimer, &QTimer::timeout, this, [this]() {
        if (m_isLoadingMovies) {
            int currentCount = DatabaseManager::instance().getMovieCount();
            int percentage = (currentCount * 100) / m_targetMovieCount;
            showStatusMessage(QString("Loading comprehensive movie database... %1% complete (%2/%3 movies)")
                             .arg(percentage).arg(currentCount).arg(m_targetMovieCount));
        }
    });

    // Create pagination buttons
    QPushButton* prevButton = new QPushButton("← Previous", this);
    prevButton->setObjectName("prevPageButton");
    QPushButton* nextButton = new QPushButton("Next →", this);
    nextButton->setObjectName("nextPageButton");

    // Create page indicator label
    QLabel* pageLabel = new QLabel("Page 1", this);
    pageLabel->setObjectName("pageLabel");
    pageLabel->setAlignment(Qt::AlignCenter);

    // Add pagination controls to a layout
    QHBoxLayout* paginationLayout = new QHBoxLayout();
    paginationLayout->addWidget(prevButton);
    paginationLayout->addWidget(pageLabel);
    paginationLayout->addWidget(nextButton);

    // Add the pagination layout to the main layout
    // Find the vertical layout that contains everything
    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(ui->centralwidget->layout());
    if (mainLayout) {
        // Insert the pagination layout before the last item (which is the buttons layout)
        mainLayout->insertLayout(mainLayout->count() - 1, paginationLayout);
    }

    // Connect pagination buttons
    connect(prevButton, &QPushButton::clicked, this, &MoviesWindow::on_prevPageButton_clicked);
    connect(nextButton, &QPushButton::clicked, this, &MoviesWindow::on_nextPageButton_clicked);

    // Show a message to the user
    showStatusMessage("Loading movies from TMDb...");

    // Load initial data - first from database, then from API
    loadMovies();

    // Use a short delay to ensure UI is responsive first
    QTimer::singleShot(500, this, [this]() {
        qDebug() << "Fetching initial movies from API...";
        m_apiManager->fetchPopularMovies(1, 1); // Only 1 page for instant feedback
        // Load more pages in the background
        QTimer::singleShot(1000, this, [this]() {
            m_apiManager->fetchPopularMovies(2, 5); // Next 5 pages
        });
    });

    m_lastRefreshTime.start();
}

MoviesWindow::~MoviesWindow()
{
    // Properly clean up resources
    clearMovieWidgets();

    // Disconnect all signals to prevent any pending callbacks
    disconnect(m_apiManager, nullptr, this, nullptr);
    disconnect(m_searchTimer, nullptr, this, nullptr);

    // Delete objects
    if (m_apiManager) {
        m_apiManager->deleteLater();
    }

    if (m_movieModel) {
        m_movieModel->deleteLater();
    }

    if (m_proxyModel) {
        m_proxyModel->deleteLater();
    }

    if (m_searchTimer) {
        m_searchTimer->stop();
        m_searchTimer->deleteLater();
    }

    if (m_loadingProgressTimer) {
        m_loadingProgressTimer->stop();
        m_loadingProgressTimer->deleteLater();
    }

    delete ui;
}

void MoviesWindow::setupModel()
{
    m_movieModel = new MovieModel(this);
    m_proxyModel = new QSortFilterProxyModel(this);

    m_proxyModel->setSourceModel(m_movieModel);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(MovieModel::Title);
}

void MoviesWindow::setupApiManager()
{
    m_apiManager = new ApiManager(this);

    // Connect API signals
    connect(m_apiManager, &ApiManager::moviesLoaded, this, &MoviesWindow::onMoviesLoaded);
    connect(m_apiManager, &ApiManager::movieDetailsLoaded, this, &MoviesWindow::onMovieDetailsLoaded);
    connect(m_apiManager, &ApiManager::movieTrailerFound, this, &MoviesWindow::onMovieTrailerFound);
    connect(m_apiManager, &ApiManager::error, this, &MoviesWindow::onApiError);
    connect(m_apiManager, &ApiManager::genresLoaded, this, &MoviesWindow::onGenresLoaded);

    // Check if genres are already loaded
    showStatusMessage("Loading movie genres...");

    // We'll wait for the genresLoaded signal in any case
    // The signal handler will call loadMoviesFromApi
}

void MoviesWindow::loadMovies()
{
    int currentTab = ui->tabWidget->currentIndex();
    QString sortBy = ui->sortComboBox->currentData().toString();

    switch (currentTab) {
        case 0: // All movies
            m_movieModel->loadMovies(sortBy, m_userId);
            break;
        case 1: // Favorites
            m_movieModel->loadFavorites(m_userId);
            break;
        case 2: // Watchlist
            m_movieModel->loadWatchlist(m_userId);
            break;
    }

    // Display the movies
    displayMovies();

    // Check if some movies don't have duration and request details if necessary
    QList<int> moviesToFetch;
    for (const Movie& movie : m_allMovies) {
        if (movie.duration == 0) {
            qDebug() << "Movie" << movie.title << "has no duration, fetching details";
            moviesToFetch.append(movie.apiId);
        }
    }

    // If there are movies without duration, request details using the new batch method
    if (!moviesToFetch.isEmpty()) {
        qDebug() << "Fetching details for" << moviesToFetch.size() << "movies without duration using high-speed batch processing";

        // Use the new batch fetch method for maximum speed
        m_apiManager->fetchMovieDetailsBatch(moviesToFetch);
    }
}

void MoviesWindow::displayMovies()
{
    clearMovieWidgets();

    // Get movies from the model and store them
    m_allMovies.clear();
    for (int i = 0; i < m_movieModel->rowCount(); ++i) {
        Movie movie;
        movie.id = m_movieModel->getMovieId(i);
        movie.apiId = m_movieModel->getApiId(i);
        movie.title = m_movieModel->data(m_movieModel->index(i, MovieModel::Title)).toString();
        movie.year = m_movieModel->data(m_movieModel->index(i, MovieModel::Year)).toInt();
        movie.rating = m_movieModel->data(m_movieModel->index(i, MovieModel::Rating)).toDouble();
        movie.duration = m_movieModel->data(m_movieModel->index(i, MovieModel::Duration)).toString().replace(" min", "").toInt();
        movie.inFavorites = m_movieModel->data(m_movieModel->index(i, MovieModel::Favorite), Qt::CheckStateRole).toBool();
        movie.inWatchlist = m_movieModel->data(m_movieModel->index(i, MovieModel::Watchlist), Qt::CheckStateRole).toBool();
        movie.imagePath = m_movieModel->getImagePath(i);

        m_allMovies.append(movie);
    }

    // Reset to first page
    m_currentPage = 0;

    // Display the first page of movies
    displayMoviesPage(m_currentPage);

    // Update pagination controls
    updatePaginationControls();
}

void MoviesWindow::displayMoviesPage(int page)
{
    clearMovieWidgets();

    QGridLayout* gridLayout = nullptr;
    int currentTab = ui->tabWidget->currentIndex();

    switch (currentTab) {
        case 0: // All movies
            gridLayout = qobject_cast<QGridLayout*>(ui->scrollAreaWidgetContents->layout());
            break;
        case 1: // Favorites
            gridLayout = qobject_cast<QGridLayout*>(ui->favoritesScrollAreaWidgetContents->layout());
            break;
        case 2: // Watchlist
            gridLayout = qobject_cast<QGridLayout*>(ui->watchlistScrollAreaWidgetContents->layout());
            break;
    }

    if (!gridLayout) {
        return;
    }

    // Calculate start and end indices for the current page
    int startIndex = page * m_moviesPerPage;
    int endIndex = qMin(startIndex + m_moviesPerPage, m_allMovies.size());

    int row = 0;
    int col = 0;
    int maxCols = 5; // Optimized for 5x4 grid (20 movies per page)

    // Optimized spacing for better performance and visual appeal
    gridLayout->setSpacing(12);  // Slightly increased spacing for better visual separation
    gridLayout->setContentsMargins(15, 15, 15, 15);  // Balanced margins

    for (int i = startIndex; i < endIndex; ++i) {
        const Movie &movie = m_allMovies.at(i);
        MovieWidget* movieWidget = createMovieWidget(movie);
        m_movieWidgets.append(movieWidget);

        // Utiliser Qt::AlignCenter pour centrer le widget dans sa cellule
        gridLayout->addWidget(movieWidget, row, col, 1, 1, Qt::AlignCenter);

        col++;
        if (col >= maxCols) {
            col = 0;
            row++;
        }
    }
}

void MoviesWindow::updatePaginationControls()
{
    int totalPages = (m_allMovies.size() + m_moviesPerPage - 1) / m_moviesPerPage;

    // Update the page label
    QLabel* pageLabel = findChild<QLabel*>("pageLabel");
    if (pageLabel) {
        pageLabel->setText(QString("Page %1 of %2").arg(m_currentPage + 1).arg(totalPages));
    }

    // Enable/disable pagination buttons
    QPushButton* prevButton = findChild<QPushButton*>("prevPageButton");
    if (prevButton) {
        prevButton->setEnabled(m_currentPage > 0);
    }

    QPushButton* nextButton = findChild<QPushButton*>("nextPageButton");
    if (nextButton) {
        nextButton->setEnabled(m_currentPage < totalPages - 1);
    }
}

MovieWidget* MoviesWindow::createMovieWidget(const Movie &movie)
{
    MovieWidget* widget = new MovieWidget(
        movie.id, movie.apiId, movie.title, movie.year, movie.rating,
        movie.duration, movie.inFavorites, movie.inWatchlist,
        movie.imagePath, this
    );

    connect(widget, &MovieWidget::favoriteClicked, this, &MoviesWindow::onMovieWidgetFavoriteClicked);
    connect(widget, &MovieWidget::watchlistClicked, this, &MoviesWindow::onMovieWidgetWatchlistClicked);
    connect(widget, &MovieWidget::movieClicked, this, &MoviesWindow::onMovieWidgetClicked);
    connect(widget, &MovieWidget::trailerClicked, this, &MoviesWindow::onMovieWidgetTrailerClicked);
    connect(widget, &MovieWidget::castClicked, this, &MoviesWindow::onMovieWidgetCastClicked);

    return widget;
}

void MoviesWindow::clearMovieWidgets()
{
    for (MovieWidget* widget : m_movieWidgets) {
        widget->deleteLater();
    }
    m_movieWidgets.clear();
}

void MoviesWindow::on_sortComboBox_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    loadMovies();
}

void MoviesWindow::on_tabWidget_currentChanged(int index)
{
    Q_UNUSED(index);
    loadMovies();
}

void MoviesWindow::toggleFavorite(int movieId)
{
    if (movieId < 0)
        return;

    DatabaseManager::instance().addToFavorites(m_userId, movieId);
    loadMovies();
}

void MoviesWindow::toggleWatchlist(int movieId)
{
    if (movieId < 0)
        return;

    DatabaseManager::instance().addToWatchlist(m_userId, movieId);
    loadMovies();
}

void MoviesWindow::onMovieWidgetFavoriteClicked(int movieId)
{
    toggleFavorite(movieId);
}

void MoviesWindow::onMovieWidgetWatchlistClicked(int movieId)
{
    toggleWatchlist(movieId);
}

void MoviesWindow::onMovieWidgetClicked(int movieId)
{
    m_selectedMovieId = movieId;
}

void MoviesWindow::onMovieWidgetTrailerClicked(int movieId)
{
    // Afficher un message indiquant que la bande-annonce est en cours de chargement
    showStatusMessage("Chargement de la bande-annonce...");

    // Trouver l'ID API correspondant à l'ID local
    int apiId = -1;
    for (const Movie& movie : m_allMovies) {
        if (movie.id == movieId) {
            apiId = movie.apiId;
            qDebug() << "Found API ID" << apiId << "for local movie ID" << movieId;
            break;
        }
    }

    if (apiId > 0) {
        // Demander à l'API de récupérer la bande-annonce pour ce film en utilisant l'ID API
        m_apiManager->fetchMovieTrailers(apiId);
    } else {
        showStatusMessage("Erreur: Impossible de trouver l'ID API pour ce film");
        qDebug() << "Error: Could not find API ID for movie with local ID" << movieId;
    }
}

void MoviesWindow::onMovieWidgetCastClicked(int movieId)
{
    // Find the movie title and API ID
    QString movieTitle = "";
    int apiId = -1;

    for (const Movie& movie : m_allMovies) {
        if (movie.id == movieId) {
            movieTitle = movie.title;
            apiId = movie.apiId;
            break;
        }
    }

    if (movieTitle.isEmpty() || apiId <= 0) {
        showStatusMessage("Error: Could not find movie information");
        return;
    }

    // Show status message
    showStatusMessage("Loading cast and crew for " + movieTitle + "...");

    // Create and show the cast dialog
    CastDialog *dialog = new CastDialog(apiId, movieTitle, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose); // Auto-delete when closed
    dialog->show();
}

void MoviesWindow::on_rateButton_clicked()
{
    if (m_selectedMovieId < 0)
    {
        QMessageBox::information(this, "Rate Movie", "Please select a movie first.");
        return;
    }

    bool ok;
    double rating = QInputDialog::getDouble(this, "Rate Movie",
                                          "Enter rating (0-10):",
                                          5.0, 0.0, 10.0, 1, &ok);

    if (ok) {
        DatabaseManager::instance().updateMovieRating(m_selectedMovieId, rating);
        loadMovies();
    }
}

void MoviesWindow::on_searchLineEdit_textChanged(const QString &text)
{
    // Use a timer to delay the search until the user stops typing
    m_searchTimer->start();
}

void MoviesWindow::loadMoviesFromApi(const QString &searchQuery)
{
    // Set loading state
    m_isLoadingMovies = true;
    m_loadingProgressTimer->start();

    showStatusMessage("Initializing comprehensive movie database loading...");

    // Fetch the first page instantly for immediate feedback
    if (searchQuery.isEmpty()) {
        m_apiManager->fetchPopularMovies(1, 1);
        // Fetch the rest in small, rapid, staggered batches
        int batchSize = 3;
        int totalPages = 100; // Fetch up to 100 pages for maximum movies
        int delay = 150; // ms between batches
        int batch = 0;
        for (int page = 2; page <= totalPages; page += batchSize, ++batch) {
            QTimer::singleShot(500 + batch * delay, this, [this, page, batchSize]() {
                m_apiManager->fetchPopularMovies(page, batchSize);
            });
        }
    } else {
        // For search, use the same batching logic but with searchMovies
        m_apiManager->searchMovies(searchQuery, 1, 1);
        int batchSize = 3;
        int totalPages = 50; // Fetch up to 50 pages for search
        int delay = 150;
        int batch = 0;
        for (int page = 2; page <= totalPages; page += batchSize, ++batch) {
            QTimer::singleShot(500 + batch * delay, this, [this, searchQuery, page, batchSize]() {
                m_apiManager->searchMovies(searchQuery, page, batchSize);
            });
        }
    }
}

void MoviesWindow::onMoviesLoaded(const QList<Movie> &movies)
{
    qDebug() << "Movies loaded from API:" << movies.size();

    if (movies.isEmpty()) {
        showStatusMessage("No movies found");
        return;
    }

    // Save movies to database
    bool success = DatabaseManager::instance().saveMoviesFromApi(movies);
    qDebug() << "Saved movies to database:" << (success ? "success" : "failed");

    int totalMovies = DatabaseManager::instance().getMovieCount();

    // Throttle UI refresh: only update if a new page is available or 1.5s have passed
    static int lastDisplayedCount = 0;
    if (totalMovies - lastDisplayedCount >= m_moviesPerPage || m_lastRefreshTime.elapsed() > 1500) {
        lastDisplayedCount = totalMovies;
        m_lastRefreshTime.restart();
        if (!m_currentGenre.isEmpty()) {
            qDebug() << "Maintaining genre filter for:" << m_currentGenre;
            m_movieModel->loadMoviesByGenre(m_currentGenre, ui->sortComboBox->currentData().toString(), m_userId);
            m_allMovies = m_movieModel->getMovies();
            displayMoviesPage(m_currentPage);
            updatePaginationControls();
        } else {
            loadMovies();
        }
        showStatusMessage(QString("Displaying %1 movies with complete information").arg(totalMovies));
    }

    // Professional loading approach: Only display movies when we have substantial content
    // and avoid showing incomplete information during loading
    static bool initialDisplayDone = false;
    static bool loadingInProgress = false;

    if (totalMovies >= 100 && !initialDisplayDone && !loadingInProgress) {
        // We have enough movies to show a professional initial display
        initialDisplayDone = true;
        loadingInProgress = true;

        showStatusMessage("Preparing movie display with complete information...");

        // Wait a moment for details to be fetched before showing
        QTimer::singleShot(2000, this, [this, totalMovies]() {
            // If we have a genre filter active, we need to maintain it
            if (!m_currentGenre.isEmpty()) {
                qDebug() << "Maintaining genre filter for:" << m_currentGenre;
                m_movieModel->loadMoviesByGenre(m_currentGenre, ui->sortComboBox->currentData().toString(), m_userId);
                m_allMovies = m_movieModel->getMovies();
                displayMoviesPage(m_currentPage);
                updatePaginationControls();
            } else {
                // Load movies from database to show complete info
                loadMovies();
            }
            showStatusMessage(QString("Displaying %1 movies with complete information").arg(totalMovies));
            loadingInProgress = false;
        });
    } else if (initialDisplayDone && !loadingInProgress) {
        // Only update display if initial display is done and we're not currently loading
        // This prevents flickering and incomplete displays
        if (!m_currentGenre.isEmpty()) {
            qDebug() << "Maintaining genre filter for:" << m_currentGenre;
            m_movieModel->loadMoviesByGenre(m_currentGenre, ui->sortComboBox->currentData().toString(), m_userId);
            m_allMovies = m_movieModel->getMovies();
            displayMoviesPage(m_currentPage);
            updatePaginationControls();
        } else {
            // Load movies from database to show updated info
            loadMovies();
        }
    } else {
        // Still loading initial set - the progress timer will handle status updates
        // Just ensure we're showing that we're loading
        if (!m_isLoadingMovies) {
            showStatusMessage(QString("Loading comprehensive movie database... (%1 movies loaded, target: %2)")
                             .arg(totalMovies).arg(m_targetMovieCount));
        }
    }

    // Check if we've reached our target and can stop loading
    if (totalMovies >= m_targetMovieCount && m_isLoadingMovies) {
        m_isLoadingMovies = false;
        m_loadingProgressTimer->stop();
        showStatusMessage(QString("Movie database fully loaded! %1 movies available for browsing.").arg(totalMovies));
    }

    // Show a status message with the total count
    showStatusMessage(QString("Loaded %1 movies - Fetching complete details...").arg(totalMovies));

    // Professional refresh schedule: Only update display when we have meaningful improvements
    // and avoid showing incomplete data during the loading process

    // Only schedule refreshes if initial display is already done
    static bool refreshScheduled = false;
    if (initialDisplayDone && !refreshScheduled) {
        refreshScheduled = true;

        // First refresh after 3 seconds (when substantial details should be available)
        QTimer::singleShot(3000, this, [this, totalMovies]() {
            if (!m_currentGenre.isEmpty()) {
                m_movieModel->loadMoviesByGenre(m_currentGenre, ui->sortComboBox->currentData().toString(), m_userId);
                m_allMovies = m_movieModel->getMovies();
                displayMoviesPage(m_currentPage);
                updatePaginationControls();
            } else {
                loadMovies();
            }
            showStatusMessage(QString("Enhanced movie details loaded - %1 movies available").arg(totalMovies));
        });

        // Final refresh after 8 seconds (when all movies should have complete details)
        QTimer::singleShot(8000, this, [this, totalMovies]() {
            if (!m_currentGenre.isEmpty()) {
                m_movieModel->loadMoviesByGenre(m_currentGenre, ui->sortComboBox->currentData().toString(), m_userId);
                m_allMovies = m_movieModel->getMovies();
                displayMoviesPage(m_currentPage);
                updatePaginationControls();
            } else {
                loadMovies();
            }

            // Final loading state cleanup
            if (m_isLoadingMovies) {
                m_isLoadingMovies = false;
                m_loadingProgressTimer->stop();
                int finalCount = DatabaseManager::instance().getMovieCount();
                showStatusMessage(QString("Movie database ready! %1 movies loaded with complete details.").arg(finalCount));
            }

            // Show appropriate message based on genre filter
            if (!m_currentGenre.isEmpty()) {
                showStatusMessage(QString("Showing %1 movies in genre: %2")
                                 .arg(m_allMovies.size())
                                 .arg(m_currentGenre));
            } else {
                showStatusMessage(QString("All movie details updated - %1 movies available").arg(totalMovies));
            }
        });
    }
}

void MoviesWindow::onMovieDetailsLoaded(const Movie &movie)
{
    // Update the movie in the database
    int localId = DatabaseManager::instance().getLocalMovieId(movie.apiId);
    if (localId > 0) {
        // Mettre à jour la note et la durée dans la base de données
        DatabaseManager::instance().updateMovieRating(localId, movie.rating);

        // S'assurer que la durée est également mise à jour dans la base de données
        if (movie.duration > 0) {
            qDebug() << "Updating duration for movie with local ID" << localId << "(API ID:" << movie.apiId << "):" << movie.duration << "minutes";
            DatabaseManager::instance().updateMovieDuration(movie.apiId, movie.duration);
        }

        // Find and update the movie in our current list
        for (int i = 0; i < m_allMovies.size(); i++) {
            if (m_allMovies[i].apiId == movie.apiId) {
                // Update the movie data
                m_allMovies[i].rating = movie.rating;
                m_allMovies[i].duration = movie.duration;

                // Find the corresponding widget and update it
                for (MovieWidget* widget : m_movieWidgets) {
                    if (widget->apiId() == movie.apiId) {
                        // Update the widget with new data
                        widget->updateMovieData(movie.rating, movie.duration);
                        break;
                    }
                }
                break;
            }
        }
    }

    // No need to refresh the entire display - we've updated the specific movie
}

void MoviesWindow::onMovieTrailerFound(int apiId, const QString &trailerUrl)
{
    if (trailerUrl.isEmpty()) {
        showStatusMessage("Aucune bande-annonce trouvée pour ce film");
        return;
    }

    // Afficher un message indiquant que la bande-annonce a été trouvée
    showStatusMessage("Bande-annonce trouvée, ouverture dans le navigateur...");

    // Ouvrir la bande-annonce dans le navigateur par défaut
    QDesktopServices::openUrl(QUrl(trailerUrl));
}

void MoviesWindow::onApiError(const QString &errorMessage)
{
    showStatusMessage("Error: " + errorMessage);
}

void MoviesWindow::onGenresLoaded()
{
    showStatusMessage("Genres loaded, fetching movies...");

    // Now that genres are loaded, we can safely load movies
    // Use a short delay to ensure the UI is responsive
    QTimer::singleShot(100, this, [this]() {
        loadMoviesFromApi("");
    });
}

void MoviesWindow::on_refreshButton_clicked()
{
    loadMoviesFromApi(ui->searchLineEdit->text());
}

void MoviesWindow::showStatusMessage(const QString &message, int timeout)
{
    statusBar()->showMessage(message, timeout);
}

void MoviesWindow::on_logoutButton_clicked()
{
    // Émettre le signal de déconnexion
    emit logoutRequested();

    // Cacher la fenêtre au lieu de la fermer
    hide();

    // Ne pas appeler close() ici pour éviter de détruire la fenêtre
}

void MoviesWindow::on_genreFilterButton_clicked()
{
    // Get all available genres from the database
    QStringList genres = DatabaseManager::instance().getAllGenres();

    if (genres.isEmpty()) {
        QMessageBox::information(this, "No Genres", "No movie genres found in the database.");
        return;
    }

    // Create a custom popup widget for 2-column layout
    QWidget *popupWidget = new QWidget(this);
    popupWidget->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    popupWidget->setAttribute(Qt::WA_DeleteOnClose);

    // Set the styling for the popup widget
    popupWidget->setStyleSheet(
        "QWidget {"
        "   background-color: #2c3e50;"
        "   border: 1px solid #34495e;"
        "   border-radius: 6px;"
        "   padding: 8px;"
        "}"
        "QPushButton {"
        "   background-color: transparent;"
        "   color: #ecf0f1;"
        "   padding: 6px 12px;"
        "   margin: 1px 2px;"
        "   border-radius: 3px;"
        "   height: 28px;"
        "   text-align: left;"
        "   border: none;"
        "   min-width: 140px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #3498db;"
        "}"
        "QPushButton:checked {"
        "   background-color: #2980b9;"
        "   color: white;"
        "   font-weight: bold;"
        "}"
        "QLabel {"
        "   color: #e74c3c;"
        "   font-weight: bold;"
        "   padding: 4px 8px;"
        "   margin: 2px 0px;"
        "   background-color: #34495e;"
        "   border-radius: 3px;"
        "}"
        "QLabel[class=\"title\"] {"
        "   background-color: #1a2530;"
        "   color: #f39c12;"
        "   font-weight: bold;"
        "   padding: 8px 16px;"
        "   margin: 0px 0px 8px 0px;"
        "   border-radius: 3px;"
        "   border-bottom: 1px solid #3498db;"
        "}"
    );

    // Create main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(popupWidget);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // Add title
    QLabel *titleLabel = new QLabel("🎬 Select Genre", popupWidget);
    titleLabel->setProperty("class", "title");
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // Add "All Genres" button (full width)
    QPushButton *allGenresBtn = new QPushButton("🌐 All Genres", popupWidget);
    allGenresBtn->setCheckable(true);
    if (m_currentGenre.isEmpty()) {
        allGenresBtn->setChecked(true);
    }
    connect(allGenresBtn, &QPushButton::clicked, this, [this, popupWidget]() {
        showStatusMessage("Filtering by all genres...");
        m_currentGenre.clear();
        loadMovies();
        showStatusMessage("Showing all movies");
        popupWidget->close();
    });
    mainLayout->addWidget(allGenresBtn);

    // Group genres by category for better organization
    QMap<QString, QStringList> genreCategories;
    genreCategories["Popular"] = QStringList() << "Action" << "Adventure" << "Comedy" << "Drama" << "Thriller";
    genreCategories["Specialty"] = QStringList() << "Animation" << "Family" << "Fantasy" << "Science Fiction" << "Horror";
    genreCategories["Other"] = QStringList();

    // Categorize remaining genres and create buttons
    for (auto it = genreCategories.begin(); it != genreCategories.end(); ++it) {
        const QString &category = it.key();
        const QStringList &categoryGenres = it.value();

        if (categoryGenres.isEmpty()) continue;

        // Add category header
        QLabel *categoryLabel = new QLabel(QString("⭐ %1 Genres").arg(category), popupWidget);
        mainLayout->addWidget(categoryLabel);

        // Create 2-column layout for this category
        QGridLayout *categoryGrid = new QGridLayout();
        categoryGrid->setSpacing(2);
        categoryGrid->setContentsMargins(0, 0, 0, 4);

        int row = 0;
        int col = 0;
        for (const QString &genre : categoryGenres) {
            if (!genres.contains(genre)) continue; // Skip if not in database

            QString emoji = getGenreEmoji(genre);
            QPushButton *genreBtn = new QPushButton(QString("%1 %2").arg(emoji, genre), popupWidget);
            genreBtn->setCheckable(true);

            if (m_currentGenre == genre) {
                genreBtn->setChecked(true);
            }

            connect(genreBtn, &QPushButton::clicked, this, [this, genre, popupWidget]() {
                showStatusMessage(QString("Filtering by %1 movies...").arg(genre));
                m_currentGenre = genre;

                // Load movies with the selected genre
                m_movieModel->loadMoviesByGenre(genre, ui->sortComboBox->currentData().toString(), m_userId);

                // Get the filtered movies
                m_allMovies = m_movieModel->getMovies();

                if (m_allMovies.isEmpty()) {
                    showStatusMessage(QString("No movies found in genre: %1").arg(genre));
                } else {
                    // Reset to first page and display
                    m_currentPage = 0;
                    displayMoviesPage(m_currentPage);
                    updatePaginationControls();

                    showStatusMessage(QString("Showing %1 movies in genre: %2")
                                     .arg(m_allMovies.size())
                                     .arg(genre));
                }
                popupWidget->close();
            });

            categoryGrid->addWidget(genreBtn, row, col);

            col++;
            if (col >= 2) {
                col = 0;
                row++;
            }
        }

        mainLayout->addLayout(categoryGrid);
    }

    // Handle "Other" genres that weren't categorized
    QStringList otherGenres;
    for (const QString &genre : genres) {
        if (genre == "All Genres") continue;

        bool categorized = false;
        for (auto it = genreCategories.begin(); it != genreCategories.end(); ++it) {
            if (it.value().contains(genre)) {
                categorized = true;
                break;
            }
        }

        if (!categorized) {
            otherGenres.append(genre);
        }
    }

    if (!otherGenres.isEmpty()) {
        QLabel *otherLabel = new QLabel("📂 Other Genres", popupWidget);
        mainLayout->addWidget(otherLabel);

        QGridLayout *otherGrid = new QGridLayout();
        otherGrid->setSpacing(2);
        otherGrid->setContentsMargins(0, 0, 0, 4);

        int row = 0;
        int col = 0;
        for (const QString &genre : otherGenres) {
            QString emoji = getGenreEmoji(genre);
            QPushButton *genreBtn = new QPushButton(QString("%1 %2").arg(emoji, genre), popupWidget);
            genreBtn->setCheckable(true);

            if (m_currentGenre == genre) {
                genreBtn->setChecked(true);
            }

            connect(genreBtn, &QPushButton::clicked, this, [this, genre, popupWidget]() {
                showStatusMessage(QString("Filtering by %1 movies...").arg(genre));
                m_currentGenre = genre;

                // Load movies with the selected genre
                m_movieModel->loadMoviesByGenre(genre, ui->sortComboBox->currentData().toString(), m_userId);

                // Get the filtered movies
                m_allMovies = m_movieModel->getMovies();

                if (m_allMovies.isEmpty()) {
                    showStatusMessage(QString("No movies found in genre: %1").arg(genre));
                } else {
                    // Reset to first page and display
                    m_currentPage = 0;
                    displayMoviesPage(m_currentPage);
                    updatePaginationControls();

                    showStatusMessage(QString("Showing %1 movies in genre: %2")
                                     .arg(m_allMovies.size())
                                     .arg(genre));
                }
                popupWidget->close();
            });

            otherGrid->addWidget(genreBtn, row, col);

            col++;
            if (col >= 2) {
                col = 0;
                row++;
            }
        }

        mainLayout->addLayout(otherGrid);
    }

    // Set the popup size and position
    popupWidget->setFixedWidth(320);
    popupWidget->adjustSize();

    // Position the popup below the genre filter button
    QPoint buttonPos = ui->genreFilterButton->mapToGlobal(QPoint(0, ui->genreFilterButton->height()));
    popupWidget->move(buttonPos);

    // Show the popup
    popupWidget->show();
}

void MoviesWindow::on_nextPageButton_clicked()
{
    int totalPages = (m_allMovies.size() + m_moviesPerPage - 1) / m_moviesPerPage;
    if (m_currentPage < totalPages - 1) {
        m_currentPage++;
        displayMoviesPage(m_currentPage);
        updatePaginationControls();
    }
}

void MoviesWindow::on_prevPageButton_clicked()
{
    if (m_currentPage > 0) {
        m_currentPage--;
        displayMoviesPage(m_currentPage);
        updatePaginationControls();
    }
}

void MoviesWindow::closeEvent(QCloseEvent *event)
{
    // Arrêter tous les timers
    if (m_searchTimer) {
        m_searchTimer->stop();
    }

    // Déconnecter tous les signaux
    disconnect(m_apiManager, nullptr, this, nullptr);

    // Nettoyer les widgets
    clearMovieWidgets();

    // Traiter tous les événements en attente
    QApplication::processEvents();

    // Si l'utilisateur ferme la fenêtre avec le bouton X, on émet le signal de déconnexion
    if (event->spontaneous()) {
        emit logoutRequested();
    }

    // Cacher la fenêtre au lieu de la fermer
    hide();

    // Ignorer l'événement de fermeture pour éviter que la fenêtre ne soit détruite
    event->ignore();
}

void MoviesWindow::changeEvent(QEvent *event)
{
    // Handle window state changes
    if (event->type() == QEvent::WindowStateChange) {
        QWindowStateChangeEvent *stateEvent = static_cast<QWindowStateChangeEvent*>(event);

        // Check if window was minimized
        if (isMinimized()) {
            qDebug() << "MoviesWindow minimized";

            // Ensure window stays in taskbar when minimized
            if (windowFlags() & Qt::Dialog) {
                // If window has Dialog flag, change it to Window
                setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint |
                              Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
                // Show the window again (minimized)
                show();
            }
        }
        // Check if window was restored from minimized state
        else if (stateEvent->oldState() & Qt::WindowMinimized) {
            qDebug() << "MoviesWindow restored from minimized state";
        }
    }

    // Let the base class handle the event
    QMainWindow::changeEvent(event);
}

QString MoviesWindow::getGenreEmoji(const QString &genre)
{
    if (genre == "Action" || genre == "Adventure" || genre == "Thriller") {
        return "💥";
    } else if (genre == "Comedy") {
        return "😂";
    } else if (genre == "Horror") {
        return "👻";
    } else if (genre == "Drama" || genre == "Romance") {
        return "💔";
    } else if (genre == "Science Fiction" || genre == "Fantasy") {
        return "🚀";
    } else if (genre == "Animation") {
        return "🎬";
    } else if (genre == "Family") {
        return "👪";
    } else if (genre == "Documentary") {
        return "📚";
    } else if (genre == "History") {
        return "🏛️";
    } else if (genre == "Music") {
        return "🎵";
    } else if (genre == "Mystery") {
        return "🔍";
    } else if (genre == "War") {
        return "⚔️";
    } else if (genre == "Western") {
        return "🤠";
    } else if (genre == "Crime") {
        return "🔪";
    } else {
        return "🎭"; // Default emoji for unknown genres
    }
}

void MoviesWindow::searchMoviesLocally(const QString &searchQuery)
{
    if (searchQuery.isEmpty()) {
        loadMovies();
        return;
    }

    showStatusMessage(QString("Searching for '%1'...").arg(searchQuery));

    // Clear current genre filter when searching
    m_currentGenre.clear();

    // Use optimized database search function
    QString sortBy = ui->sortComboBox->currentData().toString();
    m_movieModel->searchMovies(searchQuery, sortBy, m_userId);

    // Get the search results
    m_allMovies = m_movieModel->getMovies();

    if (m_allMovies.isEmpty()) {
        showStatusMessage(QString("No local results found for '%1'. Searching online...").arg(searchQuery));

        // If no local results, search online but with limited scope for speed
        QTimer::singleShot(100, this, [this, searchQuery]() {
            m_apiManager->searchMovies(searchQuery, 1, 2); // Only search first 2 pages (40 movies)
        });
    } else {
        // Reset to first page and display local results
        m_currentPage = 0;
        displayMoviesPage(m_currentPage);
        updatePaginationControls();

        showStatusMessage(QString("Found %1 movies matching '%2'")
                         .arg(m_allMovies.size())
                         .arg(searchQuery));
    }
}

