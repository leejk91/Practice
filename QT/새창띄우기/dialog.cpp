#include "dialog.h"
#include "ui_dialog.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"

Dialog::Dialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog)
{
    ui->setupUi(this);

    connect(this,SIGNAL(send_value(QString,int,int)),QMainWindow(),SLOT(recv_value(QString,int,int)));

}

Dialog::~Dialog()
{
    delete ui;


}

void Dialog::on_pushButton_clicked()
{
    name = ui->lineEdit->text();
    value = ui->lineEdit_2->text().toInt();
    state = ui->lineEdit_3->text().toInt();

    qDebug() << "n: "<< name << "v: " << value << "s: " << state;

    emit send_value(name,value,state);

    this->close();
}


void Dialog::on_pushButton_2_clicked()
{
    this->close();
}
