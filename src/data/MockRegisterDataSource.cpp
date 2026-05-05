#include "data/MockRegisterDataSource.h"

#include <complex>
#include <QRandomGenerator>
#include <QtGlobal>

#include "generateDataRegisters.h"

MockRegisterDataSource::MockRegisterDataSource(QObject *parent)
    : RegisterDataSource(parent) {
    m_cycleTimer.setSingleShot(true);
    connect(&m_cycleTimer, &QTimer::timeout, this, &MockRegisterDataSource::startNextCycle);

    const auto omega = [] -> double {
        constexpr double min = 0.01;
        constexpr double max = 1;
        return QRandomGenerator::global()->bounded(max-min) + min;
    };
    const auto phase = [] -> double {
        constexpr double min = 0;
        constexpr double max = 2 * M_PI;
        return QRandomGenerator::global()->bounded(max-min) + min;
    };

    const auto registers = GetRegisters();
#define GEN(min, max) {min, max, omega(), phase()}
    m_generators[0x0005] = GEN(0, 100);
    m_generators[0x0007] = GEN(200, 292);
    m_generators[0x0008] = GEN(0, 1000);
    m_generators[0x0009] = GEN(0, 300);
    m_generators[0x000A] = GEN(0, 1000);
    m_generators[0x000B] = GEN(0, 240);
    m_generators[0x000C] = GEN(0, 1000);
    m_generators[0x000D] = GEN(0, 500);
    m_generators[0x000E] = GEN(0, 1000);
    m_generators[0x001D] = GEN(0, 1000);
    m_generators[0x002C] = GEN(0, 4500);
#undef GEN

    m_registers = registers;
}

const QVector<RegisterDefinition> & MockRegisterDataSource::getRegisters() {
    return m_registers;
}

void MockRegisterDataSource::start() {
    if (m_isConnected) {
        return;
    }
    m_isConnected = true;
    m_cycleInFlight = false;
    m_pendingEmissions = 0;
    scheduleNextCycle(100);
    emit connected();
}

void MockRegisterDataSource::stop() {
    if (!m_isConnected) {
        return;
    }
    m_isConnected = false;
    m_cycleTimer.stop();
    m_cycleInFlight = false;
    m_pendingEmissions = 0;
    emit disconnected();
}

MockRegisterDataSource::MockGenerator::MockGenerator(const uint16_t min, const uint16_t max,
    const double omega, const double phase) : m_amplitude((max - min)/2), m_omega(omega), m_phase(phase) {
    m_value = next();
}

uint16_t MockRegisterDataSource::MockGenerator::next() {
    const auto value = m_amplitude * std::sin(2 * M_PI * m_omega * m_step + m_phase);
    m_step += 0.01;
    return static_cast<uint16_t>(value + m_amplitude);
}

void MockRegisterDataSource::MockGenerator::reset() {
    m_step = 0;
    m_value = next();
}

void MockRegisterDataSource::startNextCycle() {
    if (!m_isConnected || m_cycleInFlight) {
        return;
    }

    m_cycleInFlight = true;
    m_cycleElapsedTimer.restart();

    QMap<const RegisterDefinition *, RegisterValueState> values;

    for (const RegisterDefinition &reg: m_registers) {
        const auto val = m_generators[reg.address].next();
        values.insert(&reg, {true, true, val});
    }

    emit refreshCycleCompleted(values);
    finalizeCycleIfReady();
}

void MockRegisterDataSource::scheduleNextCycle(const int delayMs) {
    if (!m_isConnected) {
        return;
    }
    m_cycleTimer.start(qMax(0, delayMs));
}

void MockRegisterDataSource::finalizeCycleIfReady() {
    if (!m_cycleInFlight) {
        return;
    }

    m_cycleInFlight = false;
    if (!m_isConnected) {
        return;
    }

    scheduleNextCycle(500);
}
