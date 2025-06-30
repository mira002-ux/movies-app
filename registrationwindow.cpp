#include "registrationwindow.h"
#include "ui_registrationwindow.h"
#include "databasemanager.h"

RegistrationWindow::RegistrationWindow(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RegistrationWindow)
{
    ui->setupUi(this);
    setWindowTitle("Register");

    // Set window flags to ensure the window stays in the taskbar
    setWindowFlags(Qt::Window | Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint);

    // Set attributes to ensure proper window behavior
    setAttribute(Qt::WA_DeleteOnClose, false);
}

RegistrationWindow::~RegistrationWindow()
{
    delete ui;
}

void RegistrationWindow::on_registerButton_clicked()
{
    QString username = ui->usernameLineEdit->text();
    QString password = ui->passwordLineEdit->text();
    QString confirmPassword = ui->confirmPasswordLineEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "Registration Error", "Username and password cannot be empty");
        return;
    }

    if (password != confirmPassword) {
        QMessageBox::warning(this, "Registration Error", "Passwords do not match");
        return;
    }

    if (DatabaseManager::instance().registerUser(username, password)) {
        QMessageBox::information(this, "Registration", "Registration successful! You can now login.");
        emit registrationSuccessful();
        accept();
    } else {
        QMessageBox::warning(this, "Registration Error", "Username already exists or database error");
    }
}

void RegistrationWindow::on_backToLoginButton_clicked()
{
    emit showLogin();
    reject();
}

void RegistrationWindow::changeEvent(QEvent *event)
{
    // Handle window state changes
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized()) {
            qDebug() << "RegistrationWindow minimized";
        }
    }

    // Let the base class handle the event
    QDialog::changeEvent(event);
}