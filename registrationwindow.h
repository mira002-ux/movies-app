#ifndef REGISTRATIONWINDOW_H
#define REGISTRATIONWINDOW_H

#include <QDialog>
#include <QMessageBox>

namespace Ui {
class RegistrationWindow;
}

class RegistrationWindow : public QDialog
{
    Q_OBJECT

public:
    explicit RegistrationWindow(QWidget *parent = nullptr);
    ~RegistrationWindow();

signals:
    void registrationSuccessful();
    void showLogin();

protected:
    // Override change event to handle window state changes
    void changeEvent(QEvent *event) override;

private slots:
    void on_registerButton_clicked();
    void on_backToLoginButton_clicked();

private:
    Ui::RegistrationWindow *ui;
};

#endif // REGISTRATIONWINDOW_H