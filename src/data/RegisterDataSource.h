#pragma once

#include <QObject>
#include "model/registerTypes.h"

class RegisterDataSource : public QObject {
    Q_OBJECT

public:
    explicit RegisterDataSource(QObject *parent = nullptr) : QObject(parent) {
    }

    ~RegisterDataSource() override = default;

    virtual const QVector<RegisterDefinition> &getRegisters() = 0;

    virtual void start() = 0;

    virtual void stop() = 0;

signals:
    void connected();

    void disconnected();

    void refreshCycleCompleted(QMap<const RegisterDefinition *, RegisterValueState> values);

    void statusChanged(const QString &statusMessage);

    void errorOccurred(const QString &errorMessage);
};
