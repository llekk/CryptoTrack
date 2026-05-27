/********************************************************************************
** Form generated from reading UI file 'ai_project.ui'
**
** Created by: Qt User Interface Compiler version 6.10.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_AI_PROJECT_H
#define UI_AI_PROJECT_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ai_project
{
public:
    QWidget *centralwidget;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *ai_project)
    {
        if (ai_project->objectName().isEmpty())
            ai_project->setObjectName("ai_project");
        ai_project->resize(800, 600);
        centralwidget = new QWidget(ai_project);
        centralwidget->setObjectName("centralwidget");
        ai_project->setCentralWidget(centralwidget);
        menubar = new QMenuBar(ai_project);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 800, 25));
        ai_project->setMenuBar(menubar);
        statusbar = new QStatusBar(ai_project);
        statusbar->setObjectName("statusbar");
        ai_project->setStatusBar(statusbar);

        retranslateUi(ai_project);

        QMetaObject::connectSlotsByName(ai_project);
    } // setupUi

    void retranslateUi(QMainWindow *ai_project)
    {
        ai_project->setWindowTitle(QCoreApplication::translate("ai_project", "ai_project", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ai_project: public Ui_ai_project {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_AI_PROJECT_H
