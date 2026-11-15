/* selfreg.h - 首次运行自注册到用户 PATH，以及双击启动的控制台体验。 */
#ifndef SELFREG_H
#define SELFREG_H

#include <stdbool.h>

/* 把 exe 所在目录写入用户环境变量 Path（HKCU\Environment）。
   已注册且路径未变时不做任何写入；exe 被移动后运行时会把旧目录替换为新目录。
   静默执行，失败不影响主流程。设 EUI_NO_PATH=1 可关闭。 */
bool selfreg_ensure_path(void);

/* 当前进程独占一个新控制台窗口（双击 / 资源管理器启动）。 */
bool selfreg_owns_console(void);

/* 双击场景下等待按键，避免窗口一闪而过。 */
void selfreg_wait_key(void);

#endif
