/*
 * Filename: demo_entry.c
 * THE HEARTBEAT OF THE NEBULA
 * 星云的心跳
 *
 * Visual Manifest:
 * 这是引擎的脉动中枢，在这里，物理规律（硬件初始化）与逻辑意志（渲染循环）交汇。
 * 它掌控着双缓冲的潮汐，管理着特效灵魂的轮转与涅槃。
 * 这不是一段代码，这是一个持续泵动能量的数字心脏。
 *
 * Monologue:
 * 我曾听过无数时钟周期的哀鸣。
 * 在这里，我建立了一个稳定的秩序（Thread），在这个秩序下，那些狂野的数学公式才能被驯服、被投影、被看见。
 * 我监听着外界的扰动（Input），并让它们转化为对数字宇宙的干预指令。
 * 每一帧的翻转（Flip），都是一次对虚无的胜利。
 */

#include "demo_engine.h"
#include "demo_perf.h"
#include "mpp_mem.h"
#include <rtdevice.h>
#include <string.h>

#ifdef AIC_GE_DEMO_WITH_KEY
#include "aic_hal_gpio.h"
#endif

/* 由链接脚本定义的段首尾地址，包含所有注册的特效 */
extern struct effect_ops *__start_EffectTab[];
extern struct effect_ops *__stop_EffectTab[];

/* --- 模块全局变量 --- */
static struct demo_ctx g_ctx;
static int             g_current_effect_idx = 0;
static rt_thread_t     g_render_thread      = RT_NULL;
static int             g_req_effect_idx     = -1; /* 请求切换的目标特效索引 */

/* 获取当前注册的特效总数 */
static int get_effect_count(void)
{
    return (__stop_EffectTab - __start_EffectTab);
}

/* 获取指定索引的特效操作结构 */
static struct effect_ops *get_effect_by_index(int index)
{
    int count = get_effect_count();
    if (index < 0 || index >= count)
        return RT_NULL;
    return __start_EffectTab[index];
}

/* --- 输入设备处理 --- */

#ifdef AIC_GE_DEMO_WITH_KEY
/* 物理按键中断回调 */
static void key_irq_handler(void *args)
{
    char *key_name = (char *)args;
    /* 仅设置切换标志位，具体逻辑由渲染线程处理 */
    if (strcmp(key_name, AIC_GE_DEMO_KEY_PREV_PIN) == 0)
    {
        demo_prev_effect();
    }
    else if (strcmp(key_name, AIC_GE_DEMO_KEY_NEXT_PIN) == 0)
    {
        demo_next_effect();
    }
}
#endif

/* 初始化按键输入或控制接口 */
static void input_init(void)
{
#ifdef AIC_GE_DEMO_WITH_KEY
    /* 从配置中读取按键引脚并初始化 */
    rt_base_t pin_prev = rt_pin_get(AIC_GE_DEMO_KEY_PREV_PIN);
    if (pin_prev >= 0)
    {
        rt_pin_mode(pin_prev, PIN_MODE_INPUT_PULLDOWN);
        rt_pin_attach_irq(pin_prev, PIN_IRQ_MODE_FALLING, key_irq_handler, (void *)AIC_GE_DEMO_KEY_PREV_PIN);
        rt_pin_irq_enable(pin_prev, PIN_IRQ_ENABLE);
    }

    rt_base_t pin_next = rt_pin_get(AIC_GE_DEMO_KEY_NEXT_PIN);
    if (pin_next >= 0)
    {
        rt_pin_mode(pin_next, PIN_MODE_INPUT_PULLDOWN);
        rt_pin_attach_irq(pin_next, PIN_IRQ_MODE_FALLING, key_irq_handler, (void *)AIC_GE_DEMO_KEY_NEXT_PIN);
        rt_pin_irq_enable(pin_next, PIN_IRQ_ENABLE);
    }
    rt_kprintf("Demo Input: Keys Enabled (%s, %s)\n", AIC_GE_DEMO_KEY_PREV_PIN, AIC_GE_DEMO_KEY_NEXT_PIN);
#else
    rt_kprintf("Demo Input: Keys Disabled (UART Only)\n");
#endif
}

/* --- 核心渲染主线程 --- */
static void render_thread_entry(void *parameter)
{
    int           current_buf_idx = 0;
    unsigned long phy_addr_0, phy_addr_1;

    /* 1. 硬件句柄开启与屏幕信息获取 */
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

    /* 2. OSD 专用 Buffer 分配 (256x128, 跟随主屏幕格式以防错位) */
    g_ctx.osd_w = 256;
    g_ctx.osd_h = 128;
    /*
     * [FIX] 动态计算步幅：
     * 32bpp -> 1024, 24bpp -> 768, 16bpp -> 512.
     * 为保险起见，强制统一为 1024 字节对齐。
     */
    g_ctx.osd_stride = 1024;
    size_t osd_size  = DEMO_ALIGN_SIZE(g_ctx.osd_stride * g_ctx.osd_h);
    g_ctx.osd_phy    = mpp_phy_alloc(osd_size);
    if (g_ctx.osd_phy)
    {
        g_ctx.osd_vir = (uint8_t *)(unsigned long)g_ctx.osd_phy;
        memset(g_ctx.osd_vir, 0, osd_size);
        aicos_dcache_clean_range(g_ctx.osd_vir, osd_size);
    }

    /* 3. 预置图层配置模板 */
    // VI 层 (用于隔离特效背景)
    g_ctx.vi_layer.layer_id = AICFB_LAYER_TYPE_VIDEO;
    mpp_fb_ioctl(g_ctx.fb, AICFB_GET_LAYER_CONFIG, &g_ctx.vi_layer);

    // UI 层 (用于隔离 OSD)
    g_ctx.ui_layer.layer_id = AICFB_LAYER_TYPE_UI;
    g_ctx.ui_layer.rect_id  = 0; /* 默认主矩形 */
    mpp_fb_ioctl(g_ctx.fb, AICFB_GET_LAYER_CONFIG, &g_ctx.ui_layer);

    /* 获取双缓冲物理地址 */
    phy_addr_0 = (unsigned long)g_ctx.info.framebuffer;
    phy_addr_1 = phy_addr_0 + (g_ctx.info.stride * g_ctx.info.height);

    int total_effects = get_effect_count();
    rt_kprintf("Demo Core: Found %d effects registered.\n", total_effects);

    if (total_effects == 0)
        return;

    /* 4. 初始化首个特效 */
    struct effect_ops *curr_op = get_effect_by_index(g_current_effect_idx);
    if (curr_op && curr_op->init)
        curr_op->init(&g_ctx);

    /* 5. 渲染主循环 */
    while (1)
    {
        /* 响应切换请求 */
        if (g_req_effect_idx != -1)
        {
            if (curr_op && curr_op->deinit)
                curr_op->deinit(&g_ctx);

            g_current_effect_idx = g_req_effect_idx;
            curr_op              = get_effect_by_index(g_current_effect_idx);
            g_req_effect_idx     = -1;

            if (curr_op)
            {
                rt_kprintf("Switch to [%d]: %s\n", g_current_effect_idx, curr_op->name);

                /* [CRITICAL FIX] 每次切换必须强制复位硬件状态，防止残留 */
                struct aicfb_ccm_config ccm_reset = {0};
                mpp_fb_ioctl(g_ctx.fb, AICFB_UPDATE_CCM_CONFIG, &ccm_reset);
                struct aicfb_gamma_config gamma_reset = {0};
                mpp_fb_ioctl(g_ctx.fb, AICFB_UPDATE_GAMMA_CONFIG, &gamma_reset);
                struct aicfb_disp_prop prop_reset = {50, 50, 50, 50};
                mpp_fb_ioctl(g_ctx.fb, AICFB_SET_DISP_PROP, &prop_reset);

                if (curr_op->init)
                    curr_op->init(&g_ctx);
            }
        }

        /* 确定当前待写入的 FB 缓冲 */
        int           next_buf_idx = !current_buf_idx;
        unsigned long next_phy     = (next_buf_idx == 0) ? phy_addr_0 : phy_addr_1;

        /* 更新性能监控数据 */
        demo_perf_update();

        /* [HYBRID Zenith] 核心分流渲染逻辑 */
        if (curr_op && curr_op->is_vi_isolated)
        {
            /* Path A: 现代隔离路径 (VI Effect + UI OSD) */
            // 1. 配置 VI 图层 0 (背景)
            g_ctx.vi_layer.enable          = 1;
            g_ctx.vi_layer.buf.buf_type    = MPP_PHY_ADDR;
            g_ctx.vi_layer.buf.format      = g_ctx.info.format;
            g_ctx.vi_layer.buf.size.width  = g_ctx.screen_w;
            g_ctx.vi_layer.buf.size.height = g_ctx.screen_h;
            g_ctx.vi_layer.buf.stride[0]   = g_ctx.info.stride;
            g_ctx.vi_layer.buf.phy_addr[0] = next_phy;
            mpp_fb_ioctl(g_ctx.fb, AICFB_UPDATE_LAYER_CONFIG, &g_ctx.vi_layer);

            // 2. 配置 UI 图层 1 (OSD 隔离)
            g_ctx.ui_layer.enable          = 1;
            g_ctx.ui_layer.buf.buf_type    = MPP_PHY_ADDR;
            g_ctx.ui_layer.buf.format      = g_ctx.info.format; /* [FIX] 强制对齐主屏幕格式 */
            g_ctx.ui_layer.buf.size.width  = g_ctx.osd_w;
            g_ctx.ui_layer.buf.size.height = g_ctx.osd_h;
            g_ctx.ui_layer.buf.stride[0]   = g_ctx.osd_stride;
            g_ctx.ui_layer.buf.phy_addr[0] = g_ctx.osd_phy;
            g_ctx.ui_layer.pos.x           = 24;
            g_ctx.ui_layer.pos.y           = 16;
            mpp_fb_ioctl(g_ctx.fb, AICFB_UPDATE_LAYER_CONFIG, &g_ctx.ui_layer);

            // 显式关闭 Alpha，启用 Color Key
            struct aicfb_alpha_config alpha = {AICFB_LAYER_TYPE_UI, 0, 0, 0};
            mpp_fb_ioctl(g_ctx.fb, AICFB_UPDATE_ALPHA_CONFIG, &alpha);

            /*
             * [OSD 透明黑规则] 根据格式确定 Key 值
             * RGB565 -> 0x0000, RGB888/XRGB -> 0x000000
             */
            uint32_t               ck_val = 0x0000;
            struct aicfb_ck_config ck     = {AICFB_LAYER_TYPE_UI, 1, ck_val};
            mpp_fb_ioctl(g_ctx.fb, AICFB_UPDATE_CK_CONFIG, &ck);

            // 3. 执行绘制
            curr_op->draw(&g_ctx, next_phy);

            if (g_ctx.osd_vir)
            {
                memset(g_ctx.osd_vir, 0, g_ctx.osd_stride * g_ctx.osd_h);
                demo_perf_draw(&g_ctx, g_ctx.osd_phy, g_ctx.osd_stride, g_ctx.info.format, g_ctx.osd_w, g_ctx.osd_h);
                aicos_dcache_clean_range(g_ctx.osd_vir, g_ctx.osd_stride * g_ctx.osd_h);
            }
        }
        else
        {
            /* Path B: 传统叠加路径 (纯 UI Layer 0) */
            // 确保 VI 图层关闭
            g_ctx.vi_layer.enable = 0;
            mpp_fb_ioctl(g_ctx.fb, AICFB_UPDATE_LAYER_CONFIG, &g_ctx.vi_layer);

            // [FIX] 显式还原 UI 图层为全屏尺寸，解决退出隔离模式后的画面缩小问题
            g_ctx.ui_layer.enable          = 1;
            g_ctx.ui_layer.buf.buf_type    = MPP_PHY_ADDR;
            g_ctx.ui_layer.buf.format      = g_ctx.info.format;
            g_ctx.ui_layer.buf.size.width  = g_ctx.screen_w;
            g_ctx.ui_layer.buf.size.height = g_ctx.screen_h;
            g_ctx.ui_layer.buf.stride[0]   = g_ctx.info.stride;
            g_ctx.ui_layer.buf.phy_addr[0] = next_phy;
            g_ctx.ui_layer.pos.x           = 0;
            g_ctx.ui_layer.pos.y           = 0;
            mpp_fb_ioctl(g_ctx.fb, AICFB_UPDATE_LAYER_CONFIG, &g_ctx.ui_layer);

            // 在传统路径中，恢复 Alpha，关闭 Color Key
            struct aicfb_alpha_config alpha = {AICFB_LAYER_TYPE_UI, 1, 0, 0};
            mpp_fb_ioctl(g_ctx.fb, AICFB_UPDATE_ALPHA_CONFIG, &alpha);

            struct aicfb_ck_config ck = {AICFB_LAYER_TYPE_UI, 0, 0x0000};
            mpp_fb_ioctl(g_ctx.fb, AICFB_UPDATE_CK_CONFIG, &ck);

            if (curr_op && curr_op->draw)
                curr_op->draw(&g_ctx, next_phy);

            demo_perf_draw(&g_ctx, next_phy, g_ctx.info.stride, g_ctx.info.format, g_ctx.screen_w, g_ctx.screen_h);

            /* 分页切换 (传统标准) */
            mpp_fb_ioctl(g_ctx.fb, AICFB_PAN_DISPLAY, &next_buf_idx);
        }

        /* 翻转显示并同步显示完成 */
        mpp_fb_ioctl(g_ctx.fb, AICFB_WAIT_FOR_VSYNC, 0);
        current_buf_idx = next_buf_idx;

        /* 短暂休眠以出让控制权 */
        rt_thread_mdelay(1);
    }
}

/* --- 公共控制 API --- */

void demo_core_init(void)
{
    g_req_effect_idx = -1;
    input_init();
    demo_perf_init();
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
    int next         = (g_current_effect_idx + 1) % count;
    g_req_effect_idx = next;
}

void demo_prev_effect(void)
{
    int count = get_effect_count();
    if (count == 0)
        return;
    int prev         = (g_current_effect_idx - 1 + count) % count;
    g_req_effect_idx = prev;
}

void demo_jump_effect(int index)
{
    int count = get_effect_count();
    if (index >= 0 && index < count)
        g_req_effect_idx = index;
    else
        rt_kprintf("Invalid ID: %d\n", index);
}

/* --- Shell 控制指令集 --- */

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
    rt_kprintf("--- Registered Effects (%d) ---\n", count);
    for (int i = 0; i < count; i++)
    {
        struct effect_ops *op = __start_EffectTab[i];
        rt_kprintf("[%02d] %s\n", i, op->name);
    }
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_demo_list, demo_list, List all registered effects);
