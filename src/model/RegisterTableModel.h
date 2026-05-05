#pragma once

#include <QAbstractTableModel>
#include "model/registerTypes.h"

class RegisterTableModel final : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit RegisterTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent) const override;

    int columnCount(const QModelIndex &parent) const override;

    QVariant data(const QModelIndex &index, int role) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setRegisters(QVector<RegisterDefinition> definitions);

    void upsertRegister(const RegisterDefinition &definition);

    void setRegisterValue(std::uint16_t address, std::uint16_t rawValue, bool isValid);

    void clearValues();

private:
    static QString formatRegisterValue(const RegisterDefinition &definition, const RegisterValueState &valueState);

    struct RegisterRow {
        RegisterDefinition definition;
        RegisterValueState value;
    };

    QVector<RegisterRow> m_rows;
};
