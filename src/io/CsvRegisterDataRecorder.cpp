#include "io/CsvRegisterDataRecorder.h"

#include <QDateTime>

#include "model/registerFormatting.h"

bool CsvRegisterDataRecorder::start(const QString &filePath, const QVector<RegisterDefinition> &registers, QString &errorMessage) {
    stop();

    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        errorMessage = m_file.errorString();
        return false;
    }

    m_stream.setDevice(&m_file);
    m_stream << "timestamp_ms";
    int data_col_idx = 0;
    for (const RegisterDefinition &reg: registers) {
        m_addressByOrder.insert(data_col_idx++, reg.address);
        m_stream << ",\"" << reg.name << " [" << reg.unit << "]\"";
    }
    m_stream << "\n";
    m_stream.flush();

    return true;
}

void CsvRegisterDataRecorder::stop() {
    if (!m_file.isOpen()) {
        return;
    }

    m_stream.flush();
    m_stream.setDevice(nullptr);
    m_file.close();

    m_addressByOrder.clear();
}

bool CsvRegisterDataRecorder::isActive() const {
    return m_file.isOpen();
}

void CsvRegisterDataRecorder::pushRow(QMap<const RegisterDefinition *, RegisterValueState> &values) {
    if (!isActive()) {
        return;
    }
    
    const auto ts = QDateTime::currentMSecsSinceEpoch();
    
    m_stream << ts;
    
    const auto data_col_n = m_addressByOrder.size();
    for (int i = 0; i < data_col_n; ++i) {
        const auto address = m_addressByOrder[i];

        std::optional<std::pair<const RegisterDefinition *, RegisterValueState>> value = std::nullopt;
        for (auto pair: values.asKeyValueRange()) {
            if (pair.first->address == address) {
                value = pair;
                break;
            }
        }

        m_stream << ",";

        if (value) {
            const auto &definition = *value->first;

            if (const auto [hasValue, isValid, rawValue] = value->second; hasValue && isValid) {
                if (definition.preprocess) {
                    const std::optional<double> convertedValue = definition.preprocess(rawValue);
                    assert(convertedValue.has_value());
                    m_stream << QString::number(*convertedValue, 'f', definition.decimals);
                } else {
                    m_stream << rawValue;
                }
            } else {
                m_stream << 0;
            }
        } else {
            m_stream << 0;
        }
    }

    m_stream << "\n";
    m_stream.flush();
}