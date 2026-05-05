#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include "data/RegisterDataSource.h"

class MockRegisterDataSource final : public RegisterDataSource {
    Q_OBJECT

    class MockGenerator {
    public:
        MockGenerator(uint16_t min = 0, uint16_t max = UINT16_MAX, double omega = 1, double phase = 0);

        uint16_t next();

        void reset();

    private:
        uint16_t m_amplitude;
        double m_omega;
        double m_phase;

        double m_step = 0;
        uint16_t m_value;
    };

public:
    explicit MockRegisterDataSource(QObject *parent = nullptr);

    const QVector<RegisterDefinition> & getRegisters() override;

    void start() override;

    void stop() override;

private slots:
    void startNextCycle();

private:
    void scheduleNextCycle(int delayMs);

    void finalizeCycleIfReady();

    bool m_isConnected = false;
    bool m_cycleInFlight = false;
    int m_pendingEmissions = 0;
    QVector<RegisterDefinition> m_registers;
    QTimer m_cycleTimer;
    QElapsedTimer m_cycleElapsedTimer;

    QMap<std::uint16_t, MockGenerator> m_generators; // Map register_id -> generator
};
