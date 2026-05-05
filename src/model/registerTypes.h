#pragma once

#include <QMetaType>
#include <QMap>
#include <QString>
#include <QVector>
#include <functional>
#include <optional>
#include <cstdint>

enum class RegisterLogicalType {
    Numeric,
    Boolean,
    Enum,
    Bitmask
};

struct BitmaskFlagDefinition {
    std::uint16_t mask = 0;
    QString label;
};

using RegisterPreprocessor = std::function<std::optional<double>(std::uint16_t rawValue)>;

struct RegisterDefinition {
    std::uint16_t address = 0;
    QString name;
    QString description = "";
    QString unit;
    RegisterLogicalType logicalType = RegisterLogicalType::Numeric;
    QMap<std::uint16_t, QString> enumLabels;
    QVector<BitmaskFlagDefinition> bitmaskFlags;
    QString falseText = "False";
    QString trueText = "True";
    RegisterPreprocessor preprocess;
    int decimals = 2;
};

struct RegisterValueState {
    bool hasValue = false;
    bool isValid = true;
    std::uint16_t rawValue = 0;
};

Q_DECLARE_METATYPE(RegisterDefinition)
