#include "demo_engine.h"
#include <rtdevice.h>
#include <string.h>

/* 仅在启用按键时包含 GPIO 头文件 */
#ifdef AIC_GE_DEMO_WITH_KEY
#include "aic_hal_gpio.h"
#endif

/*
 * === 自动收集魔法 ===
 * GCC Linker 会自动生成这两个符号，指向 "EffectTab" 段的开头和结尾。
 * 这个段里存放的是 (struct effect_ops *) 类型的指针。
 */
extern struct effect_ops *__start_EffectTab[];
extern struct effect_ops *__stop_EffectTab[];

/* --- Global State --- */
static struct demo_ctx g_ctx;
static int             g_current_effect_idx = 0;
static rt_thread_t     g_render_thread      = RT_NULL;
static int             g_req_effect_idx     = -1; // 请求切换的目标索引

/* 获取特效总数 */
static int get_effect_count(void)
{
    return (__stop_EffectTab - __start_EffectTab);
}

/* 根据索引获取特效指针 */
static struct effect_ops *get_effect_by_index(int index)
{
    int count = get_effect_count();
    if (index < 0 || index >= count)
        return RT_NULL;
    return __start_EffectTab[index];
}

/* --- Input Handling --- */

#ifdef AIC_GE_DEMO_WITH_KEY
/* 仅在启用按键时编译中断处理函数 */

// 简单的去抖动回调
static void key_irq_handler(void *args)
{
    char *key_name = (char *)args;
    // 发送事件到 Mailbox 或设置标志位。这里为了简单直接调用切换。
    // 注意：中断里不能做耗时操作，也不能直接调 MSH。
    // 最佳实践是发送信号量给控制线程，但我们这里直接改标志位给渲染线程看。

    if (strcmp(key_name, AIC_GE_DEMO_KEY_PREV_PIN) == 0)
    {
        demo_prev_effect();
    }
    else if (strcmp(key_name, AIC_GE_DEMO_KEY_NEXT_PIN) == 0)
    {
        demo_next_effect();
    }
}
#endif /* AIC_GE_DEMO_WITH_KEY */

static void input_init(void)
{
#ifdef AIC_GE_DEMO_WITH_KEY
    /*
     * 从 Kconfig 读取引脚名称字符串
     * AIC_GE_DEMO_KEY_PREV_PIN 和 AIC_GE_DEMO_KEY_NEXT_PIN
     */

    // Prev Key
    rt_base_t pin_prev = rt_pin_get(AIC_GE_DEMO_KEY_PREV_PIN);
    if (pin_prev >= 0)
    {
        rt_pin_mode(pin_prev, PIN_MODE_INPUT_PULLDOWN);
        rt_pin_attach_irq(pin_prev, PIN_IRQ_MODE_FALLING, key_irq_handler, (void *)AIC_GE_DEMO_KEY_PREV_PIN);
        rt_pin_irq_enable(pin_prev, PIN_IRQ_ENABLE);
    }

    // Next Key
    rt_base_t pin_next = rt_pin_get(AIC_GE_DEMO_KEY_NEXT_PIN);
    if (pin_next >= 0)
    {
        rt_pin_mode(pin_next, PIN_MODE_INPUT_PULLDOWN);
        rt_pin_attach_irq(pin_next, PIN_IRQ_MODE_FALLING, key_irq_handler, (void *)AIC_GE_DEMO_KEY_NEXT_PIN);
        rt_pin_irq_enable(pin_next, PIN_IRQ_ENABLE);
    }

    rt_kprintf("Demo Input: Keys Enabled (%s, %s)\n", AIC_GE_DEMO_KEY_PREV_PIN, AIC_GE_DEMO_KEY_NEXT_PIN);
#else
    rt_kprintf("Demo Input: Keys Disabled (UART Control Only)\n");
#endif
}

/* --- Render Thread --- */
static void render_thread_entry(void *parameter)
{
    struct aicfb_layer_data layer           = {0};
    int                     current_buf_idx = 0;
    unsigned long           phy_addr_0, phy_addr_1;

    /* 1. Hardware Init */
    g_ctx.fb = mpp_fb_open();
    g_ctx.ge = mpp_ge_open();
    if (!g_ctx.fb || !g_ctx.ge)
    {
        rt_kprintf("Demo Error: Hardware Init Failed.\n");
        return;
    }

    mpp_fb_ioctl(g_ctx.fb, AICFB_GET_SCREENINFO, &g_ctx.info);
    g_ctx.screen_w = g_ctx.info.width;
    g_ctx.screen_h = g_ctx.info.height;

    // Layer Setup
    layer.layer_id = AICFB_LAYER_TYPE_UI;
    mpp_fb_ioctl(g_ctx.fb, AICFB_GET_LAYER_CONFIG, &layer);
    layer.enable       = 1;
    layer.buf.buf_type = MPP_PHY_ADDR;
    mpp_fb_ioctl(g_ctx.fb, AICFB_UPDATE_LAYER_CONFIG, &layer);

    // Buffers
    phy_addr_0 = (unsigned long)g_ctx.info.framebuffer;
    phy_addr_1 = phy_addr_0 + (g_ctx.info.stride * g_ctx.info.height);

    int total_effects = get_effect_count();
    rt_kprintf("Demo Core: Found %d effects registered.\n", total_effects);

    // 如果没有特效，直接退出线程，避免后续空指针访问
    if (total_effects == 0)
        return;

    /* 2. Load First Effect */
    struct effect_ops *curr_op = get_effect_by_index(g_current_effect_idx);
    if (curr_op && curr_op->init)
        curr_op->init(&g_ctx);

    /* 3. Main Loop */
    while (1)
    {
        // Handle Switch Request
        if (g_req_effect_idx != -1)
        {
            // Deinit old
            if (curr_op && curr_op->deinit)
                curr_op->deinit(&g_ctx);

            // Switch
            g_current_effect_idx = g_req_effect_idx;
            curr_op              = get_effect_by_index(g_current_effect_idx);
            g_req_effect_idx     = -1;

            // Init new
            if (curr_op)
            {
                rt_kprintf("Switch to [%d]: %s\n", g_current_effect_idx, curr_op->name);
                if (curr_op->init)
                    curr_op->init(&g_ctx);
            }

            rt_kprintf("Switched to: %s\n", curr_op->name);
        }

        // Render
        int           next_buf_idx = !current_buf_idx;
        unsigned long next_phy     = (next_buf_idx == 0) ? phy_addr_0 : phy_addr_1;

        if (curr_op && curr_op->draw)
        {
            curr_op->draw(&g_ctx, next_phy);
        }

        // Flip & Sync
        mpp_fb_ioctl(g_ctx.fb, AICFB_PAN_DISPLAY, &next_buf_idx);
        mpp_fb_ioctl(g_ctx.fb, AICFB_WAIT_FOR_VSYNC, 0);
        current_buf_idx = next_buf_idx;

        // Yield to let shell run (非常重要！)
        // 虽然 WAIT_FOR_VSYNC 本身会挂起线程，但加一个 yield 是双重保险
        rt_thread_mdelay(1);
    }
}

/* --- Public API --- */

void demo_core_init(void)
{
    g_req_effect_idx = -1;
    input_init();
}

void demo_core_start(void)
{
    g_render_thread = rt_thread_create("ge_render", render_thread_entry, RT_NULL, 4096, 20, 10);
    if (g_render_thread)
    {
        rt_thread_startup(g_render_thread);
        rt_kprintf("GE Render Thread Started.\n");
    }
}

void demo_next_effect(void)
{
    int count = get_effect_count();
    if (count == 0)
        return;
    int next = g_current_effect_idx + 1;
    if (next >= count)
        next = 0;
    g_req_effect_idx = next;
}

void demo_prev_effect(void)
{
    int count = get_effect_count();
    if (count == 0)
        return;
    int prev = g_current_effect_idx - 1;
    if (prev < 0)
        prev = count - 1;
    g_req_effect_idx = prev;
}

void demo_jump_effect(int index)
{
    int count = get_effect_count();
    if (index >= 0 && index < count)
    {
        g_req_effect_idx = index;
    }
    else
    {
        rt_kprintf("Invalid ID: %d (Max %d)\n", index, count - 1);
    }
}

/* --- MSH Commands --- */

static int cmd_demo_next(int argc, char **argv)
{
    demo_next_effect();
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_demo_next, demo_next, Switch to next effect);

static int cmd_demo_prev(int argc, char **argv)
{
    demo_prev_effect();
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_demo_prev, demo_prev, Switch to prev effect);

static int cmd_demo_jump(int argc, char **argv)
{
    if (argc < 2)
        return -1;
    demo_jump_effect(atoi(argv[1]));
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_demo_jump, demo_jump, Jump to effect ID);

static int cmd_demo_list(int argc, char **argv)
{
    int count = get_effect_count();
    rt_kprintf("--- Effect List (%d) ---\n", count);
    for (int i = 0; i < count; i++)
    {
        struct effect_ops *op = __start_EffectTab[i];
        rt_kprintf("[%02d] %s\n", i, op->name);
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_demo_list, demo_list, List all effects);
