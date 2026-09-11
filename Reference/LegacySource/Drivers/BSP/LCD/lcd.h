
#ifndef __LCD_H
#define __LCD_H

#include "./SYSTEM/sys/sys.h"

/* 定义LCD尺寸 */
#define LCD_WIDTH                160
#define LCD_HEIGHT               128

/* 偏移量 */
#define LCD_COLUMN_OFFSET        2
#define LCD_LINE_OFFSET          3

/* 引脚定义 */
#define LCD_PWR_GPIO_PORT            GPIOB                                                      /*背光调节*/
#define LCD_PWR_GPIO_PIN             GPIO_PIN_9
#define LCD_PWR_GPIO_CLK_ENABLE()    do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)
#define LCD_CS_GPIO_PORT             GPIOA                                                      /*片选信号*/
#define LCD_CS_GPIO_PIN              GPIO_PIN_6
#define LCD_CS_GPIO_CLK_ENABLE()     do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)
#define LCD_WR_GPIO_PORT             GPIOC                                                      /*命令选择 0 命令 1 数据*/
#define LCD_WR_GPIO_PIN              GPIO_PIN_5
#define LCD_WR_GPIO_CLK_ENABLE()     do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)
#define LCD_RST_GPIO_PORT            GPIOC                                                      /*复位信号*/
#define LCD_RST_GPIO_PIN             GPIO_PIN_4
#define LCD_RST_GPIO_CLK_ENABLE()    do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)

/* IO操作 */
#define LCD_PWR(x)                   do{ x ?                                                                                     \
                                                HAL_GPIO_WritePin(LCD_PWR_GPIO_PORT, LCD_PWR_GPIO_PIN, GPIO_PIN_SET) :    \
                                                HAL_GPIO_WritePin(LCD_PWR_GPIO_PORT, LCD_PWR_GPIO_PIN, GPIO_PIN_RESET);   \
                                            }while(0)
#define LCD_CS(x)                    do{ x ?                                                                                     \
                                                HAL_GPIO_WritePin(LCD_CS_GPIO_PORT, LCD_CS_GPIO_PIN, GPIO_PIN_SET) :      \
                                                HAL_GPIO_WritePin(LCD_CS_GPIO_PORT, LCD_CS_GPIO_PIN, GPIO_PIN_RESET);     \
                                            }while(0)
#define LCD_WR(x)                    do{ x ?                                                                                     \
                                                HAL_GPIO_WritePin(LCD_WR_GPIO_PORT, LCD_WR_GPIO_PIN, GPIO_PIN_SET) :      \
                                                HAL_GPIO_WritePin(LCD_WR_GPIO_PORT, LCD_WR_GPIO_PIN, GPIO_PIN_RESET);     \
                                            }while(0)
#define LCD_RST(x)                   do{ x ?                                                                                     \
                                                HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_SET) :    \
                                                HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_GPIO_PIN, GPIO_PIN_RESET);   \
                                            }while(0)



/*RGB565格式为：5位红色，6位绿色，5位蓝色。所以每个颜色的最大值：R=31(0x1F), G=63(0x3F), B=31(0x1F)。
我们可以通过以下公式计算RGB565值：
color = ((red & 0x1F) << 11) | ((green & 0x3F) << 5) | (blue & 0x1F);*/

/* 基础颜色 */
#define WHITE           0xFFFF      /* 白色 */
#define BLACK           0x0000      /* 黑色 */
#define RED             0xF800      /* 红色 */
#define GREEN           0x07E0      /* 绿色 */
#define BLUE            0x001F      /* 蓝色 */
#define MAGENTA         0xF81F      /* 品红色 */
#define YELLOW          0xFFE0      /* 黄色 */
#define CYAN            0x07FF      /* 青色 */

/* 红色系 */
#define DARK_RED        0x8000      /* 深红色 */
#define MAROON          0x7800      /* 栗色 */
#define CRIMSON         0xD8A7      /* 深红色 */
#define TOMATO          0xFB08      /* 番茄红 */
#define SALMON          0xFC0E      /* 鲑鱼红 */
#define CORAL           0xFBE0      /* 珊瑚红 */
#define ORANGE_RED      0xFA20      /* 橙红色 */
#define INDIAN_RED      0xCAEB      /* 印度红 */
#define FIREBRICK       0xB104      /* 砖红色 */

/* 橙色系 */
#define ORANGE          0xFD20      /* 橙色 */
#define DARK_ORANGE     0xFC60      /* 深橙色 */
#define GOLD            0xFEA0      /* 金色 */
#define GOLDENROD       0xDD24      /* 金菊黄 */
#define DARK_GOLDENROD  0xBC21      /* 深金菊黄 */
#define PERU            0xCC27      /* 秘鲁色 */
#define CHOCOLATE       0xD344      /* 巧克力色 */
#define SADDLE_BROWN    0x8A22      /* 鞍棕色 */

/* 黄色系 */
#define LIGHT_YELLOW    0xFFF0      /* 浅黄色 */
#define LEMON_CHIFFON   0xFFD8      /* 柠檬绸色 */
#define KHAKI           0xF731      /* 卡其色 */
#define PALE_GOLDENROD  0xEF55      /* 灰金菊黄 */
#define DARK_KHAKI      0xBDD7      /* 深卡其色 */
#define OLIVE           0x8400      /* 橄榄色 */
#define YELLOW_GREEN    0x9E66      /* 黄绿色 */
#define OLIVE_DRAB      0x6B64      /* 橄榄军服绿 */

/* 绿色系 */
#define LIME            0x07E0      /* 酸橙色 */
#define LIME_GREEN      0x3666      /* 酸橙绿 */
#define FOREST_GREEN    0x2444      /* 森林绿 */
#define GREEN_YELLOW    0xAFE5      /* 绿黄色 */
#define SPRING_GREEN    0x07EF      /* 春绿色 */
#define MEDIUM_SPRING_GREEN 0x07D7  /* 中春绿色 */
#define SEA_GREEN       0x344B      /* 海绿色 */
#define MEDIUM_SEA_GREEN 0x3D8E     /* 中海绿色 */
#define DARK_GREEN      0x0320      /* 深绿色 */
#define DARK_OLIVE_GREEN 0x5345     /* 深橄榄绿 */
#define DARK_SEA_GREEN  0x8DF1      /* 深海绿色 */
#define LIGHT_GREEN     0x9772      /* 浅绿色 */
#define PALE_GREEN      0x9FD3      /* 灰绿色 */
#define LAWN_GREEN      0x7FE0      /* 草坪绿 */
#define CHARTREUSE      0x7FE0      /* 查特酒绿 */
#define MEDIUM_AQUAMARINE 0x6675    /* 中碧绿色 */

/* 青色/蓝色系 */
#define TURQUOISE       0x46F0      /* 绿松石色 */
#define MEDIUM_TURQUOISE 0x4E99     /* 中绿松石色 */
#define DARK_TURQUOISE  0x4D1F      /* 深绿松石色 */
#define LIGHT_SEA_GREEN 0x2595      /* 浅海绿色 */
#define CADET_BLUE      0x5CF4      /* 军服蓝 */
#define STEEL_BLUE      0x4416      /* 钢蓝色 */
#define LIGHT_STEEL_BLUE 0xAE1F     /* 浅钢蓝色 */
#define POWDER_BLUE     0xB71C      /* 粉末蓝 */
#define LIGHT_BLUE      0xAEDC      /* 浅蓝色 */
#define SKY_BLUE        0x867F      /* 天蓝色 */
#define LIGHT_SKY_BLUE  0x867F      /* 浅天蓝色 */
#define DEEP_SKY_BLUE   0x05FF      /* 深天蓝色 */
#define DODGER_BLUE     0x249F      /* 道奇蓝 */
#define CORNFLOWER_BLUE 0x64BD      /* 矢车菊蓝 */
#define ROYAL_BLUE      0x435C      /* 皇家蓝 */
#define MEDIUM_BLUE     0x0019      /* 中蓝色 */
#define DARK_BLUE       0x0011      /* 深蓝色 */
#define NAVY            0x0010      /* 海军蓝 */
#define MIDNIGHT_BLUE   0x18CE      /* 午夜蓝 */

/* 紫色系 */
#define LAVENDER        0xE73F      /* 薰衣草紫 */
#define THISTLE         0xDDFB      /* 蓟色 */
#define PLUM            0xDD1B      /* 李子色 */
#define VIOLET          0xEC1D      /* 紫罗兰色 */
#define ORCHID          0xDB9A      /* 兰花紫 */
#define FUCHSIA         0xF81F      /* 紫红色 */
#define MAGENTA_DARK    0xD01F      /* 深品红色 */
#define MEDIUM_ORCHID   0xBABA      /* 中兰花紫 */
#define MEDIUM_PURPLE   0x939B      /* 中紫色 */
#define BLUE_VIOLET     0x895C      /* 蓝紫罗兰色 */
#define DARK_VIOLET     0x901A      /* 深紫罗兰色 */
#define DARK_ORCHID     0x9999      /* 深兰花紫 */
#define DARK_MAGENTA    0x8811      /* 深品红色 */
#define PURPLE          0x8010      /* 紫色 */
#define INDIGO          0x4810      /* 靛蓝色 */
#define DARK_SLATE_BLUE 0x49F1      /* 深石板蓝 */
#define SLATE_BLUE      0x6AD9      /* 石板蓝 */
#define MEDIUM_SLATE_BLUE 0x7B5D    /* 中石板蓝 */

/* 粉色系 */
#define PINK            0xFE19      /* 粉色 */
#define LIGHT_PINK      0xFF9C      /* 浅粉色 */
#define HOT_PINK        0xFB56      /* 艳粉色 */
#define DEEP_PINK       0xF8B2      /* 深粉色 */
#define PALE_VIOLET_RED 0xDB92      /* 灰紫红 */
#define MEDIUM_VIOLET_RED 0xC0B0    /* 中紫红色 */

/* 棕色系 */
#define BROWN           0xA145      /* 棕色 */
#define SIENNA          0x9A85      /* 赭色 */
#define SADDLE_BROWN_DARK 0x8A22   /* 深鞍棕色 */
#define CHOCOLATE_DARK  0xD344      /* 深巧克力色 */
#define PERU_DARK       0xCC27      /* 深秘鲁色 */
#define ROSY_BROWN      0xBC71      /* 玫瑰棕 */
#define BURLYWOOD       0xDDD0      /* 原木色 */
#define TAN             0xD5B1      /* 棕褐色 */
#define SANDY_BROWN     0xF52C      /* 沙棕色 */
#define WHEAT           0xF6F6      /* 小麦色 */
#define BEIGE           0xF7BB      /* 米色 */
#define MOCCASIN        0xFF16      /* 鹿皮色 */
#define NAVAJO_WHITE    0xFF36      /* 纳瓦白 */
#define PEACH_PUFF      0xFED7      /* 桃色 */
/* 灰色系 */
#define GAINSBORO       0xDEFB      /* 庚斯博罗灰 */
#define LIGHT_GRAY      0xD69A      /* 浅灰色 */
#define SILVER          0xC618      /* 银色 */
#define DARK_GRAY       0x7BEF      /* 深灰色 */
#define GRAY            0x8410      /* 灰色 */
#define DIM_GRAY        0x6B4D      /* 暗灰色 */
#define LIGHT_SLATE_GRAY 0x7BD7     /* 浅石板灰 */
#define SLATE_GRAY      0x7412      /* 石板灰 */
#define DARK_SLATE_GRAY 0x4A69      /* 深石板灰 */

/* 特殊颜色 */
#define SNOW            0xFFDF      /* 雪白色 */
#define HONEYDEW        0xF7FE      /* 蜜瓜色 */
#define MINT_CREAM      0xF7FF      /* 薄荷奶油色 */
#define AZURE           0xF7FF      /* 天青蓝 */
#define ALICE_BLUE      0xF7DF      /* 爱丽丝蓝 */
#define GHOST_WHITE     0xFFDF      /* 幽灵白 */
#define WHITE_SMOKE     0xF7BE      /* 烟白色 */
#define SEASHELL        0xFFBD      /* 贝壳色 */
#define BEIGE_LIGHT     0xF7BB      /* 浅米色 */
#define OLD_LACE        0xFFBC      /* 旧蕾丝色 */
#define FLORAL_WHITE    0xFFDE      /* 花卉白 */
#define IVORY           0xFFFE      /* 象牙白 */
#define ANTIQUE_WHITE   0xF7BC      /* 古董白 */
#define LINEN           0xFF9C      /* 亚麻色 */
#define LAVENDER_BLUSH  0xFF9E      /* 薰衣草红 */
#define MISTY_ROSE      0xFF3C      /* 雾玫瑰色 */
#define GAINSBORO_LIGHT 0xDEFB      /* 浅庚斯博罗灰 */
#define LIGHT_CYAN      0xE7FF      /* 浅青色 */
#define PALE_TURQUOISE  0xAF7D      /* 灰绿松石色 */
#define AQUAMARINE      0x7FFA      /* 碧绿色 */
#define MEDIUM_AQUAMARINE_LIGHT 0x6675 /* 中浅碧绿色 */
#define TURQUOISE_LIGHT 0x46F0      /* 浅绿松石色 */
#define AQUA            0x07FF      /* 水色 */
#define DARK_CYAN       0x0451      /* 深青色 */
#define TEAL            0x0410      /* 凫色 */

/* 操作函数 */
void lcd_init(void);                                                                                                                             /* 初始化 */
void lcd_display_on(void);                                                                                                                       /* 开启LCD背光 */
void lcd_display_off(void);                                                                                                                      /* 关闭LCD背光 */
void lcd_fill(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color);                                                               /* LCD区域填充 */
void lcd_fill_circle(uint16_t x, uint16_t y, uint16_t r, uint16_t color);
void lcd_clear(uint16_t color);                                                                                                                  /* LCD清屏 */
void lcd_draw_point(uint16_t x, uint16_t y, uint16_t color);                                                                                     /* LCD画点 */
void lcd_draw_line(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color);                                                          /* LCD画线段 */
void lcd_draw_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);                                                          /* LCD画矩形框 */
void lcd_draw_circle(uint16_t x, uint16_t y, uint16_t r, uint16_t color);                                                                        /* LCD画圆形框 */

void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color, uint16_t g_back_color);                       /* 显示一个字符 */
void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color, uint16_t g_back_color);                     /* 显示数字 */
void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color, uint16_t g_back_color);      /* 扩展显示数字 */
void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color, uint16_t g_back_color);   /* 显示字符串 */
void lcd_draw_rounded_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t radius, uint16_t color);            /*圆角矩形*/
void lcd_showChinese(uint16_t x, uint16_t y,uint8_t num, uint8_t size, uint16_t color ,uint16_t back_color);/*汉字*/

void lcd_show_pic(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t *pic);                                                        /* LCD图片 */
void lcd_draw_data_curve(uint16_t x0, uint16_t y0, uint16_t width, uint16_t height, 
                         float *data, uint16_t data_len, 
                         float min_val, float max_val, 
                         uint16_t color, uint16_t bg_color, uint8_t initial);
                         
                         
#endif
