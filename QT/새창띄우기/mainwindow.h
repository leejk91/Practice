#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>

#include "dialog.h"
#include <QDebug>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QString name;
    int value;
    int state;

public slots:

    void recv_value(QString A,int B,int C);

private slots:
    void on_pushButton_clicked();

private:
    Ui::MainWindow *ui;

    QTreeWidget* tree = new QTreeWidget();
    //Dialog *Dialog; 새창 띄우는 방법2



};

#endif // MAINWINDOW_H
