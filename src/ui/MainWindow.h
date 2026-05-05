#pragma once

#include <QMainWindow>
#include <memory>
#include "data/RegisterDataSource.h"
#include "io/RegisterDataRecorder.h"

class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableView;
class RegisterTableModel;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    ~MainWindow() override = default;

private slots:
    void onConnectClicked();

    void onDisconnectClicked();

    void onStartSaveClicked();

    void onStopSaveClicked();

    void onConnected();

    void onDisconnected();

    void onRefreshCycleCompleted(QMap<const RegisterDefinition*, RegisterValueState> values);

    void onDataSourceStatusChanged(const QString &statusMessage);

private:
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Connected,
        Disconnecting,
    };

    void buildUi();

    void applyConnectionState(ConnectionState state);

    void updateSaveButtonsState() const;

    QTableView *m_tableView = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_disconnectButton = nullptr;
    QPushButton *m_startSaveButton = nullptr;
    QPushButton *m_stopSaveButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QProgressBar *m_connectionProgress = nullptr;
    RegisterTableModel *m_model = nullptr;
    std::unique_ptr<RegisterDataSource> m_dataSource;
    std::unique_ptr<RegisterDataRecorder> m_dataRecorder;
    ConnectionState m_connectionState = ConnectionState::Disconnected;
};
