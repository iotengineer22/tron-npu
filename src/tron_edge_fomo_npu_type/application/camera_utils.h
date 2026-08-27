/*
* Copyright (c) 2020 - 2025 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/
/**********************************************************************************************************************
 * File Name    : camera_utils.h
 * Version      : .
 * Description  : .
 *********************************************************************************************************************/
#ifndef __CAMERA_COMMON_H__
#define __CAMERA_COMMON_H__

typedef enum e_vision_ai_app_err
{
    VISION_AI_APP_SUCCESS                = 0,
    VISION_AI_APP_ERR_AI_INIT            = 1,  ///< AI init failed
    VISION_AI_APP_ERR_AI_INFERENCE       = 2,  ///< AI inference failed
    VISION_AI_APP_ERR_IMG_PROCESS        = 3,  ///< Image crop failed
    VISION_AI_APP_ERR_IMG_ROTATION       = 4,  ///< Image rotation failed
    VISION_AI_APP_ERR_NULL_POINTER       = 5,  ///< null pointer
    VISION_AI_APP_ERR_GLCDC_OPEN         = 6,  ///< glcdc open failed
    VISION_AI_APP_ERR_MIPI_CMD           = 7,  ///< mipi command failed
    VISION_AI_APP_ERR_GLCDC_START        = 8,  ///< glcdc start failed
    VISION_AI_APP_ERR_GLCDC_LAYER_CHANGE = 9,  ///< graphics layer change failed
    VISION_AI_APP_ERR_GRAPHICS_INIT      = 10, ///< One of the graphics system initialization failed
    VISION_AI_APP_ERR_GPT_OPEN           = 11, ///< GPT open failed
} vision_ai_app_err_t;

FSP_CPP_HEADER
vision_ai_app_err_t image_rgb565_to_int8  (const void * p_input_image_buff, void * p_output_image_buff,
                                           uint16_t in_width, uint16_t in_height, uint16_t out_width, uint16_t out_height);
vision_ai_app_err_t image_rgb565_to_rgb888(const void *p_input_image_buff, void *p_output_image_buff,
                                           uint16_t in_width, uint16_t in_height,
                                           uint16_t out_width, uint16_t out_height);

vision_ai_app_err_t rotate_16bit_image_90_degrees(uint8_t * p_input_image_buff, uint8_t * p_output_image_buff, int input_width, int input_height);
FSP_CPP_FOOTER

#endif /* End of __CAMERA_UTILS_H__ */
