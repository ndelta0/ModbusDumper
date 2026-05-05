#pragma once

#include "model/registerTypes.h"

class RegisterDataRecorder {
public:
    virtual ~RegisterDataRecorder() = default;

    virtual bool start(const QString &filePath, const QVector<RegisterDefinition> &registers, QString &errorMessage) = 0;

    virtual void stop() = 0;

    [[nodiscard]] virtual bool isActive() const = 0;

    virtual void pushRow(QMap<const RegisterDefinition*, RegisterValueState> &values) = 0;
};
