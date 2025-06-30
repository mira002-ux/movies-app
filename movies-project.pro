QT       += core gui sql network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET = movies-project
TEMPLATE = app

SOURCES += \
    apimanager.cpp \
    castdialog.cpp \
    main.cpp \
    mainwindow.cpp \
    loginwindow.cpp \
    moviemodel.cpp \
    moviewidget.cpp \
    registrationwindow.cpp \
    movieswindow.cpp \
    databasemanager.cpp



HEADERS += \
    apimanager.h \
    castdialog.h \
    mainwindow.h \
    loginwindow.h \
    registrationwindow.h \
    movieswindow.h \
    databasemanager.h \
    moviemodel.h \
    moviewidget.h


FORMS += \
    loginwindow.ui \
    registrationwindow.ui \
    movieswindow.ui

# Resources
RESOURCES += resources.qrc \
    images.qrc

# Correction: syntaxe DISTFILES correcte
DISTFILES += \
    images/country.jpg \
    images/the_devils_hacksaw.jpg
