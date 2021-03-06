/******************************************************************************
 * FileName: web_iohw.c
 * Description: Small WEB server
 * Author: PV`
 ******************************************************************************/
#include "user_config.h"
#include "bios.h"
#include "hw/esp8266.h"
#include "hw/eagle_soc.h"
#include "hw/uart_register.h"
#include "sdk/add_func.h"
#include "ets_sys.h"
#include "osapi.h"
#include "flash_eep.h"
#include "web_iohw.h"
#include "tcp2uart.h"
//=============================================================================
// get_addr_gpiox_mux(pin_num)
//-----------------------------------------------------------------------------
volatile uint32 * ICACHE_FLASH_ATTR get_addr_gpiox_mux(uint8 pin_num)
{
	return &GPIOx_MUX(pin_num & 0x0F);
}
//=============================================================================
// set_gpiox_mux_func(pin_num, func)
//-----------------------------------------------------------------------------
void ICACHE_FLASH_ATTR set_gpiox_mux_func(uint8 pin_num, uint8 func)
{
	volatile uint32 *goio_mux = get_addr_gpiox_mux(pin_num); // volatile uint32 *goio_mux = &GPIOx_MUX(PIN_NUM)
	*goio_mux = (*goio_mux & (~GPIO_MUX_FUN_MASK)) | (((func + 0x0C) & 0x013) << GPIO_MUX_FUN_BIT0);
}
//=============================================================================
// set_gpiox_mux_pull(pin_num, pull)
//-----------------------------------------------------------------------------
void ICACHE_FLASH_ATTR set_gpiox_mux_pull(uint8 pin_num, uint8 pull)
{
	volatile uint32 *goio_mux = get_addr_gpiox_mux(pin_num); // volatile uint32 *goio_mux = &GPIOx_MUX(PIN_NUM)
	*goio_mux = (*goio_mux & (~(3 << GPIO_MUX_PULLDOWN_BIT))) | ((pull & 3) << GPIO_MUX_PULLDOWN_BIT);
}
//=============================================================================
// set_gpiox_mux_func_ioport(pin_num)
//-----------------------------------------------------------------------------
void ICACHE_FLASH_ATTR set_gpiox_mux_func_ioport(uint8 pin_num)
{
    set_gpiox_mux_func(pin_num, ((uint32)(_FUN_IO_PORT >> (pin_num << 1)) & 0x03));
}
//=============================================================================
// select cpu frequency 80 or 160 MHz
//-----------------------------------------------------------------------------
void ICACHE_FLASH_ATTR set_cpu_clk(void)
{
	ets_intr_lock();
	if(syscfg.cfg.b.hi_speed_enable) {
		Select_CLKx2(); // REG_SET_BIT(0x3ff00014, BIT(0));
		ets_update_cpu_frequency(160);
	}
	else {
		Select_CLKx1(); // REG_CLR_BIT(0x3ff00014, BIT(0));
		ets_update_cpu_frequency(80);
	}
	ets_intr_unlock();
}
//=============================================================================
//  Пристартовый тест пина RX для сброса конфигурации
//=============================================================================

#define GPIO_TEST 3 // GPIO3 (RX)

void GPIO_intr_handler(void * test_edge)
{
	uint32 gpio_status = GPIO_STATUS;
	GPIO_STATUS_W1TC = gpio_status;
	if(gpio_status & (1 << GPIO_TEST)) *((uint8 *)test_edge) = 1; // test_edge++;
//    gpio_pin_intr_state_set(GPIO_TEST, GPIO_PIN_INTR_ANYEDGE);
}

void ICACHE_FLASH_ATTR test_pin_clr_wifi_config(void)
{
	struct UartxCfg ucfg;
	uint32 x = 0;
	uint8 test_edge = 0;
	if(flash_read_cfg(&ucfg, ID_CFG_UART0, sizeof(ucfg)) == sizeof(ucfg)) {
		if(ucfg.cfg.b.rxd_inv) x = 1 << GPIO_TEST;
	}
	gpio_output_set(0,0,0, 1 << GPIO_TEST); // GPIO OUTPUT DISABLE отключить вывод в порту GPIO3
	set_gpiox_mux_func_ioport(GPIO_TEST); // установить RX (GPIO3) в режим порта i/o
	if((GPIO_IN & (1 << GPIO_TEST)) == x) {
		ets_isr_mask(1 << ETS_GPIO_INUM);
		ets_isr_attach(ETS_GPIO_INUM, GPIO_intr_handler, (void *)&test_edge);
        gpio_pin_intr_state_set(GPIO_TEST, GPIO_PIN_INTR_ANYEDGE);
		GPIO_STATUS_W1TC = 1 << GPIO_TEST;
		ets_isr_unmask(1 << ETS_GPIO_INUM);
		ets_delay_us(25000); //25 ms
		ets_isr_mask(1 << ETS_GPIO_INUM);
	    if(test_edge == 0) { // изменений не было
#if DEBUGSOO > 0
			os_printf("WiFi configuration reset\n");
#endif
	    	flash_save_cfg(&x, ID_CFG_WIFI, 0); // создать запись нулевой длины
	//    	flash_save_cfg(&x, ID_CFG_UART0, 0);
	//    	flash_save_cfg(&x, ID_CFG_SYS, 0);
	    }
	}
	set_uartx_invx(UART0, ucfg.cfg.b.rxd_inv, UART_RXD_INV); // установить RX (GPIO3) в режим RX UART, если требуется
}




