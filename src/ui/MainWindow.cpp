#include "ui/MainWindow.h"

#include <QHeaderView>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
#include <QDateTime>
#include <QApplication>
#include <QVBoxLayout>
#include <QWidget>

#include "data/MarsrockMpptBleDataSource.hpp"
#include "io/CsvRegisterDataRecorder.h"
#include "model/RegisterTableModel.h"
#include "model/registerTypes.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    buildUi();

    m_model = new RegisterTableModel(this);
    m_tableView->setModel(m_model);

    m_dataSource = std::make_unique<MarsrockMpptBleDataSource>(this);
    m_dataRecorder = std::make_unique<CsvRegisterDataRecorder>();
    connect(m_dataSource.get(), &RegisterDataSource::connected, this, &MainWindow::onConnected);
    connect(m_dataSource.get(), &RegisterDataSource::disconnected, this, &MainWindow::onDisconnected);
    connect(m_dataSource.get(), &RegisterDataSource::refreshCycleCompleted, this, &MainWindow::onRefreshCycleCompleted);
    connect(m_dataSource.get(), &RegisterDataSource::statusChanged, this, &MainWindow::onDataSourceStatusChanged);
    connect(m_dataSource.get(), &RegisterDataSource::errorOccurred, this, [this](const QString &message) {
        QMessageBox::critical(this, "Data Source Error", message);
    });

    applyConnectionState(ConnectionState::Disconnected);
}

void MainWindow::onConnectClicked() {
    applyConnectionState(ConnectionState::Connecting);
    m_model->clearValues();
    m_dataSource->start();
}

void MainWindow::onDisconnectClicked() {
    applyConnectionState(ConnectionState::Disconnecting);
    if (m_dataRecorder->isActive()) {
        m_dataRecorder->stop();
    }
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    m_dataSource->stop();
}

void MainWindow::onStartSaveClicked() {
    if (m_connectionState != ConnectionState::Connected) {
        return;
    }

    const QString defaultName =
            QStringLiteral("capture_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    const QString path = QFileDialog::getSaveFileName(this, "Save CSV", defaultName, "CSV Files (*.csv)");
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!m_dataRecorder->start(path, m_dataSource->getRegisters(), error)) {
        QMessageBox::critical(this, "CSV Start Error", QStringLiteral("Could not start CSV capture:\n%1").arg(error));
        return;
    }
    updateSaveButtonsState();
}

void MainWindow::onStopSaveClicked() {
    m_dataRecorder->stop();
    updateSaveButtonsState();
}

void MainWindow::onConnected() {
    applyConnectionState(ConnectionState::Connected);

    m_model->setRegisters(m_dataSource->getRegisters());
}

void MainWindow::onDisconnected() {
    applyConnectionState(ConnectionState::Disconnected);
}

void MainWindow::onRefreshCycleCompleted(QMap<const RegisterDefinition *, RegisterValueState> values) {
    for (const auto [key, val]: values.asKeyValueRange()) {
        if (val.hasValue) {
            m_model->setRegisterValue(key->address, val.rawValue, val.isValid);
        }
    }

    if (m_dataRecorder->isActive()) {
        m_dataRecorder->pushRow(values);
    }
}

void MainWindow::onDataSourceStatusChanged(const QString &statusMessage) {
    if (m_connectionState != ConnectionState::Connecting) {
        return;
    }
    m_statusLabel->setText(statusMessage);
}

void MainWindow::buildUi() {
    setWindowTitle("Register Monitor");
    resize(760, 420);

    auto *central = new QWidget(this);
    setCentralWidget(central);

    auto *mainLayout = new QVBoxLayout(central);
    auto *controlsLayout = new QHBoxLayout();

    m_connectButton = new QPushButton("Connect", this);
    m_disconnectButton = new QPushButton("Disconnect", this);
    m_startSaveButton = new QPushButton("Start Save", this);
    m_stopSaveButton = new QPushButton("Stop Save", this);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setMinimumWidth(96);
    m_connectionProgress = new QProgressBar(this);
    m_connectionProgress->setRange(0, 0);
    m_connectionProgress->setFixedWidth(72);
    m_connectionProgress->setTextVisible(false);
    m_connectionProgress->setVisible(false);

    controlsLayout->addWidget(m_connectButton);
    controlsLayout->addWidget(m_disconnectButton);
    controlsLayout->addSpacing(16);
    controlsLayout->addWidget(m_startSaveButton);
    controlsLayout->addWidget(m_stopSaveButton);
    controlsLayout->addStretch();
    controlsLayout->addWidget(new QLabel("Status:", this));
    controlsLayout->addWidget(m_connectionProgress);
    controlsLayout->addWidget(m_statusLabel);

    m_tableView = new QTableView(this);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_tableView->horizontalHeader()->resizeSection(0, 88);
    m_tableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

    mainLayout->addLayout(controlsLayout);
    mainLayout->addWidget(m_tableView, 1);

    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(m_startSaveButton, &QPushButton::clicked, this, &MainWindow::onStartSaveClicked);
    connect(m_stopSaveButton, &QPushButton::clicked, this, &MainWindow::onStopSaveClicked);
}

void MainWindow::applyConnectionState(const ConnectionState state) {
    m_connectionState = state;

    switch (state) {
        case ConnectionState::Disconnected:
            m_connectionProgress->setVisible(false);
            m_connectButton->setText("Connect");
            m_connectButton->setEnabled(true);
            m_disconnectButton->setText("Disconnect");
            m_disconnectButton->setEnabled(false);
            m_statusLabel->setText("Disconnected");
            break;
        case ConnectionState::Connecting:
            m_connectionProgress->setVisible(true);
            m_connectButton->setText("Connecting...");
            m_connectButton->setEnabled(false);
            m_disconnectButton->setText("Disconnect");
            m_disconnectButton->setEnabled(false);
            m_statusLabel->setText("Scanning...");
            break;
        case ConnectionState::Connected:
            m_connectionProgress->setVisible(false);
            m_connectButton->setText("Connect");
            m_connectButton->setEnabled(false);
            m_disconnectButton->setText("Disconnect");
            m_disconnectButton->setEnabled(true);
            m_statusLabel->setText("Connected");
            break;
        case ConnectionState::Disconnecting:
            m_connectionProgress->setVisible(false);
            m_connectButton->setText("Connect");
            m_connectButton->setEnabled(false);
            m_disconnectButton->setText("Disconnecting...");
            m_disconnectButton->setEnabled(false);
            m_statusLabel->setText("Disconnecting...");
            break;
    }
    updateSaveButtonsState();
}

void MainWindow::updateSaveButtonsState() const {
    const bool saving = m_dataRecorder->isActive();
    const bool connected = m_connectionState == ConnectionState::Connected;
    m_startSaveButton->setEnabled(connected && !saving);
    m_stopSaveButton->setEnabled(connected && saving);
}
