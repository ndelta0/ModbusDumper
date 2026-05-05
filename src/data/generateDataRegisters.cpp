#include "generateDataRegisters.h"

#define PRE_SCALE(scale) [](const std::uint16_t raw) -> std::optional<double> { return static_cast<double>(raw) * scale; }

QVector<RegisterDefinition> GetRegisters() {
    return {
        {
            .address = 0x0005,
            .name = "Battery SoC",
            .unit = "%",
            .decimals = 0,
        },
        {
            .address = 0x0007,
            .name = "Battery Voltage",
            .unit = "V",
            .preprocess = PRE_SCALE(0.1),
            .decimals = 1
        },
        {
            .address = 0x0008,
            .name = "Battery Current",
            .unit = "A",
            .preprocess = PRE_SCALE(0.1),
            .decimals = 1
        },
        {
            .address = 0x0009,
            .name = "PV Voltage",
            .unit = "V",
            .preprocess = PRE_SCALE(0.1),
            .decimals = 1
        },
        {
            .address = 0x000A,
            .name = "PV Current",
            .unit = "A",
            .preprocess = PRE_SCALE(0.1),
            .decimals = 1
        },
        {
            .address = 0x000B,
            .name = "Turbine Voltage",
            .unit = "V",
            .preprocess = PRE_SCALE(0.1),
            .decimals = 1
        },
        {
            .address = 0x000C,
            .name = "Turbine Current",
            .unit = "A",
            .preprocess = PRE_SCALE(0.1),
            .decimals = 1
        },
        {
            .address = 0x000D,
            .name = "Turbine Unloaded Voltage",
            .unit = "V",
            .preprocess = PRE_SCALE(0.1),
            .decimals = 1
        },
        {
            .address = 0x000E,
            .name = "Turbine Unloaded Current",
            .unit = "A",
            .preprocess = PRE_SCALE(0.1),
            .decimals = 1
        },
        {
            .address = 0x001D,
            .name = "Turbine RPM",
            .unit = "RPM",
            .decimals = 0
        },
        {
            .address = 0x002C,
            .name = "Wind Speed",
            .unit = "m/s",
            .preprocess = PRE_SCALE(0.01),
            .decimals = 2
        }
    };
}
