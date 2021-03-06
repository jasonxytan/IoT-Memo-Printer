/**
 * \file
 *
 * \brief SAM RTC Driver (Calendar Interrupt Mode)
 *
 * Copyright (c) 2013-2018 Microchip Technology Inc. and its subsidiaries.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 *
 */
/*
 * Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
 */
#include "rtc_calendar_interrupt.h"

extern struct rtc_module *_rtc_instance[RTC_INST_NUM];

/**
 * \brief Registers callback for the specified callback type
 *
 * Associates the given callback function with the
 * specified callback type.
 * To enable the callback, the \ref rtc_calendar_enable_callback function
 * must be used.
 *
 * \param[in,out]  module  Pointer to the software instance struct
 * \param[in]  callback      Pointer to the function desired for the specified
 *                           callback
 * \param[in]  callback_type Callback type to register
 *
 * \return                        Status of registering callback.
 * \retval STATUS_OK              Registering was done successfully
 * \retval STATUS_ERR_INVALID_ARG If trying to register, a callback is not available
 */
enum status_code rtc_calendar_register_callback(
		struct rtc_module *const module,
		rtc_calendar_callback_t callback,
		enum rtc_calendar_callback callback_type)
{

	enum status_code status = STATUS_OK;

	/* Overflow callback */
	if (callback_type == RTC_CALENDAR_CALLBACK_OVERFLOW) {
		status = STATUS_OK;
	} else if (callback_type > RTC_NUM_OF_ALARMS) {
		/* Make sure alarm callback can be registered */
		status = STATUS_ERR_INVALID_ARG;
	}

	if (status == STATUS_OK) {
		/* Register callback */
		module->callbacks[callback_type] = callback;
		/* Set corresponding bit to set callback as registered */
		module->registered_callback |= (1 << callback_type);
	}

	return status;
}

/**
 * \brief Unregisters callback for the specified callback type
 *
 * When called, the currently registered callback for the given callback type
 * will be removed.
 *
 * \param[in,out]  module  Pointer to the software instance struct
 * \param[in]     callback_type  Specifies the callback type to unregister
 *
 * \return                        Status of unregistering callback.
 * \retval STATUS_OK              Unregistering was done successfully
 * \retval STATUS_ERR_INVALID_ARG If trying to unregister, a callback is not available
 */
enum status_code rtc_calendar_unregister_callback(
		struct rtc_module *const module,
		enum rtc_calendar_callback callback_type)
{
	enum status_code status = STATUS_OK;

	/* Overflow callback */
	if (callback_type == RTC_CALENDAR_CALLBACK_OVERFLOW) {
		status = STATUS_OK;
	} else if (callback_type > RTC_NUM_OF_ALARMS) {
		/* Make sure alarm callback can be unregistered */
		status = STATUS_ERR_INVALID_ARG;
	}

	if (status == STATUS_OK) {
		/* Unregister callback */
		module->callbacks[callback_type] = NULL;

		/* Clear corresponding bit to set callback as unregistered */
		module->registered_callback &= ~(1 << callback_type);
	}
	return status;
}

/**
 * \brief Enables callback
 *
 * Enables the callback specified by the callback_type.
 *
 * \param[in,out]  module  Pointer to the software instance struct
 * \param[in]     callback_type Callback type to enable
 */
void rtc_calendar_enable_callback(
		struct rtc_module *const module,
		enum rtc_calendar_callback callback_type)
{
	/* Sanity check arguments */
	Assert(module);
	Assert(module->hw);

	Rtc *const rtc_module = module->hw;

	if (callback_type == RTC_CALENDAR_CALLBACK_OVERFLOW) {
		rtc_module->MODE2.INTENSET.reg = RTC_MODE2_INTFLAG_OVF;
	} else {
		rtc_module->MODE2.INTENSET.reg = RTC_MODE2_INTFLAG_ALARM(1 << callback_type);
	}
	/* Mark callback as enabled. */
	module->enabled_callback |= (1 << callback_type);
}

/**
 * \brief Disables callback
 *
 * Disables the callback specified by the callback_type.
 *
 * \param[in,out]  module  Pointer to the software instance struct
 * \param[in]     callback_type Callback type to disable
 */
void rtc_calendar_disable_callback(
		struct rtc_module *const module,
		enum rtc_calendar_callback callback_type)
{
	/* Sanity check arguments */
	Assert(module);
	Assert(module->hw);

	Rtc *const rtc_module = module->hw;

	/* Disable interrupt */
	if (callback_type == RTC_CALENDAR_CALLBACK_OVERFLOW) {
		rtc_module->MODE2.INTENCLR.reg = RTC_MODE2_INTFLAG_OVF;
	} else {
		rtc_module->MODE2.INTENCLR.reg = RTC_MODE2_INTFLAG_ALARM(1 << callback_type);
	}

	/* Mark callback as disabled. */
	module->enabled_callback &= ~(1 << callback_type);
}

/**
 * \internal Interrupt handler for RTC
 *
 * \param [in] instance_index  Default value 0
 */
static void _rtc_interrupt_handler(const uint32_t instance_index)
{
	struct rtc_module *module = _rtc_instance[instance_index];

	Rtc *const rtc_module = module->hw;

	/* Combine callback registered and enabled masks */
	uint16_t callback_mask = module->enabled_callback;
	callback_mask &= module->registered_callback;

	/* Read and mask interrupt flag register */
	uint16_t interrupt_status = rtc_module->MODE2.INTFLAG.reg;
	interrupt_status &= rtc_module->MODE2.INTENSET.reg;

	if (interrupt_status & RTC_MODE2_INTFLAG_OVF) {
		/* Overflow interrupt */
		if (callback_mask & (1 << RTC_CALENDAR_CALLBACK_OVERFLOW)) {
			module->callbacks[RTC_CALENDAR_CALLBACK_OVERFLOW]();
		}

		/* Clear interrupt flag */
		rtc_module->MODE2.INTFLAG.reg = RTC_MODE2_INTFLAG_OVF;

	} else if (interrupt_status & RTC_MODE2_INTFLAG_ALARM(1 << 0)) {
		/* Alarm 0 interrupt */
		if (callback_mask & (1 << RTC_CALENDAR_CALLBACK_ALARM_0)) {
			module->callbacks[RTC_CALENDAR_CALLBACK_ALARM_0]();
		}
		/* Clear interrupt flag */
		rtc_module->MODE2.INTFLAG.reg = RTC_MODE2_INTFLAG_ALARM(1 << 0);

	} else if (interrupt_status & RTC_MODE2_INTFLAG_ALARM(1 << 1)) {
		#if (RTC_NUM_OF_ALARMS > 1) || defined(__DOXYGEN__)
		/* Alarm 1 interrupt */
		if (callback_mask & (1 << RTC_CALENDAR_CALLBACK_ALARM_1)) {
			module->callbacks[RTC_CALENDAR_CALLBACK_ALARM_1]();
		}
		/* Clear interrupt flag */
		rtc_module->MODE2.INTFLAG.reg = RTC_MODE2_INTFLAG_ALARM(1 << 1);
		#endif

	} else if (interrupt_status & RTC_MODE2_INTFLAG_ALARM(1 << 2)) {
		#if (RTC_NUM_OF_ALARMS > 2)	|| defined(__DOXYGEN__)
		/* Alarm 2 interrupt */
		if (callback_mask & (1 << RTC_CALENDAR_CALLBACK_ALARM_2)) {
			module->callbacks[RTC_CALENDAR_CALLBACK_ALARM_2]();
		}
		/* Clear interrupt flag */
		rtc_module->MODE2.INTFLAG.reg = RTC_MODE2_INTFLAG_ALARM(1 << 2);
		#endif

	} else if (interrupt_status & RTC_MODE2_INTFLAG_ALARM(1 << 3)) {
		#if (RTC_NUM_OF_ALARMS > 3)	|| defined(__DOXYGEN__)
		/* Alarm 3 interrupt */
		if (callback_mask & (1 << RTC_CALENDAR_CALLBACK_ALARM_3)) {
			module->callbacks[RTC_CALENDAR_CALLBACK_ALARM_3]();
		}
		/* Clear interrupt flag */
		rtc_module->MODE2.INTFLAG.reg = RTC_MODE2_INTFLAG_ALARM(1 << 3);
		#endif
	}
}

/**
 * \internal ISR handler for RTC
 */
#if (RTC_INST_NUM == 1)
void RTC_Handler(void)
{
	_rtc_interrupt_handler(0);
}
#elif (RTC_INST_NUM > 1)
#  define _RTC_INTERRUPT_HANDLER(n, unused) \
		void RTC##n##_Handler(void) \
		{ \
			_rtc_interrupt_handler(n); \
		}

MREPEAT(RTC_INST_NUM, _RTC_INTERRUPT_HANDLER, ~)
#endif /* (RTC_INST_NUM > 1) */