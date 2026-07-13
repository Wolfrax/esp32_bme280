#ifndef MAX17048_H
#define MAX17048_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t max17048_init(void);
esp_err_t max17048_read(float *volts, float *soc_pct);

#ifdef __cplusplus
}
#endif

#endif // MAX17048_H
