#include "ble.h"
#include "ble_sensor_service.h"
#include "ble_srv_common.h"

#include "nrf_log.h"


#define BLE_UUID_SENSOR_SERVICE 0x2234  
#define BLE_UUID_SENSOR_SERVICE_CHARACTERISTIC_1 0x2235               
#define BLE_UUID_SENSOR_SERVICE_CHARACTERISTIC_2 0x2236      


#define SENSOR_SERVICE_BASE_UUID    {{0x41, 0xee, 0x68, 0x3a, 0x99, 0x0f, 0x0e, 0x72, 0x85, 0x49, 0x8d, 0xb3, 0x00, 0x00, 0x00, 0x00}}

char user_desc_1[] = "Start communication with 0x01.";
char user_desc_2[] = "Get data from notify characteristic.";

static void on_connect(ble_sensor_service_t * p_sensor_service, ble_evt_t const * p_ble_evt)
{
    ret_code_t                          err_code;
    ble_sensor_service_evt_t            evt;
    ble_gatts_value_t                   gatts_val;
    uint8_t                             cccd_value[2];
    ble_sensor_service_client_context_t * p_client = NULL;

    err_code = blcm_link_ctx_get(p_sensor_service->p_link_ctx_storage,
                                 p_ble_evt->evt.gap_evt.conn_handle,
                                 (void *) &p_client);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("Link context for 0x%02X connection handle could not be fetched.",
                      p_ble_evt->evt.gap_evt.conn_handle);
    }

    /* Check the hosts CCCD value to inform of readiness to send data using the NOTIFY characteristic(sensor_service_handles_1) */
    memset(&gatts_val, 0, sizeof(ble_gatts_value_t));
    gatts_val.p_value = cccd_value;
    gatts_val.len     = sizeof(cccd_value);
    gatts_val.offset  = 0;

    err_code = sd_ble_gatts_value_get(p_ble_evt->evt.gap_evt.conn_handle,
                                      p_sensor_service->sensor_service_handles_1.cccd_handle,
                                      &gatts_val);

    if ((err_code == NRF_SUCCESS)     &&
        (p_sensor_service->data_handler != NULL) &&
        ble_srv_is_notification_enabled(gatts_val.p_value))
    {
        
        if (p_client != NULL)
        {
            p_client->is_notification_enabled = true;
        }

        memset(&evt, 0, sizeof(ble_sensor_service_evt_t));
        evt.type                 = BLE_SENSOR_SERVICE_EVT_COMM_STARTED;
        evt.p_sensor_service     = p_sensor_service;
        evt.conn_handle          = p_ble_evt->evt.gap_evt.conn_handle;
        evt.p_link_ctx           = p_client;

        p_sensor_service->data_handler(&evt);
    }
}


/**@brief Function for handling the @ref BLE_GATTS_EVT_WRITE event from the SoftDevice.
 *
 * @param[in] p_sensor_service     Nordic UART Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_write(ble_sensor_service_t * p_sensor_service, ble_evt_t const * p_ble_evt)
{
    ret_code_t                             err_code;
    ble_sensor_service_evt_t                 evt;
    ble_sensor_service_client_context_t    * p_client;
    ble_gatts_evt_write_t const * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    err_code = blcm_link_ctx_get(p_sensor_service->p_link_ctx_storage,
                                 p_ble_evt->evt.gatts_evt.conn_handle,
                                 (void *) &p_client);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("Link context for 0x%02X connection handle could not be fetched.",
                      p_ble_evt->evt.gatts_evt.conn_handle);
    }

    memset(&evt, 0, sizeof(ble_sensor_service_evt_t));
    evt.p_sensor_service      = p_sensor_service;
    evt.conn_handle         = p_ble_evt->evt.gatts_evt.conn_handle;
    evt.p_link_ctx          = p_client;

    if ((p_evt_write->handle == p_sensor_service->sensor_service_handles_1.value_handle) &&
             (p_sensor_service->data_handler != NULL))
    {
        evt.type                  = BLE_SENSOR_SERVICE_EVT_DATA_RECEIVED_CHAR1;
        evt.params.received_data.p_data = p_evt_write->data;
        evt.params.received_data.length = p_evt_write->len;

        p_sensor_service->data_handler(&evt);
    }
    else if ((p_evt_write->handle == p_sensor_service->sensor_service_handles_2.cccd_handle) &&
        (p_evt_write->len == 2))
    {
        if (p_client != NULL)
        {
            if (ble_srv_is_notification_enabled(p_evt_write->data))
            {
                NRF_LOG_DEBUG("Notification Enabled");
                p_client->is_notification_enabled = true;
                evt.type                          = BLE_SENSOR_SERVICE_EVT_COMM_STARTED;
            }
            else
            {
                NRF_LOG_DEBUG("Notification Disabled");
                p_client->is_notification_enabled = false;
                evt.type                          = BLE_SENSOR_SERVICE_EVT_COMM_STOPPED;
            }

            if (p_sensor_service->data_handler != NULL)
            {
                p_sensor_service->data_handler(&evt);
            }

        }
    }
    else
    {
        // Do Nothing. This event is not relevant for this service.
    }
}


/**@brief Function for handling the @ref BLE_GATTS_EVT_HVN_TX_COMPLETE event from the SoftDevice.
 *
 * @param[in] p_sensor_service     Nordic UART Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_hvx_send_data_complete(ble_sensor_service_t * p_sensor_service, ble_evt_t const * p_ble_evt)
{
    ret_code_t                 err_code;
    ble_sensor_service_evt_t              evt;
    ble_sensor_service_client_context_t * p_client;

    err_code = blcm_link_ctx_get(p_sensor_service->p_link_ctx_storage,
                                 p_ble_evt->evt.gatts_evt.conn_handle,
                                 (void *) &p_client);
    if (err_code != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("Link context for 0x%02X connection handle could not be fetched.",
                      p_ble_evt->evt.gatts_evt.conn_handle);
        return;
    }

    if (p_client->is_notification_enabled)
    {
        memset(&evt, 0, sizeof(ble_sensor_service_evt_t));
        evt.type                 = BLE_SENSOR_SERVICE_EVT_TRANSMIT_RDY;
        evt.p_sensor_service     = p_sensor_service;
        evt.conn_handle          = p_ble_evt->evt.gatts_evt.conn_handle;
        evt.p_link_ctx           = p_client;

        p_sensor_service->data_handler(&evt);
    }
}


void ble_sensor_service_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
{
    if ((p_context == NULL) || (p_ble_evt == NULL))
    {
        return;
    }

    ble_sensor_service_t * p_sensor_service = (ble_sensor_service_t *)p_context;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_DEBUG("sensor_service -> on_connect");
            on_connect(p_sensor_service, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE:
            NRF_LOG_DEBUG("sensor_service -> on_write");
            on_write(p_sensor_service, p_ble_evt);
            break;

        case BLE_GATTS_EVT_HVN_TX_COMPLETE:
            NRF_LOG_DEBUG("sensor_service -> on_send_data");
            on_hvx_send_data_complete(p_sensor_service, p_ble_evt);
            break;

        default:
            // No implementation needed.
            break;
    }
}


uint32_t ble_sensor_service_init(ble_sensor_service_t * p_sensor_service, ble_sensor_service_init_t const * p_sensor_service_init)
{
    ret_code_t                err_code;
    ble_uuid_t                ble_uuid;
    ble_uuid128_t             sensor_service_base_uuid = SENSOR_SERVICE_BASE_UUID;
    ble_add_char_params_t     add_char_params;
    ble_add_char_user_desc_t  user_descr;

    VERIFY_PARAM_NOT_NULL(p_sensor_service);
    VERIFY_PARAM_NOT_NULL(p_sensor_service_init);

    // Initialize the service structure.
    p_sensor_service->data_handler = p_sensor_service_init->data_handler;

    /**@snippet [Adding proprietary Service to the SoftDevice] */
    // Add a custom base UUID.
    err_code = sd_ble_uuid_vs_add(&sensor_service_base_uuid, &p_sensor_service->uuid_type);
    VERIFY_SUCCESS(err_code);

    ble_uuid.type = p_sensor_service->uuid_type;
    ble_uuid.uuid = BLE_UUID_SENSOR_SERVICE;

    // Add the service.
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &p_sensor_service->service_handle);
    /**@snippet [Adding proprietary Service to the SoftDevice] */
    VERIFY_SUCCESS(err_code);
    
    /**/
    // Add the READ/WRITE Characteristic 1.
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid                     = BLE_UUID_SENSOR_SERVICE_CHARACTERISTIC_1;
    add_char_params.uuid_type                = p_sensor_service->uuid_type;
    add_char_params.max_len                  = 1;
    add_char_params.p_init_value             = p_sensor_service->init_value_1;
    add_char_params.init_len                 = sizeof(uint8_t);
    add_char_params.is_var_len               = true;
    add_char_params.char_props.write         = 1;
    add_char_params.char_props.write_wo_resp = 0;
    add_char_params.char_props.read          = 1;
    add_char_params.char_props.notify        = 0;

    memset(&user_descr, 0, sizeof(user_descr));
    user_descr.p_char_user_desc = (uint8_t *) user_desc_1;
    user_descr.size = strlen(user_desc_1);
    user_descr.max_size = strlen(user_desc_1);
    user_descr.read_access  = SEC_OPEN;
    add_char_params.p_user_descr = &user_descr;
  
    add_char_params.read_access  = SEC_OPEN;
    add_char_params.write_access = SEC_OPEN;
    add_char_params.cccd_write_access = SEC_OPEN;

    err_code = characteristic_add(p_sensor_service->service_handle, &add_char_params, &p_sensor_service->sensor_service_handles_1);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Add the NOTIFY Characteristic 2.
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid                     = BLE_UUID_SENSOR_SERVICE_CHARACTERISTIC_2;
    add_char_params.uuid_type                = p_sensor_service->uuid_type;
    add_char_params.max_len                  = BLE_SENSOR_SERVICE_MAX_DATA_LEN;
    add_char_params.p_init_value             = p_sensor_service->init_value_2;
    add_char_params.init_len                 = sizeof(uint8_t);
    add_char_params.is_var_len               = true;
    add_char_params.char_props.write         = 0;
    add_char_params.char_props.write_wo_resp = 0;
    add_char_params.char_props.read          = 0;
    add_char_params.char_props.notify        = 1;

    memset(&user_descr, 0, sizeof(user_descr));
    user_descr.p_char_user_desc = (uint8_t *) user_desc_2;
    user_descr.size = strlen(user_desc_2);
    user_descr.max_size = strlen(user_desc_2);
    user_descr.read_access  = SEC_OPEN;
    add_char_params.p_user_descr = &user_descr;
  
    add_char_params.read_access  = SEC_OPEN;
    add_char_params.write_access = SEC_OPEN;
    add_char_params.cccd_write_access = SEC_OPEN;

    err_code = characteristic_add(p_sensor_service->service_handle, &add_char_params, &p_sensor_service->sensor_service_handles_2);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    return err_code;    
    /**@snippet [Adding proprietary characteristic to the SoftDevice] */
}


ret_code_t ble_sensor_service_update_char1(ble_sensor_service_t *p_sensor_service,
                                           uint8_t  *p_data,
                                           uint16_t  p_length)
{
    if (p_sensor_service == NULL)
    {
        return NRF_ERROR_NULL;
    }

    ret_code_t         err_code = NRF_SUCCESS;
    ble_gatts_value_t  gatts_value;

    // Initialize value struct.
    memset(&gatts_value, 0, sizeof(gatts_value));

    gatts_value.len     = p_length;
    gatts_value.offset  = 0;
    gatts_value.p_value = p_data;

    // Update database.
    err_code = sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID,
                                      p_sensor_service->sensor_service_handles_1.value_handle,
                                      &gatts_value);
       
    return err_code;
}


ret_code_t ble_sensor_service_update_char2(ble_sensor_service_t *p_sensor_service,
                                           uint8_t  *p_data,
                                           uint16_t  p_length)
{
    if (p_sensor_service == NULL)
    {
        return NRF_ERROR_NULL;
    }

    ret_code_t         err_code = NRF_SUCCESS;
    ble_gatts_value_t  gatts_value;

    // Initialize value struct.
    memset(&gatts_value, 0, sizeof(gatts_value));

    gatts_value.len     = p_length;
    gatts_value.offset  = 0;
    gatts_value.p_value = p_data;

    // Update database.
    err_code = sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID,
                                      p_sensor_service->sensor_service_handles_2.value_handle,
                                      &gatts_value);
       
    return err_code;
}


uint32_t ble_sensor_service_send_char2(ble_sensor_service_t * p_sensor_service,
                           uint8_t   *p_data,
                           uint16_t  p_length,
                           uint16_t  conn_handle)
{
    ret_code_t                 err_code;
    ble_gatts_hvx_params_t     hvx_params;
    ble_sensor_service_client_context_t * p_client;

    VERIFY_PARAM_NOT_NULL(p_sensor_service);

    err_code = blcm_link_ctx_get(p_sensor_service->p_link_ctx_storage, conn_handle, (void *) &p_client);
    VERIFY_SUCCESS(err_code);

    if ((conn_handle == BLE_CONN_HANDLE_INVALID) || (p_client == NULL))
    {
        return NRF_ERROR_NOT_FOUND;
    }

    if (!p_client->is_notification_enabled)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    if (p_length > BLE_SENSOR_SERVICE_MAX_DATA_LEN)
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = p_sensor_service->sensor_service_handles_2.value_handle; // NOTIFY Characteristic handle
    hvx_params.p_data = p_data;
    hvx_params.p_len  = &p_length;
    hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;

    return sd_ble_gatts_hvx(conn_handle, &hvx_params);
}