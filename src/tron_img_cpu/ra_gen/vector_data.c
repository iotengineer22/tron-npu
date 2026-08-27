/* generated vector source file - do not edit */
#include "bsp_api.h"
/* Do not build these data structures if no interrupts are currently allocated because IAR will have build errors. */
#if VECTOR_DATA_IRQ_COUNT > 0
        BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_NUM_ENTRIES] BSP_PLACE_IN_SECTION(BSP_SECTION_APPLICATION_VECTORS) =
        {
                        [0] = iic_master_rxi_isr, /* IIC1 RXI (Receive data full) */
            [1] = iic_master_txi_isr, /* IIC1 TXI (Transmit data empty) */
            [2] = iic_master_tei_isr, /* IIC1 TEI (Transmit end) */
            [3] = iic_master_eri_isr, /* IIC1 ERI (Transfer error) */
            [4] = glcdc_line_detect_isr, /* GLCDC LINE DETECT (Specified line) */
            [5] = drw_int_isr, /* DRW INT (DRW interrupt) */
            [6] = vin_status_isr, /* VIN IRQ (Interrupt Request) */
            [7] = vin_error_isr, /* VIN ERR (Interrupt Request for SYNC Error) */
            [8] = mipi_csi_rx_isr, /* MIPICSI RX (Receive interrupt) */
            [9] = mipi_csi_dl_isr, /* MIPICSI DL (Data Lane interrupt) */
            [10] = mipi_csi_vc_isr, /* MIPICSI VC (Virtual Channel interrupt) */
            [11] = mipi_csi_pm_isr, /* MIPICSI PM (Power Management interrupt) */
            [12] = mipi_csi_gst_isr, /* MIPICSI GST (Generic Short Packet interrupt) */
        };
        #if BSP_FEATURE_ICU_HAS_IELSR
        const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_NUM_ENTRIES] =
        {
            [0] = BSP_PRV_VECT_ENUM(EVENT_IIC1_RXI,GROUP0), /* IIC1 RXI (Receive data full) */
            [1] = BSP_PRV_VECT_ENUM(EVENT_IIC1_TXI,GROUP1), /* IIC1 TXI (Transmit data empty) */
            [2] = BSP_PRV_VECT_ENUM(EVENT_IIC1_TEI,GROUP2), /* IIC1 TEI (Transmit end) */
            [3] = BSP_PRV_VECT_ENUM(EVENT_IIC1_ERI,GROUP3), /* IIC1 ERI (Transfer error) */
            [4] = BSP_PRV_VECT_ENUM(EVENT_GLCDC_LINE_DETECT,GROUP4), /* GLCDC LINE DETECT (Specified line) */
            [5] = BSP_PRV_VECT_ENUM(EVENT_DRW_INT,GROUP5), /* DRW INT (DRW interrupt) */
            [6] = BSP_PRV_VECT_ENUM(EVENT_VIN_IRQ,GROUP6), /* VIN IRQ (Interrupt Request) */
            [7] = BSP_PRV_VECT_ENUM(EVENT_VIN_ERR,GROUP7), /* VIN ERR (Interrupt Request for SYNC Error) */
            [8] = BSP_PRV_VECT_ENUM(EVENT_MIPICSI_RX,GROUP0), /* MIPICSI RX (Receive interrupt) */
            [9] = BSP_PRV_VECT_ENUM(EVENT_MIPICSI_DL,GROUP1), /* MIPICSI DL (Data Lane interrupt) */
            [10] = BSP_PRV_VECT_ENUM(EVENT_MIPICSI_VC,GROUP2), /* MIPICSI VC (Virtual Channel interrupt) */
            [11] = BSP_PRV_VECT_ENUM(EVENT_MIPICSI_PM,GROUP3), /* MIPICSI PM (Power Management interrupt) */
            [12] = BSP_PRV_VECT_ENUM(EVENT_MIPICSI_GST,GROUP4), /* MIPICSI GST (Generic Short Packet interrupt) */
        };
        #endif
        #endif
