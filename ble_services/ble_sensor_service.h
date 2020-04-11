#ifndef __BLE_SENSOR_SERVICE_H
#define __BLE_SENSOR_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "sdk_config.h"
#include "ble.h"
#include "ble_srv_common.h"
#include "nrf_sdh_ble.h"
#include "ble_link_ctx_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_SENSOR_SERVICE_BLE_OBSERVER_PRIO 2

#define BLE_SENSOR_SERVICE_DEF(_name, _sensor_service_max_clients)           \
    BLE_LINK_CTX_MANAGER_DEF(CONCAT_2(_name, _link_ctx_storage),             \
                             (_sensor_service_max_clients),                  \
                             sizeof(ble_sensor_service_client_context_t));   \
    static ble_sensor_service_t _name =                                      \
    {                                                                        \
        .p_link_ctx_storage = &CONCAT_2(_name, _link_ctx_storage)            \
    };                                                                       \
    NRF_SDH_BLE_OBSERVER(_name ## _obs,                                      \
                         BLE_SENSOR_SERVICE_BLE_OBSERVER_PRIO,               \
                         ble_sensor_service_on_ble_evt,                      \
                         &_name)


#define OPCODE_LENGTH        1
#define HANDLE_LENGTH        2

/**@brief   Maximum length of data (in bytes) that can be transmitted to the peer by the SENSOR service module. */
#if defined(NRF_SDH_BLE_GATT_MAX_MTU_SIZE) && (NRF_SDH_BLE_GATT_MAX_MTU_SIZE != 0)
    #define BLE_SENSOR_SERVICE_MAX_DATA_LEN (NRF_SDH_BLE_GATT_MAX_MTU_SIZE - OPCODE_LENGTH - HANDLE_LENGTH)
#else
    #define BLE_SENSOR_SERVICE_MAX_DATA_LEN (BLE_GATT_MTU_SIZE_DEFAULT - OPCODE_LENGTH - HANDLE_LENGTH)
    #warning NRF_SDH_BLE_GATT_MAX_MTU_SIZE is not defined.
#endif


/**@brief   SENSOR Service event types. */
typedef enum
{
    BLE_SENSOR_SERVICE_EVT_DATA_RECEIVED_CHAR1,      /**< Data received for characteristic 1. */
    BLE_SENSOR_SERVICE_EVT_DATA_RECEIVED_CHAR2,      /**< Data received for characteristic 2. */

    BLE_SENSOR_SERVICE_EVT_TRANSMIT_RDY,       /**< Service is ready to accept new data to be transmitted. */
    BLE_SENSOR_SERVICE_EVT_COMM_STARTED,      /**< Notification has been enabled. */
    BLE_SENSOR_SERVICE_EVT_COMM_STOPPED,      /**< Notification has been disabled. */
} ble_sensor_service_evt_type_t;


/* Forward declaration of the ble_sensor_service_t type. */
typedef struct ble_sensor_service_s ble_sensor_service_t;


/**@brief   SENSOR Service @ref BLE_SENSOR_SERVICE_EVT_DATA_RECEIVED event data.
 *
 * @details This structure is passed to an event when @ref BLE_SENSOR_SERVICE_EVT_DATA_RECEIVED occurs.
 */
typedef struct
{
    uint8_t const * p_data; /**< A pointer to the buffer with received data. */
    uint16_t        length; /**< Length of received data. */
} ble_sensor_service_evt_received_data_t;


/**@brief SENSOR Service client context structure.
 *
 * @details This structure contains state context related to hosts.
 */
typedef struct
{
    bool is_notification_enabled; /**< Variable to indicate if the peer has enabled notification of the characteristic.*/
} ble_sensor_service_client_context_t;


/**@brief   SENSOR Service event structure.
 *
 * @details This structure is passed to an event coming from service.
 */
typedef struct
{
    ble_sensor_service_evt_type_t                 type;                  /**< Event type. */
    ble_sensor_service_t                          *p_sensor_service;     /**< A pointer to the instance. */
    uint16_t                                      conn_handle;           /**< Connection handle. */
    ble_sensor_service_client_context_t *         p_link_ctx;            /**< A pointer to the link context. */
    union
    {
        ble_sensor_service_evt_received_data_t received_data;           /**< @ref BLE_sensor_SERVICE_EVT_RECEIVED_DATA event data. */
    } params;
} ble_sensor_service_evt_t;


/**@brief SENSOR Service event handler type. */
typedef void (* ble_sensor_service_data_handler_t) (ble_sensor_service_evt_t * p_evt);


/**@brief   SENSOR Service initialization structure.
 *
 * @details This structure contains the initialization information for the service. The application
 * must fill this structure and pass it to the service using the @ref ble_sensor_service_init
 *          function.
 */
typedef struct
{
    ble_sensor_service_data_handler_t   data_handler; /**< Event handler to be called for handling received data. */
} ble_sensor_service_init_t;


/**@brief   SENSOR Service structure.
 *
 * @details This structure contains status information related to the service.
 */
struct ble_sensor_service_s
{
    uint8_t                             uuid_type;          /**< UUID type for Nordic UART Service Base UUID. */
    uint16_t                            service_handle;     /**< Handle of Nordic UART Service (as provided by the SoftDevice). */
    
    ble_gatts_char_handles_t            sensor_service_handles_1;         /**< Handles related to the characteristic 1(as provided by the SoftDevice). */
    ble_gatts_char_handles_t            sensor_service_handles_2;         /**< Handles related to the characteristic 2(as provided by the SoftDevice). */

    uint8_t  *init_value_1;
    uint8_t  *init_value_2;

    blcm_link_ctx_storage_t * const       p_link_ctx_storage; /**< Pointer to link context storage with handles of all current connections and its context. */
    ble_sensor_service_data_handler_t     data_handler;       /**< Event handler to be called for handling received data. */
};


uint32_t ble_sensor_service_init(ble_sensor_service_t * p_sensor, ble_sensor_service_init_t const * p_sensor_init);


void ble_sensor_service_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context);


ret_code_t ble_sensor_service_update_char1(ble_sensor_service_t *p_sensor_service,
                                           uint8_t  *p_data,
                                           uint16_t  p_length);

ret_code_t ble_sensor_service_update_char2(ble_sensor_service_t *p_sensor_service,
                                           uint8_t  *p_data,
                                           uint16_t  p_length);

uint32_t ble_sensor_service_send_char2(ble_sensor_service_t * p_sensor_service,
                           uint8_t   *p_data,
                           uint16_t  p_length,
                           uint16_t  conn_handle);

#ifdef __cplusplus
}
#endif

#endif // __BLE_SENSOR_SERVICE_H

/** @} */
