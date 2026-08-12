#ifndef __ESP_IDF_LIB_HELPERS_H__
#define __ESP_IDF_LIB_HELPERS_H__

#include "esp_idf_version.h"

// Автоматическое определение поддержки фич в зависимости от версии IDF
#define HELPER_TARGET_IS_ESP32 1

#ifndef I2C_DEV_TAKE_MUTEX
#define I2C_DEV_TAKE_MUTEX(dev)
#endif

#ifndef I2C_DEV_GIVE_MUTEX
#define I2C_DEV_GIVE_MUTEX(dev)
#endif

#ifndef I2C_DEV_CHECK
#define I2C_DEV_CHECK(dev, X) X
#endif

#endif /* __ESP_IDF_LIB_HELPERS_H__ */