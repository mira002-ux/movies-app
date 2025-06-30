#include "mainwindow.h"
#include "databasemanager.h"
#include <QCloseEvent>
#include <QApplication>
#include <QTimer>
#include <QEvent>
#include <QWindowStateChangeEvent>
#include <QVBoxLayout>
#include <QLabel>
#include <QIcon>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_loginWindow(nullptr)
    , m_registrationWindow(nullptr)
    , m_moviesWindow(nullptr)
{
    // Set window properties
    resize(800, 600);
    setWindowTitle("Movie Database");
    setWindowIcon(QIcon(":/images/logo.png"));

    // Set window flags to ensure it stays in the taskbar
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);

    // Set attributes to ensure proper window behavior
    setAttribute(Qt::WA_DeleteOnClose, false);

    // Create central widget
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // Create layout
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    // Create welcome label
    QLabel *welcomeLabel = new QLabel("Welcome to Movie Database", centralWidget);
    QFont font = welcomeLabel->font();
    font.setPointSize(18);
    font.setBold(true);
    welcomeLabel->setFont(font);
    welcomeLabel->setAlignment(Qt::AlignCenter);

    // Add widgets to layout
    layout->addWidget(welcomeLabel);

    // Initialize database
    if (!DatabaseManager::instance().initDatabase()) {
        qDebug() << "Failed to initialize database";
    }

    // Show login window on startup
    showLoginWindow();
}

MainWindow::~MainWindow()
{
    // Clean up all windows
    if (m_loginWindow) {
        m_loginWindow->deleteLater();
    }

    if (m_registrationWindow) {
        m_registrationWindow->deleteLater();
    }

    if (m_moviesWindow) {
        m_moviesWindow->deleteLater();
    }
}

void MainWindow::showLoginWindow()
{
    // Create a new login window if it doesn't exist
    if (!m_loginWindow) {
        m_loginWindow = new LoginWindow(this);
        connect(m_loginWindow, &LoginWindow::loginSuccessful, this, &MainWindow::onLoginSuccessful);
        connect(m_loginWindow, &LoginWindow::showRegistration, this, &MainWindow::showRegistrationWindow);
    }

    // Display the login window in full screen and bring it to the front
    m_loginWindow->showMaximized();
    m_loginWindow->raise();
    m_loginWindow->activateWindow();

    // Hide the main window
    hide();
}

void MainWindow::showRegistrationWindow()
{
    // Create a new registration window if it doesn't exist
    if (!m_registrationWindow) {
        m_registrationWindow = new RegistrationWindow(this);
        connect(m_registrationWindow, &RegistrationWindow::registrationSuccessful,
                this, &MainWindow::onRegistrationSuccessful);
        connect(m_registrationWindow, &RegistrationWindow::showLogin,
                this, &MainWindow::showLoginWindow);
    }

    // Display the registration window in full screen and bring it to the front
    m_registrationWindow->showMaximized();
    m_registrationWindow->raise();
    m_registrationWindow->activateWindow();

    // Close and delete the login window if it exists
    if (m_loginWindow) {
        // Disconnect signals to prevent any pending callbacks
        disconnect(m_loginWindow, nullptr, this, nullptr);

        // Close and delete the login window
        m_loginWindow->close();
        m_loginWindow->deleteLater();
        m_loginWindow = nullptr;
    }
}

void MainWindow::onLoginSuccessful(int userId)
{
    // Close and delete the login window
    if (m_loginWindow) {
        // Disconnect signals to prevent any pending callbacks
        disconnect(m_loginWindow, nullptr, this, nullptr);

        // Close and delete the login window
        m_loginWindow->close();
        m_loginWindow->deleteLater();
        m_loginWindow = nullptr;
    }

    // Clean up any existing movies window
    if (m_moviesWindow) {
        m_moviesWindow->hide();
        m_moviesWindow->deleteLater();
        m_moviesWindow = nullptr;
    }

    // Create a new movies window
    m_moviesWindow = new MoviesWindow(userId, this);
    connect(m_moviesWindow, &MoviesWindow::logoutRequested, this, &MainWindow::onLogoutRequested);

    // Set the Qt::WA_DeleteOnClose flag to false so the window isn't deleted when closed
    m_moviesWindow->setAttribute(Qt::WA_DeleteOnClose, false);

    // Display the movies window in full screen
    m_moviesWindow->setWindowState(Qt::WindowActive | Qt::WindowMaximized);
    m_moviesWindow->showMaximized();
    m_moviesWindow->raise();
    m_moviesWindow->activateWindow();
}

void MainWindow::onRegistrationSuccessful()
{
    // Clean up the registration window
    if (m_registrationWindow) {
        // Disconnect signals to prevent any pending callbacks
        disconnect(m_registrationWindow, nullptr, this, nullptr);

        // Close and delete the registration window
        m_registrationWindow->close();
        m_registrationWindow->deleteLater();
        m_registrationWindow = nullptr;
    }

    // Show the login window
    showLoginWindow();
}

void MainWindow::onLogoutRequested()
{
    // Clean up the movies window
    if (m_moviesWindow) {
        // Disconnect signals to prevent any pending callbacks
        disconnect(m_moviesWindow, nullptr, this, nullptr);

        // Close and delete the movies window
        m_moviesWindow->close();
        m_moviesWindow->deleteLater();
        m_moviesWindow = nullptr;
    }

    // Show the login window (this will create a new one if needed)
    showLoginWindow();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Clean up all windows before closing
    if (m_loginWindow) {
        // Disconnect signals to prevent any pending callbacks
        disconnect(m_loginWindow, nullptr, this, nullptr);

        // Close and delete the login window
        m_loginWindow->close();
        m_loginWindow->deleteLater();
        m_loginWindow = nullptr;
    }

    if (m_registrationWindow) {
        // Disconnect signals to prevent any pending callbacks
        disconnect(m_registrationWindow, nullptr, this, nullptr);

        // Close and delete the registration window
        m_registrationWindow->close();
        m_registrationWindow->deleteLater();
        m_registrationWindow = nullptr;
    }

    if (m_moviesWindow) {
        // Disconnect signals to prevent any pending callbacks
        disconnect(m_moviesWindow, nullptr, this, nullptr);

        // Close and delete the movies window
        m_moviesWindow->close();
        m_moviesWindow->deleteLater();
        m_moviesWindow = nullptr;
    }

    // Process all pending events
    QApplication::processEvents();

    // Accept the close event
    event->accept();

    // Force quit the application after a short delay
    QTimer::singleShot(100, this, &MainWindow::forceQuit);
}

void MainWindow::changeEvent(QEvent *event)
{
    // Handle window state changes
    if (event->type() == QEvent::WindowStateChange) {
        QWindowStateChangeEvent *stateEvent = static_cast<QWindowStateChangeEvent*>(event);

        // Check if window was minimized
        if (isMinimized()) {
            qDebug() << "MainWindow minimized";

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
            qDebug() << "MainWindow restored from minimized state";
        }
    }

    // Let the base class handle the event
    QMainWindow::changeEvent(event);
}

void MainWindow::forceQuit()
{
    // Close all windows
    if (m_loginWindow) {
        // Disconnect signals to prevent any pending callbacks
        disconnect(m_loginWindow, nullptr, this, nullptr);

        // Close and delete the login window
        m_loginWindow->close();
        m_loginWindow->deleteLater();
        m_loginWindow = nullptr;
    }

    if (m_registrationWindow) {
        // Disconnect signals to prevent any pending callbacks
        disconnect(m_registrationWindow, nullptr, this, nullptr);

        // Close and delete the registration window
        m_registrationWindow->close();
        m_registrationWindow->deleteLater();
        m_registrationWindow = nullptr;
    }

    if (m_moviesWindow) {
        // Disconnect signals to prevent any pending callbacks
        disconnect(m_moviesWindow, nullptr, this, nullptr);

        // Close and delete the movies window
        m_moviesWindow->close();
        m_moviesWindow->deleteLater();
        m_moviesWindow = nullptr;
    }

    // Process all pending events
    QApplication::processEvents();

    // Quit the application
    QApplication::quit();
}


