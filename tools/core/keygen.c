/**
 * @file    keygen.c
 * @brief   Ed25519 密钥对生成工具
 *
 * 使用 libhydrogen 生成 Ed25519 签名密钥对（公钥 32 字节，私钥 64 字节）。
 * 输出为十六进制格式，可直接用于 binpkg 签名工具和 BootLoader 配置。
 *
 * 用法：
 *   keygen [-o <前缀>] [-h]
 */

#include "hydrogen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>

/* ============================================================
 *  内部函数声明
 * ============================================================ */

static void print_usage(const char *prog);
static int  write_hex_file(const char *path, const uint8_t *data, size_t len);
static void print_pubkey_c_array(FILE *fp, const uint8_t pk[hydro_sign_PUBLICKEYBYTES]);

/* ============================================================
 *  main
 * ============================================================ */

int main(int argc, char **argv)
{
    const char       *out_prefix = NULL;
    char              hex_buf[hydro_sign_SECRETKEYBYTES * 2 + 1];
    hydro_sign_keypair kp;
    int                opt;

    /* ---- 解析命令行参数 ---- */
    while ((opt = getopt(argc, argv, "o:h")) != -1) {
        switch (opt) {
        case 'o':
            out_prefix = optarg;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    /* ---- 初始化 libhydrogen ---- */
    if (hydro_init() != 0) {
        fprintf(stderr, "错误: RNG 初始化失败\n");
        return 1;
    }

    /* ---- 生成 Ed25519 密钥对 ---- */
    hydro_sign_keygen(&kp);

    /* ---- 公钥 → 十六进制 ---- */
    hydro_bin2hex(hex_buf, sizeof(hex_buf),
                  kp.pk, hydro_sign_PUBLICKEYBYTES);
    printf("PUBKEY:  %s\n", hex_buf);

    /* ---- 私钥 → 十六进制 ---- */
    hydro_bin2hex(hex_buf, sizeof(hex_buf),
                  kp.sk, hydro_sign_SECRETKEYBYTES);
    printf("SECKEY:  %s\n", hex_buf);

    /* ---- 打印 C 数组格式公钥（方便复制到 configBootloader.h）---- */
    printf("\n/* 复制以下内容到 configBootloader.h 的 configED25519_PUBKEY: */\n");
    print_pubkey_c_array(stdout, kp.pk);

    /* ---- 写入文件 ---- */
    if (out_prefix != NULL) {
        char path[1024];

        /* 公钥文件 */
        snprintf(path, sizeof(path), "%s.pub", out_prefix);
        if (write_hex_file(path, kp.pk, hydro_sign_PUBLICKEYBYTES) != 0) {
            return 1;
        }
        printf("\n公钥已写入: %s\n", path);

        /* 私钥文件 */
        snprintf(path, sizeof(path), "%s.key", out_prefix);
        if (write_hex_file(path, kp.sk, hydro_sign_SECRETKEYBYTES) != 0) {
            return 1;
        }
        printf("私钥已写入: %s\n", path);
    }

    return 0;
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
        "用法: %s [-o <前缀>] [-h]\n"
        "\n"
        "生成 Ed25519 密钥对（公钥 32 字节，私钥 64 字节），用于固件签名。\n"
        "\n"
        "选项：\n"
        "  -o <前缀>     将密钥写入 <前缀>.pub 和 <前缀>.key 文件\n"
        "  -h            显示此帮助信息\n"
        "\n"
        "标准输出始终以以下格式打印密钥：\n"
        "  PUBKEY:  <64个十六进制字符>\n"
        "  SECKEY:  <128个十六进制字符>\n"
        "\n"
        "如果指定了 -o，同时写入文件：\n"
        "  <前缀>.pub  — 64个十六进制字符，无换行（公钥，32字节）\n"
        "  <前缀>.key  — 128个十六进制字符，无换行（私钥，64字节）\n"
        "\n"
        "公钥的十六进制格式可直接转换为 configBootloader.h 中\n"
        "configED25519_PUBKEY 的 C 数组格式（标准输出中会附带该格式）。\n",
        name
    );
}

/**
 * @brief 将二进制数据以十六进制字符串写入文件
 *
 * @param path  输出文件路径
 * @param data  二进制数据
 * @param len   数据长度（字节）
 * @return 0 = 成功, -1 = 失败
 */
static int write_hex_file(const char *path, const uint8_t *data, size_t len)
{
    char *hex_buf;
    FILE *fp;

    hex_buf = (char *)malloc(len * 2 + 1);
    if (hex_buf == NULL) {
        fprintf(stderr, "错误: 内存分配失败\n");
        return -1;
    }

    hydro_bin2hex(hex_buf, len * 2 + 1, data, len);

    fp = fopen(path, "w");
    if (fp == NULL) {
        perror("错误: 无法创建密钥文件");
        free(hex_buf);
        return -1;
    }

    fprintf(fp, "%s", hex_buf);
    fclose(fp);
    free(hex_buf);
    return 0;
}

/**
 * @brief 以 C 数组初始化器格式打印 Ed25519 公钥
 *
 * 输出格式与 configBootloader.h 中 configED25519_PUBKEY 一致：
 *   每行 4 字节，0xNN 格式，逗号分隔。
 *
 * @param fp  输出文件指针
 * @param pk  公钥（32 字节）
 */
static void print_pubkey_c_array(FILE *fp, const uint8_t pk[hydro_sign_PUBLICKEYBYTES])
{
    fprintf(fp, "#define configED25519_PUBKEY  { \\\n");
    for (int i = 0; i < hydro_sign_PUBLICKEYBYTES; i++) {
        if (i % 4 == 0) {
            fprintf(fp, "    ");
        }
        fprintf(fp, "0x%02X", pk[i]);
        if (i < hydro_sign_PUBLICKEYBYTES - 1) {
            fprintf(fp, ", ");
        }
        if ((i + 1) % 4 == 0 && i < hydro_sign_PUBLICKEYBYTES - 1) {
            fprintf(fp, "\\\n");
        }
    }
    fprintf(fp, " \\\n}\n");
}
