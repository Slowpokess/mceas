#ifndef MCEAS_MODULE_H
#define MCEAS_MODULE_H

#include "common.h"
#include "types.h"
#include <string>

namespace mceas {

// Интерфейс модуля
class Module {
public:
    virtual ~Module() = default;
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual ModuleResult execute() = 0;
    virtual bool handleCommand(const Command& command) = 0;
    virtual std::string getName() const = 0;
    virtual ModuleType getType() const = 0;
};

} // namespace mceas

#endif // MCEAS_MODULE_H