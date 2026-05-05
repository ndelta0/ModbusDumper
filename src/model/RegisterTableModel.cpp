#include "model/RegisterTableModel.h"
#include "model/registerFormatting.h"

RegisterTableModel::RegisterTableModel(QObject *parent)
    : QAbstractTableModel(parent) {
}

int RegisterTableModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

int RegisterTableModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return 3;
}

QVariant RegisterTableModel::data(const QModelIndex &index, const int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const auto &[definition, value] = m_rows[index.row()];
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0:
                return QStringLiteral("0x") + QStringLiteral("%1").arg(definition.address, 4, 16, QChar('0')).toUpper();
            case 1:
                return definition.name;
            case 2:
                return formatRegisterValue(definition, value);
            default:
                return {};
        }
    }

    if (role == Qt::ToolTipRole && index.column() == 1 && !definition.description.isEmpty()) {
        return definition.description;
    }

    if (role == Qt::TextAlignmentRole) {
        const auto alignment = index.column() == 2
                                   ? (Qt::AlignRight | Qt::AlignVCenter)
                                   : (Qt::AlignLeft | Qt::AlignVCenter);
        return QVariant(alignment);
    }

    return {};
}

QVariant RegisterTableModel::headerData(const int section, const Qt::Orientation orientation, const int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
        case 0:
            return QStringLiteral("Address");
        case 1:
            return QStringLiteral("Name");
        case 2:
            return QStringLiteral("Value");
        default:
            return {};
    }
}

void RegisterTableModel::setRegisters(QVector<RegisterDefinition> definitions) {
    beginResetModel();
    m_rows.clear();
    m_rows.reserve(definitions.size());
    for (const RegisterDefinition &definition: definitions) {
        m_rows.push_back(RegisterRow{definition, RegisterValueState{}});
    }
    endResetModel();
}

void RegisterTableModel::upsertRegister(const RegisterDefinition &definition) {
    for (int row = 0; row < m_rows.size(); ++row) {
        auto &[regDefinition, regValue] = m_rows[row];
        if (regDefinition.address != definition.address) {
            continue;
        }

        regDefinition = definition;
        emit dataChanged(index(row, 0), index(row, 2), {Qt::DisplayRole, Qt::ToolTipRole});
        return;
    }

    const int newRow = m_rows.size();
    beginInsertRows(QModelIndex(), newRow, newRow);
    m_rows.push_back(RegisterRow{definition, RegisterValueState{}});
    endInsertRows();
}

void RegisterTableModel::setRegisterValue(const std::uint16_t address, const std::uint16_t rawValue,
                                          const bool isValid) {
    for (int row = 0; row < m_rows.size(); ++row) {
        auto &[definition, value] = m_rows[row];
        if (definition.address != address) {
            continue;
        }

        value.hasValue = true;
        value.isValid = isValid;
        value.rawValue = rawValue;
        const QModelIndex changedIndex = index(row, 2);
        emit dataChanged(changedIndex, changedIndex, {Qt::DisplayRole});
        return;
    }
}

void RegisterTableModel::clearValues() {
    if (m_rows.empty()) {
        return;
    }

    for (auto &[definition, value]: m_rows) {
        value = RegisterValueState{};
    }
    emit dataChanged(index(0, 2), index(m_rows.size() - 1, 2), {Qt::DisplayRole});
}

QString RegisterTableModel::formatRegisterValue(const RegisterDefinition &definition,
                                                const RegisterValueState &valueState) {
    return formatRegisterDisplayValue(definition, valueState);
}
