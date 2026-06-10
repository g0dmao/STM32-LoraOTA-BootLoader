/**
 * @file    binpkg.c
 * @brief   固件签名打包工具
 *
 * 对 .bin 固件文件计算 Ed25519 签名，并将 FirmwareFooter_t 追加到文件末尾。
 * 签名覆盖仅固件本体（不含 footer），与 BootLoader 的验签逻辑完全匹配。
 *
 * 用法：
 *   binpkg -i <固件.bin> -k <私钥.hex> -v <版本号> [-o <输出.bin>] [-h]
 */

#include "hydrogen.h"
#include "firmware_footer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

/* ============================================================
 *  内部函数声明
 * ============================================================ */

static void     print_usage(const char *prog);
static size_t   read_file(const char *path, uint8_t **out);
static int      read_hex_key(const char *path, uint8_t *key, size_t expected_len);
static char    *make_output_path(const char *input_path);
static void     print_hex(FILE *fp, const uint8_t *data, size_t len);
static int      sign_and_build_footer(const uint8_t *fw_data, size_t fw_size,
                                       const uint8_t sk[hydro_sign_SECRETKEYBYTES],
                                       uint32_t version, FirmwareFooter_t *footer);
static int      copy_file(const char *src, const char *dst);

/* ============================================================
 *  main
 * ============================================================ */

int main(int argc, char **argv)
{
    const char       *input_path  = NULL;
    const char       *key_path    = NULL;
    const char       *output_path = NULL;
    const char       *version_str = NULL;
    int               in_place    = 0;
    int               opt;

    uint8_t          *fw_data    = NULL;
    size_t            fw_size;
    uint8_t           sk[hydro_sign_SECRETKEYBYTES];
    uint32_t          version;
    FirmwareFooter_t  footer;
    char             *default_out  = NULL;
    int               ret          = 0;

    /* ---- 解析命令行参数 ---- */
    while ((opt = getopt(argc, argv, "i:k:v:o:h-:")) != -1) {
        switch (opt) {
        case 'i':
            input_path = optarg;
            break;
        case 'k':
            key_path = optarg;
            break;
        case 'v':
            version_str = optarg;
            break;
        case 'o':
            output_path = optarg;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        case '-':
            /* 长选项 --in-place */
            if (strcmp(optarg, "in-place") == 0) {
                in_place = 1;
            } else {
                fprintf(stderr, "错误: 未知选项 --%s\n", optarg);
                print_usage(argv[0]);
                return 1;
            }
            break;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    /* ---- 校验必需参数 ---- */
    if (input_path == NULL || key_path == NULL || version_str == NULL) {
        fprintf(stderr, "错误: 缺少必需参数 (-i, -k, -v)\n\n");
        print_usage(argv[0]);
        return 1;
    }

    /* ---- 解析版本号 ---- */
    char *endptr;
    unsigned long v = strtoul(version_str, &endptr, 0);
    if (*endptr != '\0' || v > UINT32_MAX) {
        fprintf(stderr, "错误: 无效的版本号 '%s'\n", version_str);
        return 1;
    }
    version = (uint32_t)v;

    /* ---- 确定输出路径 ---- */
    if (output_path != NULL && in_place) {
        fprintf(stderr, "错误: -o 和 --in-place 不能同时使用\n");
        return 1;
    }
    if (output_path == NULL && !in_place) {
        default_out = make_output_path(input_path);
        output_path = default_out;
    }
    if (in_place) {
        output_path = input_path;
    }

    /* ---- 读取私钥 ---- */
    if (read_hex_key(key_path, sk, hydro_sign_SECRETKEYBYTES) != 0) {
        ret = 1;
        goto cleanup;
    }

    /* ---- 读取固件二进制 ---- */
    fw_size = read_file(input_path, &fw_data);
    if (fw_data == NULL) {
        ret = 1;
        goto cleanup;
    }

    if (fw_size == 0) {
        fprintf(stderr, "错误: 固件文件为空\n");
        ret = 1;
        goto cleanup;
    }

    /* ---- 原地模式：先备份 ---- */
    if (in_place) {
        char *bak_path = NULL;
        size_t len = strlen(input_path) + 5; /* .bak */
        bak_path = (char *)malloc(len);
        if (bak_path == NULL) {
            fprintf(stderr, "错误: 内存分配失败\n");
            ret = 1;
            goto cleanup;
        }
        snprintf(bak_path, len, "%s.bak", input_path);
        printf("备份原始文件: %s -> %s\n", input_path, bak_path);
        if (copy_file(input_path, bak_path) != 0) {
            fprintf(stderr, "错误: 备份失败\n");
            free(bak_path);
            ret = 1;
            goto cleanup;
        }
        free(bak_path);
    }

    /* ---- 初始化 libhydrogen ---- */
    if (hydro_init() != 0) {
        fprintf(stderr, "错误: RNG 初始化失败\n");
        ret = 1;
        goto cleanup;
    }

    /* ---- 签名并构建 Footer ---- */
    if (sign_and_build_footer(fw_data, fw_size, sk, version, &footer) != 0) {
        ret = 1;
        goto cleanup;
    }

    /* ---- 写入输出文件：固件本体 + Footer ---- */
    {
        FILE *out_fp = fopen(output_path, "wb");
        if (out_fp == NULL) {
            fprintf(stderr, "错误: 无法创建输出文件 '%s': %s\n",
                    output_path, strerror(errno));
            ret = 1;
            goto cleanup;
        }

        if (fwrite(fw_data, 1, fw_size, out_fp) != fw_size) {
            fprintf(stderr, "错误: 写入固件数据失败\n");
            fclose(out_fp);
            ret = 1;
            goto cleanup;
        }

        if (fwrite(&footer, 1, sizeof(footer), out_fp) != sizeof(footer)) {
            fprintf(stderr, "错误: 写入 Footer 失败\n");
            fclose(out_fp);
            ret = 1;
            goto cleanup;
        }

        fclose(out_fp);
    }

    /* ---- 打印摘要 ---- */
    printf("\n========== 签名完成 ==========\n");
    printf("输入文件:   %s\n", input_path);
    printf("固件大小:   %zu 字节\n", fw_size);
    printf("版本号:     %u\n", version);
    printf("签名 (hex): ");
    print_hex(stdout, footer.signature, FW_SIGNATURE_SIZE);
    printf("\n魔数:       0x%08X\n", footer.footer_magic);
    printf("Footer 大小: %u 字节\n", footer.footer_size);
    printf("输出文件:   %s (%zu 字节)\n",
           output_path, fw_size + sizeof(footer));
    printf("==============================\n");

cleanup:
    free(fw_data);
    free(default_out);
    return ret;
}

/* ============================================================
 *  内部函数实现
 * ============================================================ */

/**
 * @brief 打印帮助信息
 */
static void print_usage(const char *prog)
{
    const char *name = strrchr(prog, '/');
    name = name ? name + 1 : prog;

    printf(
        "用法: %s -i <固件.bin> -k <私钥.hex> -v <版本号> [-o <输出.bin>] [-h]\n"
        "\n"
        "对 .bin 固件文件计算 Ed25519 签名，并将 Footer 追加到文件末尾。\n"
        "签名覆盖仅固件本体（不含 footer），与 BootLoader 验签逻辑完全匹配。\n"
        "\n"
        "必需参数：\n"
        "  -i <文件>      输入原始固件二进制文件 (.bin)\n"
        "  -k <文件>      私钥文件，十六进制格式（128个十六进制字符 = 64字节）\n"
        "  -v <版本号>    固件版本号（uint32_t，十进制）\n"
        "\n"
        "可选参数：\n"
        "  -o <文件>      输出文件（默认：<输入>_signed.bin）\n"
        "  --in-place     原地覆盖输入文件（自动备份为 <输入>.bak）\n"
        "  -h             显示此帮助信息\n"
        "\n"
        "签名上下文:  \"%s\"（固定值，必须与 BootLoader 一致）\n"
        "Footer 大小: %u 字节（版本 4B + 签名 64B + 魔数 4B + 尺寸 4B）\n",
        name, FW_SIGN_CONTEXT, FW_FOOTER_SIZE
    );
}

/**
 * @brief 读取整个文件到堆内存缓冲区
 *
 * @param path  文件路径
 * @param out   输出：指向文件数据的指针（调用者需 free）
 * @return 文件大小（字节），失败时 *out = NULL 并返回 0
 */
static size_t read_file(const char *path, uint8_t **out)
{
    FILE   *fp;
    size_t  size;
    uint8_t *buf;

    *out = NULL;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "错误: 无法打开文件 '%s': %s\n", path, strerror(errno));
        return 0;
    }

    /* 获取文件大小 */
    if (fseek(fp, 0, SEEK_END) != 0) {
        fprintf(stderr, "错误: 无法获取文件大小 '%s': %s\n", path, strerror(errno));
        fclose(fp);
        return 0;
    }
    size = (size_t)ftell(fp);
    rewind(fp);

    /* 分配内存并读取 */
    buf = (uint8_t *)malloc(size);
    if (buf == NULL) {
        fprintf(stderr, "错误: 内存分配失败 (%zu 字节)\n", size);
        fclose(fp);
        return 0;
    }

    if (fread(buf, 1, size, fp) != size) {
        fprintf(stderr, "错误: 读取文件失败 '%s': %s\n", path, strerror(errno));
        free(buf);
        fclose(fp);
        return 0;
    }

    fclose(fp);
    *out = buf;
    return size;
}

/**
 * @brief 读取十六进制格式的私钥文件，解码为二进制
 *
 * 自动去除空白字符（空格、换行、制表符等），然后解码。
 * 验证解码结果恰好为 expected_len 字节。
 *
 * @param path          私钥文件路径
 * @param key           输出：解码后的密钥
 * @param expected_len  期望的密钥长度（字节）
 * @return 0 = 成功, -1 = 失败
 */
static int read_hex_key(const char *path, uint8_t *key, size_t expected_len)
{
    FILE   *fp;
    size_t  file_size;
    char   *raw_hex;
    size_t  stripped_len = 0;
    int     decoded;

    fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "错误: 无法打开私钥文件 '%s': %s\n", path, strerror(errno));
        return -1;
    }

    /* 读取整个文件 */
    fseek(fp, 0, SEEK_END);
    file_size = (size_t)ftell(fp);
    rewind(fp);

    raw_hex = (char *)malloc(file_size + 1);
    if (raw_hex == NULL) {
        fprintf(stderr, "错误: 内存分配失败\n");
        fclose(fp);
        return -1;
    }

    if (fread(raw_hex, 1, file_size, fp) != file_size) {
        fprintf(stderr, "错误: 读取私钥文件失败 '%s'\n", path);
        free(raw_hex);
        fclose(fp);
        return -1;
    }
    raw_hex[file_size] = '\0';
    fclose(fp);

    /* 去除空白字符 */
    {
        size_t i;
        for (i = 0; i < file_size; i++) {
            if (!isspace((unsigned char)raw_hex[i])) {
                raw_hex[stripped_len++] = raw_hex[i];
            }
        }
        raw_hex[stripped_len] = '\0';
    }

    /* 检查长度：2 个十六进制字符 = 1 字节 */
    if (stripped_len != expected_len * 2) {
        fprintf(stderr, "错误: 私钥长度不正确\n");
        fprintf(stderr, "       期望 %zu 个十六进制字符（%zu 字节），实际 %zu 个字符\n",
                expected_len * 2, expected_len, stripped_len);
        free(raw_hex);
        return -1;
    }

    /* 解码十六进制 → 二进制 */
    decoded = hydro_hex2bin(key, expected_len, raw_hex, stripped_len, NULL, NULL);
    free(raw_hex);

    if (decoded != (int)expected_len) {
        fprintf(stderr, "错误: 私钥文件包含无效的十六进制字符\n");
        return -1;
    }

    return 0;
}

/**
 * @brief 根据输入文件名生成默认输出文件名
 *
 * 在扩展名 .bin 前插入 _signed，例如：
 *   firmware.bin → firmware_signed.bin
 *   无扩展名     → firmware_signed
 *
 * @param input_path  输入文件路径
 * @return 新分配的字符串（调用者需 free），失败返回 NULL
 */
static char *make_output_path(const char *input_path)
{
    const char *dot_bin;
    char       *out;
    size_t      base_len, out_len;

    /* 查找最后一个 .bin 扩展名 */
    dot_bin = strstr(input_path, ".bin");
    /* 确认 .bin 确实在末尾（没有其他扩展名跟在后面） */
    if (dot_bin != NULL && strcmp(dot_bin, ".bin") == 0) {
        base_len = (size_t)(dot_bin - input_path);
    } else {
        base_len = strlen(input_path);
    }

    /* 分配：base_len + "_signed" + ".bin" + '\0' */
    out_len = base_len + 7 + 4 + 1;
    out = (char *)malloc(out_len);
    if (out == NULL) {
        fprintf(stderr, "错误: 内存分配失败\n");
        return NULL;
    }

    memcpy(out, input_path, base_len);
    memcpy(out + base_len, "_signed", 7);
    memcpy(out + base_len + 7, ".bin", 5); /* 包含 '\0' */

    return out;
}

/**
 * @brief 以十六进制换行格式打印字节数组
 *
 * 每行 32 字节（64 个十六进制字符），便于阅读。
 *
 * @param fp    输出文件指针
 * @param data  数据
 * @param len   数据长度（字节）
 */
static void print_hex(FILE *fp, const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        fprintf(fp, "%02X", data[i]);
        if ((i + 1) % 32 == 0 && i < len - 1) {
            fprintf(fp, "\n              ");
        }
    }
}

/**
 * @brief 计算 Ed25519 签名并构建 Footer
 *
 * 签名覆盖 fw_data 的全部内容（不含 footer）。
 * 使用 hydro_sign_create 一次性签名 API。
 *
 * @param fw_data   固件二进制数据
 * @param fw_size   固件大小（字节）
 * @param sk        私钥（64 字节）
 * @param version   版本号
 * @param footer    输出：构建好的 Footer
 * @return 0 = 成功, -1 = 失败
 */
static int sign_and_build_footer(const uint8_t *fw_data, size_t fw_size,
                                  const uint8_t sk[hydro_sign_SECRETKEYBYTES],
                                  uint32_t version, FirmwareFooter_t *footer)
{
    uint8_t csig[hydro_sign_BYTES];

    /* Ed25519 签名（覆盖固件本体） */
    if (hydro_sign_create(csig, fw_data, fw_size, FW_SIGN_CONTEXT, sk) != 0) {
        fprintf(stderr, "错误: 签名计算失败\n");
        return -1;
    }

    /* 构建 Footer */
    memset(footer, 0, sizeof(*footer));
    footer->version = version;
    memcpy(footer->signature, csig, FW_SIGNATURE_SIZE);
    footer->footer_magic = FW_FOOTER_MAGIC;
    footer->footer_size  = FW_FOOTER_SIZE;

    return 0;
}

/**
 * @brief 复制文件（用于 --in-place 模式备份）
 *
 * @param src  源文件路径
 * @param dst  目标文件路径
 * @return 0 = 成功, -1 = 失败
 */
static int copy_file(const char *src, const char *dst)
{
    FILE   *f_src = NULL;
    FILE   *f_dst = NULL;
    uint8_t buf[8192];
    size_t  n;
    int     ret = -1;

    f_src = fopen(src, "rb");
    if (f_src == NULL) {
        fprintf(stderr, "错误: 无法打开源文件 '%s': %s\n", src, strerror(errno));
        goto done;
    }

    f_dst = fopen(dst, "wb");
    if (f_dst == NULL) {
        fprintf(stderr, "错误: 无法创建目标文件 '%s': %s\n", dst, strerror(errno));
        goto done;
    }

    while ((n = fread(buf, 1, sizeof(buf), f_src)) > 0) {
        if (fwrite(buf, 1, n, f_dst) != n) {
            fprintf(stderr, "错误: 写入目标文件失败 '%s'\n", dst);
            goto done;
        }
    }

    ret = 0;

done:
    if (f_src) fclose(f_src);
    if (f_dst) fclose(f_dst);
    return ret;
}
