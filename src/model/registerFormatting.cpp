#include "model/registerFormatting.h"

#include <QStringList>

QString formatRegisterDisplayValue(const RegisterDefinition &definition, const RegisterValueState &valueState) {
    if (!valueState.hasValue) {
        return QStringLiteral("-");
    }
    if (!valueState.isValid) {
        return QStringLiteral("Invalid");
    }

    const std::uint16_t rawValue = valueState.rawValue;
    auto appendUnit = [&definition](const QString &baseValue) {
        if (definition.unit.isEmpty()) {
            return baseValue;
        }
        return QStringLiteral("%1 %2").arg(baseValue, definition.unit);
    };

    switch (definition.logicalType) {
        case RegisterLogicalType::Numeric: {
            if (definition.preprocess) {
                const std::optional<double> convertedValue = definition.preprocess(rawValue);
                if (!convertedValue.has_value()) {
                    return QStringLiteral("Invalid");
                }
                return appendUnit(QString::number(*convertedValue, 'f', definition.decimals));
            }
            return appendUnit(QString::number(rawValue, 'f', definition.decimals));
        }
        case RegisterLogicalType::Boolean:
            return appendUnit(rawValue == 0 ? definition.falseText : definition.trueText);
        case RegisterLogicalType::Enum: {
            if (const auto enumIter = definition.enumLabels.constFind(rawValue);
                enumIter != definition.enumLabels.cend()) {
                return appendUnit(enumIter.value());
            }
            return appendUnit(QStringLiteral("Unknown (%1)").arg(rawValue));
        }
        case RegisterLogicalType::Bitmask: {
            QStringList activeFlags;
            for (const auto &[mask, label]: definition.bitmaskFlags) {
                if ((rawValue & mask) == mask) {
                    activeFlags.push_back(label);
                }
            }

            const QString hexString = QStringLiteral("0x%1").arg(rawValue, 4, 16, QChar('0')).toUpper();
            if (activeFlags.isEmpty()) {
                return appendUnit(QStringLiteral("%1 (None)").arg(hexString));
            }
            return appendUnit(QStringLiteral("%1 (%2)").arg(hexString, activeFlags.join(QStringLiteral(" | "))));
        }
    }

    return QStringLiteral("Invalid");
}
