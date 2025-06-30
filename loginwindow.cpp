#include "loginwindow.h"
#include "ui_loginwindow.h"
#include "databasemanager.h"

LoginWindow::LoginWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LoginWindow)
{
    ui->setupUi(this);
    setWindowTitle("Login");

    // Set window flags to ensure the window stays in the taskbar
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);

    // Set attributes to ensure proper window behavior
    setAttribute(Qt::WA_DeleteOnClose, false);
}

LoginWindow::~LoginWindow()
{
    delete ui;
}

void LoginWindow::on_loginButton_clicked()
{
    QString username = ui->usernameLineEdit->text();
    QString password = ui->passwordLineEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Login Error", "Username and password cannot be empty");
        return;
    }

    if (DatabaseManager::instance().loginUser(username, password)) {
        QSqlQuery query;
        query.prepare("SELECT id FROM users WHERE username = ?");
        query.addBindValue(username);

        if (query.exec() && query.next()) {
            int userId = query.value(0).toInt();
            emit loginSuccessful(userId);
            accept();
        }
    } else {
        QMessageBox::warning(this, "Login Error", "Invalid username or password");
    }
}

void LoginWindow::on_registerButton_clicked()
{
    emit showRegistration();
}

void LoginWindow::changeEvent(QEvent *event)
{
    // Handle window state changes
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized()) {
            qDebug() << "LoginWindow minimized";
        }
    }

    // Let the base class handle the event
    QDialog::changeEvent(event);
}