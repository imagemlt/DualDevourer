#ifndef LOGGER_H
#define LOGGER_H

#include <spdlog/logger.h>

typedef unsigned short ushort; 
using logger = spdlog::logger;
using Logger_t = std::shared_ptr<spdlog::logger>;

#endif /* LOGGER_H */
