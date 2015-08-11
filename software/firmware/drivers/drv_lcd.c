#include <board.h>
#include <rtdevice.h>
#include "drv_lcd.h"

#define USING_MULTI_BUF 0
#if USING_MULTI_BUF
typedef enum
{
    READY,
    BUSY,
    FINISH,
} lcd_buf_status_t;
typedef struct lcd_buffer
{
    lcd_buf_status_t status;
    /* framebuffer address and ops */
    volatile rt_uint8_t *framebuffer_addr;
    struct lcd_buffer *priv;
    struct lcd_buffer *next;
} lcd_buffer_t;

#define NUM_BUFFERS  2 // Number of multiple buffers to be used
static lcd_buffer_t lcd_buf[NUM_BUFFERS];
static  lcd_buffer_t *current_buf = RT_NULL;
#else
static  rt_uint16_t *lcd_framebuffer = RT_NULL;
#endif
static  rt_uint16_t *_rt_framebuffer = RT_NULL;

#define RT_HW_LCD_WIDTH     480
#define RT_HW_LCD_HEIGHT    272

static struct rt_device_graphic_info _lcd_info;
static struct rt_device  lcd;


/* LCD Config */
#define LCD_H_SIZE           480
#define LCD_H_PULSE          2
#define LCD_H_FRONT_PORCH    5
#define LCD_H_BACK_PORCH     40
#define LCD_V_SIZE           272
#define LCD_V_PULSE          2
#define LCD_V_FRONT_PORCH    8
#define LCD_V_BACK_PORCH     8


#define LCD_PIX_CLK          (8*1000000UL)




static void lcd_gpio_init(void)
{
	  
}

static rt_uint32_t find_clock_divisor(rt_uint32_t clock)
{
    rt_uint32_t Divider;
    rt_uint32_t r;

    Divider = 1;
    while (((SystemCoreClock / Divider) > clock) && (Divider <= 0x3F))
    {
        Divider++;
    }
    if (Divider <= 1)
    {
        r = (1 << 26);  // Skip divider logic if clock divider is 1
    }
    else
    {
        Divider -= 2;
        r = 0
            | (((Divider >> 0) & 0x1F)
               | (((Divider >> 5) & 0x1F) << 27))
            ;
    }
    return r;
}
#if USING_MULTI_BUF
static void lcd_interrupt_enable(void)
{
    LPC_LCD->INTCLR = 0x04;
    LPC_LCD->INTMSK |= 0x04;
    NVIC_EnableIRQ(LCD_IRQn);
}
static void lcd_interrupt_disable(void)
{
    LPC_LCD->INTMSK &= ~0x04;
    NVIC_DisableIRQ(LCD_IRQn);
}
void LCD_IRQHandler(void)
{

    rt_interrupt_enter();
    if (LPC_LCD->INTSTAT & 0x04)
    {
        if (current_buf->next->status == FINISH)
        {
            LPC_LCD->UPBASE  = (rt_uint32_t)current_buf->next->framebuffer_addr;
            current_buf = current_buf->next;
            current_buf->next->status = READY;
            //  rt_kprintf("buffer switch!\n");
            lcd_interrupt_disable();
        }
        //清除帧同步中断标志位
        LPC_LCD->INTCLR |= 0x04;
    }s
    rt_interrupt_leave();

}
#endif
#if USING_HW_CURSOR

void cursor_display(rt_bool_t enable)
{
    if (RT_TRUE == enable)
    {
        LPC_LCD->CRSR_CTRL |= 0x01;
    }
    else
    {
        LPC_LCD->CRSR_CTRL &= ~(1 << 0);
    }
}
void cursor_set_position(rt_uint32_t x, rt_uint32_t y)
{
    /* set cursor position */
    LPC_LCD->CRSR_XY = ((y << 16) | (x << 0));
}
static void cursor_hw_init(void)
{
    unsigned int i;
    rt_uint32_t *puicursor = (rt_uint32_t *)CRSR_IMGBASE0;
    cursor_set_position(0, 0);
    LPC_LCD->CRSR_CTRL = 0x00;                                          /* 不显示光标                   */
    for (i = 0; i < 64; i++)
    {
        *puicursor++ = cursor[i];                                     /* 读取光标图像                 */
    }
    LPC_LCD->CRSR_CFG  = (0x01 << 1) |                                  /* 光标坐标和帧同步脉冲同步     */
                         (0x00 << 0);                                   /* 选择 32x32 p像素光标         */
    LPC_LCD->CRSR_PAL0 = 0x00ffffff;                                    /* 调色板0为黑色                */
    LPC_LCD->CRSR_PAL1 = 0x00000000;                                    /* 调色板1为白色                */
}
#endif

/* RT-Thread Device Interface */
static rt_err_t rt_lcd_init(rt_device_t dev)
{


#if USING_MULTI_BUF
    lcd_buf[0].framebuffer_addr = rt_malloc_align(sizeof(rt_uint16_t) * RT_HW_LCD_HEIGHT * RT_HW_LCD_WIDTH, 32);
    lcd_buf[0].priv = &lcd_buf[1];
    lcd_buf[0].next = &lcd_buf[1];
    lcd_buf[1].framebuffer_addr = rt_malloc_align(sizeof(rt_uint16_t) * RT_HW_LCD_HEIGHT * RT_HW_LCD_WIDTH, 32);
    lcd_buf[1].priv = &lcd_buf[0];
    lcd_buf[1].next = &lcd_buf[0];
    lcd_buf[0].status = FINISH;
    lcd_buf[1].status = READY;
    current_buf = &lcd_buf[0];
    LPC_LCD->UPBASE  = (rt_uint32_t)current_buf->framebuffer_addr;
#else
    lcd_framebuffer = rt_malloc_align(sizeof(rt_uint16_t) * RT_HW_LCD_HEIGHT * RT_HW_LCD_WIDTH, 32);
    rt_memset(lcd_framebuffer, 0, sizeof(rt_uint16_t) * RT_HW_LCD_HEIGHT * RT_HW_LCD_WIDTH);
    //LPC_LCD->UPBASE  = (rt_uint32_t)lcd_framebuffer;
#endif
    //
    // Enable LCDC
    //

#if USING_HW_CURSOR
    cursor_hw_init();
    cursor_display(RT_FALSE);
#endif

    return RT_EOK;
}

static rt_err_t rt_lcd_control(rt_device_t dev, rt_uint8_t cmd, void *args)
{
    switch (cmd)
    {
    case RTGRAPHIC_CTRL_RECT_UPDATE:
#if USING_MULTI_BUF
        while (current_buf->next->status != READY);
        rt_memcpy((void *)current_buf->next->framebuffer_addr, _rt_framebuffer, sizeof(rt_uint16_t)*RT_HW_LCD_HEIGHT * RT_HW_LCD_WIDTH);
        current_buf->next->status = FINISH;
        lcd_interrupt_enable();
#else
        rt_memcpy((void *)lcd_framebuffer, _rt_framebuffer, sizeof(rt_uint16_t)*RT_HW_LCD_HEIGHT * RT_HW_LCD_WIDTH);
#endif
        //rt_kprintf("lcd update!\n");
        break;
    case RTGRAPHIC_CTRL_POWERON:
        break;
    case RTGRAPHIC_CTRL_POWEROFF:
        break;
    case RTGRAPHIC_CTRL_GET_INFO:
        rt_memcpy(args, &_lcd_info, sizeof(_lcd_info));
        break;
    case RTGRAPHIC_CTRL_SET_MODE:
        break;
    }

    return RT_EOK;
}

void lcd_clear(rt_uint16_t color)
{
    volatile rt_uint16_t *p = (rt_uint16_t *)_lcd_info.framebuffer;
    int x, y;

    for (y = 0; y <= RT_HW_LCD_HEIGHT; y++)
    {
        for (x = 0; x <= RT_HW_LCD_WIDTH; x++)
        {
            *p++ = color; /* red */
        }
    }
}

void rt_hw_lcd_init(void)
{


    _rt_framebuffer = rt_malloc_align(sizeof(rt_uint16_t) * RT_HW_LCD_HEIGHT * RT_HW_LCD_WIDTH, 32);
    if (_rt_framebuffer == RT_NULL) return; /* no memory yet */
    //_rt_framebuffer = (rt_uint16_t *)FRAME_BUFFER;
    _lcd_info.bits_per_pixel = 16;
    _lcd_info.pixel_format = RTGRAPHIC_PIXEL_FORMAT_RGB565;
    _lcd_info.framebuffer = (void *)_rt_framebuffer;
    _lcd_info.width = RT_HW_LCD_WIDTH;
    _lcd_info.height = RT_HW_LCD_HEIGHT;

    /* init device structure */
    lcd.type = RT_Device_Class_Graphic;
    lcd.init = rt_lcd_init;
    lcd.open = RT_NULL;
    lcd.close = RT_NULL;
    lcd.control = rt_lcd_control;
    lcd.user_data = (void *)&_lcd_info;

    /* register lcd device to RT-Thread */
    rt_device_register(&lcd, "lcd", RT_DEVICE_FLAG_RDWR);

    rt_lcd_init(&lcd);
}


void lcd_test(void)
{
    lcd_clear(0xf800);
    rt_thread_delay(200);
    lcd_clear(0x07e0);
    rt_thread_delay(200);
    lcd_clear(0x001f);
    rt_thread_delay(200);
}
#ifdef RT_USING_FINSH
#include <finsh.h>
FINSH_FUNCTION_EXPORT(lcd_clear, lcd_clear);
FINSH_FUNCTION_EXPORT(lcd_test, lcd_test);
#ifdef FINSH_USING_MSH
MSH_CMD_EXPORT(lcd_clear, lcd_clear);
MSH_CMD_EXPORT(lcd_test, lcd_test);
#endif
#endif
