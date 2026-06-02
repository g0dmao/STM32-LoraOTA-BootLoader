
/**
 * @brief 提供一个假的 RNG 初始化，骗过编译器和系统的初始化检查
 *
 * @warning 仅限用于纯验证端的设备！
 */
static int hydro_random_init(void) {
    return 0;
}
