#ifndef CASTDIALOG_H
#define CASTDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProgressBar>
#include <QGridLayout>
#include <QTabWidget>

class CastMemberWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CastMemberWidget(const QString &name, const QString &character, const QString &profilePath, QWidget *parent = nullptr);

private:
    QLabel *m_imageLabel;
    QLabel *m_nameLabel;
    QLabel *m_characterLabel;
    QNetworkAccessManager *m_networkManager;

    void loadImage(const QString &profilePath);
};

class CastDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CastDialog(int apiMovieId, const QString &movieTitle, QWidget *parent = nullptr);
    ~CastDialog();

private slots:
    void onCastDataReceived();
    void onCrewDataReceived();
    void onNetworkError(QNetworkReply::NetworkError error);

private:
    int m_apiMovieId;
    QString m_movieTitle;
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_castReply;
    QNetworkReply *m_crewReply;

    QVBoxLayout *m_mainLayout;
    QLabel *m_titleLabel;
    QTabWidget *m_tabWidget;
    QWidget *m_castTab;
    QWidget *m_crewTab;
    QGridLayout *m_castLayout;
    QGridLayout *m_crewLayout;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;

    void setupUi();
    void fetchCastData();
    void displayCastMembers(const QJsonArray &castArray);
    void displayCrewMembers(const QJsonArray &crewArray);
    void showError(const QString &message);

    // TMDb API constants
    const QString m_apiKey = "caa330b87a6b657b18354ead3c8a667a";
    const QString m_baseUrl = "https://api.themoviedb.org/3";
    const QString m_imageBaseUrl = "https://image.tmdb.org/t/p/w185";
};

#endif // CASTDIALOG_H
