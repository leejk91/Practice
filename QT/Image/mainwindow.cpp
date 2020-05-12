#include "mainwindow.h"
#include "ui_mainwindow.h"

bool A = false;
int type = 0;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);


    ui->label->setStyleSheet("QLabel{font:bold 20px; color:Black;}");
    ui->label->setText("※ 테스트 메세지 테스트 메세지 ※");
    ui->label->adjustSize();
    ui->label->setGeometry(0, 100, ui->label->width(), ui->label->height());

    timer = new QTimer;
    connect(timer, SIGNAL(timeout()), this, SLOT(on_timeout()));
    timer->start(1000);

    stimer = new QTimer;
    connect(stimer, SIGNAL(timeout()), this, SLOT(on_stimeout()));





}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    stimer->start(50);
    type = 1;
}

void MainWindow::on_pushButton_2_clicked()
{
    timer->stop();
    stimer->stop();
}

void MainWindow::on_timeout()
{
    if (A==false)
    {
    ui->label->setStyleSheet("QLabel{font: 20px; color:red}");
    A=true;
    }
    else if (A==true)
    {
    ui->label->setStyleSheet("QLabel{font: 20px; color:blue}");
    A=false;
    }
}
void MainWindow::on_stimeout()
{
    QRect rect = ui->label->geometry();
    if(type == 1)
    {
    ui->label->setGeometry(rect.x() , rect.y(), rect.width()-10, rect.height());
    }
    if(type == 2)
    {
    ui->label->setGeometry(rect.x() , rect.y(), rect.width()+10, rect.height());
    }

}




void MainWindow::on_pushButton_3_clicked()
{
    type=2;
}

void MainWindow::on_pushButton_5_clicked()
{

}

void MainWindow::on_pushButton_4_clicked()
{

}
