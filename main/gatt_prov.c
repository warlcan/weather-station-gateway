#include "gatt_prov.h"

#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gatt/ble_svc_gatt.h"

#include "credentials.h"

static const char *TAG = "GATT_PROV";

static const ble_uuid128_t gatt_svc_uuid = BLE_UUID128_INIT(
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xF0, 0xFF, 0x00, 0x00);

static const ble_uuid128_t gatt_chr_ssid_uuid = BLE_UUID128_INIT(
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xF1, 0xFF, 0x00, 0x00);

static const ble_uuid128_t gatt_chr_pass_uuid = BLE_UUID128_INIT(
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xF2, 0xFF, 0x00, 0x00);

static const ble_uuid128_t gatt_chr_token_uuid = BLE_UUID128_INIT(
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xF3, 0xFF, 0x00, 0x00);

static int gatt_svr_chr_access_ssid(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
    if (om_len == 0 || om_len > PROV_SSID_MAX_LEN) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint8_t buf[PROV_SSID_MAX_LEN];
    uint16_t copied = 0;
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &copied);
    if (rc != 0) {
        ESP_LOGW(TAG, "mbuf_to_flat failed: %d", rc);
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (credentials_set_ssid(buf, copied) != ESP_OK) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    return 0;
}

static int gatt_svr_chr_access_pass(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
    if (om_len > PROV_PASS_MAX_LEN) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint8_t buf[PROV_PASS_MAX_LEN];
    uint16_t copied = 0;
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &copied);
    if (rc != 0) {
        ESP_LOGW(TAG, "mbuf_to_flat failed: %d", rc);
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (credentials_set_pass(buf, copied) != ESP_OK) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    return 0;
}

static int gatt_svr_chr_access_token(uint16_t conn_handle, uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
    if (om_len != PROV_TOKEN_LEN) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint8_t buf[PROV_TOKEN_LEN];
    uint16_t copied = 0;
    int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &copied);
    if (rc != 0) {
        ESP_LOGW(TAG, "mbuf_to_flat failed: %d", rc);
        return BLE_ATT_ERR_UNLIKELY;
    }

    if (credentials_set_token(buf, copied) != ESP_OK) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    return 0;
}

static const struct ble_gatt_chr_def gatt_svr_chrs[] = {
    {
        .uuid = &gatt_chr_ssid_uuid.u,
        .access_cb = gatt_svr_chr_access_ssid,
        .flags = BLE_GATT_CHR_F_WRITE,
    },
    {
        .uuid = &gatt_chr_pass_uuid.u,
        .access_cb = gatt_svr_chr_access_pass,
        .flags = BLE_GATT_CHR_F_WRITE,
    },
    {
        .uuid = &gatt_chr_token_uuid.u,
        .access_cb = gatt_svr_chr_access_token,
        .flags = BLE_GATT_CHR_F_WRITE,
    },
    { 0 }
};

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svc_uuid.u,
        .characteristics = gatt_svr_chrs,
    },
    { 0 }
};

esp_err_t gatt_prov_init(void)
{
    int rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "GATT provisioning service registered");
    return ESP_OK;
}