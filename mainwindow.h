#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "loginwindow.h"
#include "registrationwindow.h"
#include "movieswindow.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    // Surcharger l'événement de fermeture
    void closeEvent(QCloseEvent *event) override;

    // Surcharger l'événement de changement d'état
    void changeEvent(QEvent *event) override;

private slots:
    void showLoginWindow();
    void showRegistrationWindow();
    void onLoginSuccessful(int userId);
    void onRegistrationSuccessful();
    void onLogoutRequested();
    void forceQuit();

private:
    LoginWindow *m_loginWindow;
    RegistrationWindow *m_registrationWindow;
    MoviesWindow *m_moviesWindow;
};
#endif // MAINWINDOW_H
