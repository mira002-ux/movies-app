#ifndef MOVIESWINDOW_H
#define MOVIESWINDOW_H

#include <QMainWindow>
#include <QSortFilterProxyModel>
#include <QList>
#include <QGridLayout>
#include <QTimer>
#include <QElapsedTimer>
#include "moviemodel.h"
#include "moviewidget.h"
#include "apimanager.h"

namespace Ui {
class MoviesWindow;
}

class MoviesWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MoviesWindow(int userId, QWidget *parent = nullptr);
    ~MoviesWindow();

protected:
    // Surcharger l'événement de fermeture
    void closeEvent(QCloseEvent *event) override;

    // Surcharger l'événement de changement d'état
    void changeEvent(QEvent *event) override;

private slots:
    void on_sortComboBox_currentIndexChanged(int index);
    void on_tabWidget_currentChanged(int index);
    void on_rateButton_clicked();
    void on_searchLineEdit_textChanged(const QString &text);
    void on_logoutButton_clicked();
    void on_genreFilterButton_clicked();

    void onMovieWidgetFavoriteClicked(int movieId);
    void onMovieWidgetWatchlistClicked(int movieId);
    void onMovieWidgetClicked(int movieId);
    void onMovieWidgetTrailerClicked(int movieId);
    void onMovieWidgetCastClicked(int movieId);

    // API related slots
    void onMoviesLoaded(const QList<Movie> &movies);
    void onMovieDetailsLoaded(const Movie &movie);
    void onMovieTrailerFound(int apiId, const QString &trailerUrl);
    void onApiError(const QString &errorMessage);
    void onGenresLoaded();
    void on_refreshButton_clicked();
    void on_nextPageButton_clicked();
    void on_prevPageButton_clicked();

signals:
    void logoutRequested();

private:
    Ui::MoviesWindow *ui;
    MovieModel *m_movieModel;
    QSortFilterProxyModel *m_proxyModel;
    ApiManager *m_apiManager;
    int m_userId;
    QList<MovieWidget*> m_movieWidgets;
    int m_selectedMovieId;
    QTimer *m_searchTimer;

    // Pagination variables
    int m_currentPage;
    int m_moviesPerPage;
    QList<Movie> m_allMovies;
    QList<Movie> m_firstPageMovies; // Cache for the first page's movies

    // Genre filter
    QString m_currentGenre;

    // Loading state management
    bool m_isLoadingMovies;
    int m_targetMovieCount;
    QTimer *m_loadingProgressTimer;

    // Throttled UI refresh timer
    QElapsedTimer m_lastRefreshTime;

    void setupModel();
    void setupApiManager();
    void loadMovies();
    void loadMoviesFromApi(const QString &searchQuery = QString());
    void displayMovies();
    void displayMoviesPage(int page);
    void clearMovieWidgets();
    void toggleFavorite(int movieId);
    void toggleWatchlist(int movieId);
    MovieWidget* createMovieWidget(const Movie &movie);
    void showStatusMessage(const QString &message, int timeout = 3000);
    void updatePaginationControls();
    QString getGenreEmoji(const QString &genre);
    void searchMoviesLocally(const QString &searchQuery);
};

#endif // MOVIESWINDOW_H