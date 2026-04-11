#ifndef PEAR_NET_DB_CONTRACT_HPP_
#define PEAR_NET_DB_CONTRACT_HPP_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <stdexcept>

namespace pear::net {

// Структура, описывающая обновление файла (используется в WAL и для ответов)
struct FileUpdateInfo {
    std::string file_id;      // уникальный идентификатор файла (например, имя + версия)
    std::string name;         // имя файла
    uint64_t version;         // версия файла (монотонно возрастает)
    uint64_t owner_device_id; // ID устройства-владельца (ВУ)
};

// Структура, описывающая обновление устройства (регистрация или смена адреса)
struct DeviceUpdateInfo {
    uint64_t device_id;       // уникальный ID устройства
    std::string address;      // сетевой адрес (ip:port)
};

// Запись в WAL (Write-Ahead Log)
struct WalEntryInfo {
    uint64_t seq_id;          // глобальный порядковый номер (назначается ГУ)
    uint64_t timestamp;       // время создания записи (unix timestamp)
    int op_type;              // 0 = FILE_UPDATE, 1 = DEVICE_UPDATE
    FileUpdateInfo file;
    DeviceUpdateInfo device;
};

// Контракт для базы данных.
class DatabaseFacade {
public:
    virtual ~DatabaseFacade() = default;

    /// Возвращает все WAL-записи с seq_id > last_seq_id, отсортированные по seq_id.
    /// Используется при обновлении БД (pull).
    virtual std::vector<WalEntryInfo> getWalEntriesSince(uint64_t last_seq_id) {
        throw std::logic_error("DatabaseFacade::getWalEntriesSince not implemented");
    }

    /// Применяет список WAL-записей к локальной БД:
    /// - вставляет записи в таблицу wal,
    /// - обновляет таблицы files и devices в соответствии с op_type.
    /// Используется при синхронизации после получения записей от ГУ.
    virtual void applyWalEntries(const std::vector<WalEntryInfo>& entries) {
        throw std::logic_error("DatabaseFacade::applyWalEntries not implemented");
    }

    /// Возвращает информацию о файле по его ID и версии.
    /// Если version == 0, возвращает последнюю известную версию.
    /// Если файл не найден, возвращает std::nullopt.
    virtual std::optional<FileUpdateInfo> getFileInfo(const std::string& file_id, uint64_t version) {
        throw std::logic_error("DatabaseFacade::getFileInfo not implemented");
    }

    /// Добавляет одну WAL-запись в БД (на стороне ГУ).
    /// Назначает новый seq_id (последний + 1) и возвращает его.
    /// Используется при обработке PushWAL от клиента.
    virtual uint64_t addWalEntry(const WalEntryInfo& entry) {
        throw std::logic_error("DatabaseFacade::addWalEntry not implemented");
    }

    /// Возвращает максимальный seq_id из таблицы wal (0, если таблица пуста).
    /// Используется при формировании запроса UpdateDB.
    virtual uint64_t getLastSeqId() {
        throw std::logic_error("DatabaseFacade::getLastSeqId not implemented");
    }

    /// Возвращает список всех файлов (последние версии) для команды `pear ls`.
    virtual std::vector<FileUpdateInfo> getAllFiles() {
        throw std::logic_error("DatabaseFacade::getAllFiles not implemented");
    }

    /// Очищает staging-список (если staging хранится в БД).
    /// Если staging не используется, метод может быть пустым.
    virtual void clearStaging() {
        throw std::logic_error("DatabaseFacade::clearStaging not implemented");
    }

    // ----- Работа с устройствами -----

    /// Регистрирует новое устройство в таблице devices.
    /// address должен быть уникальным. Возвращает автоматически назначенный device_id.
    /// Используется только на стороне ГУ при обработке RegisterDevice.
    virtual uint64_t registerDevice(const std::string& address) {
        throw std::logic_error("DatabaseFacade::registerDevice not implemented");
    }

    /// Возвращает сетевой адрес устройства по его device_id.
    /// Если device_id не найден, может вернуть пустую строку или кинуть исключение.
    virtual std::string getDeviceAddress(uint64_t device_id) {
        throw std::logic_error("DatabaseFacade::getDeviceAddress not implemented");
    }

    // ----- Конфигурация локального узла (хранится в БД) -----

    /// Сохраняет адрес главного узла (ГУ), к которому подключено данное устройство. (В МВП просто первый узел)
    virtual void setMasterAddress(const std::string& address) {
        throw std::logic_error("DatabaseFacade::setMasterAddress not implemented");
    }

    /// Возвращает адрес главного узла (ГУ), к которому подключено данное устройство. (Опять же, в МВП это адрес первого узла)
    /// Если не подключены, возвращает пустую строку.
    virtual std::string getMasterAddress() {
        throw std::logic_error("DatabaseFacade::getMasterAddress not implemented");
    }

    /// Сохраняет идентификатор данного устройства (присвоенный при регистрации).
    virtual void setDeviceId(uint64_t id) {
        throw std::logic_error("DatabaseFacade::setDeviceId not implemented");
    }

    /// Возвращает идентификатор данного устройства (0, если не зарегистрировано).
    virtual uint64_t getDeviceId() {
        throw std::logic_error("DatabaseFacade::getDeviceId not implemented");
    }
};

} // namespace pear::net

#endif // PEAR_NET_DB_CONTRACT_HPP_