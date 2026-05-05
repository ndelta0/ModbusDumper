#pragma once

#include <QFile>
#include <QHash>
#include <QTextStream>

#include "io/RegisterDataRecorder.h"

class CsvRegisterDataRecorder final : public RegisterDataRecorder {
public:
    CsvRegisterDataRecorder() = default;

    ~CsvRegisterDataRecorder() override = default;

    bool start(const QString &filePath, const QVector<RegisterDefinition> &registers, QString &errorMessage) override;

    void stop() override;

    [[nodiscard]] bool isActive() const override;

    void pushRow(QMap<const RegisterDefinition*, RegisterValueState> &values) override;

private:
    QFile m_file;
    QTextStream m_stream;

    QHash<int, std::uint16_t> m_addressByOrder;
};
