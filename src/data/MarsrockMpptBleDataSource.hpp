#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "data/RegisterDataSource.h"

class MarsrockMpptBleDataSource final : public RegisterDataSource {
    Q_OBJECT

public:
    explicit MarsrockMpptBleDataSource(QObject *parent = nullptr);
    ~MarsrockMpptBleDataSource() override;

    const QVector<RegisterDefinition> &getRegisters() override;
    void start() override;
    void stop() override;

private:
    struct QueryRange {
        std::uint16_t startAddress = 0;
        std::uint16_t count = 0;
    };

    struct PollLayout {
        std::unordered_map<std::uint16_t, RegisterDefinition> registerByAddress;
        std::vector<QueryRange> queries;
    };

    void workerLoop();
    void publishError(const QString &message);
    void publishStatus(const QString &message);
    void publishConnected();
    void publishDisconnected();
    void publishRefreshCycleCompleted(const QMap<std::uint16_t, RegisterValueState> &valuesByAddress);

    PollLayout buildLayoutFromCsv() const;
    static PollLayout buildLayoutFromDefinitions(const QVector<RegisterDefinition> &definitions);

    std::atomic<bool> m_running{false};
    mutable std::mutex m_layoutMutex;
    PollLayout m_layout;
    QVector<RegisterDefinition> m_registers;

    std::mutex m_sleepMutex;
    std::condition_variable m_sleepCv;
    std::thread m_workerThread;
};
