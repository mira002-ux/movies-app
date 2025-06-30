#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    // Définir l'attribut Qt::AA_EnableHighDpiScaling avant de créer l'application
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    // Définir l'attribut Qt::AA_UseHighDpiPixmaps pour les icônes
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    // Créer l'application
    QApplication a(argc, argv);

    // Définir l'icône de l'application
    a.setWindowIcon(QIcon(":/images/logo.png"));

    // Créer la fenêtre principale
    MainWindow w;

    // Afficher la fenêtre principale
    w.show();

    // Exécuter l'application
    return a.exec();
}
