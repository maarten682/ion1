#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "bat.h"

static const char *TAG = "bat";

// ADC Channel
#if CONFIG_IDF_TARGET_ESP32
#define ADC1_CHAN ADC_CHANNEL_7
#else
#define ADC1_CHAN ADC_CHANNEL_2
#endif

// ADC Attenuation
// 11DB = 3.55 voltage gain, reference voltage should be around 1100mv,
// so max theoretical measurement would be 3905mv, actual/recommended(?) is a lot lower.
#define ADC_ATTEN ADC_ATTEN_DB_11

static bool cali_enable = false;
static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;

static void adc_calibration_init(adc_unit_t unit, adc_atten_t atten) {
    esp_err_t ret = ESP_FAIL;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!cali_enable) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {};
        cali_config.unit_id = unit;
        cali_config.atten = atten;
        cali_config.bitwidth = ADC_BITWIDTH_DEFAULT;
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle);
        if (ret == ESP_OK) {
            cali_enable = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!cali_enable) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {};
        cali_config.unit_id = unit;
        cali_config.atten = atten;
        cali_config.bitwidth = ADC_BITWIDTH_DEFAULT;
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &adc1_cali_handle);
        if (ret == ESP_OK) {
            cali_enable = true;
        }
    }
#endif

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !cali_enable) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }
}

static void adc_calibration_deinit() {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(adc1_cali_handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(adc1_cali_handle));
#endif
}

void adc_init() {

    adc_oneshot_unit_init_cfg_t init_config = {};
    init_config.unit_id = ADC_UNIT_1;
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {};
    config.atten = ADC_ATTEN;
    config.bitwidth = ADC_BITWIDTH_DEFAULT;

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC1_CHAN, &config));

    adc_calibration_init(ADC_UNIT_1, ADC_ATTEN);
}

uint8_t adc_measure() {
    int adc_raw = 0;
    int voltage = 0;

    uint32_t realvoltage = 0;
    uint8_t batterypercentage = 1;

    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC1_CHAN, &adc_raw));
    if (cali_enable) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage));

        uint32_t emptyMv = CONFIG_ION_ADC_EMPTY_MV;
        uint32_t fullMv = CONFIG_ION_ADC_FULL_MV;

        int32_t percentage = ((voltage - emptyMv) * 100) / (fullMv - emptyMv);

        if (percentage < 0) {
            batterypercentage = 0;
        } else if (percentage > 100) {
            batterypercentage = 100;
        } else {
            batterypercentage = (uint8_t)percentage;
        }

        ESP_LOGD(TAG, "Raw: %d, Measured: %d mV, Percentage: %d%%", adc_raw, voltage, batterypercentage);

        return batterypercentage;
    }

    return 0x00;
}

void adc_teardown() {

    // Tear Down
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    if (cali_enable) {
        adc_calibration_deinit();
    }
}