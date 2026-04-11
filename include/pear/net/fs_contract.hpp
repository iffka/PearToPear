#ifndef PEAR_NET_FS_CONTRACT_HPP_
#define PEAR_NET_FS_CONTRACT_HPP_

#include <cstdint>
#include <filesystem>
#include <string>
#include <stdexcept>

namespace pear::net {

// Контракт для файловой системы
// В данной заглушке методы кидают исключение "not implemented".
class FilesystemFacade {
public:
    virtual ~FilesystemFacade() = default;

    /// Возвращает путь к объекту файла в директории .peer/obj.
    /// Обычно это .peer/obj/<file_id> (или с учётом версии, если поддерживается).
    /// Используется на стороне ВУ для отправки файла.
    virtual std::filesystem::path getObjectPath(const std::string& file_id, uint64_t version) {
        throw std::logic_error("FilesystemFacade::getObjectPath not implemented");
    }

    /// Увеличивает счётчик активных скачиваний для указанной версии файла.
    /// Необходимо для того, чтобы не удалять файл, пока его кто-то качает. Мб это работа для БД или вовсе моя (Сообщите мне).
    /// Для MVP может быть заглушкой (ничего не делать).
    virtual void incrementDownloadCounter(const std::string& file_id, uint64_t version) {
        throw std::logic_error("FilesystemFacade::incrementDownloadCounter not implemented");
    }

    /// Уменьшает счётчик активных скачиваний. Аналогично предыдущему сообщите мне, если не работа ФС
    virtual void decrementDownloadCounter(const std::string& file_id, uint64_t version) {
        throw std::logic_error("FilesystemFacade::decrementDownloadCounter not implemented");
    }

    /// Пытается удалить старую версию файла, если счётчик активных скачиваний равен 0.
    /// Вызывается после завершения скачивания.
    virtual void tryDeleteOldVersion(const std::string& file_id, uint64_t version) {
        throw std::logic_error("FilesystemFacade::tryDeleteOldVersion not implemented");
    }

    /// Сохраняет файл из source_file в .peer/obj/ под именем <file_id> (с учётом версии).
    /// Используется при push: после создания нового объекта его нужно положить в obj.
    /// Возвращает путь к сохранённому объекту.
    virtual std::filesystem::path storeObject(const std::string& file_id, uint64_t version, const std::filesystem::path& source_file) {
        throw std::logic_error("FilesystemFacade::storeObject not implemented");
    }
};

} // namespace pear::net

#endif // PEAR_NET_FS_CONTRACT_HPP_