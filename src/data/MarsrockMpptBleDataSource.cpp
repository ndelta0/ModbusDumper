#include "data/MarsrockMpptBleDataSource.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <chrono>
#include <deque>
#include <stdexcept>
#include <string>

#include "data/generateDataRegisters.h"
#include <xw32ble/xw32ble.hpp>

namespace {

using xw32ble::ByteBuffer;

constexpr std::uint8_t kSlaveId = 0x02;
constexpr std::chrono::milliseconds kResponseTimeout{1500};
constexpr std::chrono::seconds kScanDuration{8};
constexpr std::uint16_t kMaxReadCount = 120;

struct EnumMapInfo {
    enum class Kind {
        Unknown,
        Enum,
        Bitfield,
    };

    Kind kind = Kind::Unknown;
    QMap<std::uint16_t, QString> enumLabels;
    QVector<BitmaskFlagDefinition> bitmaskFlags;
};

struct ScaleInfo {
    double factor = 1.0;
    int decimals = 0;
};

struct ParsedMapRow {
    int address = -1;
    QString name;
    QString category;
    int length = 1;
    QString scaleText;
    QString unit;
    QString access;
    QString minText;
    QString maxText;
    QString enumBitfieldHint;
    QString notes;
};

struct RawQueryRange {
    std::uint16_t startAddress = 0;
    std::uint16_t count = 0;
};

bool isPowerOfTwo(const std::uint16_t value) {
    return value != 0 && (value & (value - 1U)) == 0;
}

std::uint16_t crc16Modbus(const std::uint8_t *data, const size_t len) {
    std::uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1U) ? static_cast<std::uint16_t>((crc >> 1U) ^ 0xA001U) : static_cast<std::uint16_t>(crc >> 1U);
        }
    }
    return crc;
}

ByteBuffer buildReadFrame(const std::uint8_t slaveId, const std::uint16_t startAddress, const std::uint16_t count) {
    ByteBuffer frame = {
        slaveId,
        0x03,
        static_cast<std::uint8_t>(startAddress >> 8U),
        static_cast<std::uint8_t>(startAddress & 0xFFU),
        static_cast<std::uint8_t>(count >> 8U),
        static_cast<std::uint8_t>(count & 0xFFU),
    };

    const std::uint16_t crc = crc16Modbus(frame.data(), frame.size());
    frame.push_back(static_cast<std::uint8_t>(crc & 0xFFU));
    frame.push_back(static_cast<std::uint8_t>((crc >> 8U) & 0xFFU));
    return frame;
}

bool parseReadResponse(const ByteBuffer &frame,
                       const std::uint8_t slaveId,
                       const std::uint16_t expectedCount,
                       std::vector<std::uint16_t> &values) {
    const std::size_t expectedBytes = static_cast<std::size_t>(expectedCount) * 2U;
    const std::size_t expectedSize = expectedBytes + 5U;
    if (frame.size() != expectedSize) {
        return false;
    }
    if (frame[0] != slaveId || frame[1] != 0x03U || frame[2] != expectedBytes) {
        return false;
    }

    const std::uint16_t expectedCrc = crc16Modbus(frame.data(), frame.size() - 2U);
    const std::uint16_t responseCrc = static_cast<std::uint16_t>(frame[frame.size() - 1U] << 8U) |
                                      static_cast<std::uint16_t>(frame[frame.size() - 2U]);
    if (expectedCrc != responseCrc) {
        return false;
    }

    values.clear();
    values.reserve(expectedCount);
    for (std::uint16_t i = 0; i < expectedCount; ++i) {
        const std::size_t offset = 3U + (static_cast<std::size_t>(i) * 2U);
        const auto value = static_cast<std::uint16_t>((frame[offset] << 8U) | frame[offset + 1U]);
        values.push_back(value);
    }
    return true;
}

QString sanitizeScale(QString scaleText) {
    scaleText = scaleText.trimmed();
    scaleText.replace(',', '.');
    return scaleText;
}

ScaleInfo parseScale(const QString &scaleText) {
    const QString normalized = sanitizeScale(scaleText);
    if (normalized.isEmpty()) {
        return {};
    }

    bool ok = false;
    const double factor = normalized.toDouble(&ok);
    if (!ok || factor <= 0.0) {
        return {};
    }

    int decimals = 0;
    const int dotIndex = normalized.indexOf('.');
    if (dotIndex >= 0) {
        decimals = normalized.size() - dotIndex - 1;
        while (decimals > 0 && normalized[dotIndex + decimals] == '0') {
            --decimals;
        }
    }

    return {
        .factor = factor,
        .decimals = decimals,
    };
}

QString normalizeUnit(QString unit) {
    unit = unit.trimmed();
    if (unit == "*C") {
        return "C";
    }
    return unit;
}

bool parseDecInt(const QString &text, int &out) {
    bool ok = false;
    const int value = text.trimmed().toInt(&ok, 10);
    if (!ok) {
        return false;
    }
    out = value;
    return true;
}

bool parseHexAddress(const QString &hexText, std::uint16_t &address) {
    QString value = hexText.trimmed();
    if (value.startsWith("0x", Qt::CaseInsensitive)) {
        value = value.mid(2);
    }
    bool ok = false;
    const unsigned parsed = value.toUInt(&ok, 16);
    if (!ok || parsed > 0xFFFFU) {
        return false;
    }
    address = static_cast<std::uint16_t>(parsed);
    return true;
}

QVector<QString> splitCsvLine(const QString &line) {
    QVector<QString> fields;
    QString current;
    bool inQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line[i];
        if (ch == '"') {
            if (inQuotes && i + 1 < line.size() && line[i + 1] == '"') {
                current += '"';
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
            continue;
        }
        if (ch == ',' && !inQuotes) {
            fields.push_back(current);
            current.clear();
            continue;
        }
        current += ch;
    }
    fields.push_back(current);
    return fields;
}

QString readWholeFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QTextStream stream(&file);
    return stream.readAll();
}

QString resolveCsvPath(const QString &fileName) {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString cwd = QDir::currentPath();
    const QStringList candidates = {
        QDir(cwd).filePath(fileName),
        QDir(appDir).filePath(fileName),
        QDir(appDir + "/..").absoluteFilePath(fileName),
        QDir(appDir + "/../..").absoluteFilePath(fileName),
    };

    for (const QString &candidate : candidates) {
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

QMap<std::uint16_t, EnumMapInfo> parseEnumMaps(const QString &csvText) {
    QMap<std::uint16_t, EnumMapInfo> out;
    const QStringList lines = csvText.split('\n', Qt::SkipEmptyParts);
    if (lines.size() <= 1) {
        return out;
    }

    std::optional<std::uint16_t> currentAddress;
    QVector<QPair<std::uint16_t, QString>> pendingEntries;
    EnumMapInfo::Kind pendingKind = EnumMapInfo::Kind::Unknown;

    auto flushCurrent = [&]() {
        if (!currentAddress.has_value()) {
            return;
        }

        EnumMapInfo info;
        info.kind = pendingKind;
        if (info.kind == EnumMapInfo::Kind::Unknown) {
            bool looksBitmask = true;
            for (const auto &[value, _] : pendingEntries) {
                if (value == 0) {
                    continue;
                }
                if (!isPowerOfTwo(value)) {
                    looksBitmask = false;
                    break;
                }
            }
            info.kind = looksBitmask ? EnumMapInfo::Kind::Bitfield : EnumMapInfo::Kind::Enum;
        }

        for (const auto &[value, label] : pendingEntries) {
            if (info.kind == EnumMapInfo::Kind::Bitfield) {
                info.bitmaskFlags.push_back({
                    .mask = value,
                    .label = label,
                });
            } else {
                info.enumLabels.insert(value, label);
            }
        }
        out.insert(*currentAddress, info);

        pendingEntries.clear();
        pendingKind = EnumMapInfo::Kind::Unknown;
    };

    for (int i = 1; i < lines.size(); ++i) {
        const QVector<QString> fields = splitCsvLine(lines[i].trimmed());
        if (fields.isEmpty()) {
            continue;
        }

        const QString col0 = fields.value(0).trimmed();
        const QString col1 = fields.value(1).trimmed();
        const QString col2 = fields.value(2).trimmed();

        std::uint16_t newAddress = 0;
        if (parseHexAddress(col0, newAddress)) {
            flushCurrent();
            currentAddress = newAddress;
        }

        if (!currentAddress.has_value()) {
            continue;
        }

        if (col0.compare("Enum", Qt::CaseInsensitive) == 0) {
            pendingKind = EnumMapInfo::Kind::Enum;
        } else if (col0.compare("Bitflag", Qt::CaseInsensitive) == 0 ||
                   col0.compare("Bitfield", Qt::CaseInsensitive) == 0) {
            pendingKind = EnumMapInfo::Kind::Bitfield;
        }

        int parsedValue = 0;
        if (!parseDecInt(col1, parsedValue)) {
            continue;
        }
        if (parsedValue < 0 || parsedValue > 0xFFFF) {
            continue;
        }
        if (col2.isEmpty()) {
            continue;
        }
        pendingEntries.push_back({static_cast<std::uint16_t>(parsedValue), col2});
    }

    flushCurrent();
    return out;
}

QVector<ParsedMapRow> parseRegisterRows(const QString &csvText) {
    QVector<ParsedMapRow> rows;
    const QStringList lines = csvText.split('\n', Qt::SkipEmptyParts);
    if (lines.size() <= 1) {
        return rows;
    }

    const QVector<QString> header = splitCsvLine(lines[0].trimmed());
    auto headerIndex = [&](const QString &name) {
        for (int i = 0; i < header.size(); ++i) {
            if (header[i].trimmed().compare(name, Qt::CaseInsensitive) == 0) {
                return i;
            }
        }
        return -1;
    };

    const int idxAddressDec = headerIndex("Address (dec)");
    const int idxName = headerIndex("Name");
    const int idxCategory = headerIndex("Category");
    const int idxLength = headerIndex("Length");
    const int idxScale = headerIndex("Scale");
    const int idxUnit = headerIndex("Unit");
    const int idxAccess = headerIndex("Access");
    const int idxMin = headerIndex("Min");
    const int idxMax = headerIndex("Max");
    const int idxEnumBitfields = headerIndex("Enum/Bitfields");
    const int idxNotes = headerIndex("Notes");

    for (int i = 1; i < lines.size(); ++i) {
        const QVector<QString> fields = splitCsvLine(lines[i].trimmed());
        if (fields.isEmpty()) {
            continue;
        }

        ParsedMapRow row;
        if (!parseDecInt(fields.value(idxAddressDec).trimmed(), row.address) || row.address < 0 || row.address > 0xFFFF) {
            continue;
        }
        row.name = fields.value(idxName).trimmed();
        row.category = fields.value(idxCategory).trimmed();
        row.scaleText = fields.value(idxScale).trimmed();
        row.unit = fields.value(idxUnit).trimmed();
        row.access = fields.value(idxAccess).trimmed().toUpper();
        row.minText = fields.value(idxMin).trimmed();
        row.maxText = fields.value(idxMax).trimmed();
        row.enumBitfieldHint = fields.value(idxEnumBitfields).trimmed();
        row.notes = fields.value(idxNotes).trimmed();

        int parsedLength = 1;
        parseDecInt(fields.value(idxLength).trimmed(), parsedLength);
        row.length = std::max(1, parsedLength);

        rows.push_back(row);
    }

    return rows;
}

double parseOptionalDouble(const QString &value, const double fallback) {
    const QString normalized = sanitizeScale(value);
    if (normalized.isEmpty()) {
        return fallback;
    }
    bool ok = false;
    const double parsed = normalized.toDouble(&ok);
    return ok ? parsed : fallback;
}

std::optional<std::uint16_t> parseInvalidMarker(const QString &notes) {
    static const QRegularExpression abnormalRegex(R"((\d+)\s*=\s*abnormal)");
    const QRegularExpressionMatch match = abnormalRegex.match(notes);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    bool ok = false;
    const unsigned value = match.captured(1).toUInt(&ok, 10);
    if (!ok || value > 0xFFFFU) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(value);
}

std::optional<double> parseOffset(const QString &notes) {
    static const QRegularExpression offsetRegex(R"(Offset of\s*([+-]?\d+(?:[.,]\d+)?)\s*C)", QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = offsetRegex.match(notes);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    bool ok = false;
    QString value = match.captured(1);
    value.replace(',', '.');
    const double offset = value.toDouble(&ok);
    if (!ok) {
        return std::nullopt;
    }
    return offset;
}

QString buildDescription(const ParsedMapRow &row) {
    QStringList parts;
    if (!row.category.isEmpty()) {
        parts.push_back(row.category);
    }
    if (!row.access.isEmpty()) {
        parts.push_back(row.access);
    }
    if (!row.notes.isEmpty()) {
        parts.push_back(row.notes);
    }
    return parts.join(" | ");
}

RegisterDefinition buildDefinitionForWord(const ParsedMapRow &row,
                                          const EnumMapInfo &enumMap,
                                          const int wordIndex) {
    const bool splitWords = row.length > 1;
    const std::uint16_t address = static_cast<std::uint16_t>(row.address + wordIndex);
    QString displayName = row.name;
    if (splitWords) {
        displayName += QStringLiteral(" [%1]").arg(wordIndex + 1);
    }

    RegisterDefinition definition{
        .address = address,
        .name = displayName,
        .description = buildDescription(row),
        .unit = normalizeUnit(row.unit),
        .logicalType = RegisterLogicalType::Numeric,
    };

    if (enumMap.kind == EnumMapInfo::Kind::Enum) {
        definition.logicalType = RegisterLogicalType::Enum;
        definition.enumLabels = enumMap.enumLabels;
    } else if (enumMap.kind == EnumMapInfo::Kind::Bitfield) {
        definition.logicalType = RegisterLogicalType::Bitmask;
        definition.bitmaskFlags = enumMap.bitmaskFlags;
    }

    int minInt = 0;
    int maxInt = 0;
    const bool hasMinInt = parseDecInt(row.minText, minInt);
    const bool hasMaxInt = parseDecInt(row.maxText, maxInt);
    if (definition.logicalType == RegisterLogicalType::Numeric &&
        hasMinInt && hasMaxInt && minInt == 0 && maxInt == 1) {
        definition.logicalType = RegisterLogicalType::Boolean;
        definition.falseText = "Off";
        definition.trueText = "On";
    }

    const ScaleInfo scale = parseScale(row.scaleText);
    definition.decimals = scale.decimals;
    const std::optional<std::uint16_t> invalidMarker = parseInvalidMarker(row.notes);
    const std::optional<double> offset = parseOffset(row.notes);
    const double offsetValue = offset.value_or(0.0);

    if (definition.logicalType == RegisterLogicalType::Numeric &&
        (scale.factor != 1.0 || offsetValue != 0.0 || invalidMarker.has_value())) {
        definition.preprocess = [scale, offsetValue, invalidMarker](const std::uint16_t raw) -> std::optional<double> {
            if (invalidMarker.has_value() && raw == invalidMarker.value()) {
                return std::nullopt;
            }
            return static_cast<double>(raw) * scale.factor + offsetValue;
        };
    }

    return definition;
}

std::vector<RawQueryRange> buildQueryRanges(const std::vector<std::uint16_t> &addresses) {
    std::vector<RawQueryRange> ranges;
    if (addresses.empty()) {
        return ranges;
    }

    RawQueryRange current{
        .startAddress = addresses.front(),
        .count = 1,
    };

    for (std::size_t i = 1; i < addresses.size(); ++i) {
        const std::uint16_t address = addresses[i];
        const std::uint16_t expectedNext = static_cast<std::uint16_t>(current.startAddress + current.count);
        if (address == expectedNext && current.count < kMaxReadCount) {
            ++current.count;
            continue;
        }
        ranges.push_back(current);
        current = RawQueryRange{
            .startAddress = address,
            .count = 1,
        };
    }
    ranges.push_back(current);
    return ranges;
}

} // namespace

MarsrockMpptBleDataSource::MarsrockMpptBleDataSource(QObject *parent)
    : RegisterDataSource(parent) {
    m_registers = GetRegisters();
    m_layout = buildLayoutFromDefinitions(m_registers);
}

MarsrockMpptBleDataSource::~MarsrockMpptBleDataSource() {
    stop();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

const QVector<RegisterDefinition> &MarsrockMpptBleDataSource::getRegisters() {
    return m_registers;
}

void MarsrockMpptBleDataSource::start() {
    if (m_running.exchange(true)) {
        return;
    }
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    m_workerThread = std::thread([this] {
        workerLoop();
    });
}

void MarsrockMpptBleDataSource::stop() {
    const bool wasRunning = m_running.exchange(false);

    if (wasRunning) {
        m_sleepCv.notify_all();
    }
}

void MarsrockMpptBleDataSource::workerLoop() {
    PollLayout layoutCopy;
    {
        std::lock_guard lock(m_layoutMutex);
        layoutCopy = m_layout;
    }
    if (layoutCopy.registerByAddress.empty() || layoutCopy.queries.empty()) {
        publishError("No registers configured. Could not build poll layout.");
        m_running.store(false);
        publishDisconnected();
        return;
    }

    xw32ble::XW32BleDevice device;
    std::mutex rxMutex;
    std::condition_variable rxCv;
    std::deque<ByteBuffer> rxQueue;

    device.on_data([&](ByteBuffer data) {
        std::lock_guard lock(rxMutex);
        rxQueue.push_back(std::move(data));
        rxCv.notify_all();
    });

    try {
        publishStatus("Scanning...");
        auto scanned = device.scan_for_xw32ble(kScanDuration);
        if (scanned.empty()) {
            throw std::runtime_error("No XW32-BLE devices found during scan");
        }
        std::ranges::sort(scanned, [](const auto &a, const auto &b) {
            return a.rssi > b.rssi;
        });
        publishStatus("Connecting...");
        device.connect(scanned.front().address);

        auto atEnter = device.enter_at_mode(std::chrono::seconds(3));
        if (!atEnter.ok) {
            throw std::runtime_error("Failed to enter AT mode");
        }
        auto atExit = device.exit_at_mode(std::chrono::seconds(3));
        if (!atExit.ok) {
            throw std::runtime_error("Failed to exit AT mode");
        }
    } catch (const std::exception &ex) {
        publishError(QStringLiteral("Connection setup failed: %1").arg(ex.what()));
        try {
            device.disconnect();
        } catch (...) {
        }
        m_running.store(false);
        publishDisconnected();
        return;
    }

    publishConnected();

    while (m_running.load()) {
        const auto cycleStart = std::chrono::steady_clock::now();
        QMap<std::uint16_t, RegisterValueState> cycleValues;

        {
            std::lock_guard lock(m_layoutMutex);
            layoutCopy = m_layout;
        }

        for (const QueryRange &query : layoutCopy.queries) {
            if (!m_running.load()) {
                break;
            }

            {
                std::lock_guard lock(rxMutex);
                rxQueue.clear();
            }

            try {
                const ByteBuffer frame = buildReadFrame(kSlaveId, query.startAddress, query.count);
                device.send(frame);
            } catch (const std::exception &ex) {
                publishError(QStringLiteral("Failed to send Modbus request at 0x%1: %2")
                                 .arg(query.startAddress, 4, 16, QChar('0'))
                                 .arg(ex.what()));
                m_running.store(false);
                break;
            }

            std::vector<std::uint16_t> values;
            bool received = false;
            const auto deadline = std::chrono::steady_clock::now() + kResponseTimeout;
            while (m_running.load() && std::chrono::steady_clock::now() < deadline) {
                ByteBuffer frame;
                {
                    std::unique_lock lock(rxMutex);
                    if (rxQueue.empty()) {
                        rxCv.wait_for(lock, std::chrono::milliseconds(50));
                    }
                    if (rxQueue.empty()) {
                        continue;
                    }
                    frame = std::move(rxQueue.front());
                    rxQueue.pop_front();
                }

                if (parseReadResponse(frame, kSlaveId, query.count, values)) {
                    received = true;
                    break;
                }
            }

            for (std::uint16_t i = 0; i < query.count; ++i) {
                const std::uint16_t address = static_cast<std::uint16_t>(query.startAddress + i);
                if (!layoutCopy.registerByAddress.contains(address)) {
                    continue;
                }

                if (received && i < values.size()) {
                    cycleValues.insert(address, {
                        .hasValue = true,
                        .isValid = true,
                        .rawValue = values[i],
                    });
                } else {
                    cycleValues.insert(address, {
                        .hasValue = true,
                        .isValid = false,
                        .rawValue = 0,
                    });
                }
            }
        }

        const auto cycleEnd = std::chrono::steady_clock::now();
        const auto cycleDurationMs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(cycleEnd - cycleStart).count());
        publishRefreshCycleCompleted(cycleValues);

        const int waitMs = std::max(0, 500 - static_cast<int>(cycleDurationMs));
        if (waitMs > 0 && m_running.load()) {
            std::unique_lock lock(m_sleepMutex);
            m_sleepCv.wait_for(lock, std::chrono::milliseconds(waitMs), [&] {
                return !m_running.load();
            });
        }
    }

    try {
        device.disconnect();
    } catch (...) {
    }
    m_running.store(false);
    publishDisconnected();
}

void MarsrockMpptBleDataSource::publishError(const QString &message) {
    QMetaObject::invokeMethod(this, [this, message] {
        emit errorOccurred(message);
    }, Qt::QueuedConnection);
}

void MarsrockMpptBleDataSource::publishStatus(const QString &message) {
    QMetaObject::invokeMethod(this, [this, message] {
        emit statusChanged(message);
    }, Qt::QueuedConnection);
}

void MarsrockMpptBleDataSource::publishConnected() {
    QMetaObject::invokeMethod(this, [this] {
        emit connected();
    }, Qt::QueuedConnection);
}

void MarsrockMpptBleDataSource::publishDisconnected() {
    QMetaObject::invokeMethod(this, [this] {
        emit disconnected();
    }, Qt::QueuedConnection);
}

void MarsrockMpptBleDataSource::publishRefreshCycleCompleted(const QMap<std::uint16_t, RegisterValueState> &valuesByAddress) {
    QMetaObject::invokeMethod(this, [this, valuesByAddress] {
        QMap<const RegisterDefinition *, RegisterValueState> values;
        for (const RegisterDefinition &reg : m_registers) {
            const auto it = valuesByAddress.constFind(reg.address);
            if (it != valuesByAddress.constEnd()) {
                values.insert(&reg, *it);
            }
        }
        emit refreshCycleCompleted(values);
    }, Qt::QueuedConnection);
}

MarsrockMpptBleDataSource::PollLayout MarsrockMpptBleDataSource::buildLayoutFromCsv() const {
    PollLayout layout;
    const QString mapPath = resolveCsvPath("MPPT Modbus - Register map.csv");
    const QString enumsPath = resolveCsvPath("MPPT Modbus - Enums_Bitfields.csv");
    if (mapPath.isEmpty() || enumsPath.isEmpty()) {
        return layout;
    }

    const QString mapText = readWholeFile(mapPath);
    const QString enumsText = readWholeFile(enumsPath);
    if (mapText.isEmpty() || enumsText.isEmpty()) {
        return layout;
    }

    const QMap<std::uint16_t, EnumMapInfo> enumMaps = parseEnumMaps(enumsText);
    const QVector<ParsedMapRow> rows = parseRegisterRows(mapText);
    std::vector<std::uint16_t> allAddresses;

    for (const ParsedMapRow &row : rows) {
        if (row.access != "RO" && row.access != "RW") {
            continue;
        }
        if (row.length != 1 && row.length != 2) {
            continue;
        }

        const std::uint16_t baseAddress = static_cast<std::uint16_t>(row.address);
        EnumMapInfo enumInfo = enumMaps.value(baseAddress);
        if (enumInfo.kind == EnumMapInfo::Kind::Unknown) {
            const QString hint = row.enumBitfieldHint.toLower();
            if (hint.contains("enum")) {
                enumInfo.kind = EnumMapInfo::Kind::Enum;
            } else if (hint.contains("bitfield") || hint.contains("bitflag")) {
                enumInfo.kind = EnumMapInfo::Kind::Bitfield;
            }
        }

        for (int wordIndex = 0; wordIndex < row.length; ++wordIndex) {
            const RegisterDefinition definition = buildDefinitionForWord(row, enumInfo, wordIndex);
            layout.registerByAddress[definition.address] = definition;
            allAddresses.push_back(definition.address);
        }
    }

    std::ranges::sort(allAddresses);
    allAddresses.erase(std::unique(allAddresses.begin(), allAddresses.end()), allAddresses.end());
    const std::vector<RawQueryRange> ranges = buildQueryRanges(allAddresses);
    layout.queries.reserve(ranges.size());
    for (const RawQueryRange &range : ranges) {
        layout.queries.push_back({
            .startAddress = range.startAddress,
            .count = range.count,
        });
    }

    return layout;
}

MarsrockMpptBleDataSource::PollLayout MarsrockMpptBleDataSource::buildLayoutFromDefinitions(
    const QVector<RegisterDefinition> &definitions) {
    PollLayout layout;
    std::vector<std::uint16_t> addresses;
    addresses.reserve(definitions.size());

    for (const RegisterDefinition &definition : definitions) {
        layout.registerByAddress[definition.address] = definition;
        addresses.push_back(definition.address);
    }

    std::ranges::sort(addresses);
    addresses.erase(std::unique(addresses.begin(), addresses.end()), addresses.end());
    const std::vector<RawQueryRange> ranges = buildQueryRanges(addresses);
    layout.queries.reserve(ranges.size());
    for (const RawQueryRange &range : ranges) {
        layout.queries.push_back({
            .startAddress = range.startAddress,
            .count = range.count,
        });
    }

    return layout;
}
